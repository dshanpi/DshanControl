"""S4 init command contract, isolated from host and board service state.

Only the configuration path is relocated for the command fixture. An explicit
shell argv can exercise the same script with the target BusyBox under QEMU.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXTERNAL = ROOT / "boards/t113s4-sdnand/buildroot"
INIT = EXTERNAL / "package/dnsmasq/S80dnsmasq"


class S4DnsmasqInitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dshan-dns-init-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.conf = self.root / "dnsmasq.conf"
        self.conf.touch()
        self.calls = self.root / "calls"
        source = Path(os.environ.get("DSHAN_DNSMASQ_INIT_SCRIPT", str(INIT)))
        self.init = self.root / "S80dnsmasq"
        self.init.write_text(source.read_text().replace("/etc/dnsmasq.conf", str(self.conf)))
        self.init.chmod(0o755)
        stub = self.root / "start-stop-daemon"
        stub.write_text(
            '#!/bin/sh\nprintf "%s\\n" "$*" >> "$DS_CALLS"\n'
            'case "$1" in -S) exit "$DS_START_RC";; '
            '-K) exit "$DS_STOP_RC";; *) exit 99;; esac\n')
        stub.chmod(0o755)
        self.shell = json.loads(os.environ.get("DSHAN_DNSMASQ_INIT_SHELL", '["/bin/sh"]'))

    def invoke(self, action, start=0, stop=0):
        if self.calls.exists():
            self.calls.unlink()
        env = dict(os.environ, PATH=str(self.root) + ":" + os.environ["PATH"],
                   DS_CALLS=str(self.calls), DS_START_RC=str(start), DS_STOP_RC=str(stop))
        result = subprocess.run(self.shell + [str(self.init), action], env=env,
                                capture_output=True, text=True, timeout=5)
        calls = self.calls.read_text().splitlines() if self.calls.exists() else []
        return result, calls

    def test_start_failure_reaches_caller(self):
        result, calls = self.invoke("start", start=7)
        self.assertEqual(result.returncode, 7)
        self.assertIn("FAIL", result.stdout)
        self.assertNotIn("OK", result.stdout)
        self.assertEqual(calls, ["-S -x /usr/sbin/dnsmasq"])

    def test_stop_failure_reaches_caller(self):
        result, calls = self.invoke("stop", stop=9)
        self.assertEqual(result.returncode, 9)
        self.assertIn("FAIL", result.stdout)
        self.assertEqual(calls, ["-K -q -x /usr/sbin/dnsmasq"])

    def test_failed_stop_prevents_restart_and_reload(self):
        for action in ("restart", "reload"):
            with self.subTest(action=action):
                result, calls = self.invoke(action, stop=9)
                self.assertEqual(result.returncode, 9)
                self.assertEqual(calls, ["-K -q -x /usr/sbin/dnsmasq"])

    def test_failed_restart_start_reaches_caller(self):
        for action in ("restart", "reload"):
            with self.subTest(action=action):
                result, calls = self.invoke(action, start=7)
                self.assertEqual(result.returncode, 7)
                self.assertEqual(calls, ["-K -q -x /usr/sbin/dnsmasq",
                                         "-S -x /usr/sbin/dnsmasq"])
                self.assertTrue(result.stdout.rstrip().endswith("FAIL"))

    def test_successful_commands_keep_executable_selection(self):
        for action, args in (("start", "-S -x /usr/sbin/dnsmasq"),
                             ("stop", "-K -q -x /usr/sbin/dnsmasq")):
            with self.subTest(action=action):
                result, calls = self.invoke(action)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(calls, [args])
                self.assertIn("OK", result.stdout)

    def test_successful_restart_and_reload_preserve_order(self):
        for action in ("restart", "reload"):
            with self.subTest(action=action):
                result, calls = self.invoke(action)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(calls, ["-K -q -x /usr/sbin/dnsmasq",
                                         "-S -x /usr/sbin/dnsmasq"])

    def test_unknown_action_does_not_manage_processes(self):
        result, calls = self.invoke("invalid")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(calls, [])

    def test_missing_configuration_keeps_inherited_noop(self):
        self.conf.unlink()
        for action in ("start", "stop", "restart", "reload"):
            with self.subTest(action=action):
                result, calls = self.invoke(action, start=7, stop=9)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(calls, [])
                self.assertEqual(result.stdout, "")


class S4DnsmasqInitRecipeTests(unittest.TestCase):
    def test_external_recipe_installs_script_without_running_service(self):
        with tempfile.TemporaryDirectory(prefix="dshan-dns-install-") as temp:
            root = Path(temp)
            target = root / "target"
            makefile = root / "Makefile"
            makefile.write_text(
                "BR2_EXTERNAL_DSHANCONTROL_PATH := " + str(EXTERNAL) + "\n"
                "TARGET_DIR := " + str(target) + "\nINSTALL := install\n"
                "SDK_MEMORY_DIR := memory\nSDK_ENDEC_DIR := endec\nSDK_CAMERA_DIR := camera\n"
                "SDK_COMMON_DIR := common\nTPLAYER_DIR := tplayer\n"
                "define DNSMASQ_INSTALL_INIT_SYSV\n\tfalse\nendef\n"
                "include $(BR2_EXTERNAL_DSHANCONTROL_PATH)/external.mk\n"
                "install:\n\t$(DNSMASQ_INSTALL_INIT_SYSV)\n")
            env = dict(os.environ)
            for name in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
                env.pop(name, None)
            result = subprocess.run(["make", "--no-print-directory", "-C", str(root), "install"],
                                    env=env, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            installed = target / "etc/init.d/S80dnsmasq"
            self.assertEqual(installed.read_bytes(), INIT.read_bytes())
            self.assertEqual(installed.stat().st_mode & 0o777, 0o755)


if __name__ == "__main__":
    unittest.main()
