"""Exercise the real lifecycle shell with isolated system paths and no live PIDs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class S4ServiceIdentityTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.calls = self.root / 'calls'
        native = self.root / 'native'
        self.native = native
        self.marker = self.root / 'platform-service.json'
        native.write_text('#!/bin/sh\nprintf "%s\\n" "$1" >> "' + str(self.calls) +
                          '"\n[ "$1" != application-stop ] || exit 0\nexit 19\n')
        native.chmod(0o700)
        source = (ROOT / 'boards/t113s4-sdnand/runtime/dshancontrol-services').read_text()
        source = source.replace('/usr/bin/dshancontrol-runtime', str(native))
        source = source.replace('/etc/dshancontrol/platform-service.json', str(self.marker))
        source = source.replace('/mnt/UDISK/dshancontrol', str(self.root / 'managed'))
        source = source.replace('/var/run/dshancontrol', str(self.root / 'run'))
        source = source.replace('/etc/dshancontrol/display.env', str(self.root / 'display.env'))
        source = source.replace('/usr/lib/dshancontrol-update/storage.py', str(self.root / 'missing-storage.py'))
        source = source.replace('/etc/dshancontrol/maintenance-check', str(self.root / 'missing-maintenance-check'))
        self.hook = self.root / 'hook'
        self.hook.write_text(source)

    def invoke(self, operation):
        return subprocess.run(['/bin/sh', str(self.hook), operation],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=3)

    def test_stop_does_not_depend_on_reading_a_broken_application_identity(self):
        self.assertEqual(0, self.invoke('stop').returncode)
        self.assertEqual(['application-stop'], self.calls.read_text().splitlines())
        self.assertFalse((self.root / 'managed').exists())

    def test_start_refuses_invalid_identity_and_health_runs_the_native_probe_directly(self):
        self.assertEqual(19, self.invoke('start').returncode)
        self.assertEqual(['application-commit'], self.calls.read_text().splitlines())
        self.assertFalse((self.root / 'run').exists())
        self.calls.unlink()
        self.assertEqual(19, self.invoke('health').returncode)
        self.assertEqual(['application-health'], self.calls.read_text().splitlines())

    def test_native_system_with_unavailable_binary_refuses_before_legacy_actions(self):
        self.native.chmod(0o600)
        self.marker.write_text('{}')
        for operation in ['start','stop','health','restart-web','maintenance']:
            with self.subTest(operation=operation):
                result=self.invoke(operation)
                self.assertEqual(1,result.returncode)
                self.assertIn('native runtime unavailable',result.stderr)
                self.assertFalse(self.calls.exists())
                self.assertFalse((self.root/'managed').exists())
        for name in ['S60mount_udisk','S70dshancontrol','S71dshancontrol-update',
                     'S72dshancontrol-wifi','dshancontrol-services']:
            source=(ROOT/'boards/t113s4-sdnand/runtime'/name).read_text()
            source=source.replace('/usr/bin/dshancontrol-runtime',str(self.native))
            source=source.replace('/etc/dshancontrol/platform-service.json',str(self.marker))
            source=source.replace('/etc/dshancontrol/display.env',str(self.root/'display.env'))
            script=self.root/name;script.write_text(source)
            # An unknown action is harmless even if the guard regresses.
            result=subprocess.run(['/bin/sh',str(script),'guard-probe'],
                capture_output=True,text=True,timeout=3)
            self.assertEqual(1,result.returncode,name)
            self.assertIn('native runtime unavailable',result.stderr,name)
