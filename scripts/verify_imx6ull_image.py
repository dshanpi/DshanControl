#!/usr/bin/env python3
"""Read-only checks of the i.MX6ULL development image, without mounting it."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path, offset=0, length=None):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        stream.seek(offset)
        while length is None or length > 0:
            block = stream.read(min(length, 1024 * 1024) if length is not None else 1024 * 1024)
            if not block:
                require(length in (None, 0), 'Unexpected end of image')
                break
            digest.update(block)
            if length is not None:
                length -= len(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--buildroot-output', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, evidence = args.buildroot_output.resolve(), args.output.resolve()
    require(not evidence.exists(), 'Use a new evidence directory')
    evidence.mkdir(parents=True)
    images, target = build / 'images', build / 'target'
    report = {'scope': 'Read-only image structure and content validation; no boot or hardware test',
              'passed': False, 'hardware_validation': 'not-run'}

    def run(*command):
        return subprocess.check_output([str(item) for item in command], text=True, stderr=subprocess.PIPE)

    def host_tool(name):
        for path in (build / 'host/sbin' / name, build / 'host/bin' / name):
            if path.is_file():
                return path
        found = shutil.which(name)
        require(found, 'Missing host tool: ' + name)
        return found

    try:
        names = ['u-boot-dtb.imx', 'zImage', 'imx6ull-14x14-evk.dtb', 'rootfs.ext2', 'boot.vfat', 'sdcard.img']
        for name in names:
            require((images / name).is_file() and (images / name).stat().st_size, 'Missing artifact: ' + name)
        report['artifacts'] = {name: {'bytes': (images / name).stat().st_size,
                                     'sha256': sha256(images / name)} for name in names}
        (evidence / 'SHA256SUMS').write_text(''.join(report['artifacts'][name]['sha256'] + '  ' + name + '\n' for name in names))

        disk, rootfs = images / 'sdcard.img', images / 'rootfs.ext2'
        require(rootfs.stat().st_size == 512 * 1024 ** 2, 'Unexpected root filesystem size')
        with disk.open('rb') as stream:
            mbr = stream.read(512)
        require(mbr[510:512] == b'\x55\xaa', 'Invalid MBR signature')
        parts = []
        for i in range(4):
            entry = mbr[446 + 16 * i:462 + 16 * i]
            if entry[4]:
                start, sectors = struct.unpack_from('<II', entry, 8)
                parts.append({'number': i + 1, 'bootable': entry[0] == 128,
                              'type': entry[4], 'offset': start * 512, 'bytes': sectors * 512})
        require(len(parts) == 2, 'Expected two MBR partitions')
        require(parts[0] == {'number': 1, 'bootable': True, 'type': 12,
                             'offset': 8 * 1024 ** 2, 'bytes': 16 * 1024 ** 2}, 'Unexpected boot partition')
        require(parts[1]['type'] == 0x83 and parts[1]['offset'] == 24 * 1024 ** 2
                and parts[1]['bytes'] == rootfs.stat().st_size, 'Unexpected root partition')
        for name, offset in [('u-boot-dtb.imx', 1024), ('boot.vfat', parts[0]['offset']), ('rootfs.ext2', parts[1]['offset'])]:
            require(sha256(disk, offset, (images / name).stat().st_size) == report['artifacts'][name]['sha256'],
                    'Embedded image differs: ' + name)
        report['partitions'] = parts
        report['embedded_images_match'] = True
        require((images / 'u-boot-dtb.imx').stat().st_size + 1024 <= 0xE0000,
                'U-Boot overlaps the configured environment offset')
        boot_bytes = (images / 'u-boot-dtb.imx').read_bytes()
        boot_defaults = ['image=zImage', 'console=ttymxc0', 'mmcdev=1', 'mmcpart=1',
                         'mmcroot=/dev/mmcblk1p2 rootwait rw']
        require(all(value.encode() + b'\0' in boot_bytes for value in boot_defaults),
                'Unexpected compiled U-Boot defaults')
        require(b'setenv fdt_file imx6ull-14x14-evk.dtb' in boot_bytes, 'Missing expected DTB selection')
        report['uboot_compiled_defaults'] = boot_defaults
        kernel_configs = list((build / 'build').glob('linux-*/.config'))
        require(len(kernel_configs) == 1, 'Expected one kernel build configuration')
        kernel_config = kernel_configs[0].read_text().splitlines()
        builtin = ['UNIX', 'DEVTMPFS', 'INPUT_EVDEV', 'TOUCHSCREEN_GT9XX', 'SERIAL_IMX_CONSOLE',
                   'I2C_IMX', 'FB_MXS', 'MMC', 'MMC_SDHCI_ESDHC_IMX', 'EXT4_FS']
        require(all('CONFIG_' + name + '=y' in kernel_config for name in builtin),
                'Missing built-in boot/display/input support')
        report['kernel_required_builtins'] = builtin
        report['buildroot_config_sha256'] = sha256(build / '.config')
        report['kernel_config_sha256'] = sha256(kernel_configs[0])

        fsck = subprocess.run([str(host_tool('e2fsck')), '-fn', str(rootfs)], text=True, capture_output=True)
        (evidence / 'e2fsck.log').write_text(fsck.stdout + fsck.stderr)
        require(fsck.returncode == 0, 'Read-only e2fsck failed')
        superblock = run(host_tool('dumpe2fs'), '-h', rootfs)
        (evidence / 'superblock.txt').write_text(superblock)
        features = re.search(r'^Filesystem features:\s*(.+)$', superblock, re.M).group(1).split()
        require('extent' in features and not set(features) & {'64bit', 'metadata_csum_seed', 'orphan_file'},
                'Unexpected filesystem feature set')
        report['filesystem_features'] = features

        debugfs = host_tool('debugfs')

        def fsread(path):
            data = run(debugfs, '-R', 'cat ' + path, rootfs)
            require(data, 'Missing or empty rootfs file: ' + path)
            return data

        installed = ['usr/bin/' + name for name in ('omnigate-hmi', 'dshancontrol-desktop-service',
                     'dshancontrol-media-worker', 'dshancontrol-runtime')]
        installed += ['usr/lib/qt/plugins/platforms/libqlinuxfb.so', 'usr/lib/qt/plugins/platforms/libqoffscreen.so',
                      'etc/init.d/S99dshancontrol', 'etc/init.d/S99lvgl9-demo', 'etc/dshancontrol/display.env',
                      'usr/share/dshancontrol/platform.json', 'usr/lib/dshancontrol/web/app.py',
                      'usr/lib/dshancontrol/web/hmi_api.py', 'etc/fonts/fonts.conf']
        for pattern in ('usr/lib/libfontconfig.so.*', 'usr/share/fonts/**/*.ttf'):
            matches = [p for p in target.glob(pattern) if p.is_file() and not p.is_symlink()]
            require(matches, 'Missing target files: ' + pattern)
            installed.extend(str(p.relative_to(target)) for p in matches)
        report['installed_files_sha256'] = {}
        with tempfile.TemporaryDirectory(prefix='dshan-image-check-') as temporary:
            for index, path in enumerate(installed):
                extracted = Path(temporary) / str(index)
                run(debugfs, '-R', 'dump /' + path + ' ' + str(extracted), rootfs)
                require(extracted.is_file() and sha256(extracted) == sha256(target / path), 'Rootfs content mismatch: ' + path)
                report['installed_files_sha256'][path] = sha256(extracted)
            for name in ('zImage', 'imx6ull-14x14-evk.dtb'):
                extracted = Path(temporary) / name
                run(host_tool('mcopy'), '-i', images / 'boot.vfat', '::/' + name, extracted)
                require(sha256(extracted) == report['artifacts'][name]['sha256'], 'FAT content mismatch: ' + name)

        passwd, group = fsread('/etc/passwd'), fsread('/etc/group')
        users = {line.split(':')[0]: line.split(':') for line in passwd.splitlines()}
        for name, uid in [('dshan-web', 400), ('dshan-hmi', 401), ('dshan-desktop', 402)]:
            require(users[name][2:4] == [str(uid), '400'], 'Incorrect identity: ' + name)
        groups = {line.split(':')[0]: line.split(':') for line in group.splitlines()}
        require(all('dshan-hmi' in groups[name][3].split(',') for name in ('video', 'input')), 'Missing HMI device groups')
        report['service_identities'] = {name: {'uid': int(users[name][2]), 'gid': int(users[name][3])}
                                        for name in ('dshan-web', 'dshan-hmi', 'dshan-desktop')}
        require('exit 0' in fsread('/etc/init.d/S99lvgl9-demo'), 'LVGL start was not disabled')
        for path in ('/etc/init.d/S99dshancontrol', '/etc/init.d/S99lvgl9-demo'):
            stat = run(debugfs, '-R', 'stat ' + path, rootfs)
            require(re.search(r'Mode:\s+0755', stat), 'Init script is not executable: ' + path)
        manifest = json.loads(fsread('/usr/share/dshancontrol/platform.json'))
        require(manifest['display']['width'] == 1024 and manifest['display']['height'] == 600, 'Incorrect display manifest')

        fdtget, dtb = host_tool('fdtget'), images / 'imx6ull-14x14-evk.dtb'

        def prop(node, name, kind='u'):
            return run(fdtget, '-t', kind, dtb, node, name).strip()

        nodes = []
        def walk(node):
            nodes.append(node)
            for child in run(fdtget, '-l', dtb, node).splitlines():
                walk(node.rstrip('/') + '/' + child)
        walk('/')
        props = {node: run(fdtget, '-p', dtb, node).splitlines() for node in nodes}
        timings = [node for node in nodes if 'hactive' in props[node] and 'vactive' in props[node]]
        touch = [node for node in nodes if 'touchscreen-size-x' in props[node]]
        require(len(timings) == 1 and len(touch) == 1, 'Unexpected display/touch nodes')
        require(prop(timings[0], 'hactive') == '1024' and prop(timings[0], 'vactive') == '600', 'Incorrect LCD timing size')
        require(prop(touch[0], 'touchscreen-size-x') == '1024' and prop(touch[0], 'touchscreen-size-y') == '600', 'Incorrect touch size')
        mmc = prop('/aliases', 'mmc1', 's')
        require(prop(mmc, 'bus-width') == '4' and prop(mmc, 'max-frequency') == '50000000'
                and 'no-1-8-v' in props[mmc], 'Incorrect eMMC compatibility configuration')
        report['device_tree'] = {'lcd_node': timings[0], 'touch_node': touch[0], 'lcd': [1024, 600],
                                 'touch': [1024, 600], 'mmc1_node': mmc, 'bus_width': 4,
                                 'max_frequency': 50000000, 'no_1_8_v': True}
        (evidence / 'device-tree.dts').write_text(run(host_tool('dtc'), '-I', 'dtb', '-O', 'dts', dtb))
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
