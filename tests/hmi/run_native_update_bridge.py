"""Qt panel -> real C++ RPC/service/worker using private synthetic fixtures only.

Creates ephemeral fixture signing keys, never enrolls production trust. The
successful download record is seeded, not claimed as a real OTA download.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests'))
from test_native_service import NativeServiceTests


def run(qt_binary, native_binary, output):
    output.mkdir(parents=True, exist_ok=False)
    NativeServiceTests.setUpClass()
    fixture = NativeServiceTests('test_failed_install_keeps_raw_stage_code_and_does_not_retry')
    try:
        fixture.setUp()
        fixture.binary = native_binary
        params = fixture.source()
        source = params['source_task_id']
        (fixture.tasks / 'latest.json').write_text(json.dumps({'task_id': source}))
        fixture.hook.write_text('#!/bin/sh\nprintf "%s\\n" "$1" >> "' + str(fixture.f.root / 'operations') + '"\n[ "$1" != maintenance ] || exit 17\nexit 0\n')
        before = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in {
            'contract': fixture.f.root / 'contract.json', 'factory': fixture.f.app / 'releases/factory/installed.json',
            'user_data': fixture.f.data}.items()}
        fixture.start()
        env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
            DSHANCONTROL_NATIVE_PLATFORM_SOCKET=fixture.socket_path,
            DSHANCONTROL_UPDATER_EXECUTABLE=str(native_binary),
            DSHANCONTROL_TEST_DOWNLOADED_TASK=source,
            DSHANCONTROL_TEST_UI_EVIDENCE=str(output / 'ui-result.json'))
        began = time.monotonic()
        result = subprocess.run([str(qt_binary), '-o', str(output / 'qt-results.xml') + ',xml'],
            env=env, capture_output=True, text=True, timeout=120)
        (output / 'qt-test.log').write_text(result.stdout + result.stderr)
        fixture.assertEqual(0, result.returncode, result.stdout + result.stderr)
        xml = ET.parse(output / 'qt-results.xml')
        incidents = [n.attrib['type'] for n in xml.findall('.//Incident')]
        fixture.assertEqual(['pass'] * 3, incidents)
        ui = json.loads((output / 'ui-result.json').read_text())
        task = fixture.request('update.status', {'task_id': ui['failed_task_id']})['result']
        fixture.assertEqual('failed', task['status'])
        fixture.assertEqual('install', task['operation'])
        fixture.assertEqual(source, task['source_task_id'])
        fixture.assertEqual('maintenance', task['error']['stage'])
        fixture.assertEqual('17', task['error']['code'])
        fixture.assertEqual(['maintenance'], fixture.operations())
        fixture.assertEqual('factory', fixture.f.updater.current_path().name)
        fixture.assertEqual(0, fixture.f.updater.state()['high_water'])
        fixture.assertFalse((fixture.f.app / 'transaction.json').exists())
        fixture.assertEqual(2, len([p for p in fixture.tasks.glob('*.json') if p.name != 'latest.json']))
        for name, path in {'contract': fixture.f.root / 'contract.json',
                'factory': fixture.f.app / 'releases/factory/installed.json', 'user_data': fixture.f.data}.items():
            fixture.assertEqual(before[name], hashlib.sha256(path.read_bytes()).hexdigest())
        report = {'passed': True, 'scope': 'private synthetic downloaded task; real Qt/C++ RPC/worker; no production install or download claim',
            'qt_incidents': incidents, 'elapsed_seconds': time.monotonic() - began,
            'native_sha256': hashlib.sha256(native_binary.read_bytes()).hexdigest(),
            'qt_sha256': hashlib.sha256(qt_binary.read_bytes()).hexdigest(), 'task': task,
            'operations': fixture.operations(), 'task_count': 2, 'current': 'factory', 'high_water': 0,
            'preserved_hashes': before, 'ui': ui}
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        print(json.dumps(report, ensure_ascii=False))
    finally:
        fixture.doCleanups()
        NativeServiceTests.tearDownClass()


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--qt-binary', type=Path, required=True)
    p.add_argument('--native-binary', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    run(a.qt_binary.resolve(), a.native_binary.resolve(), a.output.resolve())
