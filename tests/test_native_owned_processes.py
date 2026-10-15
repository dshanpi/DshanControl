"""Real owned subprocesses and pidfds; never signals a process outside the fixture."""
import fcntl
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import unittest

import test_native_application_health as health_fixture


class NativeOwnedProcessTests(unittest.TestCase):
    def setUp(self):
        self.fixture=health_fixture.NativeApplicationHealthTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def stop(self, role=None, expected=0, code=None):
        args=[str(health_fixture.BINARY),'--config',str(self.fixture.config),'application-stop']
        if role is not None:args.append(role)
        result=subprocess.run(args,capture_output=True,text=True,timeout=15)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        value=json.loads(result.stdout if expected==0 else result.stderr)
        if code:self.assertEqual(code,value['code'])
        return value

    def all_alive(self):
        for child in self.fixture.children.values():self.assertIsNone(child.poll())

    def test_stops_only_the_three_verified_processes_and_preserves_application_state(self):
        f=self.fixture
        before={str(p):p.read_bytes() for p in [f.factory/'installed.json',f.contract,*f.data.iterdir()]}
        self.assertEqual(3,self.stop()['cleared_pid_files'])
        for name,child in f.children.items():
            self.assertEqual(-signal.SIGTERM,child.wait(timeout=3))
            self.assertFalse((f.pids/(name+'.pid')).exists())
        self.assertEqual(before,{name:Path(name).read_bytes() for name in before})
        self.assertFalse((f.apps/'update-state.json').exists())

    def test_foreign_last_role_refuses_before_any_owned_process_is_signalled(self):
        (self.fixture.pids/'web.pid').write_text(str(os.getpid()))
        self.stop(expected=1,code='stop-process')
        self.all_alive()

    def test_same_binary_with_another_role_argument_is_not_owned(self):
        (self.fixture.pids/'hmi.pid').write_text(str(self.fixture.children['web'].pid))
        self.stop(expected=1,code='stop-process');self.all_alive()

    def test_unsafe_pid_files_and_directory_never_authorize_signals(self):
        f=self.fixture;path=f.pids/'hmi.pid'
        path.chmod(0o666);self.stop(expected=1,code='stop-process');self.all_alive()
        path.chmod(0o600);other=f.root/'linked.pid';os.link(str(path),str(other))
        self.stop(expected=1,code='stop-process');self.all_alive();other.unlink()
        path.rename(other);path.symlink_to(other)
        self.stop(expected=1,code='stop-process');self.all_alive()
        path.unlink();other.rename(path);f.pids.chmod(0o777)
        self.stop(expected=1,code='stop-process');self.all_alive()

    def test_unknown_role_and_lifecycle_lock_refuse_without_a_signal(self):
        self.stop('arbitrary-command',expected=1,code='stop-contract');self.all_alive()
        with (self.fixture.pids/'.lifecycle.lock').open('w') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            self.stop(expected=1,code='resource-busy');self.all_alive()

    def test_one_role_stop_and_dead_stale_pid_cleanup_do_not_touch_other_roles(self):
        f=self.fixture;child=f.children['hmi'];child.terminate();child.wait(timeout=3)
        self.assertEqual(1,self.stop('hmi')['cleared_pid_files'])
        self.assertFalse((f.pids/'hmi.pid').exists())
        for name in ['web','supervisor']:self.assertIsNone(f.children[name].poll())
        self.assertEqual(0,self.stop('hmi')['cleared_pid_files'])

    def test_a_frozen_process_times_out_without_force_kill_or_pid_file_removal(self):
        f=self.fixture;child=f.children['hmi'];child.send_signal(signal.SIGSTOP)
        self.stop('hmi',expected=1,code='stop-process-timeout')
        self.assertIsNone(child.poll())
        self.assertTrue((f.pids/'hmi.pid').exists())
        state=Path('/proc/%d/stat'%child.pid).read_text().rsplit(') ',1)[1].split()[0]
        self.assertIn(state,['T','t'])
        for name in ['web','supervisor']:self.assertIsNone(f.children[name].poll())

    def test_broken_installed_metadata_does_not_prevent_safe_stop(self):
        (self.fixture.factory/'installed.json').write_bytes(b'broken application identity')
        self.assertEqual(3,self.stop()['cleared_pid_files'])
        self.assertEqual(b'broken application identity',(self.fixture.factory/'installed.json').read_bytes())

    def test_replaced_pid_record_is_preserved_and_replacement_process_not_signalled(self):
        f=self.fixture;f.children['hmi'].terminate();f.children['hmi'].wait(timeout=3)
        script=f.root/'replace-pid.py';ready=f.root/'handler.ready';pidfile=f.pids/'hmi.pid'
        script.write_text('''import os,signal,sys,time
from pathlib import Path
pidfile=Path(sys.argv[1]);replacement=sys.argv[2];ready=Path(sys.argv[3])
def stop(*args):
    pidfile.write_text(replacement)
    sys.exit(0)
signal.signal(signal.SIGTERM,stop)
ready.write_text('ready')
while True:signal.pause()
''')
        args=[sys.executable,str(script),str(pidfile),str(f.children['web'].pid),str(ready)]
        child=subprocess.Popen(args);f.children['hmi']=child
        pidfile.write_text(str(child.pid))
        f.settings['application_health']['fixture_processes']['hmi']=args;f.save()
        import time
        deadline=time.monotonic()+3
        while not ready.exists() and time.monotonic()<deadline:time.sleep(0.01)
        self.assertTrue(ready.exists())
        self.stop('hmi',expected=1,code='stop-pidfile-changed')
        self.assertEqual(0,child.wait(timeout=3))
        self.assertEqual(str(f.children['web'].pid),pidfile.read_text())
        self.assertIsNone(f.children['web'].poll())
