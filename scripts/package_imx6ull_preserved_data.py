#!/usr/bin/env python3
"""Prepare p1/p2 images for the observed 32/768/256 MiB layout; never flash.

The UUU recipe must be reviewed and saved in the board's MCP channel. Runtime
identity, backup and maintenance checks remain mandatory before executing it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--buildroot-output', type=Path, required=True)
    parser.add_argument('--validated-image-report', type=Path, required=True)
    parser.add_argument('--cid', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{32}', args.cid):
        parser.error('Expected the observed 128-bit eMMC CID')
    build, output = args.buildroot_output.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Use a new output directory')
    images = build / 'images'
    report = json.loads(args.validated_image_report.read_text())
    if not report.get('passed'):
        parser.error('Base image verification must pass')
    for name in ('rootfs.ext2', 'zImage', 'imx6ull-14x14-evk.dtb'):
        if digest(images / name) != report['artifacts'][name]['sha256']:
            parser.error('Build artifact changed after verification: ' + name)
    output.mkdir(parents=True)

    def host_tool(name):
        for path in (build / 'host/sbin' / name, build / 'host/bin' / name):
            if path.is_file():
                return str(path)
        return shutil.which(name) or name

    def run(*command):
        result = subprocess.run([str(item) for item in command], text=True, capture_output=True)
        with (output / 'preparation.log').open('a') as log:
            log.write(' '.join(map(str, command)) + '\n' + result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError('Artifact preparation failed: ' + str(command[0]))
        return result.stdout

    rootfs = output / 'rootfs-preserve.ext4'
    shutil.copy2(images / 'rootfs.ext2', rootfs)
    with rootfs.open('r+b') as stream:
        stream.truncate(768 * 1024 ** 2)
    run(host_tool('resize2fs'), rootfs)
    marker = output / 'require-data-partition'
    marker.write_text('An independent /dev/mmcblk1p3 is required by this deployment.\n')
    run(host_tool('debugfs'), '-w', '-R', 'write ' + str(marker) + ' /etc/dshancontrol/require-data-partition', rootfs)
    if 'required by this deployment' not in run(host_tool('debugfs'), '-R', 'cat /etc/dshancontrol/require-data-partition', rootfs):
        raise RuntimeError('Persistent-storage requirement was not embedded')
    run(host_tool('e2fsck'), '-fn', rootfs)
    for name in ('zImage', 'imx6ull-14x14-evk.dtb'):
        shutil.copy2(images / name, output / name)
    config = output / 'genimage-boot.cfg'
    config.write_text('''image boot-preserve.vfat {
  vfat {
    label = "boot"
    files = { "zImage", "imx6ull-14x14-evk.dtb" }
  }
  size = 32M
}
''')
    run(host_tool('genimage'), '--rootpath', build / 'target', '--tmppath', output / 'genimage-tmp',
        '--inputpath', output, '--outputpath', output, '--config', config)
    for name in ('zImage', 'imx6ull-14x14-evk.dtb'):
        extracted = output / (name + '.readback')
        run(host_tool('mcopy'), '-i', output / 'boot-preserve.vfat', '::/' + name, extracted)
        if digest(extracted) != digest(images / name):
            raise RuntimeError('FAT readback mismatch: ' + name)
        extracted.unlink()
    # MBR fields are copied to aligned RAM before 32-bit comparisons on ARM.
    lines = ['uuu_version 1.4.193', '# Prepared recipe; requires live target/backup/MCP checks.',
             '# No MBR, bootloader, environment or p3 writes.',
             'FB: ucmd setenv target_ubootdev 1', 'FB: ucmd setenv fastboot_dev mmc',
             'FB: ucmd mmc dev 1 0', 'FB: ucmd mmc read 0x82000000 0 1',
             'FB: ucmd itest.w *0x820001fe == 0xaa55',
             'FB: ucmd cp.b 0x820001be 0x82001000 0x40']
    partitions = [(16384, 65536, 0x0c), (81920, 1572864, 0x83), (1654784, 524288, 0x83)]
    for index, (start, sectors, kind) in enumerate(partitions):
        address = 0x82001000 + index * 16
        lines.extend([f'FB: ucmd itest.b *0x{address + 4:08x} == 0x{kind:x}',
                      f'FB: ucmd itest.l *0x{address + 8:08x} == 0x{start:x}',
                      f'FB: ucmd itest.l *0x{address + 12:08x} == 0x{sectors:x}'])
    lines += ['FB: ucmd itest.b *0x82001034 == 0',
              'FB: getvar partition-size:mmcsdb1', 'FB: getvar partition-size:mmcsdb2',
              # -S limits the transfer buffer despite an oversized U-Boot advertisement.
              'FB: flash -raw2sparse -S 0x01000000 mmcsdb1 boot-preserve.vfat',
              'FB: flash -raw2sparse -S 0x01000000 mmcsdb2 rootfs-preserve.ext4', 'FB: done']
    recipe = output / 'preserve-data.uuu'
    recipe.write_text('\n'.join(lines) + '\n')
    files = {name: {'bytes': (output / name).stat().st_size, 'sha256': digest(output / name)}
             for name in ('boot-preserve.vfat', 'rootfs-preserve.ext4', 'preserve-data.uuu')}
    (output / 'SHA256SUMS').write_text(''.join(value['sha256'] + '  ' + name + '\n' for name, value in files.items()))
    manifest = {'scope': 'Prepared p1/p2 replacement; not flashed or hardware-qualified',
        'target': {'board': 'i.MX6ULL Pro', 'emmc_cid': args.cid, 'device': 'mmc1',
                   'partitions': [{'number': n + 1, 'start_lba': p[0], 'sectors': p[1]} for n, p in enumerate(partitions)]},
        'files': files, 'requires_data_partition': True, 'maximum_transfer_bytes': 16 * 1024 ** 2,
        'retains': ['MBR', 'U-Boot', 'U-Boot environment', 'p3 /data', 'eMMC hardware boot partitions'],
        'pending': ['Supported NXP MCP mode transition', 'Live CID and bound USB verification',
                    'Quiesced p2/p3 rollback backups', 'Review/save this UUU recipe in the channel',
                    'Readback hashes before mounting rootfs, normal boot and device validation'],
        'base_image_sha256': report['artifacts']['sdcard.img']['sha256']}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
