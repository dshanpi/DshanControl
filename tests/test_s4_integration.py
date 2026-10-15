import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class S4IntegrationTests(unittest.TestCase):
    def install_source(self, base):
        source = base / 'source'
        for name in ('components', 'boards/t113s4-sdnand'):
            shutil.copytree(str(ROOT / name), str(source / name))
        (source / 'build-provenance.json').write_text(json.dumps({
            'version': '0.1.0-dev', 'application_commit': 'a' * 40,
            'status': 'development-draft'}))
        hmi = base / 'omnigate-hmi'
        hmi.write_text('#!/bin/sh\nexit 0\n')
        return source, hmi

    def test_native_system_install_keeps_updater_outside_application_and_trust_pending(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source, hmi = self.install_source(base)
            target = base / 'target'
            native = ROOT / 'dist/native-runtime/dshancontrol-runtime'
            subprocess.run([sys.executable, str(ROOT / 'scripts/install_s4_runtime.py'),
                '--source', str(source), '--target', str(target), '--hmi', str(hmi),
                '--runtime', str(native)], check=True)
            self.assertEqual(native.read_bytes(), (target / 'usr/bin/dshancontrol-runtime').read_bytes())
            self.assertIn('exec /usr/bin/dshancontrol-runtime "$@"',
                          (target / 'usr/bin/dshancontrol-update').read_text())
            config = json.loads((target / 'etc/dshancontrol/updater.json').read_text())
            self.assertEqual(0o600, (target / 'etc/dshancontrol/updater.json').stat().st_mode & 0o777)
            self.assertIs(True, config['task_service'])
            self.assertIs(False, config['development_mode'])
            self.assertIs(False, config['automatic_install'])
            service = json.loads((target / 'etc/dshancontrol/platform-service.json').read_text())
            self.assertEqual([0], service['allowed_uids'])
            self.assertEqual(0o600, (target / 'etc/dshancontrol/platform-service.json').stat().st_mode & 0o777)
            self.assertTrue(os.access(target / 'etc/init.d/S69dshancontrol-runtime', os.X_OK))
            contract = json.loads((target / 'etc/dshancontrol/system-release.json').read_text())
            self.assertEqual('ext4', contract['storage']['filesystem'])
            self.assertEqual('pending', contract['hardware_revision'])
            self.assertEqual('not-run', contract['validation'])
            self.assertFalse((target / 'etc/dshancontrol/update-public.pem').exists())
            self.assertFalse((target / 'mnt/UDISK').exists())
            factory = target / 'usr/share/dshancontrol/factory'
            self.assertFalse(list(factory.rglob('*runtime*')))
            self.assertFalse((factory / 'etc').exists())
            # Reproduction of the old Python image cannot silently leave a mixed target.
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/install_s4_runtime.py'),
                '--source', str(source), '--target', str(target), '--hmi', str(hmi)],
                capture_output=True, text=True)
            self.assertNotEqual(0, result.returncode)
            self.assertIn('native target requires --runtime', result.stderr)
            self.assertIs(True, json.loads((target / 'etc/dshancontrol/updater.json').read_text())['task_service'])

    def test_native_install_rejects_links_before_replacing_existing_factory(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source, hmi = self.install_source(base)
            outside = base / 'outside'
            outside.mkdir()
            sentinel = outside / 'sentinel'
            sentinel.write_bytes(b'preserve external data')
            for index, relative in enumerate(('usr/bin', 'etc/dshancontrol/platform-service.json',
                    'etc/init.d/S69dshancontrol-runtime', 'usr/share/dshancontrol/defaults/web.json',
                    'usr/lib/dshancontrol-update/health.py')):
                with self.subTest(path=relative):
                    target = base / ('target-' + str(index))
                    old = target / 'usr/share/dshancontrol/factory/preserve'
                    old.parent.mkdir(parents=True)
                    old.write_bytes(b'old factory')
                    link = target / relative
                    link.parent.mkdir(parents=True, exist_ok=True)
                    link.symlink_to(outside if relative == 'usr/bin' else sentinel)
                    result = subprocess.run([sys.executable, str(ROOT / 'scripts/install_s4_runtime.py'),
                        '--source', str(source), '--target', str(target), '--hmi', str(hmi),
                        '--runtime', str(ROOT / 'dist/native-runtime/dshancontrol-runtime')],
                        capture_output=True, text=True)
                    self.assertNotEqual(0, result.returncode)
                    self.assertIn('symlink staging destination', result.stderr)
                    self.assertEqual(b'old factory', old.read_bytes())
                    self.assertEqual(b'preserve external data', sentinel.read_bytes())
                    self.assertEqual([sentinel], list(outside.iterdir()))

    def test_vendor_stages_build_gnu_cpp11_without_leaking_to_dependencies(self):
        # Exercise GNU make's real target scoping with a compiler whose default
        # is deliberately C++98, as on the S4 GCC 5 toolchain.
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            (base / 'memory.cpp').write_text(
                '#include <atomic>\n#include <functional>\n'
                'int main() { int *p = nullptr; typeof(p) q = p; '
                'std::atomic<int> n(0); std::function<int()> f = [&] { return n.load(); }; '
                'return (q != nullptr) + f(); }\n')
            (base / 'dependency.cpp').write_text(
                '#if __cplusplus != 199711L\n#error flags leaked to dependency\n#endif\n'
                'int main() { return 0; }\n')
            (base / 'interface.c').write_text(
                '#ifdef __cplusplus\n#error C interface must stay C\n#endif\n'
                'int interface(void) { return 0; }\n')
            (base / 'common.cpp').write_text(
                '#if __cplusplus != 199711L\n#error unnecessary language change\n#endif\n'
                'int main() { return atoi("0"); }\n')
            stages = ['memory/.stamp_built', 'endec/.stamp_staging_installed',
                      'camera/.stamp_built', 'camera/.stamp_staging_installed']
            for folder in ('memory', 'endec', 'camera', 'common'):
                (base / folder).mkdir()
            (base / 'Makefile').write_text(
                'BR2_EXTERNAL_DSHANCONTROL_PATH := ' + str(ROOT / 'boards/t113s4-sdnand/buildroot') + '\n'
                'TARGET_CC := cc -std=c99\nTARGET_CXX := c++ -std=c++98\n'
                'SDK_MEMORY_DIR := memory\nSDK_ENDEC_DIR := endec\nSDK_CAMERA_DIR := camera\n'
                'SDK_COMMON_DIR := common\n'
                'TPLAYER_DIR := tplayer\n'
                'include $(BR2_EXTERNAL_DSHANCONTROL_PATH)/external.mk\n'
                '.PHONY: all\nall: common/.stamp_built ' + ' '.join(stages) + '\n'
                + ' '.join(stages) + ': dependency\n'
                '\t$(TARGET_CC) -Werror -c interface.c -o $@.o\n'
                '\t$(TARGET_CXX) -Werror memory.cpp $@.o -o $@.probe\n\t./$@.probe\n'
                'common/.stamp_built: dependency\n'
                '\t$(TARGET_CXX) -Werror common.cpp -o $@.probe\n\t./$@.probe\n'
                'dependency:\n\t$(TARGET_CXX) -Werror dependency.cpp -o dependency\n\t./dependency\n')
            subprocess.run(['make', '-C', str(base), 'all'], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def test_tplayer_install_selects_synced_softfp_libraries_only(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            for name, content in [('tplayer/arm-linux-gnueabi', b'softfp'),
                                  ('tplayer/arm-linux-gnueabihf', b'hardfloat'),
                                  ('unrelated', b'unchanged')]:
                folder = base / name
                folder.mkdir(parents=True)
                (folder / 'libAudioGain.a').write_bytes(content)
            stages = ['tplayer/.stamp_staging_installed', 'tplayer/.stamp_target_installed']
            (base / 'Makefile').write_text(
                'BR2_EXTERNAL_DSHANCONTROL_PATH := ' + str(ROOT / 'boards/t113s4-sdnand/buildroot') + '\n'
                'SDK_MEMORY_DIR := memory\nSDK_ENDEC_DIR := endec\nSDK_CAMERA_DIR := camera\n'
                'SDK_COMMON_DIR := common\nTPLAYER_DIR := tplayer\nAUDIO_LIBS := unrelated\n'
                'include $(BR2_EXTERNAL_DSHANCONTROL_PATH)/external.mk\n'
                '.PHONY: all\nall: ' + ' '.join(stages) + '\n'
                + ' '.join(stages) + ': dependency\n'
                '\tcp $(AUDIO_LIBS)/libAudioGain.a $@.copy\n'
                'dependency:\n\tcp $(AUDIO_LIBS)/libAudioGain.a dependency\n')
            subprocess.run(['make', '-C', str(base), 'all'], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(b'unchanged', (base / 'dependency').read_bytes())
            for stage in stages:
                self.assertEqual(b'softfp', (base / (stage + '.copy')).read_bytes())

    def test_cedarse_patch_rejects_unknown_toolchain_and_recovers(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            # Minimal original vendor selector, retaining the actual patch context.
            (base / 'Makefile').write_text(
                '\tifeq ($(BR2_TOOLCHAIN_EXTERNAL_CUSTOM), y)\n'
                '\t\tifeq ($(findstring gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf, '
                '$(BR2_TOOLCHAIN_EXTERNAL_PATH)), gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf)\n'
                '\t\t\tLIB_PATH = ./lib/buildroot-gcc-arm-arm-10.3\n'
                '\t\telse\n\t\t\t$(error error:TOOLCHAIN unsupport!)\n\t\tendif\n\tendif\n'
                'report:\n\t@echo $(LIB_PATH)\n')
            patch_file = ROOT / ('boards/t113s4-sdnand/buildroot/patches/libcedarse/'
                                 '0001-support-pinned-preinstalled-linaro5-softfp.patch')
            subprocess.run(['patch', '--batch', '-p1', '-i', str(patch_file)], cwd=str(base),
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            fields = {'BR2_TOOLCHAIN_EXTERNAL_CUSTOM': 'y',
                      'BR2_TOOLCHAIN_EXTERNAL_CUSTOM_PREFIX': '"arm-linux-gnueabi"',
                      'BR2_TOOLCHAIN_EXTERNAL_GCC_5': 'y', 'BR2_ARM_EABI': 'y',
                      'BR2_TOOLCHAIN_EXTERNAL_CUSTOM_GLIBC': 'y'}

            def invoke(values):
                return subprocess.run(['make', '--no-print-directory', '-C', str(base), 'report']
                                      + [k + '=' + v for k, v in values.items()],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

            self.assertEqual(0, invoke(fields).returncode)
            for key, value in [('BR2_TOOLCHAIN_EXTERNAL_CUSTOM_PREFIX', '"arm-linux-gnueabihf"'),
                               ('BR2_TOOLCHAIN_EXTERNAL_GCC_5', ''), ('BR2_ARM_EABI', ''),
                               ('BR2_TOOLCHAIN_EXTERNAL_CUSTOM_GLIBC', '')]:
                bad = dict(fields, **{key: value})
                self.assertNotEqual(0, invoke(bad).returncode, key)
            result = invoke(fields)
            self.assertEqual(0, result.returncode, result.stdout)
            self.assertEqual('./lib/buildroot-toolchain-external-linaro-armsf', result.stdout.strip())

    def test_full_rootfs_installs_vendor_init_without_overwriting_unknown_entries(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            target, busybox_build = base / 'target', base / 'busybox-build'
            (target / 'bin').mkdir(parents=True)
            busybox_build.mkdir()
            binary = target / 'bin/busybox'
            binary.write_text('#!/bin/sh\nexit 99\n')
            binary.chmod(0o755)
            config = busybox_build / '.config'
            config.write_text('CONFIG_INIT=y\n')
            (base / 'Makefile').write_text(
                'BR2_EXTERNAL_DSHANCONTROL_PATH := ' + str(ROOT / 'boards/t113s4-sdnand/buildroot') + '\n'
                'TARGET_DIR := ' + str(target) + '\nBUSYBOX_DIR := ' + str(busybox_build) + '\n'
                'SDK_MEMORY_DIR := memory\nSDK_ENDEC_DIR := endec\nSDK_CAMERA_DIR := camera\n'
                'SDK_COMMON_DIR := common\nTPLAYER_DIR := tplayer\n'
                'include $(BR2_EXTERNAL_DSHANCONTROL_PATH)/external.mk\n'
                '.PHONY: finalize\nfinalize:\n\t$(call $(TARGET_FINALIZE_HOOKS))\n')

            def invoke(destination=None):
                argv = ['make', '--no-print-directory', '-C', str(base), 'finalize']
                if destination is not None:
                    argv.append('TARGET_DIR=' + destination)
                return subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

            self.assertNotEqual(0, invoke('/').returncode)
            init = target / 'init'
            self.assertEqual(0, invoke().returncode)
            self.assertTrue(init.is_symlink())
            self.assertEqual('bin/busybox', os.readlink(init))
            self.assertEqual(binary.read_bytes(), init.read_bytes())
            self.assertEqual(0, invoke().returncode)  # Idempotent; never executes BusyBox.
            init.unlink()
            init.write_bytes(b'unknown vendor entry')
            self.assertNotEqual(0, invoke().returncode)
            self.assertEqual(b'unknown vendor entry', init.read_bytes())
            init.unlink()
            outside = base / 'outside'
            outside.mkdir()
            sentinel = outside / 'busybox'
            sentinel.write_bytes(b'preserve outside target')
            sentinel.chmod(0o755)
            init.symlink_to(sentinel)
            self.assertNotEqual(0, invoke().returncode)
            self.assertEqual(str(sentinel), os.readlink(init))
            init.unlink()
            binary.chmod(0o644)
            self.assertNotEqual(0, invoke().returncode)
            self.assertFalse(init.exists())
            binary.chmod(0o755)
            config.write_text('# CONFIG_INIT is not set\n')
            self.assertNotEqual(0, invoke().returncode)
            self.assertFalse(init.exists())
            config.write_text('CONFIG_INIT=y\n')
            (target / 'bin').rename(target / 'bin-saved')
            (target / 'bin').symlink_to(outside, target_is_directory=True)
            self.assertNotEqual(0, invoke().returncode)
            self.assertEqual(b'preserve outside target', sentinel.read_bytes())
            (target / 'bin').unlink()
            (target / 'bin-saved').rename(target / 'bin')
            self.assertEqual(0, invoke().returncode)
            self.assertEqual('bin/busybox', os.readlink(init))

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
                         'lib/core/omnigate_core/api.py', 'share/web/index.html',
                         'share/licenses/DshanSans-OFL.txt', 'share/licenses/DshanSans-manifest.json'):
                self.assertTrue((factory / name).is_file(), name)
            metadata = json.loads((factory / 'share/licenses/DshanSans-manifest.json').read_text())
            self.assertEqual(metadata['license_sha256'], hashlib.sha256(
                (factory / 'share/licenses/DshanSans-OFL.txt').read_bytes()).hexdigest())
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
