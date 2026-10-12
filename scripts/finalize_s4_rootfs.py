#!/usr/bin/env python3
"""Apply S4 storage policy after all vendor packages have installed their files."""
import argparse
import json
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
    runtime = ROOT / 'boards/t113s4-sdnand/runtime'
    for source, relative, mode in [('S60mount_udisk', 'etc/init.d/S60mount_udisk', 0o755),
                                   ('storage.py', 'usr/lib/dshancontrol-update/storage.py', 0o644)]:
        destination = target / relative
        if destination.is_symlink():
            raise ValueError('refusing symlink destination: ' + relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(runtime / source), str(destination))
        destination.chmod(mode)
    print('S4 rootfs storage policy applied; factory ext4 provisioning still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=Path)
    finalize(parser.parse_args().target)
