"""Native storage parser and real durable initialization; fixtures never mount a disk."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
BINARY=Path(os.environ.get('DSHANCONTROL_NATIVE_UPDATER',str(ROOT/'dist/native-runtime/dshancontrol-runtime')))
DRIVER=Path(os.environ.get('DSHANCONTROL_STORAGE_TEST_DRIVER',str(ROOT/'dist/native-storage-tests/storage-test-driver')))


def file_digest(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(65536),b''):digest.update(block)
    return digest.hexdigest()


class NativeBootTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        self.apps=self.root/'managed/apps'
        self.data=self.root/'managed/data'
        self.source=self.root/'factory'
        for name in ['bin/omnigate-hmi','lib/web/app.py','lib/supervisor/supervisor.py','share/asset']:
            path=self.source/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(('factory:'+name).encode())
            path.chmod(0o755 if name.startswith('bin/') else 0o644)
        self.identity=dict(commit='a'*40,version='0.1.0-dev',data_schema=1)
        (self.source/'installed.json').write_text(json.dumps(self.identity))
        self.defaults=self.root/'defaults';self.defaults.mkdir()
        for name in ['web.json','hmi.json']:(self.defaults/name).write_text(json.dumps({'default':name}))
        self.contract=self.root/'contract.json'
        self.contract.write_text(json.dumps(dict(application_root=str(self.apps),data_root=str(self.data),data_schema=1)))
        self.settings=dict(application_root=str(self.apps),system_contract=str(self.contract),development_mode=True,
                           factory_root=str(self.source),defaults_root=str(self.defaults))
        self.config=self.root/'config.json';self.save()

    def save(self):self.config.write_text(json.dumps(self.settings))

    def initialize(self, expected=0, timeout=15):
        result=subprocess.run([str(BINARY),'--config',str(self.config),'initialize'],
            capture_output=True,text=True,timeout=timeout)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        return json.loads(result.stdout if expected==0 else result.stderr)

    def parse(self, expected=0, **value):
        path=self.root/'parser.json';path.write_text(json.dumps(value))
        result=subprocess.run([str(DRIVER),str(path)],capture_output=True,text=True,timeout=3)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        return json.loads(result.stdout if expected==0 else result.stderr)

    def test_boot_map_requires_one_exact_sd_partition(self):
        for device in ['mmcblk0p9','mmcblk1p12']:
            self.assertEqual('/dev/'+device,self.parse(operation='boot-map',
                cmdline='root=/dev/mmcblk0p5 partitions=boot@mmcblk0p2:UDISK@'+device)['device'])
        for cmdline in ['', 'partitions=UDISK@mmcblk0p9 partitions=UDISK@mmcblk0p9',
                        'partitions=UDISK@mmcblk0p9:UDISK@mmcblk0p9', 'partitions=UDISK@mmcblk0p0',
                        'partitions=UDISK@../../disk','partitions=UDISK@sda9','partitions=other@mmcblk0p9']:
            with self.subTest(cmdline=cmdline):
                self.assertEqual('storage-boot-map',self.parse(expected=1,operation='boot-map',cmdline=cmdline)['code'])

    @unittest.skipUnless(Path('/sys/class/net/lo/type').exists(), 'requires real sysfs loopback attribute')
    def test_sysfs_nominal_page_size_does_not_replace_bounded_content_validation(self):
        path=Path('/sys/class/net/lo/type');body=path.read_text()
        self.assertGreater(path.stat().st_size,len(body.encode()))
        self.assertEqual(body,self.parse(operation='sysfs-attribute',path=str(path),limit=32)['value'])
        for candidate,limit in [(str(path),1),(str(path),0),(str(path),4097),
                                (str(self.contract),4096),('/sys/../etc/passwd',4096)]:
            with self.subTest(path=candidate,limit=limit):
                self.assertEqual('permissions-device',self.parse(expected=1,
                    operation='sysfs-attribute',path=candidate,limit=limit)['code'])

    def test_mount_requires_unique_expected_device_ext4_root_and_rw_in_both_fields(self):
        valid='35 21 179:9 / /mnt/UDISK rw,nosuid,nodev - ext4 /dev/mmcblk0p9 rw\n'
        args=dict(operation='mount-info',mount='/mnt/UDISK',identity='179:9')
        self.assertTrue(self.parse(mountinfo=valid,**args)['mounted'])
        self.assertFalse(self.parse(mountinfo='',**args)['mounted'])
        for invalid in [valid+valid,valid.replace('179:9','179:8'),valid.replace(' / /mnt',' /sub /mnt'),
                        valid.replace('ext4','vfat'),valid.replace('rw,nosuid','ro,nosuid'),
                        valid.replace('p9 rw','p9 ro'),valid.replace(' - ext4',' ? ext4'),
                        '35 21 179:9 / /mnt/UDISK\n',
                        valid.replace('p9 rw','p9 rw,ro')]:
            with self.subTest(mountinfo=invalid):self.parse(expected=1,mountinfo=invalid,**args)

    def test_initial_publish_matches_factory_and_preserves_user_configuration_and_data(self):
        self.data.mkdir(parents=True)
        (self.data/'web.json').write_bytes(b'user config deliberately not parsed')
        (self.data/'edge.db').write_bytes(b'USER DATABASE MUST SURVIVE')
        result=self.initialize()
        self.assertTrue(result['initialized'])
        self.assertFalse(result['user_data_overwritten'])
        self.assertEqual('releases/factory',os.readlink(str(self.apps/'current')))
        for path in self.source.rglob('*'):
            if path.is_file():self.assertEqual(path.read_bytes(),(self.apps/'releases/factory'/path.relative_to(self.source)).read_bytes())
        self.assertEqual(0o755,(self.apps/'releases/factory/bin/omnigate-hmi').stat().st_mode & 0o777)
        self.assertEqual(0o600,(self.data/'hmi.json').stat().st_mode & 0o777)
        self.assertEqual(b'user config deliberately not parsed',(self.data/'web.json').read_bytes())
        self.assertEqual(b'USER DATABASE MUST SURVIVE',(self.data/'edge.db').read_bytes())
        self.assertFalse(self.initialize()['initialized'])

    def test_current_updated_application_is_retained_without_requiring_factory_source(self):
        self.initialize()
        release=self.apps/'releases/accepted';release.mkdir()
        identity=dict(self.identity,commit='b'*40,version='0.2.0')
        (release/'installed.json').write_text(json.dumps(identity))
        (self.apps/'current').unlink();(self.apps/'current').symlink_to('releases/accepted')
        self.settings['factory_root']=str(self.root/'missing-factory');self.save()
        result=self.initialize()
        self.assertFalse(result['initialized']);self.assertEqual('b'*40,result['application_commit'])
        self.assertEqual(release,(self.apps/'current').resolve())

    def test_invalid_current_or_factory_is_not_replaced_or_silently_reset(self):
        self.apps.mkdir(parents=True)
        current=self.apps/'current';current.write_bytes(b'preserve broken current')
        self.initialize(expected=1)
        self.assertEqual(b'preserve broken current',current.read_bytes())
        self.assertFalse(self.data.exists())
        current.unlink();current.symlink_to(self.root/'outside')
        self.initialize(expected=1);self.assertTrue(current.is_symlink());self.assertFalse(self.data.exists())
        current.unlink();factory=self.apps/'releases/factory';factory.mkdir(parents=True)
        (factory/'unknown').write_bytes(b'preserve old factory')
        self.initialize(expected=1)
        self.assertEqual(b'preserve old factory',(factory/'unknown').read_bytes())
        self.assertFalse(current.exists());self.assertFalse(self.data.exists())

    def test_linked_factory_default_or_managed_path_refuses_before_writing(self):
        target=self.root/'outside';target.mkdir();(target/'private').write_bytes(b'untouched')
        entry=self.source/'share/link';entry.symlink_to(target/'private')
        self.initialize(expected=1);self.assertFalse(self.apps.exists())
        entry.unlink();default=self.defaults/'web.json';default.unlink();default.symlink_to(target/'private')
        self.initialize(expected=1);self.assertFalse(self.apps.exists())
        default.unlink();default.write_text('{}')
        self.data.parent.mkdir();self.data.symlink_to(target)
        self.initialize(expected=1);self.assertFalse(self.apps.exists())
        self.assertEqual(b'untouched',(target/'private').read_bytes())

    def test_interrupted_factory_staging_is_preserved_while_new_publish_completes(self):
        big=self.source/'share/large'
        with big.open('wb') as stream:
            for _ in range(64):stream.write(b'x'*1024*1024)
        child=subprocess.Popen([str(BINARY),'--config',str(self.config),'initialize'],
                               stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        stages=[];observation_started=time.monotonic();deadline=observation_started+180
        try:
            while time.monotonic()<deadline and child.poll() is None:
                stages=list((self.apps/'releases').glob('.factory-initialize.*'))
                if stages:
                    child.send_signal(signal.SIGSTOP)
                    child.kill();child.wait(timeout=3);break
                time.sleep(0.001)
            self.assertTrue(stages,'initializer must expose private staging within180s; child status='+str(child.poll()))
            observation_seconds=time.monotonic()-observation_started
            self.assertEqual(-signal.SIGKILL,child.returncode)
        finally:
            if child.poll() is None:child.kill();child.wait(timeout=3)
            child.communicate()
        hashes={str(p):file_digest(p) for p in stages[0].rglob('*') if p.is_file()}
        self.assertFalse((self.apps/'current').exists())
        # The physical SD NAND exceeded the host fixture's 15-second limit for
        # this 64 MiB synthetic payload. Keep the same data and bounded deadline.
        started=time.monotonic()
        self.initialize(timeout=180)
        elapsed=time.monotonic()-started
        self.assertTrue(stages[0].exists())
        self.assertEqual(hashes,{str(p):file_digest(p) for p in stages[0].rglob('*') if p.is_file()})
        self.assertEqual(file_digest(big),file_digest(self.apps/'current/share/large'))
        evidence=os.environ.get('NATIVE_BOOT_EVIDENCE')
        if evidence:
            with open(evidence,'a') as stream:
                stream.write(json.dumps(dict(test=self._testMethodName,payload_bytes=big.stat().st_size,
                    initialization_seconds=elapsed,deadline_seconds=180,staging_observation_seconds=observation_seconds,staging_observation_deadline_seconds=180,
                    interrupted_staging_preserved=True))+'\n')

    def test_unknown_staging_and_production_override_remain_untouched(self):
        stage=self.apps/'releases/.factory-initialize.unknown';stage.mkdir(parents=True)
        (stage/'keep').write_bytes(b'preserve unrecognized files')
        self.initialize();self.assertEqual(b'preserve unrecognized files',(stage/'keep').read_bytes())
        for value in [False,'true',None]:
            self.settings['development_mode']=value;self.save()
            self.initialize(expected=1)
        (self.apps/'current').unlink()
        self.assertFalse((self.apps/'current').exists())
