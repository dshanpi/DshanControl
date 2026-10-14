import copy
import io
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/updater'))
from update_protocol import (GATES, REQUIRED, TARGET_FIELDS, Rejected, digest, match_target,
                             read_json, validate_manifest, verify_signature)
from updater import Updater, atomic_json, extract_payload

TARGET = dict(board_id='100ask-t113s4-sdnand', hardware_revision='test-rev', architecture='armv7-a',
              float_abi='softfp', libc='glibc-test', runtime_id='test-runtime',
              contract_id='dshancontrol-platform-1', updater_protocol=1)


class UpdateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.private = Path(cls.keys.name) / 'private.pem'
        cls.public = Path(cls.keys.name) / 'public.pem'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048',
                        '-out', str(cls.private)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(['openssl', 'pkey', '-in', str(cls.private), '-pubout', '-out', str(cls.public)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.keys.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.app = self.root / 'apps'
        factory = self.app / 'releases/factory'
        factory.mkdir(parents=True)
        atomic_json(factory / 'installed.json', {'version': '0.1.0', 'data_schema': 1, 'commit': 'a'*40})
        (self.app / 'current').symlink_to('releases/factory')
        self.data = self.root / 'data.db'
        self.data.write_bytes(b'USER DATA MUST SURVIVE')
        self.contract = dict(TARGET, app_id='dshancontrol', repository='dshanpi/DshanControl',
                             data_schema=1, allowed_channels=['stable'], updater_version='0.1.0',
                             application_root=str(self.app))
        atomic_json(self.root / 'contract.json', self.contract)
        self.config = dict(application_root=str(self.app), system_contract=str(self.root / 'contract.json'),
                           public_key=str(self.public), service_hook='/nonexistent/system-hook',
                           development_mode=True, health_observation_seconds=0, startup_timeout_seconds=0)
        self.updater = Updater(self.config)
        self.operations = []
        self.updater.hook = lambda operation, check=True: self.operations.append(operation) or True
        self.package = self.root / 'app.tar.gz'
        self.make_package()
        target = dict(TARGET, filename='app.tar.gz', sha256=digest(self.package),
                      size=self.package.stat().st_size, unpacked_size=self.unpacked)
        self.manifest = dict(schema_version=1, app_id='dshancontrol', repository='dshanpi/DshanControl',
                             release_kind='application', channel='stable', release_state='draft',
                             minimum_updater_version='0.1.0', version='0.2.0', tag='app-v0.2.0', sequence=1, commit='b'*40,
                             data_schema=1, from_versions=['0.1.0'], targets=[target],
                             source={'filename':'source.tar.gz','sha256':'c'*64},
                             build_info={'filename':'build-info.json','sha256':'d'*64})
        self.path = self.root / 'application-release.json'
        self.signature = self.root / 'application-release.json.sig'
        self.sign()

    def make_package(self, extra=None):
        self.unpacked = 0
        with tarfile.open(str(self.package), 'w:gz') as archive:
            for name in REQUIRED:
                body = ('payload:' + name).encode()
                info = tarfile.TarInfo(name)
                info.size = len(body)
                info.mode = 0o755 if name.startswith('bin/') else 0o644
                archive.addfile(info, io.BytesIO(body))
                self.unpacked += len(body)
            if extra:
                archive.addfile(extra)

    def sign(self):
        self.path.write_text(json.dumps(self.manifest))
        subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(self.private),
                        '-out', str(self.signature), str(self.path)], check=True)

    def install(self):
        return self.updater.install(self.path, self.signature, self.package)

    def test_install_preserves_data_and_system_identity(self):
        before = (self.root / 'contract.json').read_bytes()
        name = self.install()
        self.assertEqual(name, self.updater.current_path().name)
        self.assertEqual('0.2.0', read_json(self.updater.current_path() / 'installed.json')['version'])
        self.assertEqual(1, self.updater.state()['high_water'])
        self.assertEqual(before, (self.root / 'contract.json').read_bytes())
        self.assertEqual(b'USER DATA MUST SURVIVE', self.data.read_bytes())
        self.assertEqual(['maintenance', 'stop', 'start', 'health'], self.operations)

    def test_every_identity_field_rejected_before_stop(self):
        for field in TARGET_FIELDS:
            with self.subTest(field=field):
                original = self.manifest['targets'][0][field]
                self.manifest['targets'][0][field] = 2 if field == 'updater_protocol' else 'other'
                self.sign()
                with self.assertRaises(Rejected): self.install()
                self.assertEqual([], self.operations)
                self.manifest['targets'][0][field] = original

    def test_wrong_product_repository_system_and_channel_rejected(self):
        for key, value in [('app_id','smartdesktopmimi'), ('repository','other/repo'),
                           ('release_kind','system'), ('channel','prerelease')]:
            with self.subTest(key=key):
                old = self.manifest[key]
                self.manifest[key] = value
                self.sign()
                with self.assertRaises(Rejected): self.install()
                self.assertEqual([], self.operations)
                self.manifest[key] = old

    def test_signed_manifest_tampering_rejected(self):
        self.path.write_text(self.path.read_text().replace('0.2.0', '0.3.0'))
        with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_package_tampering_rejected(self):
        with self.package.open('ab') as stream: stream.write(b'bad')
        with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_untrusted_signature_rejected(self):
        self.signature.write_bytes(b'not-a-signature')
        with self.assertRaises(Rejected): self.install()

    def test_migration_rejected(self):
        self.manifest['data_schema'] = 2
        self.sign()
        with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_no_upgrade_path_rejected(self):
        self.manifest['from_versions'] = ['0.0.1']
        self.sign()
        with self.assertRaises(Rejected): self.install()

    def test_draft_rejected_in_production(self):
        self.updater.config['development_mode'] = False
        with self.assertRaises(Rejected): self.install()

    def test_public_requires_board_evidence(self):
        self.manifest['release_state'] = 'qualified'
        with self.assertRaises(Rejected): validate_manifest(self.manifest, public=True)
        self.manifest['targets'][0]['validation'] = dict.fromkeys(GATES, 'passed')
        with self.assertRaises(Rejected): validate_manifest(self.manifest, public=True)

    def test_configuration_cannot_redirect_installation_outside_contract(self):
        config = dict(self.config, application_root=str(self.root / 'outside'))
        with self.assertRaises(Rejected): Updater(config)
        self.assertFalse((self.root / 'outside').exists())

    def test_minimum_updater_version_enforced(self):
        self.manifest['minimum_updater_version'] = '9.0.0'
        self.sign()
        with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_pending_system_contract_rejected(self):
        self.updater.contract['hardware_revision'] = 'pending'
        with self.assertRaises(Rejected): self.install()

    def test_missing_or_unknown_target_fields_fail_before_hooks(self):
        for field in TARGET_FIELDS:
            for value in (None, 'unknown', 'pending'):
                with self.subTest(field=field, value=value):
                    original = self.updater.contract.pop(field)
                    if value is not None:
                        self.updater.contract[field] = value
                    with self.assertRaises(Rejected): self.install()
                    self.assertEqual([], self.operations)
                    self.updater.contract[field] = original

    def test_channels_must_be_explicit_and_well_formed(self):
        for value in (None, [], 'stable', ['unknown'], ['stable', None]):
            with self.subTest(value=value):
                self.updater.contract.pop('allowed_channels', None)
                if value is not None:
                    self.updater.contract['allowed_channels'] = value
                with self.assertRaisesRegex(Rejected, 'explicit allowed channels'):
                    self.install()
                self.assertEqual([], self.operations)
        self.updater.contract['allowed_channels'] = ['stable']
        self.install()

    def test_development_mode_requires_boolean(self):
        for value in ('false', 'true', 0, 1, None, []):
            with self.subTest(value=value):
                with self.assertRaisesRegex(Rejected, 'development_mode must be a boolean'):
                    Updater(dict(self.config, development_mode=value))
        production = Updater({k: v for k, v in self.config.items() if k != 'development_mode'})
        production.hook = lambda operation, check=True: self.operations.append(operation) or True
        with self.assertRaisesRegex(Rejected, 'release gates incomplete'):
            production.install(self.path, self.signature, self.package)
        self.assertEqual([], self.operations)
        # Synthetic host-only evidence isolates the independent draft-state gate.
        # It is not a board qualification or a production release artifact.
        evidence = dict.fromkeys(GATES, 'passed')
        evidence.update(system_image_sha256='d'*64, system_contract_sha256='e'*64,
                        log_sha256='f'*64, tester='host-fixture-only', tested_at='host-fixture-only')
        self.manifest['targets'][0]['validation'] = evidence
        self.sign()
        with self.assertRaisesRegex(Rejected, 'draft cannot'):
            production.install(self.path, self.signature, self.package)
        self.assertEqual([], self.operations)

    def test_installed_schema_version_and_sequence_are_not_coerced(self):
        installed = self.updater.current_path() / 'installed.json'
        for record in ({}, {'version': 'pending', 'data_schema': 1},
                       {'version': '0.1.0', 'data_schema': True},
                       {'version': '0.1.0', 'data_schema': '1'}):
            with self.subTest(record=record):
                atomic_json(installed, record)
                with self.assertRaises(Rejected): self.install()
                self.assertEqual([], self.operations)
        atomic_json(installed, {'version': '0.1.0', 'data_schema': 1})
        self.updater.contract['data_schema'] = True
        with self.assertRaises(Rejected): self.install()
        self.updater.contract['data_schema'] = 1
        for value in (True, -1, '0', None):
            atomic_json(self.app / 'update-state.json', {'high_water': value, 'failed': []})
            with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_interlock_failure_does_not_stop_application(self):
        def hook(operation, check=True):
            self.operations.append(operation)
            raise Rejected('control loop is active')
        self.updater.hook = hook
        with self.assertRaises(Rejected): self.install()
        self.assertEqual(['maintenance'], self.operations)
        self.assertEqual('factory', self.updater.current_path().name)
        self.assertFalse((self.app / 'transaction.json').exists())

    def test_unhealthy_new_application_rolls_back_without_replay(self):
        self.updater.healthy = lambda: self.updater.current_path().name == 'factory'
        with self.assertRaises(Rejected): self.install()
        self.assertEqual('factory', self.updater.current_path().name)
        self.assertEqual(1, self.updater.state()['high_water'])
        self.assertEqual(1, len(self.updater.state()['failed']))
        self.assertFalse((self.app / 'transaction.json').exists())
        with self.assertRaises(Rejected): self.install()
        self.assertEqual(b'USER DATA MUST SURVIVE', self.data.read_bytes())

    def test_power_interruption_after_switch_recovers_old_version(self):
        real_switch = self.updater.switch
        def interrupt(name):
            real_switch(name)
            raise SystemExit('simulated power loss')
        self.updater.switch = interrupt
        with self.assertRaises(SystemExit): self.install()
        self.assertNotEqual('factory', self.updater.current_path().name)
        self.assertTrue((self.app / 'transaction.json').exists())
        self.updater.switch = real_switch
        self.updater.recover()
        self.assertEqual('factory', self.updater.current_path().name)
        self.assertEqual(1, self.updater.state()['high_water'])

    def test_interrupted_stop_recovers(self):
        def hook(operation, check=True):
            if operation == 'stop': raise SystemExit('power loss before switch')
            return True
        self.updater.hook = hook
        with self.assertRaises(SystemExit): self.install()
        self.updater.hook = lambda operation, check=True: True
        self.updater.recover()
        self.assertEqual('factory', self.updater.current_path().name)

    def test_failed_rollback_keeps_recoverable_journal(self):
        self.updater.healthy = lambda: False
        with self.assertRaises(Rejected): self.install()
        self.assertTrue((self.app / 'transaction.json').exists())
        self.updater.healthy = lambda: True
        self.updater.recover()
        self.assertFalse((self.app / 'transaction.json').exists())

    def test_committed_journal_does_not_rollback_successful_version(self):
        name = self.install()
        atomic_json(self.app / 'transaction.json', {'old':'factory','new':name,'phase':'committed'})
        self.updater.recover()
        self.assertEqual(name, self.updater.current_path().name)

    def test_health_allows_bounded_startup_then_observes_readiness(self):
        values = iter([False, True])
        self.updater.config['startup_timeout_seconds'] = 10
        self.updater.config['health_interval_seconds'] = 0
        self.updater.hook = lambda operation, check=True: next(values)
        self.assertTrue(self.updater.healthy())

    def test_install_lock_rejected(self):
        with self.updater.locked():
            with self.assertRaises(Rejected): self.install()

    def test_low_space_rejected_before_stop(self):
        with patch('updater.shutil.disk_usage') as usage:
            usage.return_value.free = 0
            with self.assertRaises(Rejected): self.install()
        self.assertEqual([], self.operations)

    def test_malicious_archives_rejected(self):
        for name, kind in [('../outside',tarfile.REGTYPE), ('/etc/passwd',tarfile.REGTYPE),
                           ('etc/passwd',tarfile.REGTYPE), ('lib/link',tarfile.SYMTYPE),
                           ('lib/device',tarfile.CHRTYPE), ('lib/hardlink',tarfile.LNKTYPE),
                           ('bin/omnigate-hmi',tarfile.REGTYPE)]:
            with self.subTest(name=name, kind=kind):
                info = tarfile.TarInfo(name)
                info.type = kind
                info.linkname = '/etc/passwd'
                self.make_package(info)
                with tempfile.TemporaryDirectory() as dst:
                    with self.assertRaises(Rejected): extract_payload(self.package, Path(dst), self.unpacked)

    def test_github_selects_signed_compatible_application_not_latest_system(self):
        evidence = dict.fromkeys(GATES, 'passed')
        evidence.update(system_image_sha256='d'*64, system_contract_sha256='e'*64,
                        log_sha256='f'*64, tester='test-fixture-only', tested_at='test-fixture-only')
        self.manifest['targets'][0]['validation'] = evidence
        self.manifest['release_state'] = 'qualified'
        self.sign()
        prefix = 'https://github.com/dshanpi/DshanControl/releases/download/app-v0.2.0/'
        release = {'tag_name':'app-v0.2.0','draft':False,'prerelease':False,
                   'assets':[{'name':name,'browser_download_url':prefix+name} for name in
                             ('application-release.json','application-release.json.sig','app.tar.gz')]}
        releases = [{'tag_name':'system-v99','draft':False}, release]
        def asset(url, limit):
            return self.signature.read_bytes() if url.endswith('.sig') else self.path.read_bytes()
        with patch('updater.fetch_bytes', return_value=json.dumps(releases).encode()), \
             patch('updater.fetch_asset', side_effect=asset):
            selected = self.updater.check_github()
            self.assertEqual('0.2.0', selected['version'])
            self.updater.contract['board_id'] = 't153'
            self.assertIsNone(self.updater.check_github())

    def test_github_cannot_relabel_prerelease_as_stable(self):
        evidence = dict.fromkeys(GATES, 'passed')
        evidence.update(system_image_sha256='d'*64, system_contract_sha256='e'*64,
                        log_sha256='f'*64, tester='fixture', tested_at='fixture')
        self.manifest['targets'][0]['validation'] = evidence
        self.manifest['release_state'] = 'qualified'
        self.sign()
        release = {'tag_name':'app-v0.2.0','draft':False,'prerelease':True,
                   'assets':[{'name':name,'browser_download_url':name} for name in
                             ('application-release.json','application-release.json.sig','app.tar.gz')]}
        def asset(url, limit):
            return self.signature.read_bytes() if url.endswith('.sig') else self.path.read_bytes()
        with patch('updater.fetch_bytes', return_value=json.dumps([release]).encode()), \
             patch('updater.fetch_asset', side_effect=asset):
            self.assertIsNone(self.updater.check_github())

    def test_duplicate_json_keys_rejected(self):
        self.path.write_text('{"app_id":"dshancontrol","app_id":"other"}')
        with self.assertRaises(Rejected): read_json(self.path)

    def test_expansion_limit_enforced(self):
        with tempfile.TemporaryDirectory() as dst:
            with self.assertRaises(Rejected): extract_payload(self.package, Path(dst), 1)


if __name__ == '__main__': unittest.main()
