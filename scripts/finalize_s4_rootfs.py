#!/usr/bin/env python3
"""Apply S4 boot/storage policy after all vendor packages have installed files."""
import argparse
import json
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MDEV_NULL_RULE = b'null 0:0 0666\n'

AIC_D80_FILES = (
    'aic_userconfig_8800d80-Typ.txt',
    'fmacfw_8800d80.bin', 'fmacfw_8800d80_u02.bin',
    'fmacfw_rf_8800d80.bin', 'fmacfw_rf_8800d80_u02.bin',
    'fw_adid_8800d80.bin', 'fw_adid_8800d80_u02.bin',
    'fw_patch_8800d80.bin', 'fw_patch_8800d80_u02.bin',
    'fw_patch_table_8800d80.bin', 'fw_patch_table_8800d80_u02.bin',
    'lmacfw_rf_8800d80.bin', 'lmacfw_rf_8800d80_u02.bin',
)


def validate_wifi_firmware(target, require_alias=False):
    # CONFIG_AIC_FW_PATH in the selected kernel uses this non-SDIO path.
    # The vendor firmware package installs under sdio/. Keep one copy and make
    # the mapping explicit for both the BSP and function driver's file opens.
    directory = target / 'lib/firmware/sdio/aic8800d80'
    alias = target / 'lib/firmware/aic8800d80'
    for path in (directory,) + tuple(directory.parents):
        if path == target:
            break
        if path.is_symlink():
            raise ValueError('symlink in S4 WiFi firmware directory')
    for name in AIC_D80_FILES:
        path = directory / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError('missing or unsafe S4 WiFi firmware: ' + name)
    if alias.is_symlink():
        if os.readlink(str(alias)) != 'sdio/aic8800d80':
            raise ValueError('unexpected S4 WiFi firmware alias')
    elif alias.exists() or require_alias:
        raise ValueError('S4 WiFi firmware alias must be sdio/aic8800d80')
    return alias


def finalize(target):
    target = target.resolve()
    if str(target) == '/':
        raise ValueError('refusing host root')
    contract = json.loads((target / 'etc/dshancontrol/system-release.json').read_text())
    if contract.get('app_id') != 'dshancontrol' or contract.get('board_id') != '100ask-t113s4-sdnand':
        raise ValueError('not a DshanControl S4 target')
    # The vendor MMC environment sets init=/init. A plain ext4 Buildroot build
    # installs only /sbin/init; do not depend on an old target's leftover link.
    busybox = target / 'bin/busybox'
    init = target / 'init'
    if (busybox.parent.is_symlink() or busybox.is_symlink()
            or not busybox.is_file() or not busybox.stat().st_mode & 0o111):
        raise ValueError('S4 boot requires the staged executable bin/busybox')
    if init.is_symlink():
        if os.readlink(str(init)) != 'bin/busybox':
            raise ValueError('unexpected S4 /init symlink; refusing replacement')
    elif init.exists():
        raise ValueError('unexpected S4 /init; refusing replacement')
    wifi_alias = validate_wifi_firmware(target)
    runtime = ROOT / 'boards/t113s4-sdnand/runtime'
    files = [('S60mount_udisk', 'etc/init.d/S60mount_udisk', 0o755),
             ('S72dshancontrol-wifi', 'etc/init.d/S72dshancontrol-wifi', 0o755),
             ('S73dshancontrol-time', 'etc/init.d/S73dshancontrol-time', 0o755),
             ('storage.py', 'usr/lib/dshancontrol-update/storage.py', 0o644),
             ('display.env', 'etc/dshancontrol/display.env', 0o644)]
    # mdev otherwise creates /dev/null as root:root 0660 on this vendor image.
    # Reduced-role subprocesses open it for DEVNULL before running commands.
    mdev = target / 'etc/mdev.conf'
    # Validate all destinations before replacing anything, including symlinked
    # parent directories which could otherwise write outside the staged rootfs.
    for relative in [item[1] for item in files] + ['etc/mdev.conf']:
        destination = target / relative
        if any(p.is_symlink() for p in (destination,) + tuple(destination.parents) if p != target and target in p.parents):
            raise ValueError('refusing symlink destination: ' + relative)
    if mdev.exists() and not mdev.is_file():
        raise ValueError('S4 mdev configuration must be a regular file')
    mdev_original = mdev.read_bytes() if mdev.exists() else b''
    for source, relative, mode in files:
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(runtime / source), str(destination))
        destination.chmod(mode)
    if not mdev_original.startswith(MDEV_NULL_RULE):
        mdev.write_bytes(MDEV_NULL_RULE + mdev_original)
    mdev.chmod(0o644)
    if not init.is_symlink():
        init.symlink_to('bin/busybox')
    if not wifi_alias.is_symlink():
        wifi_alias.symlink_to('sdio/aic8800d80')
    print('S4 rootfs boot/storage policy applied; factory ext4 provisioning still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=Path)
    finalize(parser.parse_args().target)
