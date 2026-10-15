"""Actual SO_PEERCRED roles: web cannot submit updates; HMI cannot manage root."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import test_native_service as reference


@unittest.skipUnless(os.geteuid()==0,'requires root to establish fixed role peers')
class NativeRolePeerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):reference.NativeServiceTests.setUpClass()
    @classmethod
    def tearDownClass(cls):reference.NativeServiceTests.tearDownClass()

    def setUp(self):
        self.f=reference.NativeServiceTests();self.addCleanup(self.f.doCleanups);self.f.setUp()
        self.public=tempfile.TemporaryDirectory(prefix='dshan-role-peer-',dir='/tmp');self.addCleanup(self.public.cleanup)
        directory=Path(self.public.name);directory.chmod(0o755)
        self.binary=directory/'runtime';shutil.copy2(str(self.f.binary),str(self.binary));self.binary.chmod(0o755)
        self.f.socket_path=str(directory/'platform.sock')
        self.settings=json.loads(self.f.service_config.read_text())
        self.settings.update(socket=self.f.socket_path,client_gid=400,allowed_uids=[0,400,401],peer_policy='s4-roles-v1')
        self.save()

    def save(self):self.f.service_config.write_text(json.dumps(self.settings))

    def child(self,uid,method,params=None):
        def drop():os.setgroups([400]);os.setresgid(400,400,400);os.setresuid(uid,uid,uid)
        return subprocess.run([str(self.binary),'rpc',method,json.dumps(params or {})],capture_output=True,text=True,timeout=12,
            preexec_fn=drop,env={'PATH':'/usr/bin:/bin','HOME':'/nonexistent','DSHANCONTROL_PLATFORM_SOCKET':self.f.socket_path})

    def test_web_and_hmi_have_distinct_method_authority_with_real_peer_uids(self):
        self.f.start();observations=[]
        for uid,method,params,success,code in [
            (400,'service.health',{},True,None),
            (400,'update.identity',{},False,'method-denied'),
            (400,'update.start',{'operation':'check'},False,'method-denied'),
            (400,'service.shutdown',{},False,'method-denied'),
            (401,'update.identity',{},True,None),
            (401,'update.status',{},True,None),
            (401,'service.health',{},True,None),
            (401,'update.start',{'operation':'invalid'},False,'rejected'),
            (401,'service.shutdown',{},False,'method-denied'),
            (401,'service.ready',{'pid':self.f.service.pid},False,'method-denied'),
            (402,'service.health',{},False,'peer-denied'),
            (400,'display.brightness.get',{'channel':'display0'},False,'display-unbound')]:
            result=self.child(uid,method,params)
            self.assertEqual(0 if success else 1,result.returncode,result.stdout+result.stderr)
            if code:self.assertEqual(code,json.loads(result.stderr)['code'],result.stderr)
            observations.append({'uid':uid,'method':method,'returncode':result.returncode,'code':code})
        self.assertEqual([],self.f.operations());self.assertIsNone(self.f.service.poll())
        self.assertFalse((self.f.tasks/'latest.json').exists())
        self.assertTrue(self.f.request('service.ready',{'pid':self.f.service.pid})['ok'])
        if os.environ.get('NATIVE_ROLE_PEER_EVIDENCE'):
            Path(os.environ['NATIVE_ROLE_PEER_EVIDENCE']).write_text(json.dumps({'observations':observations,'application_hooks_called':False,'system_owner_ready_allowed':True},indent=2)+'\n')

    def test_wrong_group_extra_uid_or_invalid_policy_refuse_before_socket(self):
        baseline=dict(self.settings)
        for change in [{'client_gid':0},{'allowed_uids':[0,400,401,402]},{'allowed_uids':[0,400,400]},
                       {'peer_policy':True},{'peer_policy':'other-policy'}]:
            self.settings=dict(baseline,**change);self.save()
            result=subprocess.run([str(self.f.binary),'serve',str(self.f.service_config)],capture_output=True,text=True,timeout=5)
            self.assertEqual(1,result.returncode);self.assertEqual('authorization',json.loads(result.stderr)['stage'])
            self.assertFalse(Path(self.f.socket_path).exists());self.assertEqual([],self.f.operations())
