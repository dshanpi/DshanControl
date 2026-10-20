"""Production provider against a private Unix voice protocol fixture, not audio proof."""
import json
import os
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from pathlib import Path
from test_desktop_service import BINARY, ROOT, eventually, rpc


@unittest.skipUnless(BINARY.exists(), 'Native desktop provider build required')
class DesktopVoiceLifecycleTests(unittest.TestCase):
    def setUp(self):
        # AF_UNIX paths are bounded; honor TMPDIR rather than nesting below a deep checkout.
        self.temp = tempfile.TemporaryDirectory(prefix='dshan-voice-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.path = self.base / 'voice.sock'
        self.ipc = self.base / 'ipc/desktop.sock'
        self.listener = socket.socket(socket.AF_UNIX)
        self.addCleanup(self.listener.close)
        self.listener.bind(str(self.path)); self.listener.listen(4); self.listener.settimeout(4)
        self.log = (self.base / 'provider.log').open('wb')
        self.addCleanup(self.log.close)
        self.buffer = b''

    def start(self, runtime_uid=None):
        cfg = self.base / 'config.json'
        cfg.write_text(json.dumps({'voice': {'socket': str(self.path),
                                            'runtime_uid': os.getuid() if runtime_uid is None else runtime_uid}}))
        cfg.chmod(0o600)
        process = subprocess.Popen([str(BINARY), '--socket', str(self.ipc), '--config', str(cfg)],
                                   stdout=self.log, stderr=self.log)
        def stop():
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=3)
        self.addCleanup(stop)
        eventually(lambda: self.ipc.exists() or process.poll() is not None)
        self.assertIsNone(process.poll(), (self.base / 'provider.log').read_text())
        self.connection, _ = self.listener.accept()
        self.addCleanup(self.connection.close)
        self.connection.settimeout(3)
        return process

    def read_command(self, timeout=3):
        self.connection.settimeout(timeout)
        while b'\n' not in self.buffer:
            chunk = self.connection.recv(4096)
            if not chunk: raise AssertionError('Voice connection closed before expected command')
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b'\n', 1)
        value = json.loads(line)
        self.assertEqual(value['version'], 1)
        return value['cmd']

    def status(self, **overrides):
        value = dict(version=1, event='status', state='idle', activated=True,
                     microphone_ready=True, speaker_ready=True)
        value.update(overrides)
        self.connection.sendall(json.dumps(value).encode() + b'\n')

    def state(self): return rpc(self.ipc)['result']['voice']
    def command(self, value): return rpc(self.ipc, 'voice.command', {'command': value})

    def test_readiness_and_disconnect_do_not_replay_start(self):
        self.start(); self.assertEqual(self.read_command(), 'status')
        for field in ['activated', 'microphone_ready', 'speaker_ready']:
            self.status(); eventually(lambda: self.state()['ready'])
            self.status(**{field: False})
            eventually(lambda: not self.state()['ready'])
            self.assertFalse(self.command('start_listen')['ok'])
        self.assertFalse(self.buffer)
        self.connection.settimeout(.15)
        with self.assertRaises(socket.timeout): self.connection.recv(1)
        self.status(); eventually(lambda: self.state()['ready'])
        self.assertTrue(self.command('start_listen')['ok'])
        self.assertEqual(self.read_command(), 'start_listen')
        self.connection.close()
        eventually(lambda: not self.state()['ready'])
        self.assertFalse(self.command('start_listen')['ok'])
        self.connection, _ = self.listener.accept(); self.addCleanup(self.connection.close)
        self.buffer = b''
        self.assertEqual(self.read_command(), 'status')
        self.status(); eventually(lambda: self.state()['ready'])
        self.connection.settimeout(.2)
        with self.assertRaises(socket.timeout): self.connection.recv(1)
        self.assertTrue(self.command('start_listen')['ok'])
        self.assertEqual(self.read_command(), 'start_listen')
        self.assertTrue(self.command('stop_listen')['ok'])
        self.assertEqual(self.read_command(), 'stop_listen')

    def test_missing_heartbeat_sends_stop_and_disconnects(self):
        self.start(); self.assertEqual(self.read_command(), 'status')
        self.status(); eventually(lambda: self.state()['ready'])
        self.assertTrue(self.command('start_listen')['ok'])
        self.assertEqual(self.read_command(), 'start_listen')
        started = time.monotonic()
        self.assertEqual(self.read_command(timeout=8), 'stop_listen')
        self.assertEqual(self.read_command(), 'exit_free_chat')
        elapsed = time.monotonic() - started
        self.assertLess(elapsed, 8)
        self.assertEqual(self.connection.recv(1), b'')
        eventually(lambda: not self.state()['ready'])
        self.assertFalse(self.command('start_listen')['ok'])
        print('VOICE_HEARTBEAT_STOP_SECONDS=%.6f' % elapsed, flush=True)

    def assert_active_deadline(self, command, seconds):
        self.start(); self.assertEqual(self.read_command(), 'status')
        self.status(); eventually(lambda: self.state()['ready'])
        stop = threading.Event(); errors = []
        def heartbeat():
            while not stop.wait(.5):
                try: self.status(state='listening')
                except OSError as error:
                    errors.append(str(error)); return
        thread = threading.Thread(target=heartbeat, daemon=True)
        thread.start()
        def finish(): stop.set(); thread.join(timeout=2)
        self.addCleanup(finish)
        started = time.monotonic()
        self.assertTrue(self.command(command)['ok'])
        self.assertEqual(self.read_command(), command)
        self.assertEqual(self.read_command(timeout=seconds+4), 'stop_listen')
        self.assertEqual(self.read_command(), 'exit_free_chat')
        elapsed = time.monotonic() - started
        self.assertGreaterEqual(elapsed, seconds)
        self.assertLess(elapsed, seconds+4)
        self.assertTrue(self.state()['ready']); self.assertFalse(errors)
        print('VOICE_DEADLINE command=%s expected=%s elapsed=%.6f' % (command, seconds, elapsed), flush=True)

    def test_listening_limit_stops_with_healthy_heartbeat(self):
        self.assert_active_deadline('start_listen', 30)

    def test_free_chat_limit_stops_with_healthy_heartbeat(self):
        self.assert_active_deadline('enter_free_chat', 120)

    def test_wrong_runtime_peer_uid_is_refused(self):
        self.start(runtime_uid=os.getuid()+1)
        self.assertEqual(self.connection.recv(1), b'')
        self.assertFalse(self.state()['ready'])
        self.assertFalse(self.command('start_listen')['ok'])


if __name__ == '__main__': unittest.main()
