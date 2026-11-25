#!/usr/bin/env python3
"""Verify a complete local or re-downloaded system draft, including archive identities."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import tarfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(stream):
    result, count = hashlib.sha256(), 0
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        result.update(block)
        count += len(block)
    return result.hexdigest(), count


def verify(directory):
    directory = Path(directory)
    checksums = {}
    for line in (directory / 'SHA256SUMS').read_text().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._-]*)', line)
        require(match is not None, 'Invalid checksum entry')
        value, name = match.groups()
        require(name not in checksums and name != 'SHA256SUMS', 'Duplicate/self checksum')
        checksums[name] = value
    actual = {p.name for p in directory.iterdir() if p.is_file() and p.name != 'SHA256SUMS'}
    require(actual == set(checksums), 'Extra or missing delivery files')
    for name, expected in checksums.items():
        path = directory / name
        require(not path.is_symlink(), 'Linked delivery asset')
        with path.open('rb') as stream:
            require(digest(stream)[0] == expected, 'Checksum mismatch: ' + name)
    lock = json.loads((directory / 'release-lock.json').read_text())
    require(lock['release_kind'] == 'system' and lock['auto_update_eligible'] is False, 'Unexpected release classification')
    names = [item['filename'] for item in lock['artifacts']]
    require(len(names) == len(set(names)), 'Duplicate lock asset')
    require(set(names) == actual - {'release-lock.json'}, 'Incomplete release lock')
    for item in lock['artifacts']:
        require(checksums[item['filename']] == item['sha256'], 'Lock hash mismatch')
        require((directory / item['filename']).stat().st_size == item['bytes'], 'Lock size mismatch')
    image = lock['image']
    require(image['filename'] in checksums and checksums[image['filename']] == image['sha256'], 'Image lock mismatch')
    with gzip.open(directory / image['filename'], 'rb') as stream:
        raw_hash, raw_size = digest(stream)
    require(raw_hash == image['uncompressed_sha256'] and raw_size == image['uncompressed_bytes'], 'Uncompressed image mismatch')
    validation = json.loads((directory / 'validation-imx6ull-pro.json').read_text())
    require(validation['image_sha256'] == raw_hash and validation['stage'] == 'draft', 'Wrong image validation')
    source_names = [name for name in names if name.startswith('DshanControl-v') and name.endswith('-source.tar.gz')]
    require(len(source_names) == 1, 'Ambiguous application source')
    for name, commit in ((source_names[0], lock['application_commit']),
                         ('hardware-source.tar.gz', lock['integration_commit']),
                         ('bsp-source.tar.gz', lock['bsp_commit'])):
        with tarfile.open(directory / name) as archive:
            archive.next()
            require(archive.pax_headers.get('comment') == commit, 'Git source identity mismatch: ' + name)
    with tarfile.open(directory / 'hardware-source.tar.gz') as archive:
        recipe = json.load(archive.extractfile('dshancontrol/release-inputs.json'))
    require(recipe['application_commit'] == lock['application_commit'] and recipe['bsp_commit'] == lock['bsp_commit'], 'Hardware recipe mismatch')
    require(recipe['application_source_sha256'] == checksums[source_names[0]], 'Hardware recipe archive mismatch')
    return {'passed': True, 'assets_verified': len(checksums) + 1,
            'image_sha256': raw_hash, 'image_bytes': raw_size,
            'application_commit': lock['application_commit'], 'hardware_commit': lock['integration_commit'],
            'scope': 'Checksums, lock closure, gzip image identity and A/H/BSP Git archives; no hardware qualification'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.directory), indent=2))
