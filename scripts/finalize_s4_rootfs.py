#!/usr/bin/env python3
"""Apply S4 boot/storage policy after all vendor packages have installed files."""
import argparse
import json
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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
    runtime = ROOT / 'boards/t113s4-sdnand/runtime'
    files = [('S60mount_udisk', 'etc/init.d/S60mount_udisk', 0o755),
             ('storage.py', 'usr/lib/dshancontrol-update/storage.py', 0o644),
             ('display.env', 'etc/dshancontrol/display.env', 0o644)]
    # Validate all destinations before replacing anything, including symlinked
    # parent directories which could otherwise write outside the staged rootfs.
    for _, relative, _ in files:
        destination = target / relative
        if any(p.is_symlink() for p in (destination,) + tuple(destination.parents) if p != target and target in p.parents):
            raise ValueError('refusing symlink destination: ' + relative)
    for source, relative, mode in files:
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(runtime / source), str(destination))
        destination.chmod(mode)
    if not init.is_symlink():
        init.symlink_to('bin/busybox')
    print('S4 rootfs boot/storage policy applied; factory ext4 provisioning still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=Path)
    finalize(parser.parse_args().target)
