"""Exec an isolated probe after credential reduction; root cases also run on ARM."""
import errno
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
DRIVER=Path(os.environ.get('DSHANCONTROL_CREDENTIAL_PROBE',
    str(ROOT/'dist/native-credentials-tests/credentials-test-driver')))


class NativeCredentialTests(unittest.TestCase):
    def setUp(self):
        # The private validation tree has0700 parents; the post-drop executable
        # needs its own public traversal path. All private fixture files stay0600.
        temporary=tempfile.TemporaryDirectory(prefix='dshan-credential-',dir='/tmp')
        self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        self.binary=self.root/'probe';shutil.copyfile(str(DRIVER),str(self.binary));self.binary.chmod(0o755)
        self.sample=self.root/'private-sample';self.sample.write_bytes(b'PRIVATE TEST SAMPLE')
        self.sample.chmod(0o600);self.root.chmod(0o755)
        self.was_root=os.geteuid()==0
        self.uid=60000 if self.was_root else os.getuid()
        self.gid=60000 if self.was_root else os.getgid()
        self.groups=[self.gid] if self.was_root else os.getgroups()

    def probe(self, expected=0, **changes):
        value=dict(uid=self.uid,gid=self.gid,groups=self.groups,bind=False,port=0,sample=str(self.sample))
        value.update(changes)
        config=self.root/'profile.json';config.write_text(json.dumps(value));config.chmod(0o600)
        result=subprocess.run([str(self.binary),'apply',str(config)],capture_output=True,text=True,timeout=5)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        output=json.loads(result.stdout if expected==0 else result.stderr)
        evidence=os.environ.get('NATIVE_CREDENTIAL_EVIDENCE')
        if evidence:
            with open(evidence,'a') as stream:
                stream.write(json.dumps(dict(test=self._testMethodName,returncode=result.returncode,
                                             started_as_root=self.was_root,value=output))+'\n')
        return output

    def check(self,value,uid,gid,groups,mask):
        self.assertEqual([uid]*4,[int(x) for x in value['Uid'].split()])
        self.assertEqual([gid]*4,[int(x) for x in value['Gid'].split()])
        self.assertEqual(sorted(groups),sorted(int(x) for x in value['Groups'].split()))
        self.assertEqual('1',value['NoNewPrivs'])
        for name in ['CapInh','CapPrm','CapEff','CapAmb']:self.assertEqual(mask,int(value[name],16),name)
        if self.was_root:self.assertEqual(mask,int(value['CapBnd'],16))
        self.assertEqual(errno.EPERM,value['regain_uid_errno'])
        self.assertEqual(errno.EPERM,value['regain_gid_errno'])
        self.assertEqual(errno.EPERM,value['regain_extra_capability_errno'])
        self.assertEqual(1,value['second_identity_stage'])
        self.assertEqual(errno.EPERM,value['second_identity_errno'])
        self.assertEqual(not self.was_root,value['sample_readable'])

    @staticmethod
    def free_low_port():
        used=set()
        for name in ['/proc/net/tcp','/proc/net/tcp6']:
            path=Path(name)
            if not path.exists():continue
            for row in path.read_text().splitlines()[1:]:
                fields=row.split()
                if len(fields)>3:used.add(int(fields[1].split(':')[1],16))
        return next(port for port in range(81,100) if port not in used)

    def test_no_capabilities_and_no_privilege_regain_survive_exec(self):
        self.check(self.probe(),self.uid,self.gid,self.groups,0)

    @unittest.skipUnless(os.geteuid()==0,'requires real root; executed on the ARM board')
    def test_web_retains_only_low_port_binding_capability_after_exec(self):
        port=self.free_low_port();value=self.probe(bind=True,port=port)
        self.check(value,self.uid,self.gid,self.groups,1<<10)
        self.assertEqual(0,value['bind_errno'])

    @unittest.skipUnless(os.geteuid()==0,'requires real root; executed on the ARM board')
    def test_hmi_has_zero_capabilities_and_cannot_read_root_private_files(self):
        port=self.free_low_port();value=self.probe(uid=60001,port=port)
        self.check(value,60001,self.gid,self.groups,0)
        threshold=int(Path('/proc/sys/net/ipv4/ip_unprivileged_port_start').read_text())
        self.assertEqual(errno.EACCES if port<threshold else 0,value['bind_errno'])

    def test_root_identity_root_groups_invalid_bounds_and_duplicates_are_rejected(self):
        for change in [dict(uid=0),dict(gid=0),dict(groups=[0]),dict(groups=[self.gid,self.gid]),
                       dict(last_cap=-1),dict(last_cap=64),dict(uid=-1),dict(uid=True),dict(gid=2147483648)]:
            with self.subTest(change=change):self.probe(expected=1,**change)
