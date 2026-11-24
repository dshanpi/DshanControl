#!/usr/bin/env python3
"""Exercise storage startup in disposable chroots with a fake mount command.

Run as root on the Ubuntu build host. No real device is opened or mounted.
"""
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / 'boards/imx6ull-pro/runtime'
if os.geteuid() != 0:
    raise SystemExit('Run this isolated chroot check with sudo on the build host')
results = []


def check(name, *, partition=False, mounts='', required=False, mount_fails=False,
          symlink=False, script='S35dshan-data', expected=None):
    with tempfile.TemporaryDirectory(prefix='dshan-storage-test-') as temporary:
        root = Path(temporary)
        for directory in ('bin', 'dev', 'proc', 'run', 'etc/init.d', 'etc/dshancontrol', 'data'):
            (root / directory).mkdir(parents=True, exist_ok=True)
        shutil.copy2('/usr/bin/busybox', root / 'bin/busybox')
        for applet in ('sh', 'rm', 'mkdir', 'awk'):
            (root / 'bin' / applet).symlink_to('busybox')
        for source in SOURCE.iterdir():
            shutil.copy2(source, root / 'etc/init.d' / source.name)
        (root / 'bin/mount').write_text('''#!/bin/sh
printf '%s\\n' "$*" > /run/mount-command
[ ! -e /run/mount-must-fail ] || exit 1
printf '%s\\n' '/dev/mmcblk1p3 /data ext4 rw,noatime,nosuid,nodev 0 0' >> /proc/mounts
''')
        (root / 'bin/mount').chmod(0o755)
        (root / 'proc/mounts').write_text(mounts)
        (root / 'data/sentinel').write_text('preserve user data\n')
        if partition:
            # An unopened dummy node; only shell -b tests see it. mount is a stub.
            os.mknod(root / 'dev/mmcblk1p3', stat.S_IFBLK | 0o600, os.makedev(7, 250))
        if required:
            (root / 'etc/dshancontrol/require-data-partition').touch()
        if mount_fails:
            (root / 'run/mount-must-fail').touch()
        if symlink:
            (root / 'data').rename(root / 'retained')
            (root / 'data').symlink_to('/retained')
        result = subprocess.run([shutil.which('chroot'), str(root), '/bin/sh', '/etc/init.d/' + script, 'start'],
                                capture_output=True, text=True, env={'PATH': '/bin'}, timeout=5)
        marker = root / 'run/dshancontrol-storage-ready'
        if expected is None:
            assert result.returncode != 0 and not marker.exists(), (name, result)
        else:
            assert result.returncode == 0 and marker.read_text().strip() == expected, (name, result)
        sentinel = root / ('retained' if symlink else 'data') / 'sentinel'
        assert sentinel.read_text() == 'preserve user data\n', name
        results.append({'case': name, 'passed': True, 'exit': result.returncode})


check('fresh-development', expected='development-rootfs')
check('required-partition-missing', required=True)
check('mount-existing-p3', partition=True, required=True, expected='emmc-p3')
check('failed-mount-blocks-services', partition=True, mount_fails=True)
check('already-mounted-p3', partition=True, mounts='/dev/mmcblk1p3 /data ext4 rw,nosuid 0 0\n', expected='emmc-p3')
check('wrong-device-mounted', partition=True, mounts='/dev/mmcblk0p3 /data ext4 rw 0 0\n')
check('readonly-p3', partition=True, mounts='/dev/mmcblk1p3 /data ext4 ro 0 0\n')
check('unrelated-data-mount', mounts='tmpfs /data tmpfs rw 0 0\n')
check('symlink-data', symlink=True)
check('desktop-requires-storage', script='S99dshancontrol')
check('wifi-requires-storage', script='S41wifi')
print(json.dumps({'scope': 'Host shell in disposable chroots, fake mount, no physical hardware', 'checks': results}, indent=2))
