#!/usr/bin/env python3
"""First-boot factory application and data setup; never overwrite user data."""
import shutil
from pathlib import Path
from updater import atomic_json, sync_directory
from storage import ensure_storage

ensure_storage()

root = Path('/mnt/UDISK/dshancontrol')
(root / 'apps/releases').mkdir(parents=True, exist_ok=True)
(root / 'data').mkdir(exist_ok=True)
for source, target in (('web.json', 'web.json'), ('hmi.json', 'hmi.json')):
    dst = root / 'data' / target
    if not dst.exists():
        from update_protocol import read_json
        atomic_json(dst, read_json(Path('/usr/share/dshancontrol/defaults') / source))
current = root / 'apps/current'
if not current.is_symlink():
    factory = root / 'apps/releases/factory'
    temporary = root / 'apps/releases/.factory'
    if temporary.exists():
        shutil.rmtree(str(temporary))
    if not factory.exists():
        shutil.copytree('/usr/share/dshancontrol/factory', str(temporary))
        import os
        for base, dirs, files in os.walk(str(temporary), topdown=False):
            for name in files:
                with open(str(Path(base) / name), 'rb') as stream:
                    os.fsync(stream.fileno())
            sync_directory(base)
        temporary.rename(factory)
        sync_directory(factory.parent)
    current.symlink_to('releases/factory')
    sync_directory(current.parent)
