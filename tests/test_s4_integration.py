import importlib.util
import json
import os
import shutil
import subprocess
import sys
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

    def test_actual_s4_manifest_serves_hmi_without_t153_channels(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            (base / 'config.json').write_text('{"auth":{},"can":{},"canopen":{},"ec20":{"profiles":{}}}')
            (base / 'settings.json').write_text('{"demo_mode":false}')
            env = dict(os.environ, PYTHONPATH=str(ROOT / 'components/core/src') + ':' + str(ROOT / 'components/web'))
            env.update(OMNIGATE_CORE_PATH=str(ROOT / 'components/core/src'),
                       OMNIGATE_PLATFORM_MANIFEST=str(ROOT / 'boards/t113s4-sdnand/platform.json'),
                       OMNIGATE_WEB_CONFIG=str(base / 'config.json'),
                       OMNIGATE_CONFIG=str(base / 'config.json'),
                       OMNIGATE_WEB_SECRET=str(base / 'secret.key'),
                       OMNIGATE_WEB_LOG=str(base / 'web.log'),
                       OMNIGATE_EDGE_DB=str(base / 'edge.db'),
                       OMNIGATE_HMI_DB=str(base / 'hmi.db'),
                       OMNIGATE_HMI_SETTINGS=str(base / 'settings.json'),
                       OMNIGATE_TB_CONFIG=str(base / 'thingsboard.json'),
                       OMNIGATE_MODBUS_CONFIG=str(base / 'modbus.json'),
                       DSHANCONTROL_APPLICATION_COMMIT='fixture-commit')
            code = """
from unittest.mock import patch
from types import SimpleNamespace
import app
with patch('subprocess.run', return_value=SimpleNamespace(returncode=1, stdout='')):
    client = app.app.test_client()
    response = client.get('/api/hmi/v1/snapshot')
    assert response.status_code == 200, response.data
    assert client.get('/api/health').get_json()['application_commit'] == 'fixture-commit'
"""
            subprocess.run([sys.executable, '-c', code], env=env, check=True)

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
