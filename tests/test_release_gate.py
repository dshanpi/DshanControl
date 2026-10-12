import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import check_application_release as gate
import test_application_update as fixtures


class ReleaseGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixtures.UpdateTests.setUpClass()

    @classmethod
    def tearDownClass(cls):
        fixtures.UpdateTests.tearDownClass()

    def setUp(self):
        self.fixture = fixtures.UpdateTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.f = self.fixture
        (self.f.root / 'source.tar.gz').write_bytes(b'test-source-artifact')
        info = dict(application_commit=self.f.manifest['commit'], application_dirty=False,
                    target=fixtures.TARGET,
                    hmi_sha256=__import__('hashlib').sha256(b'payload:bin/omnigate-hmi').hexdigest())
        (self.f.root / 'build-info.json').write_text(json.dumps(info))
        self.f.manifest['source']['sha256'] = fixtures.digest(self.f.root / 'source.tar.gz')
        self.f.manifest['build_info']['sha256'] = fixtures.digest(self.f.root / 'build-info.json')
        self.f.sign()

    def test_valid_draft_assets_pass_but_public_promotion_fails(self):
        self.assertEqual('0.2.0', gate.check(self.f.root, self.f.public)['version'])
        with self.assertRaises(fixtures.Rejected): gate.check(self.f.root, self.f.public, public=True)

    def test_source_asset_tampering_fails(self):
        (self.f.root / 'source.tar.gz').write_bytes(b'changed source')
        with self.assertRaises(fixtures.Rejected): gate.check(self.f.root, self.f.public)

    def test_signed_but_mismatched_native_build_record_fails(self):
        path = self.f.root / 'build-info.json'
        info = json.loads(path.read_text())
        info['application_commit'] = 'e'*40
        path.write_text(json.dumps(info))
        self.f.manifest['build_info']['sha256'] = fixtures.digest(path)
        self.f.sign()
        with self.assertRaises(fixtures.Rejected): gate.check(self.f.root, self.f.public)


if __name__ == '__main__': unittest.main()
