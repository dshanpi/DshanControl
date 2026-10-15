#!/usr/bin/env python3
"""Mount/check the S4 persistent volume. Never format or repair user storage."""
import os
import re
import stat
import subprocess
import sys
from pathlib import Path

MOUNT = Path('/mnt/UDISK')


def device_from_cmdline(cmdline):
    values = [v.split('=', 1)[1] for v in cmdline.split() if v.startswith('partitions=')]
    if len(values) != 1:
        raise ValueError('exactly one boot partition map required')
    names = [v.split('@', 1)[1] for v in values[0].split(':') if v.startswith('UDISK@')]
    if len(names) != 1 or not re.fullmatch(r'mmcblk[0-9]+p[1-9][0-9]*', names[0]):
        raise ValueError('missing/ambiguous SD NAND UDISK partition')
    return Path('/dev') / names[0]


def storage_device():
    # Resolve the actual bootloader map; never guess a partition number.
    device = device_from_cmdline(Path('/proc/cmdline').read_text())
    info = device.stat()
    if not stat.S_ISBLK(info.st_mode):
        raise ValueError('UDISK is not a block device')
    alias = Path('/dev/by-name/UDISK')
    if alias.exists() or alias.is_symlink():
        if alias.resolve() != device.resolve():
            raise ValueError('UDISK alias differs from boot partition map')
    return device, '{}:{}'.format(os.major(info.st_rdev), os.minor(info.st_rdev))


def check_mount(mountinfo, identity):
    rows = [line.split() for line in mountinfo.splitlines() if line.strip()]
    matches = [row for row in rows if len(row) > 6 and row[4] == str(MOUNT)]
    if not matches:
        return False
    if len(matches) != 1:
        raise ValueError('stacked UDISK mounts refused')
    row = matches[0]
    split = row.index('-')
    if (row[2] != identity or row[3] != '/' or row[split + 1] != 'ext4' or
            'rw' not in row[5].split(',') or 'rw' not in row[split + 3].split(',')):
        raise ValueError('UDISK must be the expected writable ext4 partition')
    return True


def ensure_storage(mount=False):
    device, identity = storage_device()
    if MOUNT.is_symlink():
        raise ValueError('UDISK mountpoint must not be a symlink')
    if check_mount(Path('/proc/self/mountinfo').read_text(), identity):
        return
    if not mount:
        raise ValueError('UDISK is not mounted; refusing rootfs data writes')
    # blkid's basic invocation is supported by both BusyBox and util-linux.
    result = subprocess.run(['blkid', str(device)], check=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True, timeout=10)
    if not re.search(r'(?:^|\s)TYPE="ext4"(?:\s|$)', result.stdout):
        raise ValueError('UDISK needs factory-provisioned ext4; existing data left untouched')
    MOUNT.mkdir(parents=True, exist_ok=True)
    subprocess.run(['mount', '-t', 'ext4', '-o', 'rw,nosuid,nodev', str(device), str(MOUNT)],
                   check=True, timeout=30)
    if not check_mount(Path('/proc/self/mountinfo').read_text(), identity):
        raise ValueError('UDISK mount verification failed')


if __name__ == '__main__':
    try:
        ensure_storage(mount=sys.argv[1:] == ['--mount'])
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('DshanControl storage refused: ' + str(error), file=sys.stderr)
        sys.exit(1)
