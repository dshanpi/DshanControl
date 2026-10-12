#!/usr/bin/env python3
"""Read-only S4 image preflight; an application build is not a system image."""
import argparse
import json
import re
import shutil
import struct
from pathlib import Path


def ext4_image_bytes(path):
    with path.open('rb') as stream:
        stream.seek(1024)
        superblock = stream.read(1024)
    if len(superblock) != 1024 or superblock[56:58] != b'\x53\xef':
        raise ValueError('expected a raw ext4 image, not an Android sparse image or arbitrary file')
    shift = struct.unpack_from('<I', superblock, 24)[0]
    if shift > 6:
        raise ValueError('invalid ext4 block size')
    blocks = struct.unpack_from('<I', superblock, 4)[0]
    if struct.unpack_from('<I', superblock, 96)[0] & 0x80:
        blocks += struct.unpack_from('<I', superblock, 0x150)[0] << 32
    size = blocks * (1024 << shift)
    if size < 2048 or size > path.stat().st_size:
        raise ValueError('truncated ext4 image')
    return max(size, path.stat().st_size)


def partitions(text):
    result, current = [], None
    for line in text.splitlines():
        line = line.split(';', 1)[0].strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('['):
            current = {} if line == '[partition]' else None
            if current is not None:
                result.append(current)
        elif current is not None:
            key, value = line.split('=', 1)
            key = key.strip()
            if key in current:
                raise ValueError('duplicate partition field: ' + key)
            current[key] = value.strip().strip('"')
    names = [p.get('name') for p in result]
    if not names or any(not n for n in names) or len(set(names)) != len(names):
        raise ValueError('invalid/duplicate partition names')
    for index, item in enumerate(result):
        value = item.get('size')
        if value is None:
            if index != len(result) - 1 or item['name'] != 'UDISK':
                raise ValueError('only final UDISK may use remaining capacity')
            item['size_bytes'] = None
        else:
            match = re.fullmatch(r'([0-9]+)([BKMGbkmg]?)', value)
            if not match:
                raise ValueError('unsupported partition size: ' + value)
            amount = int(match[1]) * {'': 512, 'B': 1, 'K': 1024, 'M': 1024**2, 'G': 1024**3}[match[2].upper()]
            if amount <= 0 or amount % 512:
                raise ValueError('partition size must be positive whole sectors')
            item['size_bytes'] = amount
    return result


def selected_inputs(hardware):
    chip = hardware / 'device/config/chips/t113_s4'
    board = chip / 'configs/100ask'
    # Tina build/pack copies kernel sys_* then the selected platform directory.
    partition = board / 'buildroot/sys_partition.fex'
    candidates = [chip / 'configs/default/env.cfg', board / 'env.cfg',
                  board / 'linux-5.4/env.cfg', board / 'buildroot/env.cfg']
    env = next((p for p in reversed(candidates) if p.is_file()), None)
    if not partition.is_file() or env is None:
        raise ValueError('missing approved Buildroot partition/environment input')
    return board, partition, env


def audit(hardware, buildroot, capacity_bytes=None, rootfs_image=None):
    board, partition, env = selected_inputs(hardware)
    layout = partitions(partition.read_text())
    errors, pending = [], []
    if not re.search(r'^storage_type\s*=\s*2\s*$', (board / 'sys_config.fex').read_text(), re.M):
        errors.append('board storage_type is not SD NAND baseline 2')
    root = next((p for p in layout if p['name'] == 'rootfs'), None)
    udisk = next((p for p in layout if p['name'] == 'UDISK'), None)
    if root is None or not root['size_bytes'] or udisk is None:
        errors.append('rootfs and persistent UDISK partitions required')
    config = (buildroot / '.config').read_text()
    size = re.search(r'^BR2_TARGET_ROOTFS_EXT2_SIZE="([0-9]+)([MG])"$', config, re.M)
    declared = int(size[1]) * (1024**2 if size[2] == 'M' else 1024**3) if size else None
    if not declared or 'BR2_TARGET_ROOTFS_EXT2_4=y' not in config.splitlines():
        errors.append('explicit ext4 rootfs size required')
    if root and declared and declared > root['size_bytes']:
        errors.append('configured rootfs exceeds selected partition')
    if rootfs_image:
        if not rootfs_image.is_file():
            errors.append('rootfs image missing')
        else:
            try:
                actual = ext4_image_bytes(rootfs_image)
                if root and actual > root['size_bytes']:
                    errors.append('rootfs image exceeds selected partition')
            except ValueError as error:
                errors.append(str(error))
    else:
        pending.append('complete rootfs image not supplied')
    environment = dict(line.split('=', 1) for line in env.read_text().splitlines()
                       if '=' in line and not line.lstrip().startswith('#'))
    if environment.get('bootcmd') != 'run setargs_mmc boot_normal':
        errors.append('unexpected boot command; review MMC boot path')
    if 'root=${mmc_root}' not in environment.get('setargs_mmc', ''):
        errors.append('MMC root parameter missing')
    if root and not re.fullmatch(r'/dev/mmcblk[0-9]+p' + str(layout.index(root) + 1), environment.get('mmc_root', '')):
        errors.append('MMC root device index differs from selected partition table')
    if not (buildroot / 'target/init').exists():
        pending.append('vendor /init not installed; application target alone cannot boot')
    fixed = sum(p['size_bytes'] or 0 for p in layout)
    # Conservative engineering reserve, not a measurement of the physical GPT.
    reserve = 32 * 1024**2
    minimum_data = 256 * 1024**2
    required = fixed + reserve + minimum_data
    if capacity_bytes is None:
        pending.append('actual SD NAND capacity required')
    elif capacity_bytes < required:
        errors.append('SD NAND capacity below fixed partitions + 32 MiB reserve + 256 MiB data budget')
    free = shutil.disk_usage(buildroot).free
    if free < 16 * 1024**3:
        pending.append('less than 16 GiB free for full SDK/image build')
    script = buildroot / 'target/etc/init.d/S60mount_udisk'
    if not script.is_file() or 'storage.py --mount' not in script.read_text():
        errors.append('S4 non-formatting ext4 mount policy not installed')
    pending.extend(['UDISK ext4 factory provisioning not verified', 'kernel/bootloader image chain not verified',
                    'hardware function/power-loss/rollback acceptance not run'])
    return dict(schema_version=1, release_kind='system', status='development-draft',
                board='t113_s4/100ask', rootfs='buildroot', partition_file=str(partition),
                env_file=str(env), partitions=layout, configured_rootfs_bytes=declared,
                fixed_partition_bytes=fixed, reserved_budget_bytes=reserve,
                minimum_data_budget_bytes=minimum_data, minimum_capacity_budget_bytes=required,
                actual_capacity_bytes=capacity_bytes, host_free_bytes=free,
                configuration_errors=errors, pending=pending, publishable=False)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hardware', required=True, type=Path)
    p.add_argument('--buildroot', required=True, type=Path)
    p.add_argument('--capacity-bytes', type=int)
    p.add_argument('--rootfs-image', type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    report = audit(a.hardware.resolve(), a.buildroot.resolve(), a.capacity_bytes, a.rootfs_image)
    a.output.write_text(json.dumps(report, indent=2) + '\n')
    print('S4 preflight: {} configuration errors; {} unresolved items'.format(
        len(report['configuration_errors']), len(report['pending'])))
    raise SystemExit(1 if report['configuration_errors'] else 0)
