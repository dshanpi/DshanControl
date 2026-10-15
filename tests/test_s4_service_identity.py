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
        native.write_text('#!/bin/sh\nprintf "%s\\n" "$1" >> "' + str(self.calls) + '"\nexit 19\n')
        native.chmod(0o700)
        source = (ROOT / 'boards/t113s4-sdnand/runtime/dshancontrol-services').read_text()
        source = source.replace('/usr/bin/dshancontrol-runtime', str(native))
        source = source.replace('/mnt/UDISK/dshancontrol', str(self.root / 'managed'))
        source = source.replace('/var/run/dshancontrol', str(self.root / 'run'))
        source = source.replace('/etc/dshancontrol/display.env', str(self.root / 'display.env'))
        self.hook = self.root / 'hook'
        self.hook.write_text(source)

    def invoke(self, operation):
        return subprocess.run(['/bin/sh', str(self.hook), operation],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=3)

    def test_stop_does_not_depend_on_reading_a_broken_application_identity(self):
        self.assertEqual(0, self.invoke('stop').returncode)
        self.assertFalse(self.calls.exists())
        self.assertFalse((self.root / 'managed').exists())

    def test_start_refuses_invalid_identity_and_health_runs_the_native_probe_directly(self):
        self.assertEqual(19, self.invoke('start').returncode)
        self.assertEqual(['application-commit'], self.calls.read_text().splitlines())
        self.assertFalse((self.root / 'run').exists())
        self.calls.unlink()
        self.assertEqual(19, self.invoke('health').returncode)
        self.assertEqual(['application-health'], self.calls.read_text().splitlines())
