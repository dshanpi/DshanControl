#!/usr/bin/env python3
"""Install pinned pure Python runtime into its private directory (offline)."""
import argparse
import hashlib
import json
import tarfile
import zipfile
from pathlib import Path, PurePosixPath


def install(lock, downloads, target):
    target.mkdir(parents=True, exist_ok=True)
    for item in json.loads(lock.read_text()):
        archive = downloads / item['filename']
        if hashlib.sha256(archive.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('runtime hash mismatch: ' + item['filename'])
        if item['kind'] == 'bdist_wheel':
            with zipfile.ZipFile(str(archive)) as source:
                for name in source.namelist():
                    p = PurePosixPath(name)
                    if p.is_absolute() or '..' in p.parts:
                        raise ValueError('unsafe wheel path')
                    destination = target.joinpath(*p.parts)
                    if name.endswith('/'):
                        destination.mkdir(parents=True, exist_ok=True)
                    else:
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        destination.write_bytes(source.read(name))
        else:
            # MarkupSafe's maintained _native fallback avoids target-host C ABI mixing.
            with tarfile.open(str(archive)) as source:
                for member in source:
                    prefix = 'MarkupSafe-2.0.1/src/markupsafe/'
                    if member.name.startswith(prefix) and member.name.endswith('.py') and member.isfile():
                        destination = target / 'markupsafe' / Path(member.name).name
                        destination.parent.mkdir(exist_ok=True)
                        destination.write_bytes(source.extractfile(member).read())
                license_file = source.extractfile('MarkupSafe-2.0.1/LICENSE.rst')
                (target / 'markupsafe/LICENSE.rst').write_bytes(license_file.read())


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('lock', type=Path)
    p.add_argument('downloads', type=Path)
    p.add_argument('target', type=Path)
    a = p.parse_args()
    install(a.lock, a.downloads, a.target)
