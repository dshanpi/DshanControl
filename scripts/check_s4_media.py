#!/usr/bin/env python3
"""Reject an S4 desktop payload built without the Cedar playback backend.

Static ELF evidence only: this does not qualify decoding, display, audio,
resource reclamation, target libraries or an installation for field use.
"""
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
from pathlib import Path

REQUIRED_IMPORTS = frozenset(('TPlayerCreate', 'TPlayerDestroy',
    'TPlayerSetDataSource', 'TPlayerPrepareAsync', 'TPlayerStart', 'TPlayerStop',
    'TPlayerSetDisplayRect', 'TPlayerSetNotifyCallback'))
REQUIRED_LIBRARIES = frozenset(('libtplayer.so', 'libcdx_base.so'))


def verify(path, readelf='readelf'):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not path.stat().st_mode & 0o111:
        raise ValueError('S4 media worker must be a regular executable artifact')
    raw = path.read_bytes()
    if len(raw) < 52 or raw[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('S4 media worker requires little-endian ELF32 ARM')
    kind, machine = struct.unpack_from('<HH', raw, 16)
    flags = struct.unpack_from('<I', raw, 36)[0]
    if machine != 40 or kind not in (2, 3) or flags >> 24 != 5 or flags & 0x400:
        raise ValueError('S4 media worker requires ARM EABI5 soft-float ABI')
    environment = dict(os.environ, LC_ALL='C')
    def inspect(*options):
        try:
            result = subprocess.run([str(readelf)] + list(options) + [str(path.resolve())],
                check=False, capture_output=True, text=True, timeout=30, env=environment)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise ValueError('cannot inspect S4 media ELF: ' + str(error)) from error
        if result.returncode or result.stderr.strip():
            raise ValueError('invalid S4 media ELF: ' + result.stderr.strip())
        return result.stdout
    dynamic = inspect('-dW')
    symbols = inspect('--dyn-syms', '-W')
    libraries = set(re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', dynamic))
    imports = set()
    for line in symbols.splitlines():
        columns = line.split()
        if len(columns) >= 8 and columns[3] == 'FUNC' and columns[4] == 'GLOBAL' and columns[6] == 'UND':
            imports.add(columns[7].split('@', 1)[0])
    if not REQUIRED_LIBRARIES <= libraries or not REQUIRED_IMPORTS <= imports:
        raise ValueError('S4 media worker lacks Cedar playback imports; rebuild media.pro with CONFIG+=cedar')
    # Detect concurrent artifact replacement while gathering the build evidence.
    if path.is_symlink() or path.read_bytes() != raw:
        raise ValueError('S4 media worker changed during inspection')
    return {'sha256': hashlib.sha256(raw).hexdigest(), 'size': len(raw),
            'elf': 'ARM ELF32 EABI5 soft-float', 'cedar_linkage': True,
            'needed_libraries': sorted(libraries),
            'cedar_imports': sorted(REQUIRED_IMPORTS), 'board_validation': 'not-run'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('worker', type=Path)
    parser.add_argument('--readelf', default='readelf')
    args = parser.parse_args()
    try:
        record = verify(args.worker, args.readelf)
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
