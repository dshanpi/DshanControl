#!/usr/bin/env python3
"""Repository gates: product scope, board boundaries and Python 3.7 compatibility."""
import ast
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
rules = (ROOT / 'AGENTS.md').read_text()
for required in ('DshanControl', 'T113s4 SD NAND', 'SmartDesktopMimi', 'EinkOS', 'T153'):
    assert required in rules, 'missing product scope: ' + required
manifest = json.loads((ROOT / 'boards/t113s4-sdnand/platform.json').read_text())
board = json.loads((ROOT / 'platform/boards/t113s4-sdnand.json').read_text())
assert board['supported_profiles'] == [], 'S4 support needs physical qualification'
assert manifest['board_id'] == '100ask-t113s4-sdnand'
assert manifest['capabilities'] == [], 'do not claim S4 hardware capabilities before board evidence'
assert manifest['update']['qualified'] is False
contract = json.loads((ROOT / 'boards/t113s4-sdnand/system-contract.json').read_text())
assert contract['app_id'] == 'dshancontrol'
assert contract['repository'] == 'dshanpi/DshanControl'
assert contract['validation'] == 'not-run'
assert not list((ROOT / 'boards/t113s4-sdnand').rglob('*.pem')), 'do not commit device/private signing material'
archive = ROOT / 'boards/t113s4-sdnand/validation/20261014-native-pilot'
record = json.loads((archive / 'manifest.json').read_text())
assert record['kind'] == 'reference-only-physical-pilot-source-archive'
for field in ('automatic_updates_qualified', 'production_runtime', 'automatically_installed'):
    assert record[field] is False, 'pilot archive is not a production integration'
declared = {item['path'] for item in record['files']}
assert len(declared) == len(record['files']), 'duplicate archived source'
assert {str(p.relative_to(archive)) for p in archive.rglob('*') if p.is_file()} == declared | {'manifest.json'}
for item in record['files']:
    path = archive / item['path']
    assert not Path(item['path']).is_absolute() and '..' not in Path(item['path']).parts
    assert archive.resolve() in path.resolve().parents, 'archived source escaped its directory'
    assert not path.is_symlink() and hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], 'archived physical source changed'
    if path.suffix == '.sh':
        subprocess.run(['sh', '-n', str(path)], check=True)
    elif path.suffix == '.py':
        ast.parse(path.read_text(), filename=str(path), feature_version=7)
for path in (ROOT / 'components').rglob('*.py'):
    ast.parse(path.read_text(), filename=str(path), feature_version=7)
print('DshanControl repository gates passed (S4 remains unqualified).')
