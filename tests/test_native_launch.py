"""Real native fork/exec and root-private launch records, entirely isolated."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import struct
import subprocess
import tempfile
import time
import unittest
from unittest import mock

import test_native_application_health as reference

ROOT=Path(__file__).resolve().parents[1]
PROCESS=Path(os.environ.get('DSHANCONTROL_LAUNCH_PROCESS',str(ROOT/'dist/native-launch-tests/launch-process-fixture')))


class NativeLaunchTests(unittest.TestCase):
    def setUp(self):
        f=reference.NativeApplicationHealthTests()
        # A nonroot child cannot traverse the private validation tree parents.
        with mock.patch.object(tempfile,'tempdir','/tmp'):f.setUp()
        self.f=f;self.addCleanup(f.doCleanups)
        f.stop_children()
        for path in f.pids.glob('*.pid'):path.unlink()
        f.root.chmod(0o755)
        self.exe=f.root/'launch-process';shutil.copyfile(str(PROCESS),str(self.exe));self.exe.chmod(0o755)
        for name in f.children:f.settings['application_health']['fixture_processes'][name]=[str(self.exe),name]
        f.settings['application_health']['launch_receipts_required']=True
        f.save();self.original={}
        self.addCleanup(self.cleanup_managed)

    def command(self,operation,expected=0,env=None,pass_fds=()):
        result=subprocess.run([str(reference.BINARY),'--config',str(self.f.config),operation],
            capture_output=True,text=True,timeout=35,env=env,pass_fds=pass_fds)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        return json.loads(result.stdout if expected==0 else result.stderr)

    def launch(self,**kwargs):
        value=self.command('application-start',**kwargs)
        self.remember();return value

    def remember(self):
        for path in self.f.pids.glob('*'):
            if path.name.endswith(('.pid','.launch.json')):self.original[path]=path.read_bytes()

    def cleanup_managed(self):
        # Restore deliberately corrupted fixture authority, never infer a PID.
        for path,body in self.original.items():
            if path.is_symlink():path.unlink()
            path.write_bytes(body);path.chmod(0o600)
        result=subprocess.run([str(reference.BINARY),'--config',str(self.f.config),'application-stop'],
                              capture_output=True,text=True,timeout=35)
        if result.returncode:self.fail('owned fixture cleanup refused: '+result.stderr)

    def logs(self):
        output={};deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            for name in self.f.children:
                path=self.f.pids/(name+'.log')
                if path.exists() and path.read_text().strip():output[name]=json.loads(path.read_text().splitlines()[0])
            if len(output)==3:return output
            time.sleep(0.01)
        self.fail('fixture programs did not publish their post-exec observations')

    def test_desktop_provider_is_managed_with_separate_identity_and_stop_record(self):
        policy=self.f.settings['application_health']
        policy['desktop_provider']=True
        service=self.f.root/'desktop-service'
        shutil.copyfile(str(ROOT/'dist/desktop-native/dshancontrol-desktop-service'),str(service));service.chmod(0o755)
        directory=self.f.root/'desktop-ipc';directory.mkdir(mode=0o750)
        if os.geteuid()==0:os.chown(directory,60002,60000)
        socket_path=directory/'desktop.sock';policy['fixture_desktop_socket']=str(socket_path)
        policy['fixture_processes']['desktop']=[str(service),'--socket',str(socket_path),'--config',str(self.f.root/'absent-private.json')]
        self.f.save()
        launched=self.launch()
        self.assertEqual(['desktop','web','hmi','supervisor'],launched['launched'])
        deadline=time.monotonic()+3
        while not socket_path.exists() and time.monotonic()<deadline:time.sleep(.02)
        self.assertTrue(socket_path.exists())
        self.assertEqual({'web','hmi','supervisor','desktop'},set(self.command('application-health')['processes']))
        receipt=json.loads((self.f.pids/'desktop.launch.json').read_text())
        self.assertEqual(60002 if os.geteuid()==0 else os.getuid(),receipt['uid'])
        self.assertFalse(receipt['bind_privileged_port'])
        status=Path('/proc/%d/status'%receipt['pid']).read_text()
        fields=dict(line.split(':',1) for line in status.splitlines() if ':' in line)
        self.assertEqual('1',fields['NoNewPrivs'].strip())
        self.assertEqual(0,int(fields['CapEff'],16))
        os.kill(receipt['pid'],signal.SIGSTOP)
        try:
            failed=self.command('application-health',expected=1)
            self.assertEqual('health-desktop',failed['code'])
        finally:os.kill(receipt['pid'],signal.SIGCONT)
        self.assertTrue(self.command('application-health')['ok'])
        preserved=socket_path.with_name('preserved.sock');socket_path.rename(preserved)
        listener=socket.socket(socket.AF_UNIX)
        try:
            listener.bind(str(socket_path));listener.listen(1)
            self.assertEqual('health-desktop-identity',self.command('application-health',expected=1)['code'])
        finally:
            listener.close();socket_path.unlink();preserved.rename(socket_path)
        self.assertTrue(self.command('application-health')['ok'])
        stopped=self.command('application-stop');self.original.clear()
        self.assertEqual(4,stopped['cleared_process_records'])
        self.assertFalse(list(self.f.pids.glob('*.launch.json')))

    def test_desktop_provider_requires_explicit_managed_policy_and_complete_fixtures(self):
        policy=self.f.settings['application_health']
        for value,managed in [('true',True),(True,False),(True,True)]:
            policy['desktop_provider']=value;policy['launch_receipts_required']=managed
            self.f.save()
            failure=self.command('application-start',expected=1)
            self.assertEqual('start-contract',failure['code'])
            self.assertFalse(list(self.f.pids.glob('*.launch.json')))
        policy.pop('desktop_provider');policy['launch_receipts_required']=True;self.f.save()

    def test_real_launch_is_recorded_with_reduced_credentials_and_closed_environment_and_fds(self):
        environment=dict(os.environ,DSHANCONTROL_TEST_SECRET='must-not-cross-application-boundary')
        canary=self.f.root/'private-canary';canary.write_bytes(b'fixture descriptor')
        with canary.open('rb') as stream:self.launch(env=environment,pass_fds=(stream.fileno(),))
        self.assertEqual(3,self.command('application-health')['processes'].__len__())
        for role,value in self.logs().items():
            self.assertEqual(1,value['nnp']);self.assertEqual(0,value['extra_fds'])
            self.assertFalse(value['secret_env_present']);self.assertFalse(value['lease_env_present'])
            record=json.loads((self.f.pids/(role+'.launch.json')).read_text())
            self.assertEqual(value['uid'],record['uid']);self.assertEqual(value['gid'],record['gid'])
            self.assertEqual(0o600,(self.f.pids/(role+'.launch.json')).stat().st_mode&0o777)
            self.assertEqual('a'*40,record['application_commit'])
        self.assertFalse((self.f.apps/'update-state.json').exists())

    def test_current_link_and_installed_metadata_are_not_needed_to_stop_recorded_processes(self):
        self.launch();(self.f.factory/'installed.json').write_bytes(b'broken identity')
        (self.f.apps/'current').unlink()
        result=self.command('application-stop')
        self.assertEqual(3,result['cleared_process_records'])
        self.original.clear()
        self.assertFalse(list(self.f.pids.glob('*.launch.json')))

    def test_private_receipt_is_primary_when_compatibility_pid_is_missing(self):
        self.launch()
        for path in self.f.pids.glob('*.pid'):path.unlink()
        self.assertTrue(self.command('application-health')['ok'])
        value=self.command('application-stop');self.original.clear()
        self.assertEqual(0,value['cleared_pid_files']);self.assertEqual(3,value['cleared_process_records'])

    def test_duplicate_start_refuses_without_replacing_any_record(self):
        self.launch();before=dict(self.original)
        result=self.command('application-start',expected=1)
        self.assertEqual('launch-state-exists',result['code'])
        self.assertEqual(before,{path:path.read_bytes() for path in before})

    def test_birth_boot_inode_uid_and_private_file_tampering_refuse_before_signals(self):
        self.launch();path=self.f.pids/'web.launch.json';original=path.read_bytes()
        baseline=json.loads(original)
        for key,value in [('start_ticks','0'),('boot_id','other-boot'),('exe_inode','0'),('uid',12345)]:
            with self.subTest(key=key):
                changed=dict(baseline);changed[key]=value;path.write_text(json.dumps(changed))
                self.assertNotEqual(0,subprocess.run([str(reference.BINARY),'--config',str(self.f.config),'application-stop'],
                    capture_output=True,text=True,timeout=3).returncode)
                for name in self.f.children:self.assertTrue((self.f.pids/(name+'.pid')).exists())
                path.write_bytes(original)
                self.assertTrue(self.command('application-health')['ok'])
        path.chmod(0o644)
        self.command('application-stop',expected=1);path.chmod(0o600)

    def test_record_commit_mismatch_refuses_health_but_preserves_safe_stop(self):
        self.launch();path=self.f.pids/'hmi.launch.json';value=json.loads(path.read_text())
        value['application_commit']='b'*40;path.write_text(json.dumps(value))
        self.command('application-health',expected=1)
        self.assertEqual(3,self.command('application-stop')['cleared_process_records']);self.original.clear()

    def test_active_update_control_and_lifecycle_leases_refuse_before_fork(self):
        for path in [self.f.apps/'.update.lock',self.f.apps/'.control.lock',self.f.pids/'.lifecycle.lock']:
            with path.open('w') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                value=self.command('application-start',expected=1)
                self.assertEqual('resource-busy',value['code'])
                self.assertFalse(list(self.f.pids.glob('*.launch.json')))

    def test_every_executable_is_preflighted_before_starting_first_role(self):
        self.f.settings['application_health']['fixture_processes']['hmi']=[str(self.f.root/'missing'),'hmi'];self.f.save()
        value=self.command('application-start',expected=1)
        self.assertEqual('launch-executable',value['code']);self.assertFalse(list(self.f.pids.glob('*.launch.json')))

    def test_failed_exec_retains_original_errno_and_known_partial_state_can_be_stopped(self):
        image=bytearray(self.exe.read_bytes());kind=image[4]
        if kind==1:offset=struct.unpack_from('<I',image,28)[0];size,count=struct.unpack_from('<HH',image,42)
        else:offset=struct.unpack_from('<Q',image,32)[0];size,count=struct.unpack_from('<HH',image,54)
        changed=False
        for index in range(count):
            entry=offset+index*size
            if struct.unpack_from('<I',image,entry)[0]!=3:continue
            start=struct.unpack_from('<I' if kind==1 else '<Q',image,entry+(4 if kind==1 else 8))[0]
            length=struct.unpack_from('<I' if kind==1 else '<Q',image,entry+(16 if kind==1 else 32))[0]
            replacement=b'/missing/ld\0';self.assertLessEqual(len(replacement),length)
            image[start:start+length]=replacement+b'\0'*(length-len(replacement));changed=True
        self.assertTrue(changed,'dynamic native fixture required')
        broken=self.f.root/'missing-interpreter';broken.write_bytes(image);broken.chmod(0o755)
        self.f.settings['application_health']['fixture_processes']['hmi']=[str(broken),'hmi'];self.f.save()
        value=self.command('application-start',expected=1);self.remember()
        self.assertEqual('launch-child-failed',value['code']);self.assertIn('errno=2',value['message'])
        self.assertTrue((self.f.pids/'web.launch.json').exists())
        self.assertTrue((self.f.pids/'hmi.launch.json').exists())
        self.assertEqual(2,self.command('application-stop')['cleared_process_records']);self.original.clear()

    def test_genuine_inherited_leases_are_adopted_and_never_retained_by_applications(self):
        paths=[self.f.apps/'.update.lock',self.f.apps/'.control.lock']
        descriptors=[]
        try:
            for path in paths:
                fd=os.open(str(path),os.O_RDWR|os.O_CREAT,0o600);os.fchmod(fd,0o600)
                descriptors.append(fd);fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
            env=dict(os.environ,DSHANCONTROL_UPDATE_LEASE_FD=str(descriptors[0]),
                     DSHANCONTROL_CONTROL_LEASE_FD=str(descriptors[1]))
            self.launch(env=env,pass_fds=tuple(descriptors))
            for value in self.logs().values():
                self.assertEqual(0,value['extra_fds']);self.assertFalse(value['lease_env_present'])
        finally:
            for fd in descriptors:os.close(fd)
        for path in paths:
            with path.open('r+') as lease:fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
        self.assertTrue(self.command('application-health')['ok'])

    def test_forged_and_incomplete_lease_descriptors_refuse_before_fork(self):
        canary=self.f.root/'unrelated';canary.write_bytes(b'fixture')
        with canary.open('r+') as unrelated:
            env=dict(os.environ,DSHANCONTROL_UPDATE_LEASE_FD=str(unrelated.fileno()))
            value=self.command('application-start',expected=1,env=env,pass_fds=(unrelated.fileno(),))
            self.assertIn('lease-denied',value['code'])
            env['DSHANCONTROL_CONTROL_LEASE_FD']=str(unrelated.fileno())
            value=self.command('application-start',expected=1,env=env,pass_fds=(unrelated.fileno(),))
            self.assertEqual('lease-denied',value['code'])
        self.assertFalse(list(self.f.pids.glob('*.launch.json')))

    def test_native_supervisor_is_health_only_and_never_an_application_stop_target(self):
        f=self.f
        supervisor=subprocess.Popen([str(f.exe),'supervisor'])
        f.children['supervisor']=supervisor
        (f.pids/'supervisor.pid').write_text(str(supervisor.pid));(f.pids/'supervisor.pid').chmod(0o600)
        f.settings['application_health']['native_supervisor']=True
        f.settings['application_health']['fixture_processes']['supervisor']=[str(f.exe),'supervisor']
        f.save();value=self.launch()
        self.assertEqual(['web','hmi'],value['launched'])
        self.assertEqual(supervisor.pid,self.command('application-health')['processes']['supervisor'])
        result=subprocess.run([str(reference.BINARY),'--config',str(f.config),'application-stop','supervisor'],
                              capture_output=True,text=True,timeout=3)
        self.assertEqual(1,result.returncode);self.assertEqual('stop-contract',json.loads(result.stderr)['code'])
        self.assertEqual(2,self.command('application-stop')['cleared_process_records']);self.original.clear()
        self.assertIsNone(supervisor.poll())

    def test_required_receipt_cannot_fall_back_to_a_compatibility_pid_file(self):
        self.launch();path=self.f.pids/'web.launch.json';original=path.read_bytes();path.unlink()
        value=self.command('application-stop',expected=1)
        self.assertEqual('stop-process-record',value['code'])
        for name in self.f.children:self.assertTrue((self.f.pids/(name+'.pid')).exists())
        path.write_bytes(original);path.chmod(0o600)
        self.assertTrue(self.command('application-health')['ok'])
