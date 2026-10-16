"""Run the existing fault/recovery matrix against the C++ executable."""
import json
import os
from pathlib import Path
import subprocess
import shlex
import stat
import test_application_update_cli as reference


class NativeUpdateCliTests(reference.UpdateCliTests):
    def test_private_daemon_umask_publishes_readable_immutable_application_only(self):
        args=[str(self.binary),'--config',str(self.config),'install',str(self.f.path),
              str(self.f.signature),str(self.f.package)]
        result=subprocess.run(args,capture_output=True,text=True,timeout=60,
                              preexec_fn=lambda:os.umask(0o077))
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        current=self.f.updater.current_path()
        self.assertEqual(0o755,stat.S_IMODE(current.stat().st_mode))
        for path in current.rglob('*'):
            with self.subTest(path=str(path.relative_to(current))):
                expected=0o755 if path.is_dir() or path.name=='omnigate-hmi' else 0o644
                self.assertEqual(expected,stat.S_IMODE(path.stat().st_mode))
        for name in ['.update.lock','.control.lock','update-state.json']:
            self.assertEqual(0o600,stat.S_IMODE((self.f.app/name).stat().st_mode))
        self.assertEqual(b'USER DATA MUST SURVIVE',self.f.data.read_bytes())
        reduced_uid=False
        if os.geteuid()==0 and self.f.root.parent==Path('/tmp'):
            # Only this synthetic fixture is made traversable; never relax a
            # shared private validation/user-data ancestor for a test process.
            for path in [self.f.root,self.f.app,self.f.app/'releases']:
                path.chmod(0o755)
            def drop():
                os.setgroups([])
                os.setresgid(60000,60000,60000)
                os.setresuid(60001,60001,60001)
            probe='''import os,sys,json
from pathlib import Path
root=Path(sys.argv[1]);value=json.loads((root/'current/installed.json').read_text())
assert value['sequence']==1
assert os.access(str(root/'current/bin/omnigate-hmi'),os.X_OK)
assert (root/'current/lib/web/app.py').read_bytes()
for name in ['.update.lock','.control.lock','update-state.json']:
 try:fd=os.open(str(root/name),os.O_RDONLY)
 except PermissionError:continue
 else:os.close(fd);raise AssertionError('Private updater file readable')
'''
            result=subprocess.run(['/usr/bin/python3','-c',probe,str(self.f.app)],
                capture_output=True,text=True,timeout=5,preexec_fn=drop)
            self.assertEqual(0,result.returncode,result.stdout+result.stderr)
            reduced_uid=True
        if os.environ.get('NATIVE_PUBLICATION_EVIDENCE'):
            Path(os.environ['NATIVE_PUBLICATION_EVIDENCE']).write_text(json.dumps({
                'private_daemon_umask':'077','version_directories':'0755',
                'executable_files':'0755','regular_files_and_installed_metadata':'0644',
                'update_locks_and_state':'0600','reduced_uid_kernel_access_verified':reduced_uid,
                'application_data_bytes_preserved':True},indent=2)+'\n')

    def test_native_lifecycle_hooks_adopt_real_leases_through_shell(self):
        self.f.config['application_health']={'pid_root':str(self.f.root/'absent-native-run'),
                                             'launch_receipts_required':True}
        reference.fixtures.atomic_json(self.config,self.f.config)
        self.hook.write_text('#!/bin/sh\nset -eu\nprintf "%s\\n" "$1" >> '+
            shlex.quote(str(self.f.root/'operations'))+'\nexec '+shlex.quote(str(self.binary))+
            ' --config '+shlex.quote(str(self.config))+' application-stop\n')
        self.cli()
        self.assertEqual(['maintenance','stop','start','health'],self.operations())
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
        result = subprocess.run(args, capture_output=True, text=True, timeout=60)
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

    def test_health_failure_keeps_exit_code_after_successful_rollback(self):
        self.hook.write_text(self.hook.read_text().replace(
            "sys.exit(1)\n", "print('PRIVATE_HOOK_OUTPUT_MUST_NOT_LEAK'); sys.exit(73)\n"))
        self.mode('bad-new')
        result = self.cli(expected=1)
        error = json.loads(result.stderr)
        self.assertEqual('health', error['stage'])
        self.assertEqual('73', error['code'])
        self.assertNotIn('PRIVATE_HOOK_OUTPUT_MUST_NOT_LEAK', result.stderr + result.stdout)
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertFalse((self.f.app / 'transaction.json').exists())
        self.assertEqual(1, self.f.updater.state()['high_water'])
        operations = self.operations()
        self.cli(expected=1, message='replayed or downgraded')
        self.assertEqual(operations, self.operations())

    def test_failed_rollback_preserves_original_health_and_recovery_exit_codes(self):
        self.hook.write_text(self.hook.read_text().replace(
            "sys.exit(1)\n", "sys.exit(74 if current == 'factory' else 73)\n"))
        self.mode('bad-all')
        error = json.loads(self.cli(expected=1).stderr)
        self.assertEqual('health', error['stage'])
        self.assertEqual('73', error['code'])
        self.assertIn('74 at rollback', error['message'])
        self.assertTrue((self.f.app / 'transaction.json').exists())
        self.mode('ok')
        self.cli('recover')
        self.assertFalse((self.f.app / 'transaction.json').exists())

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
