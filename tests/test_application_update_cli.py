"""Real local CLI/hook processes; synthetic keys and payloads never qualify a board."""
import copy
import json
import os
import signal
import subprocess
import sys
import tarfile
import unittest
from pathlib import Path

import test_application_update as fixtures


class UpdateCliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixtures.UpdateTests.setUpClass()

    @classmethod
    def tearDownClass(cls):
        fixtures.UpdateTests.tearDownClass()

    def setUp(self):
        self.f = fixtures.UpdateTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        self.hook = self.f.root / 'system-hook'
        self.hook.write_text('#!' + sys.executable + '\n' + '''
import json, os, signal, sys
from pathlib import Path
root = Path(__file__).parent
operation = sys.argv[1]
with (root / 'operations').open('a') as stream:
    stream.write(operation + '\\n')
    stream.flush()
    os.fsync(stream.fileno())
mode = (root / 'mode').read_text()
if mode == 'deny' and operation == 'maintenance':
    sys.exit(1)
if mode == 'kill-' + operation:
    os.kill(os.getppid(), signal.SIGKILL)
if operation == 'health':
    current = (root / 'apps/current').resolve().name
    if mode == 'bad-all' or (mode == 'bad-new' and current != 'factory'):
        sys.exit(1)
''')
        self.hook.chmod(0o755)
        self.mode('ok')
        self.config = self.f.root / 'config.json'
        self.f.config['service_hook'] = str(self.hook)
        fixtures.atomic_json(self.config, self.f.config)
        self.original_contract = (self.f.root / 'contract.json').read_bytes()
        self.original_factory = (self.f.app / 'releases/factory/installed.json').read_bytes()

    def mode(self, value):
        (self.f.root / 'mode').write_text(value)

    def operations(self):
        path = self.f.root / 'operations'
        return path.read_text().splitlines() if path.exists() else []

    def cli(self, command='install', expected=0, message=None):
        args = [sys.executable, str(fixtures.ROOT / 'components/updater/updater.py'),
                '--config', str(self.config), command]
        if command == 'install':
            args += [str(self.f.path), str(self.f.signature), str(self.f.package)]
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
        evidence = os.environ.get('UPDATE_CLI_EVIDENCE')
        if evidence:
            with open(evidence, 'a') as stream:
                stream.write(json.dumps({'test': self._testMethodName, 'command': command,
                                         'returncode': result.returncode, 'expected': expected,
                                         'stdout': result.stdout, 'stderr': result.stderr,
                                         'operations': self.operations(),
                                         'active': self.f.updater.current_path().name,
                                         'state': self.f.updater.state()}, sort_keys=True) + '\n')
        self.assertEqual(expected, result.returncode, result.stdout + result.stderr)
        if message:
            self.assertIn(message, result.stderr)
        self.assertEqual(b'USER DATA MUST SURVIVE', self.f.data.read_bytes())
        self.assertEqual(self.original_factory,
                         (self.f.app / 'releases/factory/installed.json').read_bytes())
        self.assertEqual(self.original_contract, (self.f.root / 'contract.json').read_bytes())
        return result

    def test_success_replay_and_real_process_lock(self):
        with self.f.updater.locked():
            self.cli(expected=1, message='another update is running')
        self.assertEqual([], self.operations())
        name = self.cli().stdout.strip()
        self.assertEqual(name, self.f.updater.current_path().name)
        self.assertEqual(['maintenance', 'stop', 'start', 'health'], self.operations())
        self.cli(expected=1, message='replayed or downgraded')
        self.assertEqual(4, len(self.operations()))

    def test_maintenance_rejection_then_retry(self):
        self.mode('deny')
        self.cli(expected=1, message='system hook failed: maintenance')
        self.assertEqual(['maintenance'], self.operations())
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertFalse((self.f.app / 'transaction.json').exists())
        self.assertEqual(0, self.f.updater.state()['high_water'])
        self.mode('ok')
        self.cli()

    def test_mismatches_rejected_before_any_hook(self):
        baseline = copy.deepcopy(self.f.manifest)
        mutations = [(key, 'other') for key in fixtures.TARGET_FIELDS[:-1]]
        mutations += [('updater_protocol', 2)]
        for key, value in mutations:
            with self.subTest(field=key):
                self.f.manifest = copy.deepcopy(baseline)
                self.f.manifest['targets'][0][key] = value
                self.f.sign()
                self.cli(expected=1)
        for key, value in [('app_id', 'other'), ('repository', 'other/repo'),
                           ('release_kind', 'system'), ('channel', 'prerelease'),
                           ('from_versions', ['0.0.0']), ('data_schema', 2)]:
            with self.subTest(field=key):
                self.f.manifest = dict(baseline, **{key: value})
                self.f.sign()
                self.cli(expected=1)
        self.assertEqual([], self.operations())
        self.assertEqual('factory', self.f.updater.current_path().name)

    def test_signature_and_same_size_payload_tampering(self):
        self.f.path.write_bytes(self.f.path.read_bytes() + b' ')
        self.cli(expected=1, message='manifest signature rejected')
        self.f.sign()
        original = self.f.package.read_bytes()
        self.f.package.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        self.cli(expected=1, message='package hash/size mismatch')
        self.assertEqual([], self.operations())
        self.f.package.write_bytes(original)
        self.cli()

    def test_signed_package_boundary_rejected_before_hooks(self):
        extra = tarfile.TarInfo('../outside')
        self.f.make_package(extra)
        self.f.manifest['targets'][0].update(sha256=fixtures.digest(self.f.package),
                                            size=self.f.package.stat().st_size)
        self.f.sign()
        self.cli(expected=1, message='unsafe package path')
        self.assertEqual([], self.operations())
        self.assertFalse((self.f.app / 'transaction.json').exists())
        self.assertFalse((self.f.app / 'releases/outside').exists())

    def interrupted(self, operation, phase):
        self.mode('kill-' + operation)
        self.cli(expected=-signal.SIGKILL)
        journal = self.f.app / 'transaction.json'
        self.assertEqual(phase, fixtures.read_json(journal)['phase'])
        self.assertEqual(1, self.f.updater.state()['high_water'])
        self.assertEqual(operation == 'stop', self.f.updater.current_path().name == 'factory')
        self.mode('ok')
        self.cli('recover')
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertFalse(journal.exists())
        self.assertEqual(1, len(self.f.updater.state()['failed']))
        count = len(self.operations())
        self.cli('recover')
        self.assertEqual(count, len(self.operations()))
        self.cli(expected=1, message='replayed or downgraded')

    def test_killed_before_switch_recovers_in_new_process(self):
        self.interrupted('stop', 'prepared')

    def test_killed_after_switch_recovers_in_new_process(self):
        self.interrupted('start', 'switched')

    def test_new_health_failure_rolls_back(self):
        self.mode('bad-new')
        self.cli(expected=1, message='new application health failed')
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertFalse((self.f.app / 'transaction.json').exists())
        self.assertEqual(['maintenance', 'stop', 'start', 'health', 'stop', 'start', 'health'],
                         self.operations())
        self.cli(expected=1, message='replayed or downgraded')

    def test_failed_rollback_keeps_journal_until_recovery(self):
        self.mode('bad-all')
        self.cli(expected=1, message='rollback health failed; journal retained')
        journal = self.f.app / 'transaction.json'
        self.assertTrue(journal.exists())
        self.cli('recover', expected=1, message='rollback health failed; journal retained')
        self.assertTrue(journal.exists())
        self.mode('ok')
        self.cli('recover')
        self.assertFalse(journal.exists())
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertEqual(1, self.f.updater.state()['high_water'])

    def test_committed_journal_cleanup_preserves_active_version(self):
        name = self.cli().stdout.strip()
        fixtures.atomic_json(self.f.app / 'transaction.json',
                             {'old': 'factory', 'new': name, 'phase': 'committed'})
        count = len(self.operations())
        self.cli('recover')
        self.assertEqual(name, self.f.updater.current_path().name)
        self.assertEqual(count, len(self.operations()))
        self.assertFalse((self.f.app / 'transaction.json').exists())


if __name__ == '__main__':
    unittest.main()
