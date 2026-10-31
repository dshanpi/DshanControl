"""Real host BusyBox service commands and isolated dnsmasq lifecycle.

A private copied executable makes -x select only fixture processes. Ports are
unprivileged loopback ports; no DHCP ranges, upstream DNS or board operations.
"""
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

from test_s4_dnsmasq_init import INIT


class S4DnsmasqInitRealTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        supplied = os.environ.get("DSHAN_DNSMASQ_TEST_BINARY")
        cls.busybox = shutil.which("busybox")
        if not supplied or not cls.busybox:
            raise unittest.SkipTest("pinned host dnsmasq and host BusyBox required")
        if os.geteuid() == 0:
            raise unittest.SkipTest("loopback service fixture must run without root")
        cls.source = Path(supplied).resolve(strict=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dshan-dns-real-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.binary = self.root / "dnsmasq"
        shutil.copyfile(str(self.source), str(self.binary))
        self.binary.chmod(0o755)
        self.addCleanup(self.cleanup_owned)
        self.conf = self.root / "dnsmasq.conf"
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]
        self.conf.write_text(
            "no-resolv\nno-hosts\nbind-interfaces\nlisten-address=127.0.0.1\n"
            "port=" + str(self.port) + "\naddress=/fixture.test/203.0.113.31\n"
            "pid-file=" + str(self.root / "daemon.pid") + "\n"
            "log-facility=" + str(self.root / "daemon.log") + "\n")
        self.init = self.root / "S80dnsmasq"
        self.init.write_text(INIT.read_text().replace("/etc/dnsmasq.conf", str(self.conf))
                             .replace("/usr/sbin/dnsmasq", str(self.binary)))
        self.init.chmod(0o755)
        wrapper = self.root / "start-stop-daemon"
        wrapper.write_text(
            '#!/bin/sh\nif [ "$1" = "-S" ]; then\n'
            ' exec "$DS_BUSYBOX" start-stop-daemon "$@" -- --conf-file="$DS_CONFIG"\n'
            'fi\nexec "$DS_BUSYBOX" start-stop-daemon "$@"\n')
        wrapper.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.root) + ":" + os.environ["PATH"],
                        DS_BUSYBOX=self.busybox, DS_CONFIG=str(self.conf))

    def owned(self):
        result = {}
        for directory in Path("/proc").iterdir():
            if not directory.name.isdigit():
                continue
            try:
                if os.readlink(str(directory / "exe")) != str(self.binary):
                    continue
                stat = (directory / "stat").read_text().rsplit(")", 1)[1].split()
                result[int(directory.name)] = stat[19]
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                continue
        return result

    def cleanup_owned(self):
        # Recheck identity immediately before signalling. Never use killall,
        # a system pidfile or the shared /usr/sbin/dnsmasq executable.
        for sig in (signal.SIGTERM, signal.SIGKILL):
            for pid, start in self.owned().items():
                if self.owned().get(pid) == start:
                    try:
                        os.kill(pid, sig)
                    except ProcessLookupError:
                        pass
            deadline = time.monotonic() + 2
            while self.owned() and time.monotonic() < deadline:
                time.sleep(0.02)
            if not self.owned():
                return
        self.assertEqual(self.owned(), {}, "owned fixture process did not exit")

    def invoke(self, action):
        return subprocess.run(["/bin/sh", str(self.init), action], env=self.env,
                              capture_output=True, text=True, timeout=5)

    def assert_dns_answer(self):
        query = struct.pack("!6H", 0xDA32, 0x0100, 1, 0, 0, 0)
        query += b"\x07fixture\x04test\0" + struct.pack("!HH", 1, 1)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(2)
            sock.sendto(query, ("127.0.0.1", self.port))
            answer, peer = sock.recvfrom(4096)
        self.assertEqual(peer, ("127.0.0.1", self.port))
        ident, flags, questions, answers, _, _ = struct.unpack("!6H", answer[:12])
        self.assertEqual((ident, flags & 15, questions, answers), (0xDA32, 0, 1, 1))
        self.assertEqual(socket.inet_ntoa(answer[-4:]), "203.0.113.31")

    def start_and_query(self):
        result = self.invoke("start")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(self.owned()), 1)
        self.assert_dns_answer()

    def test_start_answer_stop_and_release_ports(self):
        self.start_and_query()
        result = self.invoke("stop")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        deadline = time.monotonic() + 2
        while self.owned() and time.monotonic() < deadline:
            time.sleep(0.02)
        self.assertEqual(self.owned(), {})
        for kind in (socket.SOCK_DGRAM, socket.SOCK_STREAM):
            with socket.socket(socket.AF_INET, kind) as sock:
                sock.bind(("127.0.0.1", self.port))

    def test_invalid_configuration_returns_failure_without_daemon(self):
        self.conf.write_text("invalid-dshan-fixture-option=yes\n")
        result = self.invoke("start")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAIL", result.stdout)
        self.assertEqual(self.owned(), {})

    def test_duplicate_start_reports_failure_and_preserves_existing_daemon(self):
        self.start_and_query()
        before = self.owned()
        result = self.invoke("start")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.owned(), before)
        self.assert_dns_answer()

    def test_restart_with_no_owned_daemon_refuses_to_start(self):
        result = self.invoke("restart")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.owned(), {})
        self.assertNotIn("Starting", result.stdout)

    def test_restart_replaces_daemon_and_keeps_dns_available(self):
        self.start_and_query()
        before = self.owned()
        result = self.invoke("restart")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(self.owned()), 1)
        self.assertNotEqual(self.owned(), before)
        self.assert_dns_answer()


if __name__ == "__main__":
    unittest.main()
