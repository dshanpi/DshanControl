"""Execute the board init script with isolated paths and real process ownership."""
import os
import shutil
import signal
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class S4WifiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config_dir = self.root / 'system'
        self.config_dir.mkdir(mode=0o700)
        self.config = self.config_dir / 'wifi.conf'
        self.run_dir = self.root / 'run'
        self.pid = self.run_dir / 'wpa.pid'
        self.calls = self.root / 'calls'
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.modules = self.root / 'modules'
        self.modules.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin) + ':' + os.environ['PATH'],
                        TEST_WIFI_CALLS=str(self.calls))
        for name, body in {
            'modprobe': 'echo "module:$1" >> "$TEST_WIFI_CALLS"; exit "${TEST_MODULE_EXIT:-0}"',
            'ip': 'echo "ip:$*" >> "$TEST_WIFI_CALLS"; case "$*" in *addr*) '
                  '[ "${TEST_IPV4_PRESENT:-0}" = 0 ] || echo "4: wlan0 inet 192.0.2.2/24" ;; esac; exit 0',
            'wpa_cli': 'case "$*" in *ping*) [ "${TEST_FOREIGN_WPA:-0}" = 1 ] && echo PONG ;; '
                       '*status*) echo "wpa_state=${TEST_WPA_STATE:-COMPLETED}" ;; esac',
            'udhcpc': 'echo dhcp >> "$TEST_WIFI_CALLS"; exit "${TEST_DHCP_EXIT:-0}"',
            'sleep': 'exit 0',
        }.items():
            path = self.bin / name
            path.write_text('#!/bin/sh\n' + body + '\n')
            path.chmod(0o755)
        probe = self.root / 'storage.py'
        probe.write_text('import os,sys\nsys.exit(int(os.environ.get("TEST_STORAGE_EXIT", "0")))\n')
        # A real executable daemon preserves argv and /proc/exe across fork.
        # This lets the actual script distinguish it from a foreign process.
        source = self.root / 'supplicant.c'
        source.write_text(r'''
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--foreign")) { for (;;) pause(); }
    char *path = 0;
    for (int i=1; i+1<argc; ++i) if (!strcmp(argv[i], "-P")) path=argv[i+1];
    if (!path) return 2;
    pid_t p=fork();
    if (p<0) return 3;
    if (p) { FILE *f=fopen(path,"w"); if (!f) return 4; fprintf(f,"%ld\n",(long)p); return fclose(f); }
    int fd=open("/dev/null",O_RDWR); for (int n=0;n<3;++n) dup2(fd,n); if(fd>2) close(fd);
    for (;;) pause();
}
''')
        self.wpa = self.root / 'wpa_supplicant'
        subprocess.run(['cc', '-Wall', '-Werror', str(source), '-o', str(self.wpa)], check=True)
        script = (ROOT / 'boards/t113s4-sdnand/runtime/S72dshancontrol-wifi').read_text()
        for old, new in [('/mnt/UDISK/dshancontrol/system', str(self.config_dir)),
                         ('/var/run/dshancontrol-wifi', str(self.run_dir)),
                         ('/usr/sbin/wpa_supplicant', str(self.wpa)),
                         ('/sys/module/', str(self.modules) + '/'),
                         ('/usr/lib/dshancontrol-update/storage.py', str(probe)),
                         ('0:700', str(os.getuid()) + ':700'),
                         ('0:600', str(os.getuid()) + ':600'),
                         ('0:400', str(os.getuid()) + ':400')]:
            self.assertIn(old, script)
            script = script.replace(old, new)
        self.script = self.root / 'S72wifi'
        self.script.write_text(script)
        self.script.chmod(0o755)
        self.addCleanup(self.cleanup_daemon)

    def cleanup_daemon(self):
        if self.pid.is_file() and not self.pid.is_symlink():
            try:
                pid = int(self.pid.read_text())
                if os.readlink('/proc/%d/exe' % pid) == str(self.wpa):
                    os.kill(pid, signal.SIGTERM)
            except (OSError, ValueError):
                pass

    def run_init(self, action='start', **env):
        return subprocess.run([str(self.script), action], env=dict(self.env, **env),
                              capture_output=True, text=True, timeout=10)

    def configured(self):
        self.config.write_text('network={\n ssid="synthetic-test"\n psk="private-fixture-only"\n}\n')
        self.config.chmod(0o600)

    def test_driver_failure_is_not_retried_and_unconfigured_boot_never_connects(self):
        failed = self.run_init(TEST_MODULE_EXIT='17')
        self.assertEqual(17, failed.returncode)
        self.assertEqual(['module:aic8800_bsp'], self.calls.read_text().splitlines())
        self.calls.unlink()
        ok = self.run_init()
        self.assertEqual(0, ok.returncode, ok.stderr)
        self.assertIn('no site credentials configured', ok.stdout)
        self.assertFalse(self.run_dir.exists())
        self.assertNotIn('dhcp', self.calls.read_text())

    def test_storage_failure_prevents_driver_and_network_actions(self):
        result = self.run_init(TEST_STORAGE_EXIT='19')
        self.assertEqual(19, result.returncode)
        self.assertFalse(self.calls.exists())

    def test_private_config_associates_and_dhcp_failure_preserves_exact_error(self):
        self.configured()
        failed = self.run_init(TEST_DHCP_EXIT='23')
        self.assertEqual(23, failed.returncode)
        self.assertNotIn('private-fixture-only', failed.stdout + failed.stderr)
        original = self.pid.read_text()
        # An explicit later start observes the existing owned daemon, without
        # launching a second supplicant; IPv4 already present needs no DHCP.
        self.calls.unlink()
        ok = self.run_init(TEST_IPV4_PRESENT='1')
        self.assertEqual(0, ok.returncode, ok.stderr)
        self.assertEqual(original, self.pid.read_text())
        self.assertNotIn('dhcp', self.calls.read_text())
        self.assertEqual(0, self.run_init('status').returncode)
        self.assertEqual(0, self.run_init('stop').returncode)

    def test_symlink_and_public_credentials_refuse_without_launch_or_secret_output(self):
        self.configured()
        self.config.chmod(0o644)
        result = self.run_init()
        self.assertNotEqual(0, result.returncode)
        self.assertFalse(self.pid.exists())
        self.assertNotIn('private-fixture-only', result.stdout + result.stderr)
        self.config.rename(self.root / 'outside.conf')
        self.config.symlink_to(self.root / 'outside.conf')
        self.assertNotEqual(0, self.run_init().returncode)
        self.assertFalse(self.pid.exists())

    def test_foreign_live_pid_is_never_stopped_or_replaced(self):
        self.configured()
        self.run_dir.mkdir(mode=0o700)
        foreign = subprocess.Popen([str(self.wpa), '--foreign'])
        try:
            self.pid.write_text(str(foreign.pid) + '\n')
            for action in ('start', 'stop'):
                result = self.run_init(action)
                self.assertNotEqual(0, result.returncode)
                self.assertIsNone(foreign.poll())
                self.assertEqual(str(foreign.pid) + '\n', self.pid.read_text())
        finally:
            foreign.terminate()
            foreign.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()
