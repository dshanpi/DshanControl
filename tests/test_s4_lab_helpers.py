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
