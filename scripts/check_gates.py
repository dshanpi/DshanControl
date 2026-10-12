#!/usr/bin/env python3
"""Repository gates: product scope, board boundaries and Python 3.7 compatibility."""
import ast
import json
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
for path in (ROOT / 'components').rglob('*.py'):
    ast.parse(path.read_text(), filename=str(path), feature_version=7)
print('DshanControl repository gates passed (S4 remains unqualified).')
