"""Real Unix IPC, TLS HTTP and MQTT fixtures; no production accounts or hardware."""
import datetime
import http.server
import json
import os
import pwd
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import time
import unittest
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('DSHANCONTROL_DESKTOP_TEST_BINARY', ROOT / 'dist/desktop-native/dshancontrol-desktop-service'))


def eventually(probe, timeout=8):
    until = time.monotonic() + timeout
    last = None
    while time.monotonic() < until:
        last = probe()
        if last:
            return last
        time.sleep(.05)
    raise AssertionError(f'Fixture condition timed out: {last!r}')


def rpc(path, method='desktop.snapshot', params=None):
    with socket.socket(socket.AF_UNIX) as stream:
        stream.settimeout(3)
        stream.connect(str(path))
        stream.sendall(json.dumps({'protocol': 1, 'method': method, 'params': params or {}}).encode() + b'\n')
        received = b''
        while b'\n' not in received:
            chunk = stream.recv(65536)
            if not chunk:
                raise AssertionError('IPC closed without a response')
            received += chunk
        return json.loads(received)


class DesktopModelTests(unittest.TestCase):
    def test_native_model_invariants(self):
        binary = ROOT / 'dist/desktop-model-tests/desktop-model-test'
        if not binary.exists():
            self.skipTest('Run make build-desktop first')
        value = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        self.assertEqual(value.returncode, 0, value.stdout + value.stderr)


@unittest.skipUnless(BINARY.exists() and shutil.which('openssl') and shutil.which('mosquitto') and
                     shutil.which('mosquitto_pub') and shutil.which('mosquitto_sub'),
                     'Native desktop build and TLS/MQTT fixture tools required')
class DesktopServiceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tls_dir = tempfile.TemporaryDirectory(prefix='dshan-desktop-tls-')
        folder = Path(cls.tls_dir.name)
        cls.cert = folder / 'fixture-ca.pem'
        cls.key = folder / 'fixture.key'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                        '-keyout', str(cls.key), '-out', str(cls.cert), '-subj', '/CN=localhost',
                        '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1'],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
        cls.key.chmod(0o600)

    @classmethod
    def tearDownClass(cls):
        cls.tls_dir.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='dshan-desktop-integration-')
        self.base = Path(self.temp.name)
        self.config = self.base / 'config.json'
        self.sock = self.base / 'ipc/desktop.sock'
        self.events = {'posts': 0, 'switch': False, 'drop': False, 'bound': False, 'bad_ttl': False,
                       'online': True, 'control_allowed': True, 'authorization_expired': False}
        self.secret = 'mock+secret&percent%/for-this-test-only'
        self.token = 'mock-restricted-family-session-0123456789'
        self.received_secret = None
        self.key_received = None
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def log_message(self, *args):
                pass

            def respond(self, value):
                body = json.dumps(value).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close')
                self.end_headers()
                self.wfile.write(body)

            def do_POST(self):
                body = self.rfile.read(int(self.headers.get('Content-Length', '0')))
                if self.path == '/api/device/provision':
                    fields = urllib.parse.parse_qs(body.decode())
                    outer.received_secret = fields.get('secret', [None])[0]
                    now = int(time.time())
                    bound = outer.events['bound']
                    self.respond({'success': True, 'data': {'deviceId': 'unit-s4', 'bound': bound,
                        'bindToken': '' if bound else outer.events.get('pairing_code', '482719'), 'serverTime': now,
                        'bindTokenExpiresAt': 0 if bound else now + (601 if outer.events['bad_ttl'] else outer.events.get('pairing_ttl', 300))}})
                elif self.path == '/v1/devices/lamp/commands':
                    outer.events['posts'] += 1
                    outer.key_received = self.headers.get('Idempotency-Key')
                    outer.command_body = json.loads(body)
                    if outer.events['drop']:
                        self.close_connection = True
                        self.connection.shutdown(socket.SHUT_RDWR)
                        self.connection.close()
                    else:
                        self.respond({'accepted': True, 'state_confirmed': False})

            def do_GET(self):
                if outer.events['authorization_expired'] or self.headers.get('Authorization') != 'Bearer ' + outer.token:
                    self.send_error(403)
                elif self.path == '/v1/devices':
                    self.respond({'devices': [{'id': 'lamp', 'name': '书房台灯', 'room': '书房', 'online': outer.events['online'],
                                              'status': [{'code': 'switch', 'value': outer.events['switch']}]}]})
                elif self.path == '/v1/devices/lamp/functions':
                    self.respond({'control_allowed': outer.events['control_allowed'], 'functions': [
                        {'code': 'switch', 'type': 'Boolean', 'values': '{}'}]})
                else:
                    self.send_error(404)

        self.http = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(str(self.cert), str(self.key))
        self.http.socket = context.wrap_socket(self.http.socket, server_side=True)
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        with socket.socket() as port:
            port.bind(('127.0.0.1', 0))
            self.mqtt_port = port.getsockname()[1]
        broker_config = self.base / 'mosquitto.conf'
        broker_config.write_text(f'user {pwd.getpwuid(os.geteuid()).pw_name}\nlistener {self.mqtt_port} 127.0.0.1\n'
            f'allow_anonymous true\ncafile {self.cert}\ncertfile {self.cert}\nkeyfile {self.key}\n')
        self.log = (self.base / 'fixture.log').open('wb')
        self.broker = subprocess.Popen(['mosquitto', '-c', str(broker_config)], stdout=self.log, stderr=self.log)
        for name, value in [('secret', self.secret), ('token', self.token)]:
            path = self.base / name
            path.write_text(value)
            path.chmod(0o600)
        state = self.base / 'state'
        state.mkdir(mode=0o700)
        endpoint = f'https://localhost:{self.http.server_port}'
        mqtt = {'host': 'localhost', 'port': self.mqtt_port, 'tls': True, 'ca_file': str(self.cert)}
        config = {'state_directory': str(state), 'https_ca_file': str(self.cert),
                  'cloud': {'device_id': 'unit-s4', 'pairing_url': endpoint + '/api/device/provision',
                            'secret_file': str(self.base / 'secret'), 'mqtt': mqtt},
                  'home': {'endpoint': endpoint, 'token_file': str(self.base / 'token')},
                  'sensor_mqtt': {**mqtt, 'client_id': 'fixture-ingress', 'username': 'sensor-user',
                     'password_file': str(self.base / 'secret'), 'sources': [{'device_id': 'sensor-1',
                     'topic': 'devices/sensor-1/telemetry', 'points': {
                     'temperature': {'type': 'temperature', 'unit': '°C', 'name': '环境温度', 'scale': .001}}}]}}
        self.config.write_text(json.dumps(config))
        self.config.chmod(0o600)
        self.service = subprocess.Popen([str(BINARY), '--socket', str(self.sock), '--config', str(self.config)], stdout=self.log, stderr=self.log)
        eventually(lambda: self.sock.exists() or self.service.poll() is not None)
        self.assertIsNone(self.service.poll(), (self.base / 'fixture.log').read_text())
        self.assertTrue(rpc(self.sock)['ok'])

    def tearDown(self):
        for process in [self.service, self.broker]:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                # Only these fixture processes; no board or application services.
                process.kill()
                process.wait(timeout=3)
        self.http.shutdown()
        self.http.server_close()
        self.thread.join(timeout=2)
        self.log.close()
        self.temp.cleanup()

    def state(self):
        return rpc(self.sock)['result']

    def loaded_home(self):
        self.assertTrue(rpc(self.sock, 'home.refresh')['ok'])
        eventually(lambda: (lambda h: h['fresh'] and not h['busy'])(self.state()['home']))
        self.assertTrue(rpc(self.sock, 'home.functions', {'device_id': 'lamp'})['ok'])
        eventually(lambda: (lambda h: h['functions'].get('lamp') and not h['busy'])(self.state()['home']))

    def command(self, command_id='a' * 32):
        return rpc(self.sock, 'home.command', {'device_id': 'lamp', 'code': 'switch', 'value': True,
            'command_id': command_id, 'revision': self.state()['home']['revision'], 'confirmed': True})

    def test_pairing_tls_identity_and_private_projection(self):
        self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
        value = eventually(lambda: self.state()['cloud']['pairing'] if self.state()['cloud']['pairing']['fresh'] else None)
        self.assertEqual(value['ownership'], 'unbound')
        self.assertEqual(value['code'], '482719')
        self.assertEqual(self.received_secret, self.secret)
        self.assertLessEqual(value['expires_in'], 300)
        serialized = json.dumps(self.state())
        self.assertNotIn(self.secret, serialized)
        self.assertNotIn(self.token, serialized)
        self.events['bound'] = True
        self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
        eventually(lambda: self.state()['cloud']['pairing']['ownership'] == 'bound')
        self.assertEqual(self.state()['cloud']['pairing']['code'], '')

    def test_invalid_pairing_ttl_does_not_create_a_code(self):
        self.events['bad_ttl'] = True
        self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
        eventually(lambda: self.state()['cloud']['pairing_error'])
        self.assertEqual(self.state()['cloud']['pairing']['ownership'], 'unknown')
        self.assertEqual(self.state()['cloud']['pairing']['code'], '')

    def test_pairing_natural_expiry_same_code_cannot_extend_or_revive(self):
        # The private server issues a real three-second token; production clocks are unchanged.
        self.events['pairing_ttl'] = 3

        def refresh():
            self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
            eventually(lambda: not self.state()['cloud']['busy'])
            self.assertFalse(self.state()['cloud']['pairing_error'])
            return self.state()['cloud']['pairing']

        started = time.monotonic()
        first = refresh()
        self.assertEqual(first['code'], '482719')
        self.assertGreater(first['expires_in'], 0)
        # A later server reply claiming a longer TTL cannot extend an existing token.
        self.events['pairing_ttl'] = 300
        repeated = refresh()
        self.assertEqual(repeated['code'], first['code'])
        self.assertLessEqual(repeated['expires_in'], first['expires_in'])
        eventually(lambda: self.state()['cloud']['pairing']['code'] == '', timeout=5)
        hidden_elapsed = time.monotonic() - started
        self.assertEqual(self.state()['cloud']['pairing']['expires_in'], 0)
        # Display uses whole remaining seconds; also wait beyond the actual original deadline.
        eventually(lambda: time.monotonic() - started >= 3.2, timeout=4)
        expired = refresh()
        self.assertEqual(expired['code'], '')
        self.assertEqual(expired['expires_in'], 0)
        self.events['pairing_code'] = '593820'
        renewed = refresh()
        self.assertEqual(renewed['code'], '593820')
        self.assertGreater(renewed['expires_in'], 290)
        self.events['bound'] = True
        bound = refresh()
        self.assertEqual(bound['ownership'], 'bound')
        self.assertEqual(bound['code'], '')
        self.assertEqual(bound['expires_in'], 0)
        print('PAIRING_TTL3 hidden_after=%.6fs; same_code_no_extension_or_revival=true; new_code_and_bound=true'
              % hidden_elapsed, flush=True)

    def test_pairing_does_not_persist_code_or_log_registration_secrets(self):
        state_dir = self.base / 'state'
        before = {str(p.relative_to(state_dir)): p.read_bytes()
                  for p in state_dir.rglob('*') if p.is_file()}
        self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
        eventually(lambda: not self.state()['cloud']['busy'])
        self.assertEqual(self.state()['cloud']['pairing']['code'], '482719')
        self.assertEqual(self.received_secret, self.secret)
        self.assertNotIn(self.secret, json.dumps(self.state()))
        self.assertNotIn(self.token, json.dumps(self.state()))
        after = {str(p.relative_to(state_dir)): p.read_bytes()
                 for p in state_dir.rglob('*') if p.is_file()}
        self.assertEqual(after, before)
        self.log.flush()
        log = (self.base / 'fixture.log').read_text()
        for sensitive in [self.secret, self.token, '482719',
                          urllib.parse.quote(self.secret, safe='')]:
            self.assertNotIn(sensitive, log)
        print('PAIRING_PRIVACY fixture_log_no_code_or_credentials=true; state_bytes_unchanged=true', flush=True)

    def test_home_accepted_is_not_device_confirmation_and_no_duplicate_control(self):
        self.loaded_home()
        self.assertTrue(self.command()['ok'])
        eventually(lambda: self.state()['home']['pending'].get('lamp/switch', {}).get('state') == 'awaiting_confirmation')
        self.assertFalse(self.state()['home']['devices'][0]['status'][0]['value'])
        self.assertFalse(self.command()['ok'])
        self.assertFalse(self.command('b' * 32)['ok'])
        self.assertEqual(self.events['posts'], 1)
        self.assertEqual(self.key_received, 'a' * 32)
        self.events['switch'] = True
        eventually(lambda: not self.state()['home']['busy'])
        self.assertTrue(rpc(self.sock, 'home.refresh')['ok'])
        eventually(lambda: not self.state()['home']['pending'])
        self.assertTrue(self.state()['home']['devices'][0]['status'][0]['value'])

    def test_home_old_revision_is_rejected_before_http_control(self):
        self.loaded_home()
        revision = self.state()['home']['revision']
        self.assertTrue(rpc(self.sock, 'home.refresh')['ok'])
        eventually(lambda: self.state()['home']['revision'] > revision)
        params = {'device_id': 'lamp', 'code': 'switch', 'value': True,
                  'command_id': 'e' * 32, 'revision': revision, 'confirmed': True}
        self.assertFalse(rpc(self.sock, 'home.command', params)['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        self.assertFalse(self.state()['home']['devices'][0]['status'][0]['value'])
        # A rejected old revision must not consume the intent ID.
        params['revision'] = self.state()['home']['revision']
        self.assertTrue(rpc(self.sock, 'home.command', params)['ok'])
        eventually(lambda: self.events['posts'] == 1)
        self.assertEqual(self.key_received, 'e' * 32)

    def test_home_offline_and_readonly_are_rejected_before_http_control(self):
        self.events['online'] = False
        self.loaded_home()
        self.assertFalse(self.command()['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        self.events['online'] = True
        self.events['control_allowed'] = False
        self.loaded_home()
        eventually(lambda: not self.state()['home']['busy'])
        self.assertFalse(self.command()['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        self.events['control_allowed'] = True
        self.loaded_home()
        eventually(lambda: not self.state()['home']['busy'])
        self.assertTrue(self.command()['ok'])
        eventually(lambda: self.events['posts'] == 1)

    def test_home_expired_authorization_is_rejected_before_http_control(self):
        self.loaded_home()
        self.events['authorization_expired'] = True
        self.assertTrue(rpc(self.sock, 'home.refresh')['ok'])
        eventually(lambda: self.state()['home']['authorization'] == 'expired')
        self.assertFalse(self.state()['home']['fresh'])
        self.assertFalse(self.command()['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        self.events['authorization_expired'] = False
        self.loaded_home()
        self.assertTrue(self.command()['ok'])
        eventually(lambda: self.events['posts'] == 1)

    def test_home_natural_freshness_and_function_expiry_require_refresh(self):
        self.loaded_home()
        started = time.monotonic()
        self.assertTrue(self.state()['home']['fresh'])
        # Wait for the production 30-second monotonic TTL; no fake clock or sleep override.
        eventually(lambda: not self.state()['home']['fresh'], timeout=35)
        elapsed = time.monotonic() - started
        self.assertGreaterEqual(elapsed, 29.5)
        self.assertLess(elapsed, 35)
        self.assertFalse(self.command()['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        # Function metadata arrives after the device list. Wait beyond its own TTL too.
        eventually(lambda: time.monotonic() - started >= 30.1, timeout=2)
        # Refreshing devices alone must not revive the old function authorization.
        old_revision = self.state()['home']['revision']
        self.assertTrue(rpc(self.sock, 'home.refresh')['ok'])
        eventually(lambda: (lambda h: h['fresh'] and not h['busy'] and
                              h['revision'] > old_revision)(self.state()['home']))
        self.assertFalse(self.command()['ok'])
        self.assertEqual(self.events['posts'], 0)
        self.assertFalse(self.state()['home']['pending'])
        self.assertTrue(rpc(self.sock, 'home.functions', {'device_id': 'lamp'})['ok'])
        eventually(lambda: not self.state()['home']['busy'])
        self.assertTrue(self.command()['ok'])
        eventually(lambda: self.events['posts'] == 1)
        self.assertEqual(self.key_received, 'a' * 32)
        print('HOME_NATURAL_EXPIRY_SECONDS=%.6f; blocked_POSTs=0; recovered_POSTs=1' % elapsed,
              flush=True)

    def test_lost_http_control_response_is_not_replayed(self):
        self.loaded_home()
        self.events['drop'] = True
        self.assertTrue(self.command()['ok'])
        eventually(lambda: self.state()['home']['pending'].get('lamp/switch', {}).get('state') == 'result_unknown')
        time.sleep(.3)
        self.assertEqual(self.events['posts'], 1)
        self.assertFalse(self.command('c' * 32)['ok'])

    def test_real_tls_mqtt_ingress_normalization_and_automatic_reporting(self):
        eventually(lambda: self.state()['cloud']['mqtt'] == 'connected')
        eventually(lambda: self.state()['reporting']['sensor_mqtt'] == 'connected')
        sub = subprocess.Popen(['mosquitto_sub', '-h', 'localhost', '-p', str(self.mqtt_port), '--cafile', str(self.cert),
                                '-t', '100ask/device/unit-s4/sensor', '-C', '1', '-W', '8'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            message = {'schema_version': 1, 'device_id': 'sensor-1', 'sequence': 1, 'quality': 'good',
                       'timestamp': datetime.datetime.now(datetime.timezone.utc).isoformat().replace('+00:00', 'Z'),
                       'values': {'temperature': 24300}}
            subprocess.run(['mosquitto_pub', '-h', 'localhost', '-p', str(self.mqtt_port), '--cafile', str(self.cert),
                            '-t', 'devices/sensor-1/telemetry', '-q', '1', '-m', json.dumps(message)], check=True, timeout=5)
            eventually(lambda: self.state()['reporting']['points'])
            self.assertTrue(rpc(self.sock, 'reporting.configure', {'enabled': True, 'period_seconds': 30})['ok'])
            output, error = sub.communicate(timeout=10)
            self.assertEqual(sub.returncode, 0, error.decode())
            payload = json.loads(output)
            self.assertEqual(payload, {'data': [{'type': 'temperature', 'value': 24.3, 'unit': '°C'}]})
            eventually(lambda: self.state()['reporting']['last_broker_ack'])
            stored = json.loads((self.base / 'state/reporting.json').read_text())
            self.assertEqual(stored, {'enabled': True, 'period_seconds': 30})
        finally:
            if sub.poll() is None:
                sub.terminate()
                sub.wait(timeout=3)

    def publish_sensor_fixture(self, sequence=1, value=24300, age_seconds=0, quality='good'):
        message = {'schema_version': 1, 'device_id': 'sensor-1', 'sequence': sequence,
                   'quality': quality, 'timestamp': (datetime.datetime.now(datetime.timezone.utc)
                   - datetime.timedelta(seconds=age_seconds)).isoformat().replace('+00:00', 'Z'),
                   'values': {'temperature': value}}
        subprocess.run(['mosquitto_pub', '-h', 'localhost', '-p', str(self.mqtt_port),
                        '--cafile', str(self.cert), '-t', 'devices/sensor-1/telemetry',
                        '-q', '1', '-m', json.dumps(message)], check=True, timeout=5)

    def test_reporting_natural_expiry_rejects_empty_payload_and_recovers(self):
        eventually(lambda: self.state()['cloud']['mqtt'] == 'connected')
        eventually(lambda: self.state()['reporting']['sensor_mqtt'] == 'connected')
        empty = rpc(self.sock, 'reporting.send')
        self.assertFalse(empty['ok'])
        self.assertEqual(empty['error']['code'], 'no-data')
        self.publish_sensor_fixture(age_seconds=55)
        eventually(lambda: self.state()['reporting']['points'])
        self.assertEqual(self.state()['reporting']['points'][0]['quality'], 'good')
        start = time.monotonic()
        eventually(lambda: self.state()['reporting']['points'][0]['quality'] == 'stale', timeout=8)
        expiry_elapsed = time.monotonic() - start
        stale = self.state()['reporting']
        self.assertIsNone(stale['points'][0]['value'])
        self.assertIsNone(stale['last_broker_ack'])
        refused = rpc(self.sock, 'reporting.send')
        self.assertFalse(refused['ok'])
        self.assertEqual(refused['error']['code'], 'no-data')
        self.assertFalse(self.state()['reporting']['inflight'])
        self.publish_sensor_fixture(sequence=2, value=24500)
        eventually(lambda: self.state()['reporting']['points'][0]['quality'] == 'good')
        self.assertAlmostEqual(self.state()['reporting']['points'][0]['value'], 24.5)
        self.assertTrue(rpc(self.sock, 'reporting.send')['ok'])
        eventually(lambda: self.state()['reporting']['last_broker_ack'])
        print('REPORT_EXPIRY natural_seconds=%.6f old_timestamp_age=55 recovery_puback=true'
              % expiry_elapsed, flush=True)

    def test_reporting_disabled_still_collects_and_invalid_settings_preserve_disk(self):
        eventually(lambda: self.state()['cloud']['mqtt'] == 'connected')
        eventually(lambda: self.state()['reporting']['sensor_mqtt'] == 'connected')
        config = {'enabled': False, 'period_seconds': 60}
        self.assertTrue(rpc(self.sock, 'reporting.configure', config)['ok'])
        stored = self.base / 'state/reporting.json'
        before = stored.read_bytes()
        for invalid in [{'enabled': 'true', 'period_seconds': 60},
                        {'enabled': True, 'period_seconds': 31},
                        {'enabled': True, 'period_seconds': 30, 'unexpected': 1}]:
            self.assertFalse(rpc(self.sock, 'reporting.configure', invalid)['ok'])
            self.assertEqual(stored.read_bytes(), before)
            self.assertFalse(self.state()['reporting']['enabled'])
            self.assertEqual(self.state()['reporting']['period_seconds'], 60)
        self.publish_sensor_fixture()
        eventually(lambda: self.state()['reporting']['points'])
        original = self.state()['reporting']['points'][0]
        self.publish_sensor_fixture(sequence=1, value=99000)
        eventually(lambda: self.state()['reporting'].get('ingress_error') == 'invalid_sensor_message')
        self.assertEqual(self.state()['reporting']['points'][0], original)
        self.publish_sensor_fixture(sequence=2, value=24400)
        # Fixed-point scale conversion is represented as IEEE-754, not decimal.
        eventually(lambda: abs(self.state()['reporting']['points'][0]['value'] - 24.4) < 1e-9)
        self.assertAlmostEqual(self.state()['reporting']['points'][0]['value'], 24.4)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            current = self.state()['reporting']
            self.assertFalse(current['enabled'])
            self.assertFalse(current['inflight'])
            self.assertIsNone(current['last_broker_ack'])
            time.sleep(.1)
        self.assertEqual(json.loads(stored.read_text()), config)
        self.assertEqual(stored.read_bytes(), before)

    def test_reporting_configuration_survives_fixture_process_restart(self):
        # Restart only the owned Ubuntu fixture, preserving its private state directory.
        for enabled, period in [(False, 60), (True, 300)]:
            with self.subTest(enabled=enabled, period=period):
                settings = {'enabled': enabled, 'period_seconds': period}
                self.assertTrue(rpc(self.sock, 'reporting.configure', settings)['ok'])
                path = self.base / 'state/reporting.json'
                before = path.read_bytes()
                self.assertEqual(path.stat().st_mode & 0o777, 0o600)
                self.service.terminate()
                self.service.wait(timeout=3)
                self.service = subprocess.Popen(
                    [str(BINARY), '--socket', str(self.sock), '--config', str(self.config)],
                    stdout=self.log, stderr=self.log)

                def restarted():
                    self.assertIsNone(self.service.poll(), (self.base / 'fixture.log').read_text())
                    try:
                        return self.state()
                    except (OSError, AssertionError):
                        return None

                current = eventually(restarted)['reporting']
                self.assertTrue(current['persistent'])
                self.assertEqual(current['enabled'], enabled)
                self.assertEqual(current['period_seconds'], period)
                self.assertEqual(path.read_bytes(), before)
                eventually(lambda: self.state()['cloud']['mqtt'] == 'connected')
                current = self.state()['reporting']
                self.assertEqual(current['points'], [])
                self.assertFalse(current['inflight'])
                self.assertIsNone(current['last_broker_ack'])
                response = rpc(self.sock, 'reporting.send')
                self.assertFalse(response['ok'])
                self.assertEqual(response['error']['code'], 'no-data')
        print('REPORT_RESTART settings=false/60,true/300 preserved; mode=0600; no stale points/ACK', flush=True)

    def test_reporting_fresh_sample_expires_after_full_natural_minute(self):
        eventually(lambda: self.state()['cloud']['mqtt'] == 'connected')
        eventually(lambda: self.state()['reporting']['sensor_mqtt'] == 'connected')
        started = time.monotonic()
        self.publish_sensor_fixture()
        eventually(lambda: self.state()['reporting']['points'])
        self.assertEqual(self.state()['reporting']['points'][0]['quality'], 'good')
        # Observe the actual production TTL; no altered timestamps or shortened timer.
        deadline = started + 67
        while True:
            current = self.state()['reporting']
            elapsed = time.monotonic() - started
            sample = current['points'][0]
            self.assertFalse(current['enabled'])
            self.assertFalse(current['inflight'])
            self.assertIsNone(current['last_broker_ack'])
            if sample['quality'] == 'stale':
                self.assertGreaterEqual(elapsed, 59)
                self.assertIsNone(sample['value'])
                break
            self.assertEqual(sample['quality'], 'good')
            self.assertAlmostEqual(sample['value'], 24.3)
            self.assertLess(time.monotonic(), deadline, 'Fresh sample did not expire')
            time.sleep(.2)
        refused = rpc(self.sock, 'reporting.send')
        self.assertFalse(refused['ok'])
        self.assertEqual(refused['error']['code'], 'no-data')
        self.publish_sensor_fixture(sequence=2, value=24500)
        eventually(lambda: self.state()['reporting']['points'][0]['quality'] == 'good')
        self.assertAlmostEqual(self.state()['reporting']['points'][0]['value'], 24.5)
        self.assertTrue(rpc(self.sock, 'reporting.send')['ok'])
        eventually(lambda: self.state()['reporting']['last_broker_ack'])
        print('REPORT_FULL_NATURAL_EXPIRY_SECONDS=%.6f; stale_send=no-data; recovery_puback=true'
              % elapsed, flush=True)

    def test_ipc_envelope_rejects_invalid_protocol_fields_and_oversized_control(self):
        self.loaded_home()
        params = {'device_id': 'lamp', 'code': 'switch', 'value': True,
                  'command_id': 'd' * 32, 'revision': self.state()['home']['revision'], 'confirmed': True}
        valid = {'protocol': 1, 'method': 'home.command', 'params': params}

        def exchange(payload):
            with socket.socket(socket.AF_UNIX) as stream:
                stream.settimeout(3)
                stream.connect(str(self.sock))
                stream.sendall(payload)
                result = b''
                while b'\n' not in result:
                    try:
                        chunk = stream.recv(65536)
                    except ConnectionResetError:
                        break
                    if not chunk:
                        break
                    result += chunk
                return result

        invalid = [dict(valid, protocol=2), dict(valid, extra=True),
                   dict(valid, params=dict(params, extra=True))]
        for envelope in invalid:
            with self.subTest(envelope=envelope):
                result = json.loads(exchange(json.dumps(envelope).encode() + b'\n'))
                self.assertFalse(result['ok'])
        duplicate = json.dumps(valid).encode().replace(b'"protocol": 1', b'"protocol": 1, "protocol": 1')
        self.assertFalse(json.loads(exchange(duplicate + b'\n'))['ok'])
        oversized = dict(valid, params=dict(params, padding='x' * 66000))
        self.assertEqual(exchange(json.dumps(oversized).encode() + b'\n'), b'')
        self.assertEqual(self.events['posts'], 0)
        self.assertTrue(rpc(self.sock)['ok'])  # A rejected client must not kill the service.
        self.assertEqual(self.events['posts'], 0)

    def test_unknown_methods_and_voice_unready_are_refused(self):
        self.assertFalse(rpc(self.sock, 'shell.run', {'command': 'true'})['ok'])
        self.assertFalse(rpc(self.sock, 'voice.command', {'command': 'start_listen'})['ok'])
        self.assertFalse(rpc(self.sock, 'home.command', {})['ok'])

    @unittest.skipUnless(os.geteuid() == 0, 'Kernel peer role test requires root fixture identities')
    def test_kernel_peer_credentials_separate_hmi_web_and_unknown_user(self):
        self.assertTrue(rpc(self.sock, 'cloud.refresh')['ok'])
        eventually(lambda: self.state()['cloud']['pairing']['fresh'])
        self.base.chmod(0o755)

        def caller(uid, method):
            read_fd, write_fd = os.pipe()
            pid = os.fork()
            if pid == 0:
                try:
                    os.close(read_fd)
                    os.setgroups([400])
                    os.setgid(400)
                    os.setuid(uid)
                    value = rpc(self.sock, method)
                    os.write(write_fd, json.dumps(value).encode())
                finally:
                    os._exit(0)
            os.close(write_fd)
            body = b''
            while True:
                part = os.read(read_fd, 65536)
                if not part:
                    break
                body += part
            os.close(read_fd)
            os.waitpid(pid, 0)
            return json.loads(body)

        web = caller(400, 'desktop.snapshot')
        self.assertTrue(web['ok'])
        self.assertNotIn('code', web['result']['cloud']['pairing'])
        self.assertNotIn('history', web['result']['voice'])
        self.assertEqual(caller(400, 'cloud.refresh')['error']['code'], 'method-denied')
        self.assertEqual(caller(401, 'desktop.snapshot')['result']['cloud']['pairing']['code'], '482719')
        self.assertEqual(caller(65534, 'desktop.snapshot')['error']['code'], 'peer-denied')


@unittest.skipUnless(os.geteuid() == 0 and BINARY.exists(),
                     'Non-root provider peer-role regression needs a root fixture')
class NonRootDesktopHealthRoleTests(unittest.TestCase):
    def test_root_health_remains_read_only_after_provider_drops_root(self):
        # A root-owned provider masks a missing root health ACL, because UID0 is
        # also the provider owner. Exercise a genuinely different kernel UID.
        with tempfile.TemporaryDirectory(prefix='dshan-nonroot-health-') as folder:
            base = Path(folder)
            base.chmod(0o755)
            run = base / 'ipc'
            run.mkdir(mode=0o750)
            os.chown(run, 65534, 400)
            config = base / 'private.json'
            config.write_text('{}\n')
            config.chmod(0o600)
            os.chown(config, 65534, 400)
            path = run / 'desktop.sock'

            def drop_to_provider():
                os.setgroups([400])
                os.setgid(400)
                os.setuid(65534)

            with (base / 'helper.log').open('wb') as log:
                provider = subprocess.Popen([str(BINARY), '--socket', str(path),
                    '--config', str(config)], stdout=log, stderr=log,
                    preexec_fn=drop_to_provider)
                try:
                    eventually(lambda: path.exists() or provider.poll() is not None)
                    self.assertIsNone(provider.poll(), (base / 'helper.log').read_text())
                    with socket.socket(socket.AF_UNIX) as peer:
                        import struct
                        peer.connect(str(path))
                        pid, uid, gid = struct.unpack('3i', peer.getsockopt(
                            socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
                        self.assertEqual((pid, uid, gid), (provider.pid, 65534, 400))
                    self.assertTrue(rpc(path)['ok'])
                    mutation = rpc(path, 'reporting.configure',
                                   {'enabled': True, 'period_seconds': 60})
                    self.assertFalse(mutation['ok'])
                    self.assertEqual(mutation['error']['code'], 'method-denied')
                    self.assertTrue(rpc(path)['ok'])
                    self.assertEqual(config.read_text(), '{}\n')
                    self.assertFalse((base / 'reporting.json').exists())
                finally:
                    if provider.poll() is None:
                        provider.terminate()
                    try:
                        provider.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        provider.kill()
                        provider.wait(timeout=3)


class MediaProtocolTests(unittest.TestCase):
    def test_media_worker_sigterm_closes_protocol_without_default_signal_death(self):
        binary = ROOT / 'dist/media-native/dshancontrol-media-worker'
        if not binary.exists():
            self.skipTest('Run make build-desktop first')
        child = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            import select
            self.assertTrue(select.select([child.stdout], [], [], 3)[0])
            initial = json.loads(child.stdout.readline())
            self.assertEqual(initial['state'], 'idle')
            child.terminate()
            self.assertEqual(child.wait(timeout=3), 0)
        finally:
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=3)

    def test_host_media_worker_never_claims_hardware_decode(self):
        binary = ROOT / 'dist/media-native/dshancontrol-media-worker'
        if not binary.exists():
            self.skipTest('Run make build-desktop first')
        requests = [{'protocol': 1, 'request_id': 'test', 'command': 'open',
                     'params': {'path': '/not-a-real-video', 'volume': 60}},
                    {'protocol': 1, 'request_id': 'close', 'command': 'close', 'params': {}}]
        result = subprocess.run([str(binary)], input=''.join(json.dumps(item) + '\n' for item in requests),
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        messages = [json.loads(line) for line in result.stdout.splitlines()]
        status = [item for item in messages if item['event'] == 'status']
        self.assertTrue(status)
        self.assertTrue(all(not item['available'] and not item['hardware_decode_confirmed'] for item in status))
        self.assertTrue(any(item['event'] == 'reply' and item.get('accepted') is False for item in messages))


if __name__ == '__main__':
    unittest.main()
