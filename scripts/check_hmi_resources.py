#!/usr/bin/env python3
"""Verify bundled HMI font provenance before creating an application payload."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def verify(source):
    hmi = source / 'components/hmi/src'
    fonts = hmi / 'fonts'
    manifest = json.loads((fonts / 'manifest.json').read_text())
    expected = {'DshanSans-Regular.otf', 'DshanSans-Bold.otf'}
    records = manifest['fonts']
    if len(records) != 2 or {r['output'] for r in records} != expected:
        raise ValueError('unexpected bundled font set')
    resources = {entry.text for entry in ET.parse(str(hmi / 'icons.qrc')).iter('file')}
    for name in expected | {'OFL.txt', 'manifest.json'}:
        if 'fonts/' + name not in resources:
            raise ValueError('font resource not embedded: ' + name)
    for record in records:
        path = fonts / record['output']
        if hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            raise ValueError('font hash mismatch: ' + path.name)
    search = manifest.get('search_font')
    regular = next(record for record in records if record['output'] == 'DshanSans-Regular.otf')
    if (not isinstance(search, dict) or
            search.get('family') != 'Dshan Search' or
            search.get('output') != 'DshanSearch-Regular.otf' or
            search.get('policy') != 'search-font.conf' or
            search.get('coverage_basis_sha256') != regular['sha256']):
        raise ValueError('invalid bundled search font record')
    for name, expected_hash in ((search['output'], search.get('sha256')),
                                (search['policy'], search.get('policy_sha256'))):
        if 'fonts/' + name not in resources:
            raise ValueError('font resource not embedded: ' + name)
        path = fonts / name
        if path.is_symlink() or not path.is_file():
            raise ValueError('invalid search font resource: ' + name)
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash:
            raise ValueError('search font resource hash mismatch: ' + name)
    if (search.get('bytes') != (fonts / search['output']).stat().st_size or
            search.get('license_sha256') != manifest.get('license_sha256') or
            ''.join(str(search.get('license', '')).split()) != 'SILOpenFontLicense1.1'):
        raise ValueError('search font provenance mismatch')
    license_bytes = (fonts / 'OFL.txt').read_bytes()
    if (manifest.get('license') != 'SIL Open Font License 1.1' or
            b'SIL OPEN FONT LICENSE Version 1.1' not in license_bytes or
            hashlib.sha256(license_bytes).hexdigest() != manifest.get('license_sha256')):
        raise ValueError('font license mismatch')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    verify(args.source)
    print('Bundled HMI font hashes, license and resource paths verified')
