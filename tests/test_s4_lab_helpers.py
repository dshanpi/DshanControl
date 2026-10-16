"""Lab maintenance keeps semantic safety gates across durable JSON serialization."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

HELPERS = Path(__file__).resolve().parent / 'hardware'
sys.path.insert(0, str(HELPERS))
spec = importlib.util.spec_from_file_location('s4_lab_maintenance_fixture', HELPERS / 's4_ota_lab_maintenance.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.path.remove(str(HELPERS))
bootstrap_spec = importlib.util.spec_from_file_location('s4_lab_bootstrap_fixture', HELPERS / 's4_ota_lab_bootstrap.py')
bootstrap = importlib.util.module_from_spec(bootstrap_spec)
bootstrap_spec.loader.exec_module(bootstrap)


class LabStorageRoutingTests(unittest.TestCase):
    def setUp(self):
        self.config = dict(development_mode=True, automatic_install=False,
                           application_root=str(bootstrap.RUNTIME / 'apps'),
                           system_contract=str(bootstrap.PERSISTENT / 'system/contract.json'))
        self.formal = dict(board_id='100ask-t113s4-sdnand',
                           storage=dict(mount_point='/mnt/UDISK', device='/dev/by-name/UDISK',
                                        filesystem='ext4'))

    def test_lab_can_use_original_storage_contract_before_persistent_contract_is_readable(self):
        bootstrap.storage_routing(self.config, self.formal)

    def test_production_routing_refuses_before_mounting(self):
        self.config.update(development_mode=False, application_root='/mnt/UDISK/dshancontrol/apps')
        with self.assertRaisesRegex(RuntimeError, 'Fixed development lab routing'):
            bootstrap.storage_routing(self.config, self.formal)

    def test_different_partition_contract_refuses_before_mounting(self):
        self.formal['storage']['device'] = '/dev/mmcblk0p8'
        with self.assertRaisesRegex(RuntimeError, 'Original S4 storage contract'):
            bootstrap.storage_routing(self.config, self.formal)


class LabMaintenanceDefaultsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.defaults = Path(temporary.name) / 'defaults'
        self.data = Path(temporary.name) / 'data'
        self.defaults.mkdir()
        self.data.mkdir()
        values = {'web.json': {'can': {}, 'canopen': {'nodes': {}}, 'ec20': {'profiles': {}}},
                  'hmi.json': {'brightness': 220, 'demo_mode': False}}
        self.context = {'default_file_sha256': {}}
        for name, value in values.items():
            body = (json.dumps(value, indent=2) + '\n').encode()
            (self.defaults / name).write_bytes(body)
            (self.data / name).write_text(json.dumps(value, sort_keys=True, separators=(',', ':')))
            self.context['default_file_sha256'][name] = hashlib.sha256(body).hexdigest()

    def test_durable_json_reformatting_preserves_the_same_configuration(self):
        module.verify_default_configuration(self.context, self.defaults, self.data)
        self.assertNotEqual((self.defaults / 'web.json').read_bytes(), (self.data / 'web.json').read_bytes())

    def test_semantic_transport_change_is_still_refused(self):
        (self.data / 'web.json').write_text(json.dumps({'can': {'interface': 'can0'}}))
        with self.assertRaisesRegex(RuntimeError, 'Lab default configuration changed'):
            module.verify_default_configuration(self.context, self.defaults, self.data)

    def test_replacing_trusted_defaults_cannot_redefine_the_gate(self):
        for path in [self.defaults / 'web.json', self.data / 'web.json']:
            path.write_text(json.dumps({'can': {'interface': 'can0'}}))
        with self.assertRaisesRegex(RuntimeError, 'Trusted lab defaults changed'):
            module.verify_default_configuration(self.context, self.defaults, self.data)
