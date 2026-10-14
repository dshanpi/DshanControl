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

    def test_public_evidence_contract_must_match_payload_identity(self):
        # These synthetic gates exercise validation only, not board qualification.
        log = self.f.root / 'fixture.log'
        log.write_text('synthetic host fixture, no physical board evidence\n')
        evidence = dict.fromkeys(fixtures.GATES, 'passed')
        evidence.update(system_image_sha256='e'*64, log_file=log.name,
                        log_sha256=fixtures.digest(log), tester='host-fixture-only',
                        tested_at='host-fixture-only', system_contract_file='contract.json')
        self.f.manifest['release_state'] = 'qualified'
        self.f.manifest['targets'][0]['validation'] = evidence
        path = self.f.root / 'contract.json'
        baseline = dict(self.f.contract)
        for field in (*fixtures.TARGET_FIELDS, 'app_id', 'repository', 'data_schema'):
            with self.subTest(field=field):
                contract = dict(baseline)
                contract[field] = 2 if type(contract[field]) is int else 'other'
                path.write_text(json.dumps(contract))
                evidence['system_contract_sha256'] = fixtures.digest(path)
                self.f.sign()
                with self.assertRaises(fixtures.Rejected):
                    gate.check(self.f.root, self.f.public, public=True)
        path.write_text(json.dumps(baseline))
        evidence['system_contract_sha256'] = fixtures.digest(path)
        self.f.sign()
        self.assertEqual('0.2.0', gate.check(self.f.root, self.f.public, public=True)['version'])

    def test_public_contract_must_allow_channel_and_updater(self):
        log = self.f.root / 'fixture.log'
        log.write_text('synthetic host fixture, no physical board evidence\n')
        evidence = dict.fromkeys(fixtures.GATES, 'passed')
        evidence.update(system_image_sha256='e'*64, log_file=log.name,
                        log_sha256=fixtures.digest(log), tester='host-fixture-only',
                        tested_at='host-fixture-only', system_contract_file='contract.json')
        self.f.manifest['release_state'] = 'qualified'
        self.f.manifest['targets'][0]['validation'] = evidence
        path = self.f.root / 'contract.json'
        for field, value in [('allowed_channels', None), ('allowed_channels', []),
                             ('allowed_channels', 'stable'), ('allowed_channels', ['prerelease']),
                             ('updater_version', None), ('updater_version', '9.0.0')]:
            with self.subTest(field=field, value=value):
                contract = dict(self.f.contract)
                contract[field] = value
                path.write_text(json.dumps(contract))
                evidence['system_contract_sha256'] = fixtures.digest(path)
                self.f.sign()
                with self.assertRaises(fixtures.Rejected):
                    gate.check(self.f.root, self.f.public, public=True)

    def test_build_target_dirty_tree_and_native_hash_must_match(self):
        path = self.f.root / 'build-info.json'
        baseline = json.loads(path.read_text())
        for key, value in [('application_dirty', True), ('hmi_sha256', '0'*64),
                           ('target', dict(fixtures.TARGET, float_abi='hard'))]:
            with self.subTest(key=key):
                info = dict(baseline, **{key: value})
                path.write_text(json.dumps(info))
                self.f.manifest['build_info']['sha256'] = fixtures.digest(path)
                self.f.sign()
                with self.assertRaises(fixtures.Rejected): gate.check(self.f.root, self.f.public)


if __name__ == '__main__': unittest.main()
