#!/usr/bin/env python3
"""Offline release gate: verify signature, sources, target payloads and evidence."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/updater'))
from update_protocol import digest, read_json, require, validate_manifest, verify_signature, identifier, TARGET_FIELDS
from updater import extract_payload


def check(directory, key, public=False):
    manifest_path = directory / 'application-release.json'
    verify_signature(manifest_path, directory / 'application-release.json.sig', key)
    manifest = validate_manifest(read_json(manifest_path), public=public)
    require(digest(directory / manifest['source']['filename']) == manifest['source']['sha256'], 'source hash mismatch')
    require(digest(directory / manifest['build_info']['filename']) == manifest['build_info']['sha256'],
            'build-info hash mismatch')
    build = read_json(directory / manifest['build_info']['filename'])
    require(build.get('application_commit') == manifest['commit'] and build.get('application_dirty') is False,
            'manifest/build source mismatch')
    for item in manifest['targets']:
        require(build.get('target') == {k: item[k] for k in TARGET_FIELDS}, 'build target mismatch')
        path = directory / item['filename']
        require(path.stat().st_size == item['size'] and digest(path) == item['sha256'], 'payload hash mismatch')
        with tempfile.TemporaryDirectory() as temp:
            extract_payload(path, Path(temp), item['unpacked_size'])
            require(digest(Path(temp) / 'bin/omnigate-hmi') == build.get('hmi_sha256'),
                    'packaged native executable differs from build record')
        if public:
            evidence = item['validation']
            for filename, checksum in [('log_file', 'log_sha256'), ('system_contract_file','system_contract_sha256')]:
                require(identifier(evidence.get(filename)), 'evidence artifact missing')
                require(digest(directory / evidence[filename]) == evidence[checksum], 'evidence hash mismatch')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--public-key', required=True, type=Path)
    p.add_argument('--public', action='store_true')
    a = p.parse_args()
    m = check(a.directory, a.public_key, a.public)
    print(m['tag'] + ': ' + ('public asset checks passed; remote tag/board evidence review still required'
                            if a.public else 'signed draft asset checks passed'))
