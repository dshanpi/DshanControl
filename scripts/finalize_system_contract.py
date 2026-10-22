#!/usr/bin/env python3
"""Derive a draft system contract from built S4 runtime files, never qualify a board."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/updater'))
from update_protocol import digest, identifier, require


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', required=True, type=Path)
    p.add_argument('--hardware-revision', required=True)
    p.add_argument('--public-key', required=True, type=Path)
    a = p.parse_args()
    target = a.target.resolve()
    require(str(target) != '/', 'refusing host root')
    require(identifier(a.hardware_revision) and a.hardware_revision not in ('pending','unknown'),
            'real hardware revision required')
    hmi = target / 'usr/share/dshancontrol/factory/bin/omnigate-hmi'
    header = subprocess.check_output(['readelf', '-h', str(hmi)], universal_newlines=True)
    attrs = subprocess.check_output(['readelf', '-A', str(hmi)], universal_newlines=True)
    require('ELF32' in header and 'ARM' in header, 'S4 application must be ARM32')
    require('Tag_ABI_VFP_args: VFP registers' not in attrs and 'hard-float ABI' not in header,
            'hard-float HMI on softfp baseline')
    libraries = list((target / 'lib').glob('libc-*.so'))
    require(len(libraries) == 1, 'cannot identify target libc')
    version = libraries[0].name[len('libc-'):-len('.so')]
    files = {}
    for base in ('lib', 'usr/lib', 'usr/bin/python3.7', 'usr/share/fonts'):
        directory = target / base
        paths = directory.rglob('*') if directory.is_dir() else [directory]
        for file in sorted(paths):
            if file.is_file() and not file.is_symlink():
                relative = str(file.relative_to(target))
                # Application/updater identities have their own records.
                if relative.startswith(('usr/lib/dshancontrol-update/', 'usr/lib/omnigate')):
                    continue
                # Fontconfig 2.13 creates per-directory random cache UUIDs on
                # first scan. They identify caches, not fonts or runtime ABI.
                # Keep every other file (including other hidden files) pinned.
                if relative.startswith('usr/share/fonts/') and file.name == '.uuid':
                    continue
                files[relative] = digest(file)
    require(files and any('libQt5Core' in name for name in files), 'Qt runtime missing')
    runtime_hash = hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()
    config = target / 'etc/dshancontrol'
    contract = json.loads((config / 'system-release.json').read_text())
    contract.update(hardware_revision=a.hardware_revision, libc='glibc-' + version,
                    runtime_id='s4-' + runtime_hash, validation='not-run')
    subprocess.run(['openssl', 'rsa', '-pubin', '-in', str(a.public_key), '-noout'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    (config / 'system-release.json').write_text(json.dumps(contract, indent=2) + '\n')
    (config / 'runtime-files.json').write_text(json.dumps(files, indent=2) + '\n')
    (config / 'update-public.pem').write_bytes(a.public_key.read_bytes())
    build = json.loads((config / 'build-info.json').read_text())
    native = {'application_commit': build['application_commit'],
              'application_dirty': build['application_dirty'], 'hmi_sha256': digest(hmi),
              'target': {k: contract[k] for k in ('board_id','hardware_revision','architecture',
                        'float_abi','libc','runtime_id','contract_id','updater_protocol')}}
    (target / 'usr/share/dshancontrol/native-build-info.json').write_text(json.dumps(native, indent=2) + '\n')
    print('Draft system contract generated; board qualification and maintenance hook still required.')


if __name__ == '__main__': main()
