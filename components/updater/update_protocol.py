"""DshanControl application update protocol, Python 3.7+ (no pip dependencies)."""
import hashlib
import json
import re
import subprocess
from pathlib import Path, PurePosixPath

APP_ID = 'dshancontrol'
UPDATER_VERSION = '0.1.0'
REPOSITORY = 'dshanpi/DshanControl'
TARGET_FIELDS = ('board_id', 'hardware_revision', 'architecture', 'float_abi',
                 'libc', 'runtime_id', 'contract_id', 'updater_protocol')
REQUIRED = ('bin/omnigate-hmi', 'lib/web/app.py', 'lib/web/hmi_api.py',
            'lib/core/omnigate_core/__init__.py', 'lib/supervisor/supervisor.py',
            'share/web/index.html')
GATES = ('host_tests', 'target_build', 'board_function', 'compatibility_rejection',
         'signature_rejection', 'power_loss', 'rollback', 'data_preservation',
         'maintenance_interlock')


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique_object)


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def identifier(value):
    return isinstance(value, str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]{0,100}', value) is not None


def sha(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


def validate_target(target):
    require(isinstance(target, dict), 'target must be an object')
    for key in TARGET_FIELDS[:-1]:
        require(identifier(target.get(key)) and target[key] not in ('unknown', 'pending'),
                'missing or unknown target field: ' + key)
    require(type(target.get('updater_protocol')) is int and target['updater_protocol'] == 1,
            'unsupported updater protocol')


def validate_manifest(m, public=False):
    require(isinstance(m, dict), 'manifest must be an object')
    require(type(m.get('schema_version')) is int and m['schema_version'] == 1, 'unsupported manifest schema')
    require(m.get('app_id') == APP_ID, 'wrong application')
    require(m.get('repository') == REPOSITORY, 'wrong repository')
    require(m.get('release_kind') == 'application', 'not an application release')
    require(m.get('channel') in ('stable', 'prerelease'), 'unknown channel')
    require(m.get('release_state') in ('draft', 'qualified'), 'unknown release state')
    require(identifier(m.get('version')), 'invalid version')
    require(re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?', m['version']), 'invalid semantic version')
    require(m['channel'] != 'stable' or '-' not in m['version'], 'prerelease version on stable channel')
    require(isinstance(m.get('minimum_updater_version'), str) and
            re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', m['minimum_updater_version']),
            'minimum updater version required')
    require(m.get('tag') == 'app-v' + m['version'], 'tag/version mismatch')
    require(type(m.get('sequence')) is int and m['sequence'] > 0, 'invalid sequence')
    require(isinstance(m.get('commit'), str) and re.fullmatch(r'[0-9a-f]{40}', m['commit']),
            'full application commit required')
    require(type(m.get('data_schema')) is int and m['data_schema'] > 0, 'invalid data schema')
    require(isinstance(m.get('from_versions'), list) and m['from_versions'] and
            all(identifier(v) for v in m['from_versions']), 'explicit upgrade paths required')
    assets = m.get('targets')
    require(isinstance(assets, list) and assets, 'no target packages')
    seen = set()
    for item in assets:
        validate_target(item)
        identity = tuple(item[k] for k in TARGET_FIELDS)
        require(identity not in seen, 'duplicate target identity')
        seen.add(identity)
        require(identifier(item.get('filename')) and item['filename'].endswith('.tar.gz'),
                'invalid package filename')
        require(sha(item.get('sha256')), 'invalid package digest')
        require(type(item.get('size')) is int and item['size'] > 0, 'invalid package size')
        require(type(item.get('unpacked_size')) is int and item['unpacked_size'] > 0,
                'invalid expanded size')
        if public or m['release_state'] == 'qualified':
            evidence = item.get('validation', {})
            require(all(evidence.get(k) == 'passed' for k in GATES), 'release gates incomplete')
            require(all(sha(evidence.get(k)) for k in
                        ('system_image_sha256', 'system_contract_sha256', 'log_sha256')),
                    'validation hashes missing')
            require(bool(evidence.get('tester')) and bool(evidence.get('tested_at')),
                    'validation author/time missing')
    require(isinstance(m.get('source'), dict) and sha(m['source'].get('sha256')),
            'source artifact hash required')
    require(identifier(m['source'].get('filename')), 'invalid source filename')
    require(isinstance(m.get('build_info'), dict) and sha(m['build_info'].get('sha256')) and
            identifier(m['build_info'].get('filename')), 'build-info artifact required')
    if public:
        require(m['release_state'] == 'qualified', 'draft cannot enter device update feed')
    return m


def match_system_contract(m, contract):
    """Shared device/release pairing checks; neither infers missing system identity."""
    validate_target(contract)
    require(tuple(map(int, UPDATER_VERSION.split('.'))) >=
            tuple(map(int, m['minimum_updater_version'].split('.'))), 'updater too old')
    require(contract.get('updater_version') == UPDATER_VERSION, 'installed updater/contract mismatch')
    require(contract.get('app_id') == APP_ID, 'wrong installed product')
    require(contract.get('repository') == REPOSITORY, 'wrong installed repository')
    channels = contract.get('allowed_channels')
    require(isinstance(channels, list) and bool(channels) and
            all(c in ('stable', 'prerelease') for c in channels),
            'explicit allowed channels required')
    require(m['channel'] in channels, 'channel not allowed')


def match_target(m, contract, current, high_water=0):
    validate_manifest(m)
    match_system_contract(m, contract)
    require(isinstance(current, dict) and identifier(current.get('version')) and
            current['version'] not in ('unknown', 'pending'), 'missing installed version')
    require(type(current.get('data_schema')) is int and current['data_schema'] > 0 and
            type(contract.get('data_schema')) is int and contract['data_schema'] > 0,
            'invalid installed/system data schema')
    require(type(high_water) is int and high_water >= 0, 'invalid accepted sequence')
    require(m['sequence'] > high_water, 'replayed or downgraded release')
    require(current['version'] in m['from_versions'], 'no upgrade path from installed version')
    require(current['data_schema'] == m['data_schema'] == contract.get('data_schema'),
            'data migration unsupported by protocol v1')
    choices = [x for x in m['targets'] if all(x[k] == contract[k] for k in TARGET_FIELDS)]
    require(len(choices) == 1, 'no exact board/ABI/runtime/contract match')
    return choices[0]


def verify_signature(manifest, signature, key):
    # Public key path is system-owned. No downloaded key is ever accepted.
    result = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(key),
                             '-signature', str(signature), str(manifest)],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    require(result.returncode == 0, 'manifest signature rejected')


def payload_name(name):
    p = PurePosixPath(name)
    require(name and not p.is_absolute() and '..' not in p.parts and '\\' not in name,
            'unsafe package path')
    require(str(p) == name.rstrip('/') and p.parts[0] in ('bin', 'lib', 'share'),
            'payload outside application boundary')
    return p
