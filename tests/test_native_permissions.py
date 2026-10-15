"""Native permission plans are read-only; root applies only isolated test metadata."""
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BINARY=Path(os.environ.get('DSHANCONTROL_NATIVE_UPDATER',str(ROOT/'dist/native-runtime/dshancontrol-runtime')))


class NativePermissionsTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='dshan-permissions-',dir='/tmp');self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.apps=self.root/'apps';self.data=self.root/'data';self.run=self.root/'run'
        factory=self.apps/'releases/factory';factory.mkdir(parents=True)
        (factory/'installed.json').write_text(json.dumps({'commit':'a'*40,'version':'0.1.0','data_schema':1}))
        (factory/'payload').write_bytes(b'IMMUTABLE APPLICATION')
        for path in [self.apps,self.apps/'releases',factory]:path.chmod(0o755)
        for path in factory.iterdir():path.chmod(0o644)
        (self.apps/'current').symlink_to('releases/factory');self.data.mkdir()
        for name in ['web.json','hmi.json','session.key','edge.db','hmi.db']:(self.data/name).write_bytes(b'PRESERVE '+name.encode())
        self.fb=self.root/'fb';self.fb.write_bytes(b'FRAMEBUFFER FIXTURE')
        self.touch=self.root/'touch';self.touch.write_bytes(b'TOUCH FIXTURE')
        self.contract=self.root/'contract.json';self.contract.write_text(json.dumps({'application_root':str(self.apps),'data_root':str(self.data),'data_schema':1}))
        self.config=self.root/'updater.json'
        self.settings={'application_root':str(self.apps),'system_contract':str(self.contract),'development_mode':True,
            'application_health':{'launch_receipts_required':True,'native_supervisor':True,'pid_root':str(self.run/'processes'),'hmi_heartbeat':str(self.run/'hmi/heartbeat.json')},
            'permission_fixtures':{'web_uid':60000,'hmi_uid':60001,'gid':60000,'run_root':str(self.run),'framebuffer':str(self.fb),'touchscreen':str(self.touch)}}
        self.save();self.config.chmod(0o600)

    def save(self):self.config.write_text(json.dumps(self.settings))

    def command(self,apply=False,expected=0):
        result=subprocess.run([str(BINARY),'--config',str(self.config),'permissions-apply' if apply else 'permissions-plan'],capture_output=True,text=True,timeout=10)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        return json.loads(result.stdout if not expected else result.stderr)

    def snapshot(self):
        return {str(p.relative_to(self.root)):(p.lstat().st_uid,p.lstat().st_gid,p.lstat().st_mode,hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() and not p.is_symlink() else None)
            for p in self.root.rglob('*')}

    def test_plan_does_not_create_directories_locks_or_change_data(self):
        before=self.snapshot();value=self.command()
        self.assertFalse(value['applied']);self.assertEqual(before,self.snapshot())
        modes={e['path']:e for e in value['entries']}
        self.assertEqual('700',modes[str(self.data)]['mode'])
        self.assertEqual(60001,modes[str(self.fb)]['uid'])
        self.assertFalse(self.run.exists())

    def test_links_unknown_data_and_mutable_payload_refuse_without_changes(self):
        unknown=self.data/'unknown';unknown.write_bytes(b'PRESERVE UNKNOWN')
        before=self.snapshot();self.assertEqual('permissions-unknown-data',self.command(expected=1)['code']);self.assertEqual(before,self.snapshot());unknown.unlink()
        aux=self.data/'edge.db-wal';aux.write_bytes(b'PRESERVE WAL')
        (self.data/'edge.db').rename(self.root/'saved-edge')
        before=self.snapshot();self.command(expected=1);self.assertEqual(before,self.snapshot())
        (self.root/'saved-edge').rename(self.data/'edge.db');aux.unlink()
        linked=self.data/'session.key';body=linked.read_bytes();linked.unlink();linked.symlink_to(self.config)
        before=self.snapshot();self.command(expected=1);self.assertEqual(before,self.snapshot());linked.unlink();linked.write_bytes(body)
        payload=self.apps/'releases/factory/payload';payload.chmod(0o666)
        before=self.snapshot();self.command(expected=1);self.assertEqual(before,self.snapshot())

    def test_fixture_paths_cannot_escape_and_production_refuses_fixture_policy(self):
        self.settings['permission_fixtures']['framebuffer']='/dev/fb0';self.save()
        self.command(expected=1)
        self.settings['development_mode']=False;self.save()
        self.command(expected=1)
        self.assertFalse(self.run.exists())

    @unittest.skipUnless(os.geteuid()==0,'requires real root for isolated metadata changes')
    def test_apply_is_idempotent_preserves_data_and_enforces_separate_role_access(self):
        for name in ['hmi.db-wal','hmi.db-shm']:(self.data/name).write_bytes(b'PRESERVE SQLITE AUX '+name.encode())
        original={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.data.iterdir()}
        initialized=subprocess.run([str(BINARY),'--config',str(self.config),'initialize'],capture_output=True,text=True,timeout=20)
        self.assertEqual(0,initialized.returncode,initialized.stdout+initialized.stderr)
        value=json.loads(initialized.stdout)['permissions']
        self.assertTrue(value['applied']);self.assertFalse(value['application_data_contents_written'])
        journal=json.loads(Path(value['journal']).read_text());self.assertEqual('completed',journal['status'])
        self.command(apply=True)
        self.assertEqual(original,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.data.iterdir() if p.is_file()})
        def access(uid,path,write=False):
            def drop():os.setgroups([60000]);os.setresgid(60000,60000,60000);os.setresuid(uid,uid,uid)
            result=subprocess.run(['/usr/bin/python3','-c','import os,sys;fd=os.open(sys.argv[1],os.O_WRONLY if sys.argv[2]=="write" else os.O_RDONLY);os.close(fd)',str(path),'write' if write else 'read'],capture_output=True,text=True,timeout=5,preexec_fn=drop)
            return result.returncode==0
        observations=[]
        for uid in [60000,60001]:
            for path,allowed in [(self.data/'session.key',uid==60000),(self.fb,uid==60001),(self.touch,uid==60001),
                                 (self.config,False),(self.apps/'releases/factory/payload',True),
                                 (self.run/'processes',False)]:
                result=access(uid,path);self.assertEqual(allowed,result,(uid,str(path)))
                observations.append({'uid':uid,'target':path.name,'readable':result})
        if os.environ.get('NATIVE_PERMISSIONS_EVIDENCE'):
            Path(os.environ['NATIVE_PERMISSIONS_EVIDENCE']).write_text(json.dumps({'data_hashes_preserved':True,'idempotent':True,'observations':observations,'real_device_modes_modified':False},indent=2)+'\n')

    @unittest.skipUnless(os.geteuid()==0,'requires real root for apply preflight')
    def test_active_pid_record_and_busy_update_refuse_before_metadata_changes(self):
        self.run.mkdir();(self.run/'web.pid').write_text(str(os.getpid()))
        original={p.name:p.stat().st_uid for p in self.data.iterdir()}
        self.assertEqual('permissions-active',self.command(apply=True,expected=1)['code'])
        self.assertEqual(original,{p.name:p.stat().st_uid for p in self.data.iterdir()})
        (self.run/'web.pid').unlink()
        def drop():os.setgroups([60000]);os.setresgid(60000,60000,60000);os.setresuid(60000,60000,60000)
        child=subprocess.Popen(['/bin/sleep','30'],preexec_fn=drop)
        try:self.assertEqual('permissions-active',self.command(apply=True,expected=1)['code'])
        finally:child.terminate();child.wait(timeout=3)
        import fcntl
        with (self.apps/'.update.lock').open('a') as lease:
            fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
            self.assertEqual('resource-busy',self.command(apply=True,expected=1)['code'])
        self.assertEqual(original,{p.name:p.stat().st_uid for p in self.data.iterdir()})
        zombie=subprocess.Popen(['/bin/true'],preexec_fn=drop)
        try:
            import time
            status=Path('/proc/%d/status'%zombie.pid);end=time.monotonic()+3
            while time.monotonic()<end:
                state=next(line for line in status.read_text().splitlines() if line.startswith('State:'))
                if 'Z' in state:break
                time.sleep(0.01)
            else:self.fail('fixture child did not reach zombie state')
            self.assertTrue(self.command(apply=True)['applied'])
        finally:zombie.wait(timeout=3)
