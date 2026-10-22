"""Runtime identity must survive fontconfig cache UUID creation, but detect real changes."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('system_contract_runtime_subject', ROOT / 'scripts/finalize_system_contract.py')
subject = importlib.util.module_from_spec(spec)
spec.loader.exec_module(subject)

class RuntimeIdentityTests(unittest.TestCase):
    def fixture(self, root):
        files = {'usr/share/dshancontrol/factory/bin/omnigate-hmi': b'fixture application',
                 'lib/libc-2.21.so': b'fixture libc', 'usr/lib/libQt5Core.so.5.11.3': b'fixture Qt',
                 'usr/bin/python3.7': b'fixture python', 'usr/share/fonts/dshan/example.otf': b'fixture font'}
        for name, body in files.items():
            p = root / name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(body)
        config = root / 'etc/dshancontrol'; config.mkdir(parents=True)
        (config / 'system-release.json').write_bytes((ROOT / 'boards/t113s4-sdnand/system-contract.json').read_bytes())
        (config / 'build-info.json').write_text(json.dumps({'application_commit': '1' * 40, 'application_dirty': False}))
        key = root / 'fixture-public.pem'; key.write_bytes(b'not a production key; openssl is mocked for this identity unit test')
        return key

    def finalize(self, root, key):
        args = ['finalize_system_contract.py', '--target', str(root), '--hardware-revision', 'unit-test-only', '--public-key', str(key)]
        with patch.object(sys, 'argv', args), patch.object(subject.subprocess, 'check_output', return_value='ELF32 ARM'), patch.object(subject.subprocess, 'run'), contextlib.redirect_stdout(io.StringIO()):
            subject.main()
        config = root / 'etc/dshancontrol'
        contract = json.loads((config / 'system-release.json').read_text())
        self.assertEqual('not-run', contract['validation'])
        return contract['runtime_id'], json.loads((config / 'runtime-files.json').read_text())

    def test_fontconfig_uuid_creation_rotation_removal_preserves_runtime_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); key = self.fixture(root); baseline, original = self.finalize(root, key)
            uuids = [root / 'usr/share/fonts/.uuid', root / 'usr/share/fonts/dshan/.uuid']
            for value in ['11111111-1111-1111-1111-111111111111', '22222222-2222-2222-2222-222222222222']:
                for p in uuids: p.write_text(value)
                identity, files = self.finalize(root, key)
                self.assertEqual(baseline, identity)
                self.assertEqual(original, files)
            for p in uuids: p.unlink()
            self.assertEqual(baseline, self.finalize(root, key)[0])

    def test_font_library_and_python_changes_still_change_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); key = self.fixture(root); previous = self.finalize(root, key)[0]
            for name in ['usr/share/fonts/dshan/example.otf', 'usr/lib/libQt5Core.so.5.11.3', 'usr/bin/python3.7']:
                p = root / name; p.write_bytes(p.read_bytes() + b' changed')
                identity, files = self.finalize(root, key)
                self.assertNotEqual(previous, identity)
                self.assertIn(name, files)
                previous = identity

    def test_exclusion_is_only_exact_uuid_name_under_font_tree(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); key = self.fixture(root); previous = self.finalize(root, key)[0]
            for name in ['usr/lib/.uuid', 'usr/share/fonts/dshan/.uuid.otf', 'usr/share/fonts/dshan/.hidden-font.otf']:
                p = root / name; p.write_bytes(b'identity relevant')
                identity, files = self.finalize(root, key)
                self.assertNotEqual(previous, identity)
                self.assertIn(name, files)
                previous = identity

if __name__ == '__main__': unittest.main()
