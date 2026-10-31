"""Exercise a real dnsmasq binary on loopback; no board or public network traffic.

Set DSHAN_DNSMASQ_TEST_BINARY to the host build of the pinned S4 dnsmasq source.
The preload shim reproduces the board's ENOSYS result; it is never installed.
"""
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import tempfile
import time
import unittest


class S4DnsmasqCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = os.environ.get("DSHAN_DNSMASQ_TEST_BINARY")
        if not binary:
            raise unittest.SkipTest("pinned dnsmasq host build not supplied")
        cls.binary = str(Path(binary).resolve(strict=True))
        cls.build = tempfile.TemporaryDirectory(prefix="dshan-dnsmasq-shim-")
        cls.addClassCleanup(cls.build.cleanup)
        source = Path(cls.build.name) / "no_inotify.c"
        source.write_text("#include <errno.h>\n"
                          "int inotify_init(void) { errno=ENOSYS; return -1; }\n"
                          "int inotify_init1(int flags) { (void)flags; errno=ENOSYS; return -1; }\n")
        cls.shim = str(Path(cls.build.name) / "no_inotify.so")
        subprocess.run(["gcc", "-shared", "-fPIC", str(source), "-o", cls.shim],
                       check=True, capture_output=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dshan-dnsmasq-case-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.resolv = self.root / "resolv.conf"
        self.resolv.write_text("nameserver 127.0.0.2\n")
        self.hosts = self.root / "hosts"
        self.hosts.write_text("203.0.113.31 fixture.test\n")
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]
        self.env = dict(os.environ, LD_PRELOAD=self.shim)
        self.args = [self.binary, "--no-daemon", "--conf-file=/dev/null",
                     "--listen-address=127.0.0.1", "--bind-interfaces",
                     "--port=" + str(self.port), "--no-hosts",
                     "--resolv-file=" + str(self.resolv),
                     "--addn-hosts=" + str(self.hosts),
                     "--pid-file=" + str(self.root / "dnsmasq.pid"),
                     "--log-facility=-"]

    def start(self):
        self.log_path = self.root / "daemon.log"
        self.log = self.log_path.open("w")
        self.addCleanup(self.log.close)
        self.proc = subprocess.Popen(self.args, env=self.env, stdout=self.log,
                                     stderr=subprocess.STDOUT)
        self.addCleanup(self.stop)
        self.await_log("started, version")

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)

    def await_log(self, expected):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            text = self.log_path.read_text()
            if self.proc.poll() is not None:
                self.fail("dnsmasq exited %s: %s" % (self.proc.returncode, text))
            if expected in text:
                return
            time.sleep(0.05)
        self.fail("missing %r in %s" % (expected, self.log_path.read_text()))

    def query(self, tcp=False):
        # All names resolve from the isolated hosts file, never upstream.
        packet = struct.pack("!6H", 0xDA31, 0x0100, 1, 0, 0, 0)
        packet += b"\x07fixture\x04test\0" + struct.pack("!HH", 1, 1)
        kind = socket.SOCK_STREAM if tcp else socket.SOCK_DGRAM
        with socket.socket(socket.AF_INET, kind) as sock:
            sock.settimeout(2)
            sock.connect(("127.0.0.1", self.port))
            if tcp:
                sock.sendall(struct.pack("!H", len(packet)) + packet)
                def exact(n):
                    result = b""
                    while len(result) < n:
                        chunk = sock.recv(n - len(result))
                        self.assertTrue(chunk, "truncated DNS TCP response")
                        result += chunk
                    return result
                response = exact(struct.unpack("!H", exact(2))[0])
            else:
                sock.send(packet)
                response = sock.recv(4096)
        ident, flags, questions, answers, _, _ = struct.unpack("!6H", response[:12])
        self.assertEqual(ident, 0xDA31)
        self.assertEqual(flags & 15, 0)
        self.assertEqual((questions, answers), (1, 1))
        self.assertTrue(flags & 0x8000)
        self.assertEqual(response[12:len(packet)], packet[12:])
        # A direct hosts answer is one compressed owner, A/IN, TTL and IPv4.
        self.assertEqual(len(response), len(packet) + 16)
        self.assertEqual(struct.unpack("!HHH", response[len(packet):len(packet)+6]),
                         (0xC00C, 1, 1))
        self.assertEqual(struct.unpack("!H", response[-6:-4])[0], 4)
        return socket.inet_ntoa(response[-4:])

    def test_udp_and_tcp_dns_with_inotify_unavailable(self):
        self.start()
        self.assertEqual(self.query(), "203.0.113.31")
        self.assertEqual(self.query(tcp=True), "203.0.113.31")

    def test_resolver_changes_are_polled_without_inotify(self):
        self.start()
        self.await_log("using nameserver 127.0.0.2#53")
        prior = self.resolv.stat().st_mtime
        self.resolv.write_text("nameserver 127.0.0.3\n")
        os.utime(self.resolv, (prior + 2, prior + 2))
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            self.assertEqual(self.query(), "203.0.113.31")
            if "using nameserver 127.0.0.3#53" in self.log_path.read_text():
                return
            time.sleep(0.2)
        self.fail("replacement resolver was not detected")

    def test_hosts_reload_after_sighup(self):
        self.start()
        self.assertEqual(self.query(), "203.0.113.31")
        self.hosts.write_text("203.0.113.32 fixture.test\n")
        self.proc.send_signal(signal.SIGHUP)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.query() == "203.0.113.32":
                return
            time.sleep(0.05)
        self.fail("hosts reload did not change the actual DNS answer")

    def test_dynamic_directories_fail_closed(self):
        directory = self.root / "dynamic"
        directory.mkdir()
        result = subprocess.run(self.args + ["--hostsdir=" + str(directory)],
                                env=self.env, capture_output=True, text=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not supported on this platform", result.stderr)
        self.assertNotIn("started, version", result.stderr)


if __name__ == "__main__":
    unittest.main()
