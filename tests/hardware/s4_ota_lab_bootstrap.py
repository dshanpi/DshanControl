#!/usr/bin/env python3
"""Temporary, root-operated S4 GUI-only OTA lab mounts; never shipped in images."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess

PERSISTENT = Path('/mnt/UDISK/dshancontrol-qt-ota-lab-20261015')
RUNTIME = Path('/run/dshancontrol-qt-ota-lab-20261015')
CID = '1212340050ffffb0001280096a0196ef'


def require(value, message):
    if not value:
        raise RuntimeError(message)


def private_object(path):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and
            stat.S_IMODE(info.st_mode) == 0o600 and info.st_nlink == 1,
            'Private single-link root configuration required')
    return json.loads(path.read_text())


def roots():
    require(os.geteuid() == 0, 'Root lab operator required')
    require(Path('/sys/block/mmcblk0/device/cid').read_text().strip() == CID,
            'Bound board mismatch')
    for path in [PERSISTENT, PERSISTENT / 'system']:
        info = path.lstat()
        require(stat.S_ISDIR(info.st_mode) and info.st_uid == 0 and
                not info.st_mode & 0o022, 'Untrusted lab root')
    context = private_object(PERSISTENT / 'system/context.json')
    require(context.get('development_only') is True and context.get('cid') == CID,
            'Explicit bound development context required')
    original = PERSISTENT / 'system/original-updater.json'
    require(hashlib.sha256(original.read_bytes()).hexdigest() == context['original_updater_sha256'],
            'Original system configuration changed')
    production = private_object(original)
    require(production.get('development_mode') is False and
            production.get('application_root') == '/mnt/UDISK/dshancontrol/apps' and
            production.get('system_contract') == '/etc/dshancontrol/system-release.json',
            'Original fixed production configuration required')
    require(hashlib.sha256(Path('/etc/dshancontrol/system-release.json').read_bytes()).hexdigest() ==
            context['formal_contract_sha256'], 'Formal system identity changed')
    config = private_object(Path('/etc/dshancontrol/updater.json'))
    require(config.get('development_mode') is True and config.get('automatic_install') is False and
            config.get('application_root') == str(RUNTIME / 'apps') and
            config.get('system_contract') == str(PERSISTENT / 'system/contract.json'),
            'Fixed development lab routing required')
    contract = private_object(PERSISTENT / 'system/contract.json')
    require(contract.get('hardware_revision') == 'development-fixture-unqualified' and
            contract.get('data_root') == str(RUNTIME / 'data'), 'Explicit fixture contract required')
    return context, original


def no_roles():
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            fields = dict(line.split(':', 1) for line in (process / 'status').read_text().splitlines()
                          if ':' in line)
        except (FileNotFoundError, ProcessLookupError):
            continue
        if fields.get('State', '').strip().split()[:1] in [['Z'], ['X']]:
            continue
        require(not any(value in ('400', '401') for value in fields.get('Uid', '').split()),
                'Application roles still active')
    require(not Path('/run/dshancontrol-runtime.pid').exists(), 'Stop system daemon before lab mounts')


def storage_routing(config, formal):
    require(config.get('development_mode') is True and config.get('automatic_install') is False and
            config.get('application_root') == str(RUNTIME / 'apps') and
            config.get('system_contract') == str(PERSISTENT / 'system/contract.json'),
            'Fixed development lab routing required before storage')
    require(formal.get('board_id') == '100ask-t113s4-sdnand' and
            formal.get('storage') == dict(mount_point='/mnt/UDISK', device='/dev/by-name/UDISK',
                                         filesystem='ext4'),
            'Original S4 storage contract required')


def prepare_storage():
    # The default lab contract is on UDISK. Mount it with the original rootfs
    # contract before S60 tries to read the lab contract; no formatting fallback.
    require(os.geteuid() == 0, 'Root lab operator required')
    require(Path('/sys/block/mmcblk0/device/cid').read_text().strip() == CID,
            'Bound board mismatch')
    formal_path = Path('/etc/dshancontrol/system-release.json')
    info = formal_path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and
            info.st_nlink == 1 and not info.st_mode & 0o022,
            'Root-owned original storage contract required')
    storage_routing(private_object(Path('/etc/dshancontrol/updater.json')),
                    json.loads(formal_path.read_text()))
    no_roles()
    temporary = Path('/run/dshancontrol-qt-ota-storage.json')
    with temporary.open('x') as stream:
        os.fchmod(stream.fileno(), 0o600)
        json.dump(dict(development_mode=False, system_contract=str(formal_path)), stream)
        stream.flush()
        os.fsync(stream.fileno())
    if not Path('/dev/by-name/UDISK').exists():
        subprocess.run(['/bin/sh', '-c', '. /etc/partition_op.sh; do_device_node_switch'], check=True)
    subprocess.run(['/usr/bin/dshancontrol-runtime', '--config', str(temporary), 'storage-mount'],
                   check=True)
    roots()  # Check pinned original configuration/formal identity after the mount.
    return dict(storage_mounted_by_native_policy=True, original_contract_used=True,
                development_only=True)


def mounts():
    expected = os.stat('/dev/mmcblk0p9').st_rdev
    for name in ['apps', 'data']:
        source = PERSISTENT / name
        target = RUNTIME / name
        require(source.is_dir() and not source.is_symlink() and
                target.is_dir() and not target.is_symlink() and
                target.stat().st_dev == expected and
                target.stat().st_ino == source.stat().st_ino, 'Lab mount identity mismatch')


def start():
    context, original = roots()
    no_roles()
    require(not RUNTIME.exists() and not RUNTIME.is_symlink(),
            'Lab runtime already exists; inspect retained state, no replay')
    # Establish actual device grants through the unchanged production policy.
    subprocess.run(['/usr/bin/dshancontrol-runtime', '--config', str(original), 'initialize'], check=True)
    RUNTIME.mkdir(mode=0o750)
    os.chown(str(RUNTIME), 0, 400)
    for name in ['apps', 'data']:
        source = PERSISTENT / name
        info = source.lstat()
        require(stat.S_ISDIR(info.st_mode), 'Plain persistent lab directory required')
        target = RUNTIME / name
        target.mkdir(mode=0o750)
        subprocess.run(['/bin/mount', '--bind', str(source), str(target)], check=True)
    mounts()
    for name in ['fixture-fb', 'fixture-touch']:
        with (RUNTIME / name).open('xb') as stream:
            stream.write(b'LAB PERMISSION FIXTURE; NOT A DEVICE\n')
        (RUNTIME / name).chmod(0o600)
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    receipt = dict(boot_id=boot, mounts_verified=True, actual_devices_prepared_by_production_policy=True,
                   original_data_contents_written=False, development_only=True)
    with (PERSISTENT / 'system' / ('boot-' + boot + '.json')).open('x') as stream:
        json.dump(receipt, stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    return receipt


def stop():
    roots()
    no_roles()
    mounts()
    # No force-unmount and no persistent-data deletion.
    for name in ['apps', 'data']:
        subprocess.run(['/bin/umount', str(RUNTIME / name)], check=True)
    return dict(unmounted=True, persistent_data_preserved=True, development_only=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['start', 'stop', 'storage'])
    args = parser.parse_args()
    os.umask(0o077)
    try:
        value = prepare_storage() if args.action == 'storage' else start() if args.action == 'start' else stop()
        print(json.dumps(dict(ok=True, **value)), flush=True)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        print(json.dumps(dict(ok=False, error=str(error), state_retained=True, no_replay=True)), flush=True)
        raise SystemExit(1)
