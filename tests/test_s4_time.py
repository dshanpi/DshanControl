"""Execute one-shot startup: exact client arguments, bounded failure, no retry."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class S4TimeTests(unittest.TestCase):
    def test_client_success_failure_and_epoch_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            calls = root / 'calls'
            timeout = root / 'timeout'
            timeout.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$TIME_CALLS"\nexit "${TIME_EXIT:-0}"\n')
            timeout.chmod(0o755)
            date = root / 'date'
            date.write_text('#!/bin/sh\necho "${TIME_EPOCH:-1792060000}"\n')
            date.chmod(0o755)
            script = ROOT / 'boards/t113s4-sdnand/runtime/S73dshancontrol-time'
            env = dict(os.environ, PATH=str(root) + ':' + os.environ['PATH'], TIME_CALLS=str(calls))
            for error, epoch, expected in [('0', '1792060000', 0), ('23', '1792060000', 23),
                                            ('143', '1792060000', 143), ('0', '42', 1)]:
                with self.subTest(error=error, epoch=epoch):
                    calls.unlink(missing_ok=True)
                    result = subprocess.run(['sh', str(script), 'start'], capture_output=True,
                                            text=True, timeout=5,
                                            env=dict(env, TIME_EXIT=error, TIME_EPOCH=epoch))
                    self.assertEqual(expected, result.returncode, result.stderr)
                    self.assertEqual(['30 /usr/sbin/ntpd -n -q -p time.cloudflare.com'],
                                     calls.read_text().splitlines())
                    self.assertEqual(expected == 0, 'initialization completed' in result.stdout)
            calls.unlink()
            result = subprocess.run(['sh', str(script), 'stop'], env=env, timeout=5)
            self.assertEqual(0, result.returncode)
            self.assertFalse(calls.exists())
