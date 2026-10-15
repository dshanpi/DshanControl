"""Initialize -> permissions -> real reduced-role exec, with a separate heartbeat directory."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import unittest
import test_native_permissions as reference


def privileged_root():
    if os.geteuid()!=0:return False
    fields={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path('/proc/self/status').read_text().splitlines() if ':' in line}
    return bool(int(fields['CapEff'],16)&(1<<19))


@unittest.skipUnless(privileged_root(),'requires root and CAP_SYS_PTRACE for reduced-role process verification')
class NativeStartupTests(unittest.TestCase):
    def test_initialize_permissions_then_only_web_and_hmi_launch_with_configured_heartbeat(self):
        f=reference.NativePermissionsTests();self.addCleanup(f.doCleanups);f.setUp()
        exe=f.root/'application'
        fixture=Path(os.environ.get('DSHANCONTROL_LAUNCH_PROCESS',str(reference.ROOT/'dist/native-launch-tests/launch-process-fixture')))
        shutil.copy2(str(fixture),str(exe));exe.chmod(0o755)
        f.settings['application_health']['fixture_processes']={role:[str(exe),role] for role in ['web','hmi','supervisor']};f.save()
        def command(name):
            result=subprocess.run([str(reference.BINARY),'--config',str(f.config),name],capture_output=True,text=True,timeout=30)
            self.assertEqual(0,result.returncode,result.stdout+result.stderr)
            return json.loads(result.stdout)
        initialized=command('initialize');self.assertTrue(initialized['permissions']['applied'])
        self.addCleanup(lambda:command('application-stop'))
        result=command('application-start');self.assertEqual(['web','hmi'],result['launched'])
        records=f.run/'processes';observations=[]
        for role,uid in [('web',60000),('hmi',60001)]:
            record=json.loads((records/(role+'.launch.json')).read_text());self.assertEqual(uid,record['uid'])
            environment=Path('/proc/%d/environ'%record['pid']).read_bytes().split(b'\0')
            self.assertIn(('DSHANCONTROL_HMI_HEARTBEAT='+str(f.run/'hmi/heartbeat.json')).encode(),environment)
            log=records/(role+'.log');end=time.monotonic()+3
            while not log.read_text().strip() and time.monotonic()<end:time.sleep(0.01)
            observation=json.loads(log.read_text().splitlines()[0]);self.assertEqual(uid,observation['uid'])
            self.assertEqual(1,observation['nnp']);self.assertEqual(0,observation['extra_fds']);observations.append(observation)
        self.assertFalse((records/'supervisor.pid').exists());self.assertFalse((records/'supervisor.launch.json').exists())
        stopped=command('application-stop');self.assertEqual(2,stopped['cleared_process_records'])
        if os.environ.get('NATIVE_STARTUP_EVIDENCE'):
            Path(os.environ['NATIVE_STARTUP_EVIDENCE']).write_text(json.dumps({'permissions_applied':True,'launched':['web','hmi'],
                'observations':observations,'configured_heartbeat_environment':True,'private_records_cleared':2,'python_supervisor_started':False},indent=2)+'\n')
