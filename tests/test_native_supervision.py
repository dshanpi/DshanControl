"""Read-only native monitoring stays responsive and never restarts applications."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest
from unittest import mock
import test_native_application_health as reference


class NativeSupervisionTests(unittest.TestCase):
    def setUp(self):
        self.f = reference.NativeApplicationHealthTests()
        self.addCleanup(self.f.doCleanups)
        with mock.patch.object(tempfile,'tempdir','/tmp'):self.f.setUp()
        self.path = self.f.root/'service.sock'
        self.settings = {'updater_config':str(self.f.config),'task_root':str(self.f.root/'tasks'),
            'socket':str(self.path),'client_gid':os.getgid(),'allowed_uids':[os.getuid()],
            'supervision':{'enabled':True,'interval_seconds':2,'failure_threshold':2,'timeout_seconds':5}}
        self.config = self.f.root/'service.json'
        self.service = None
        self.addCleanup(self.stop)

    def stop(self):
        if self.service:
            if self.service.poll() is None:self.service.terminate();self.service.wait(timeout=5)
            self.service.stdout.close();self.service.stderr.close()

    def start(self):
        self.config.write_text(json.dumps(self.settings))
        self.service=subprocess.Popen([str(reference.BINARY),'serve',str(self.config)],
            stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        end=time.monotonic()+5
        while time.monotonic()<end:
            if self.service.poll() is not None:self.fail(self.service.communicate()[1].decode())
            if self.path.exists():return
            time.sleep(0.02)
        self.fail('native monitor did not open its service')

    def request(self,method='service.health',params=None):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2);connection.connect(str(self.path))
            connection.sendall((json.dumps({'protocol':1,'method':method,'params':params or {}})+'\n').encode())
            response=json.loads(connection.makefile('rb').readline(65536))
        self.assertTrue(response['ok'],response)
        return response['result']

    def wait(self,predicate,seconds=10):
        end=time.monotonic()+seconds;last=None
        while time.monotonic()<end:
            last=self.request()
            if predicate(last):return last
            time.sleep(0.05)
        self.fail('monitor state not observed: '+json.dumps(last))

    def test_real_health_then_failed_role_raises_alarm_without_signalling_or_relaunch(self):
        before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.f.data.iterdir()}
        self.start();healthy=self.wait(lambda s:s['status']=='healthy')
        self.assertEqual(self.f.commit,healthy['application_commit'])
        self.assertFalse(healthy['alarm']);self.assertFalse(healthy['hardware_watchdog'])
        victim=self.f.children['hmi'];victim.terminate();victim.wait(timeout=3)
        failed=self.wait(lambda s:s['alarm'])
        self.assertEqual('unhealthy',failed['status']);self.assertEqual('health',failed['error']['stage'])
        self.assertEqual('health-process-handle',failed['error']['code']);self.assertEqual(0,failed['restart_count'])
        self.assertEqual(str(victim.pid),(self.f.pids/'hmi.pid').read_text().strip())
        for name in ['web','supervisor']:self.assertIsNone(self.f.children[name].poll())
        self.assertEqual(before,{p:hashlib.sha256(p.read_bytes()).hexdigest() for p in self.f.data.iterdir()})
        if os.environ.get('NATIVE_SUPERVISION_EVIDENCE'):
            Path(os.environ['NATIVE_SUPERVISION_EVIDENCE']).write_text(json.dumps({
                'healthy':healthy,'failed':failed,'dead_role_record_preserved':True,
                'other_roles_unchanged':True,'databases_unchanged':True},indent=2)+'\n')

    def test_busy_update_or_control_lease_skips_observation_without_alarm(self):
        self.start();self.wait(lambda s:s['status']=='healthy')
        for name in ['.update.lock','.control.lock']:
            with (self.f.apps/name).open('a') as lease:
                fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
                value=self.wait(lambda s:s['status']=='busy')
                self.assertEqual(0,value['consecutive_failures']);self.assertFalse(value['alarm'])
            self.wait(lambda s:s['status']=='healthy')

    def test_slow_backend_probe_does_not_block_rpc_or_restart_any_role(self):
        self.f.delay=4;self.start();self.wait(lambda s:s['probe_running'])
        started=time.monotonic()
        for _ in range(8):self.request()
        self.assertLess(time.monotonic()-started,1)
        failure=self.wait(lambda s:s['status']=='unhealthy')
        self.assertEqual('health-web-timeout',failure['error']['code'])
        self.assertEqual(0,failure['restart_count'])
        for child in self.f.children.values():self.assertIsNone(child.poll())

    def test_disabled_monitor_is_truthful_and_malformed_policy_fails_before_socket(self):
        self.settings['supervision']={'enabled':False};self.start()
        value=self.request();self.assertFalse(value['enabled']);self.assertEqual('disabled',value['status'])
        self.assertFalse(value['probe_running']);self.request('service.shutdown');self.service.wait(timeout=3)
        self.stop();self.service=None
        for policy in [{'enabled':'true'},{'enabled':True,'restart':True},{'enabled':True,'interval_seconds':0}]:
            self.settings['supervision']=policy;self.config.write_text(json.dumps(self.settings))
            result=subprocess.run([str(reference.BINARY),'serve',str(self.config)],capture_output=True,text=True,timeout=5)
            self.assertEqual(1,result.returncode);self.assertEqual('supervision',json.loads(result.stderr)['stage'])
            self.assertFalse(self.path.exists())
