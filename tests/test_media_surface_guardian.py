"""Real Linux pidfd/fork/lease tests with a compile-time fake display backend.
These never claim Cedar, CMA, LCD or production lifecycle acceptance.
"""
import ctypes
import errno
import json
import os
from pathlib import Path
import select
import signal
import struct
import subprocess
import tempfile
import time
import unittest

libc = ctypes.CDLL(None, use_errno=True)
def pidfd_open(pid):
    fd=libc.syscall(434,ctypes.c_int(pid),ctypes.c_uint(0))
    if fd < 0:raise OSError(ctypes.get_errno(),"pidfd_open")
    return fd
def pidfd_signal(fd, number):
    if libc.syscall(424,ctypes.c_int(fd),ctypes.c_int(number),ctypes.c_void_p(),ctypes.c_uint(0)) < 0:
        e=ctypes.get_errno()
        if e==errno.ESRCH:raise ProcessLookupError(e,"pidfd signal")
        raise OSError(e,"pidfd signal")
ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "dist/media-surface-tests/surface-guardian-fixture"


class GuardianTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BINARY.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["g++", "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(ROOT/"tests/media/surface_guardian_fixture.cpp"),
                        str(ROOT/"components/media/src/surfaceguardian.cpp"),
                        "-o", str(BINARY)], check=True)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.runtime = self.root/"private"
        self.runtime.mkdir(mode=0o700)
        self.state = self.root/"plane"
        self.state.write_bytes(struct.pack("8i", 0, 0, 0, 0, 0, 0, 817, 0))
        self.children = []

    def tearDown(self):
        for p in self.children:
            if p.poll() is None:
                p.kill()
            try:
                p.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                raise AssertionError("paired guardian failed to release inherited pipes")
        self.tmp.cleanup()

    def start(self, ready=True):
        p = subprocess.Popen([str(BINARY), str(self.runtime), str(self.state)],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True)
        self.children.append(p)
        if ready:
            self.assertEqual(self.line(p), "READY")
        return p

    def line(self, p):
        self.assertTrue(select.select([p.stdout], [], [], 5)[0], "worker output timed out")
        return p.stdout.readline().strip()

    def command(self, p, value):
        p.stdin.write(value+"\n")
        p.stdin.flush()
        self.assertEqual(self.line(p), "ACK")

    def values(self):
        return struct.unpack("8i", self.state.read_bytes())

    def wait_for(self, test, seconds=5):
        deadline = time.monotonic()+seconds
        while time.monotonic() < deadline:
            if test():
                return
            time.sleep(.02)
        self.fail("guardian state deadline exceeded")

    def report(self):
        return json.loads((self.runtime/"media-surface-last.json").read_text())

    def test_normal_close_releases_surface_before_next_clip(self):
        p = self.start()
        self.command(p, "play")
        self.command(p, "close")
        self.assertEqual(self.values()[0], 0)
        self.assertEqual(self.values()[5:7], (1, 817))
        self.command(p, "play")
        self.assertEqual(self.values()[0], 1)
        self.command(p, "close")
        self.assertEqual(self.values()[5:7], (2, 817))
        p.stdin.write("quit\n");p.stdin.flush()
        self.assertEqual(p.wait(timeout=3), 0)

    def test_active_claim_invalidates_previous_success_before_producer_starts(self):
        p=self.start()
        self.assertTrue(self.report()["surface_retired"])
        self.command(p,"play")
        self.assertFalse(self.report()["surface_retired"])
        self.assertEqual(self.report()["cause"],"surface-armed")
        self.command(p,"close")
        self.assertTrue(self.report()["surface_retired"])
        self.command(p,"play")
        self.assertFalse(self.report()["surface_retired"])

    def test_sigkill_cleanup_uses_owned_worker_handle_and_preserves_other_child(self):
        unrelated = subprocess.Popen(["sleep", "20"])
        self.children.append(unrelated)
        p = self.start();self.command(p, "play")
        p.kill();p.wait(timeout=3)
        self.wait_for(lambda: self.values()[0] == 0)
        self.assertEqual(self.values()[5:7], (1, 817))
        self.assertIsNone(unrelated.poll())
        self.assertFalse(self.report()["paired_worker_force_stopped"])

    def test_blocked_decoder_close_is_bounded_and_force_stops_only_its_worker(self):
        p = self.start();self.command(p, "play")
        start = time.monotonic()
        p.stdin.write("block-close\n");p.stdin.flush()
        self.assertEqual(p.wait(timeout=5), -signal.SIGKILL)
        self.wait_for(lambda:self.values()[0] == 0)
        self.assertLess(time.monotonic()-start, 4.5)
        self.assertTrue(self.report()["paired_worker_force_stopped"])

    def test_resize_signal_window_accepts_previous_owned_rectangle(self):
        p=self.start();self.command(p,"play");self.command(p,"resize-before-commit")
        p.kill();p.wait(timeout=3)
        self.wait_for(lambda:self.values()[0] == 0)
        self.assertEqual(self.values()[5:7],(1,817))

    def test_unknown_surface_is_preserved_and_reported(self):
        p=self.start();self.command(p,"play")
        v=list(self.values());v[1]=12;self.state.write_bytes(struct.pack("8i",*v))
        p.kill();p.wait(timeout=3)
        self.wait_for(lambda:(self.runtime/"media-surface-last.json").stat().st_size > 0)
        self.assertFalse(self.report()["surface_retired"])
        self.assertEqual(self.values()[0],1)
        self.assertEqual(self.values()[5:7],(0,817))

    def test_second_worker_cannot_acquire_surface_while_first_holds_lease(self):
        first=self.start();self.command(first,"play")
        second=self.start(ready=False)
        _,err=second.communicate(timeout=5)
        self.assertEqual(second.returncode,4)
        self.assertIn("still owned",err)
        self.assertEqual(self.values()[0],1)

    def test_symlink_hardlink_and_nonprivate_inputs_refused(self):
        for kind in ["directory-symlink","lock-symlink","lock-hardlink","lock-mode","directory-mode"]:
            with self.subTest(kind=kind):
                lock=self.runtime/"media-surface.lock"
                if lock.exists() or lock.is_symlink():lock.unlink()
                self.runtime.chmod(0o700)
                runtime=self.runtime
                if kind=="directory-symlink":
                    runtime=self.root/"alias";runtime.symlink_to(self.runtime,target_is_directory=True)
                elif kind=="lock-symlink":lock.symlink_to(self.state)
                elif kind=="lock-hardlink":os.link(self.state,lock)
                elif kind=="lock-mode":lock.touch(mode=0o644)
                else:self.runtime.chmod(0o755)
                p=subprocess.run([str(BINARY),str(runtime),str(self.state)],
                                 input="quit\n",text=True,capture_output=True,timeout=5)
                self.assertEqual(p.returncode,4,p.stdout+p.stderr)
                self.assertEqual(self.values()[0],0)
                self.assertEqual(self.values()[5],0)

    def test_existing_foreign_enabled_plane_refuses_start_without_mutation(self):
        v=list(self.values());v[0]=1;self.state.write_bytes(struct.pack("8i",*v))
        p=self.start(ready=False);_,err=p.communicate(timeout=3)
        self.assertEqual(p.returncode,4);self.assertIn("another player",err)
        self.assertEqual(self.values()[0],1);self.assertEqual(self.values()[5],0)

    def test_hmi_owner_death_reaps_even_stopped_paired_worker(self):
        pidfile=self.root/"worker.pid"
        owner_code = (
            "import subprocess,sys,time,pathlib;"
            "p=subprocess.Popen(sys.argv[1:4],stdin=subprocess.PIPE,stdout=subprocess.PIPE,"
            "stderr=subprocess.PIPE,text=True);"
            "assert p.stdout.readline().strip()=='READY';"
            "p.stdin.write('play\\n');p.stdin.flush();"
            "assert p.stdout.readline().strip()=='ACK';"
            "pathlib.Path(sys.argv[4]).write_text(str(p.pid));time.sleep(30)")
        owner=subprocess.Popen(["python3","-c",owner_code,str(BINARY),str(self.runtime),
                                str(self.state),str(pidfile)],
                               stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.children.append(owner)
        self.wait_for(lambda:pidfile.exists())
        worker=int(pidfile.read_text());handle=pidfd_open(worker)
        try:
            pidfd_signal(handle,signal.SIGSTOP)
            owner.kill();owner.wait(timeout=3)
            self.wait_for(lambda:self.values()[0] == 0)
            self.assertTrue(select.select([handle],[],[],0)[0])
            self.assertTrue(self.report()["paired_worker_force_stopped"])
            self.assertEqual(self.report()["cause"],"hmi-exit")
        finally:
            try:pidfd_signal(handle,signal.SIGKILL)
            except ProcessLookupError:pass
            os.close(handle)


if __name__=="__main__":
    unittest.main()
