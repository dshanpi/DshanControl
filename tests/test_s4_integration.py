import importlib.util
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class S4IntegrationTests(unittest.TestCase):
    def test_runtime_install_is_confined_and_keeps_user_data_out_of_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source, target = base / 'source', base / 'target'
            for name in ('components', 'boards/t113s4-sdnand'):
                shutil.copytree(str(ROOT / name), str(source / name))
            (source / 'build-provenance.json').write_text(json.dumps({
                'version':'0.1.0-dev', 'application_commit':'a'*40, 'status':'development-draft'}))
            hmi = base / 'omnigate-hmi'
            hmi.write_text('#!/bin/sh\nexit 0\n')
            subprocess.run(['python3', str(ROOT / 'scripts/install_s4_runtime.py'), '--source', str(source),
                            '--target', str(target), '--hmi', str(hmi)], check=True)
            factory = target / 'usr/share/dshancontrol/factory'
            for name in ('bin/omnigate-hmi', 'lib/web/app.py', 'lib/supervisor/supervisor.py',
                         'lib/core/omnigate_core/api.py', 'share/web/index.html'):
                self.assertTrue((factory / name).is_file(), name)
            self.assertFalse((target / 'mnt/UDISK').exists())
            self.assertFalse((target / 'etc/dshancontrol/update-public.pem').exists())
            config = json.loads((target / 'etc/dshancontrol/updater.json').read_text())
            self.assertFalse(config['development_mode'])
            self.assertFalse(config['automatic_install'])
            default = json.loads((target / 'usr/share/dshancontrol/defaults/web.json').read_text())
            self.assertEqual({}, default['can'])
            self.assertEqual({}, default['ec20']['profiles'])
            self.assertFalse((factory / 'etc').exists())
            self.assertTrue(os.access(str(target / 'usr/libexec/dshancontrol-services'), os.X_OK))

    def test_s4_manifest_does_not_inherit_t153_fieldbus_claims(self):
        manifest = json.loads((ROOT / 'boards/t113s4-sdnand/platform.json').read_text())
        self.assertEqual('100ask-t113s4-sdnand', manifest['board_id'])
        self.assertEqual([], manifest['capabilities'])
        self.assertEqual(['display0'], list(manifest['channels']))
        self.assertEqual('not-run', manifest['validation'])

    def test_s4_uses_old_buildroot_generic_qmake_adapter(self):
        recipe = (ROOT / 'boards/t113s4-sdnand/buildroot/package/dshancontrol/dshancontrol.mk').read_text()
        self.assertIn('$(eval $(generic-package))', recipe)
        self.assertIn('$(HOST_DIR)/bin/qmake', recipe)
        self.assertIn('$(eval $(qmake-package))', (ROOT / 'components/hmi/omnigate-hmi.mk').read_text())

    def test_updater_contract_matches_documented_protocol(self):
        import sys
        sys.path.insert(0, str(ROOT / 'components/updater'))
        import update_protocol as protocol
        value = json.loads((ROOT / 'platform/contracts/application-update-v1.json').read_text())
        self.assertEqual(list(protocol.TARGET_FIELDS), value['target_fields'])
        self.assertEqual(list(protocol.REQUIRED), value['required_payload'])
        self.assertEqual(list(protocol.GATES), value['public_release_requires'])


if __name__ == '__main__': unittest.main()
