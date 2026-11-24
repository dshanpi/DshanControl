"""FFmpeg worker protocol on a generated local test clip; no real display claim."""
import json
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
BINARY=ROOT/'dist/media-software/dshancontrol-media-worker'
@unittest.skipUnless(BINARY.exists() and shutil.which('ffmpeg'),'Build software media worker and install ffmpeg')
class SoftwareMediaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.video=Path(cls.temp.name)/'fixture.mp4'
        subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc=size=320x240:rate=10',
                        '-t','5','-c:v','mpeg4','-threads','1','-pix_fmt','yuv420p',str(cls.video)],check=True,timeout=20)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.process=subprocess.Popen([str(BINARY)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.events=queue.Queue();self.history=[]
        def reader():
            for line in self.process.stdout:self.events.put(json.loads(line))
        self.thread=threading.Thread(target=reader,daemon=True);self.thread.start()
        self.wait(lambda value:value.get('event')=='status' and value.get('state')=='idle')
    def tearDown(self):
        self.process.stdin.close()
        try:self.process.wait(5)
        except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        self.thread.join(2);self.process.stdout.close();self.process.stderr.close()
    def wait(self,predicate,timeout=12):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            try:value=self.events.get(timeout=.2)
            except queue.Empty:continue
            self.history.append(value)
            if predicate(value):return value
        self.fail('Missing expected media event: '+repr(self.history[-8:]))
    def command(self,name,params=None):
        self.process.stdin.write(json.dumps({'protocol':1,'request_id':name,'command':name,'params':params or {}})+'\n');self.process.stdin.flush()
        return self.wait(lambda value:value.get('event')=='reply' and value.get('request_id')==name)
    def test_decode_pause_seek_resume_close(self):
        self.assertTrue(self.command('open',{'path':str(self.video),'volume':30})['accepted'])
        frame=self.wait(lambda value:value.get('event')=='frame')
        self.assertEqual((frame['width'],frame['height'],frame['bytes']),(512,288,442368))
        self.assertTrue(self.command('pause')['accepted'])
        self.assertTrue(self.command('seek',{'position_ms':1000})['accepted'])
        self.assertTrue(self.command('volume',{'value':20})['accepted'])
        self.assertTrue(self.command('play')['accepted'])
        self.wait(lambda value:value.get('event')=='frame')
        self.assertTrue(self.command('close')['accepted'])
        self.assertEqual([v for v in self.history if v.get('event')=='status'][-1]['state'],'idle')
    def test_missing_file_and_network_url_rejected(self):
        for path in ('/does/not/exist','https://example.org/video.mp4'):
            self.assertFalse(self.command('open',{'path':path,'volume':40})['accepted'])
    def test_symlink_rejected(self):
        linked=Path(self.temp.name)/'linked.mp4'
        if not linked.exists():linked.symlink_to(self.video)
        self.assertFalse(self.command('open',{'path':str(linked),'volume':40})['accepted'])

if __name__=='__main__':unittest.main()
