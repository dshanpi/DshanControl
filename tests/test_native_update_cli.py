"""Run the existing fault/recovery matrix against the C++ executable."""
import json
import os
from pathlib import Path
import subprocess
import test_application_update_cli as reference


class NativeUpdateCliTests(reference.UpdateCliTests):
    @classmethod
    def setUpClass(cls):
        cls.binary = Path(os.environ.get('DSHANCONTROL_NATIVE_UPDATER',
            str(reference.fixtures.ROOT / 'dist/native-runtime/dshancontrol-runtime'))).resolve()
        if not cls.binary.is_file():
            raise RuntimeError('Build the C++ runtime with make build-native before tests')
        super().setUpClass()

    def cli(self, command='install', expected=0, message=None):
        args = [str(self.binary), '--config', str(self.config), command]
        if command == 'install':
            args += [str(self.f.path), str(self.f.signature), str(self.f.package)]
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
        self.assertEqual(expected, result.returncode, result.stdout + result.stderr)
        if message:
            aliases = {'another update is running': 'owns the resource',
                       'unsafe package path': 'payload outside allowed roots',
                       'rollback health failed; journal retained': 'transaction retained'}
            self.assertIn(aliases.get(message, message), result.stderr.lower())
        self.assertEqual(b'USER DATA MUST SURVIVE', self.f.data.read_bytes())
        self.assertEqual(self.original_factory, (self.f.app / 'releases/factory/installed.json').read_bytes())
        self.assertEqual(self.original_contract, (self.f.root / 'contract.json').read_bytes())
        evidence = os.environ.get('NATIVE_UPDATE_EVIDENCE')
        if evidence:
            with open(evidence, 'a') as stream:
                stream.write(json.dumps({'test': self._testMethodName, 'command': command,
                    'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                    'operations': self.operations(), 'active': self.f.updater.current_path().name}) + '\n')
        return result

    def test_native_duplicate_json_is_rejected_before_hooks(self):
        body = self.f.path.read_text().replace('"app_id": "dshancontrol"',
            '"app_id": "other", "app_id": "dshancontrol"')
        self.f.path.write_text(body)
        subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(self.f.private),
                        '-out', str(self.f.signature), str(self.f.path)], check=True)
        self.cli(expected=1, message='duplicate json key')
        self.assertEqual([], self.operations())

    def test_production_refuses_unknown_storage_before_writing_or_hooks(self):
        self.f.config['development_mode'] = False
        reference.fixtures.atomic_json(self.config, self.f.config)
        self.cli(expected=1, message='production storage contract missing')
        self.assertEqual([], self.operations())
        self.assertFalse((self.f.app / 'update-state.json').exists())

    def test_native_pax_source_package_and_symlink_rejection(self):
        import io
        import tarfile
        self.f.unpacked = 0
        with tarfile.open(str(self.f.package), 'w:gz', format=tarfile.PAX_FORMAT) as archive:
            for name in reference.fixtures.REQUIRED:
                body = ('pax:' + name).encode()
                info = tarfile.TarInfo(name); info.size = len(body); info.mtime = 0.125
                info.mode = 0o755 if name.startswith('bin/') else 0o644
                archive.addfile(info, io.BytesIO(body)); self.f.unpacked += len(body)
        self.f.manifest['targets'][0].update(sha256=reference.fixtures.digest(self.f.package),
            size=self.f.package.stat().st_size, unpacked_size=self.f.unpacked)
        self.f.sign(); self.cli()
