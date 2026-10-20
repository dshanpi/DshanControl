#!/usr/bin/env python3
"""Offline release gate: verify signature, sources, target payloads and evidence."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/updater'))
from update_protocol import (digest, read_json, require, validate_manifest, match_system_contract,
                             verify_signature, identifier, TARGET_FIELDS, APP_ID, REPOSITORY)
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
            from application_payload import COMPONENT_PATHS
            hashes = build.get('component_sha256', {})
            require(isinstance(hashes, dict) and
                    (not hashes or set(hashes) == set(COMPONENT_PATHS.values())),
                    'incomplete or unknown component build record')
            for relative in COMPONENT_PATHS.values():
                artifact = Path(temp) / relative
                require(artifact.exists() == (relative in hashes),
                        'component presence differs from build record')
                if relative in hashes:
                    require(digest(artifact) == hashes[relative], 'component hash mismatch')
        if public:
            evidence = item['validation']
            for filename, checksum in [('log_file', 'log_sha256'), ('system_contract_file','system_contract_sha256')]:
                require(identifier(evidence.get(filename)), 'evidence artifact missing')
                require(digest(directory / evidence[filename]) == evidence[checksum], 'evidence hash mismatch')
            contract = read_json(directory / evidence['system_contract_file'])
            match_system_contract(manifest, contract)
            require(all(contract[k] == item[k] for k in TARGET_FIELDS),
                    'evidence system contract/target mismatch')
            require(contract.get('app_id') == APP_ID and contract.get('repository') == REPOSITORY and
                    type(contract.get('data_schema')) is int and
                    contract['data_schema'] == manifest['data_schema'],
                    'evidence system product/data schema mismatch')
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
