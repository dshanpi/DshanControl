"""The real native stop transaction must wait for the owned media lease."""
import fcntl
import json
import os
from pathlib import Path
import time
import threading
import unittest
import test_native_launch as launch_reference


class MediaRetirementTests(unittest.TestCase):
    def setUp(self):
        self.n=launch_reference.NativeLaunchTests()
        self.n.setUp()
        self.addCleanup(self.n.doCleanups)
        # Enable the complete managed desktop role set before launching.
        f=self.n.f
        self.socket=f.root/"desktop.sock"
        self.n.server=__import__("socket").socket(__import__("socket").AF_UNIX)
        self.n.server.bind(str(self.socket));self.n.server.listen(1)
        self.addCleanup(self.n.server.close)
        policy=f.settings["application_health"]
        policy["desktop_provider"]=True
        policy["fixture_desktop_socket"]=str(self.socket)
        policy["fixture_processes"]["desktop"]=[str(self.n.exe),"desktop"]
        f.save()
        self.n.launch()
        self.runtime=f.pids/"hmi"
        self.runtime.mkdir(exist_ok=True,mode=0o700)
        self.runtime.chmod(0o700)
        self.lock=self.runtime/"media-surface.lock"
        self.lock.touch(mode=0o600)
        self.report=self.runtime/"media-surface-last.json"
        self.report.write_text(json.dumps({"schema":1,"surface_retired":True,
            "paired_worker_force_stopped":False,"CMA_reclamation_proven":False,"cause":"decoder-close"}))
        self.report.chmod(0o600)
        policy["media_devices"]=True
        f.save()

    def test_stop_waits_and_preserves_receipt_until_lease_is_released(self):
        with self.lock.open("r+") as stream:
            fcntl.flock(stream,fcntl.LOCK_EX)
            seen=[]
            def release():
                seen.append((self.n.f.pids/"hmi.launch.json").exists())
                fcntl.flock(stream,fcntl.LOCK_UN)
            t=threading.Timer(.6,release);t.start()
            try:
                start=time.monotonic();self.n.command("application-stop")
                self.assertGreater(time.monotonic()-start,.45)
                self.assertEqual(seen,[True])
            finally:t.join()
        self.assertFalse((self.n.f.pids/"hmi.launch.json").exists())

    def test_timeout_does_not_kill_lock_holder_or_clear_hmi_receipt(self):
        with self.lock.open("r+") as stream:
            fcntl.flock(stream,fcntl.LOCK_EX)
            start=time.monotonic();result=self.n.command("application-stop",expected=1)
            self.assertEqual(result["code"],"stop-media-retirement-timeout")
            self.assertGreater(time.monotonic()-start,3.8)
            self.assertLess(time.monotonic()-start,6)
            self.assertTrue((self.n.f.pids/"hmi.launch.json").exists())
            self.assertEqual(os.fstat(stream.fileno()).st_ino,self.lock.stat().st_ino)
        self.n.command("application-stop")

    def test_failed_surface_report_refuses_success_and_preserves_receipt(self):
        value=json.loads(self.report.read_text());value["surface_retired"]=False
        raw=json.dumps(value);self.report.write_text(raw)
        result=self.n.command("application-stop",expected=1)
        self.assertEqual(result["code"],"stop-media-retirement-failed")
        self.assertEqual(self.report.read_text(),raw)
        self.assertTrue((self.n.f.pids/"hmi.launch.json").exists())
        value["surface_retired"]=True;self.report.write_text(json.dumps(value))
        self.n.command("application-stop")

    def test_interrupted_empty_retirement_report_never_counts_as_success(self):
        original=self.report.read_bytes()
        self.report.write_bytes(b"")
        result=self.n.command("application-stop",expected=1)
        self.assertEqual(result["code"],"stop-media-ownership")
        self.assertTrue((self.n.f.pids/"hmi.launch.json").exists())
        self.report.write_bytes(original)
        self.n.command("application-stop")

    def test_symlink_and_insecure_lock_are_preserved_and_refused(self):
        original=self.n.f.pids/"hmi.launch.json"
        for kind in ["symlink","mode"]:
            with self.subTest(kind=kind):
                self.lock.unlink()
                if kind=="symlink":self.lock.symlink_to(original)
                else:self.lock.touch(mode=0o644)
                result=self.n.command("application-stop",expected=1)
                self.assertEqual(result["code"],"stop-media-ownership")
                self.assertTrue(original.exists())
                if kind=="symlink":self.assertTrue(self.lock.is_symlink())
                self.lock.unlink();self.lock.touch(mode=0o600)
        self.n.command("application-stop")


if __name__=="__main__":unittest.main()
