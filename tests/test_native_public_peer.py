"""A real nonroot IPC peer uses native RPC without private configuration access."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import test_native_service as reference


@unittest.skipUnless(os.geteuid() == 0, 'requires real root to establish independent peer identities')
class NativePublicPeerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        reference.NativeServiceTests.setUpClass()

    @classmethod
    def tearDownClass(cls):
        reference.NativeServiceTests.tearDownClass()

    def test_nonroot_public_identity_and_status_cannot_manage_daemon_or_read_private_config(self):
        fixture = reference.NativeServiceTests()
        self.addCleanup(fixture.doCleanups)
        fixture.setUp()
        public = tempfile.TemporaryDirectory(prefix='dshan-public-peer-', dir='/tmp')
        self.addCleanup(public.cleanup)
        directory = Path(public.name); directory.chmod(0o755)
        binary = directory / 'runtime'; shutil.copy2(str(fixture.binary), str(binary)); binary.chmod(0o755)
        fixture.config.chmod(0o600)
        fixture.socket_path = str(directory / 'platform.sock')
        service = json.loads(fixture.service_config.read_text())
        service.update(socket=fixture.socket_path, client_gid=60000, allowed_uids=[0,60001])
        fixture.service_config.write_text(json.dumps(service)); fixture.start()
        observations = []

        def child(arguments, uid=60001):
            def reduce():
                os.setgroups([60000]); os.setresgid(60000,60000,60000); os.setresuid(uid,uid,uid)
            return subprocess.run(arguments, capture_output=True, text=True, timeout=12, preexec_fn=reduce,
                env={'PATH':'/usr/bin:/bin','HOME':'/nonexistent','DSHANCONTROL_PLATFORM_SOCKET':fixture.socket_path})

        inaccessible = child(['/usr/bin/python3','-c',
            'import pathlib,sys;pathlib.Path(sys.argv[1]).read_bytes()',str(fixture.config)])
        self.assertNotEqual(0,inaccessible.returncode)
        self.assertIn('PermissionError',inaccessible.stderr)
        for method,params,uid,expected in [('update.identity',{},60001,0),('update.status',{},60001,0),
            ('service.ready',{'pid':fixture.service.pid},60001,1),('service.shutdown',{},60001,1),
            ('update.identity',{},60002,1)]:
            result = child([str(binary),'rpc',method,json.dumps(params)],uid)
            self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
            if expected:
                self.assertEqual('peer-denied',json.loads(result.stderr)['code'])
            elif method=='update.identity':
                identity=json.loads(result.stdout)
                self.assertIs(identity['check_configured'],True)
                self.assertIn('installed',identity)
            else:self.assertEqual('null',result.stdout.strip())
            observations.append({'method':method,'uid':uid,'returncode':result.returncode})
        self.assertIsNone(fixture.service.poll())
        self.assertEqual([],fixture.operations())
        if os.environ.get('NATIVE_PUBLIC_PEER_EVIDENCE'):
            Path(os.environ['NATIVE_PUBLIC_PEER_EVIDENCE']).write_text(json.dumps({
                'private_config_read_denied':True,'observations':observations,'daemon_alive':True,
                'application_hooks_called':False},indent=2)+'\n')
