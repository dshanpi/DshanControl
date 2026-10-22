"""6ULL source identity and board image rejection regressions; no hardware writes."""
import hashlib
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('imx6ull_source_inputs', ROOT / 'boards/imx6ull-pro/source_inputs.py')
source_inputs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_inputs)
image_spec = importlib.util.spec_from_file_location('imx6ull_image_tests', ROOT / 'boards/imx6ull-pro/test-images.py')
image_tests = importlib.util.module_from_spec(image_spec)
image_spec.loader.exec_module(image_tests)
ImageBoundaryTests = image_tests.ImageBoundaryTests


class CommittedSourceIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.app = Path(self.temp.name)
        self.run_git('init', '-q')
        self.run_git('config', 'user.name', 'Fixture')
        self.run_git('config', 'user.email', 'fixture@example.invalid')
        (self.app / 'existing.cpp').write_text('old\n')
        self.run_git('add', 'existing.cpp')
        self.run_git('commit', '-qm', 'base')
        self.base = self.run_git('rev-parse', 'HEAD').decode().strip()
        (self.app / 'existing.cpp').write_text('new\n')
        (self.app / 'added.cpp').write_text('worker\n')
        patch = self.run_git('diff', '--binary', 'HEAD', '--', 'existing.cpp')
        extra = subprocess.run(['git', 'diff', '--no-index', '--binary', '--', '/dev/null', 'added.cpp'],
                               cwd=str(self.app), stdout=subprocess.PIPE)
        self.assertEqual(extra.returncode, 1)
        self.expected = {'application_commit': self.base,
                         'application_patch_paths': ['existing.cpp', 'added.cpp'],
                         'application_patch_sha256': hashlib.sha256(patch + extra.stdout).hexdigest()}

    def run_git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.app)] + list(args))

    def commit_port(self):
        self.run_git('add', 'existing.cpp', 'added.cpp')
        self.run_git('commit', '-qm', 'port')

    def test_legacy_uncommitted_patch_is_explicitly_dirty(self):
        value = source_inputs.application_identity(self.app, self.expected)
        self.assertEqual(value['application_commit'], self.base)
        self.assertTrue(value['application_dirty'])

    def test_committed_port_preserves_patch_receipt_and_records_new_head(self):
        self.commit_port()
        value = source_inputs.application_identity(self.app, self.expected)
        self.assertNotEqual(value['application_commit'], self.base)
        self.assertEqual(value['application_base_commit'], self.base)
        self.assertEqual(value['application_patch_sha256'], self.expected['application_patch_sha256'])
        self.assertFalse(value['application_dirty'])

    def test_recorded_file_tampering_is_rejected_after_commit(self):
        self.commit_port()
        (self.app / 'added.cpp').write_text('different worker\n')
        with self.assertRaisesRegex(ValueError, 'content differs'):
            source_inputs.application_identity(self.app, self.expected)

    def test_unrelated_tree_dirt_is_not_misreported_as_clean(self):
        self.commit_port()
        (self.app / 'unrecorded.txt').write_text('local change\n')
        self.assertTrue(source_inputs.application_identity(self.app, self.expected)['application_dirty'])

    def test_missing_base_and_escaping_path_are_rejected(self):
        bad = dict(self.expected, application_commit='0' * 40)
        with self.assertRaisesRegex(ValueError, 'descend'):
            source_inputs.application_identity(self.app, bad)
        bad = dict(self.expected, application_patch_paths=['../outside'])
        with self.assertRaisesRegex(ValueError, 'path'):
            source_inputs.application_identity(self.app, bad)


if __name__ == '__main__':
    unittest.main()
