"""Black-box native HTTP/data compatibility; Python is only the host test driver."""
import concurrent.futures
import hashlib
import http.client
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('DSHANCONTROL_API_TEST_BINARY', ROOT / 'dist/api-native/dshancontrol-api'))
sys.path.insert(0, str(ROOT / 'components/core/src'))
from omnigate_core.store import EdgeStore


@unittest.skipUnless(BINARY.exists(), 'Native API build required')
class NativeApiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.env = dict(os.environ)
        for key, name in {'OMNIGATE_EDGE_DB': 'edge.db', 'OMNIGATE_HMI_DB': 'hmi.db',
                          'OMNIGATE_HMI_SETTINGS': 'hmi.json', 'OMNIGATE_WEB_CONFIG': 'config.json',
                          'OMNIGATE_EDS_DIR': 'eds'}.items():
            self.env[key] = str(self.root / name)
        self.env['OMNIGATE_PLATFORM_MANIFEST'] = str(ROOT / 'boards/imx6ull-pro/platform.json')
        self.env['OMNIGATE_WEB_ASSETS'] = str(ROOT / 'components/web/assets')
        self.env['DSHANCONTROL_APPLICATION_COMMIT'] = 'b' * 40
        self.env['DSHANCONTROL_NATIVE_PLATFORM_SOCKET'] = str(self.root / 'missing.sock')
        self.password = 'Native-fixture-pass-123'
        self.config = {'auth': {'username': 'admin', 'salt': 'fixture-salt', 'role': 'administrator',
            'password_hash': hashlib.pbkdf2_hmac('sha256', self.password.encode(), b'fixture-salt', 120000).hex()}}
        self.write_config()
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0)); self.port = s.getsockname()[1]
        self.env['OMNIGATE_LISTEN_PORT'] = str(self.port)
        self.cookie = ''; self.csrf = ''
        self.process = None
        self.addCleanup(self.stop)
        self.start()

    def write_config(self):
        (self.root / 'config.json').write_text(json.dumps(self.config))

    def start(self):
        self.process = subprocess.Popen([str(BINARY)], env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for _ in range(100):
            if self.process.poll() is not None:
                out, err = self.process.communicate()
                self.fail(f'API exited: {out!r} {err!r}')
            try:
                if self.request('GET', '/api/health')[0] == 200: return
            except OSError: pass
            time.sleep(.03)
        self.fail('API startup timeout')

    def stop(self):
        if self.process:
            self.process.terminate()
            try: self.process.communicate(timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.communicate(); raise
            self.process = None

    def request(self, method, path, body=None, headers=None):
        h = {'Cookie': self.cookie, 'X-CSRF-Token': self.csrf}
        if body is not None:
            h['Content-Type'] = 'application/json'
            if not isinstance(body, bytes): body = json.dumps(body).encode()
        h.update(headers or {})
        c = http.client.HTTPConnection('127.0.0.1', self.port, timeout=15)
        try:
            c.request(method, path, body, h); r = c.getresponse(); raw = r.read()
            return r.status, json.loads(raw) if raw and r.getheader('Content-Type', '').startswith('application/json') else raw, dict(r.getheaders())
        finally: c.close()

    def login(self):
        code, value, headers = self.request('POST', '/api/login', {'username': 'admin', 'password': self.password})
        self.assertEqual(code, 200, value)
        self.cookie = headers['Set-Cookie'].split(';')[0]; self.csrf = value['csrf']

    def test_health_snapshot_and_unknown_values(self):
        code, value, _ = self.request('GET', '/api/health')
        self.assertEqual(code, 200); self.assertEqual(value['backend'], 'native-cpp')
        self.assertEqual(value['application_commit'], 'b' * 40)
        code, value, _ = self.request('GET', '/api/hmi/v1/snapshot')
        self.assertEqual(code, 200, value)
        self.assertIsNone(value['system']['cpu'])
        self.assertIsNone(value['system']['active_slot'])
        self.assertIsNone(value['summary']['online_devices'])
        self.assertEqual(value['sensors'], [])
        self.assertTrue(all(not b['online'] for b in value['buses'] if b['name'] in ('RS485', 'EtherCAT')))
        code, history, _ = self.request('GET', '/api/hmi/v1/history?metric=cpu')
        self.assertEqual(code, 200); self.assertTrue(history['points'])
        self.assertEqual(self.request('GET', '/api/hmi/v1/history?metric=cpu;DROP')[0], 400)

    def test_old_sqlite_data_roundtrip_transaction_and_restart(self):
        self.stop()
        old = EdgeStore(self.root / 'edge.db')
        device = {'id': 'pump1', 'name': '泵', 'template_id': 'pump', 'channel': 'can0', 'address': '1', 'config': {'x': 1}}
        # Use an actual channel from the board contract.
        manifest = json.loads(Path(self.env['OMNIGATE_PLATFORM_MANIFEST']).read_text())
        device['channel'] = next(iter(manifest['channels']))
        old.put_device(device, 'fixture')
        self.start(); self.login()
        code, result, _ = self.request('GET', '/api/v1/devices/pump1')
        self.assertEqual(code, 200); self.assertEqual(result['device'], old.get_device('pump1'))
        code, result, _ = self.request('PUT', '/api/v1/devices/pump1', device)
        self.assertEqual((code, result['revision']), (200, 2))
        point = {'device_id': 'pump1', 'name': '温度', 'data_type': 'int16', 'access': 'r', 'address': '10', 'scale': .1}
        self.assertEqual(self.request('PUT', '/api/v1/points/t', point)[0], 200)
        self.assertEqual(old.get_point('t')['scale'], .1)
        before = old.audit()
        point['device_id'] = 'missing'
        self.assertEqual(self.request('PUT', '/api/v1/points/t', point)[0], 400)
        self.assertEqual(old.audit(), before); self.assertEqual(old.get_point('t')['device_id'], 'pump1')
        self.stop(); self.start(); self.login()
        self.assertEqual(self.request('GET', '/api/v1/points/t')[1]['point'], old.get_point('t'))
        self.assertEqual(self.request('DELETE', '/api/v1/devices/pump1')[0], 200)
        self.assertIsNone(old.get_point('t')); self.assertIsNone(old.get_device('pump1'))

    def test_month_history_keeps_existing_minute_samples(self):
        now=int(time.time())//60*60
        with sqlite3.connect(self.root/'hmi.db') as db:
            db.executemany('INSERT INTO telemetry(bucket,temperature) VALUES(?,?)',
                           [(now-i*60,20+i/1000) for i in range(11200)])
        code,value,_=self.request('GET','/api/hmi/v1/history?metric=temperature&hours=720')
        self.assertEqual(code,200,value)
        self.assertEqual(len(value['points']),11200)
        self.assertEqual(value['points'][-1],{'bucket':now,'value':20})

    def test_alarm_ack_atomic_idempotent_and_demo_visibility(self):
        with sqlite3.connect(self.root / 'hmi.db') as db:
            db.execute("INSERT INTO alarms(ts,severity,source,code,message) VALUES(?,?,?,?,?)", (int(time.time()), 'warning', 'real', 'TEST', '测试'))
        with concurrent.futures.ThreadPoolExecutor(4) as workers:
            results = list(workers.map(lambda _: self.request('POST', '/api/hmi/v1/alarms/1/ack', {}), range(8)))
        self.assertTrue(all(r[0] == 200 for r in results), results)
        with sqlite3.connect(self.root / 'hmi.db') as db:
            self.assertEqual(db.execute("SELECT COUNT(*) FROM audit WHERE action='alarm_ack'").fetchone()[0], 1)
        self.assertEqual(self.request('PUT', '/api/hmi/v1/settings', {'demo_mode': True})[0], 200)
        self.assertEqual(len(self.request('GET', '/api/hmi/v1/alarms')[1]['alarms']), 4)
        self.assertEqual(self.request('PUT', '/api/hmi/v1/settings', {'demo_mode': False})[0], 200)
        self.assertEqual(len(self.request('GET', '/api/hmi/v1/alarms')[1]['alarms']), 1)
        self.assertEqual(self.request('POST', '/api/hmi/v1/alarms/2/ack', {})[0], 404)

    def test_auth_csrf_viewer_password_and_restart(self):
        self.assertEqual(self.request('GET', '/api/v1/devices')[0], 401)
        self.login()
        self.assertEqual(self.request('POST', '/api/password', {'password': 'new-pass-long-123'}, {'X-CSRF-Token': 'bad'})[0], 403)
        self.config['auth']['role'] = 'viewer'; self.write_config(); self.login()
        self.assertEqual(self.request('GET', '/api/v1/devices')[0], 200)
        self.assertEqual(self.request('DELETE', '/api/v1/devices/any')[0], 403)
        self.config['auth']['role'] = 'administrator'; self.write_config(); self.login()
        self.assertEqual(self.request('POST', '/api/password', {'password': 'new-pass-long-123'})[0], 200)
        self.assertEqual(self.request('GET', '/api/v1/devices')[0], 401)
        self.password = 'new-pass-long-123'; self.login()
        self.stop(); self.start()
        self.assertEqual(self.request('GET', '/api/v1/devices')[0], 401)
        self.login()

    def test_strict_json_origin_and_host_before_actions(self):
        endpoint = '/api/hmi/v1/settings'
        for payload in [b'{"demo_mode":false,"demo_mode":true}', b'{"brightness":true}', b'{"refresh_ms":1}', b'{"unknown":1}']:
            with self.subTest(payload=payload): self.assertEqual(self.request('PUT', endpoint, payload)[0], 400)
        for header in [{'Host': 'attacker.example'}, {'Origin': 'http://attacker.example'}, {'Origin': 'http://localhost:1'}, {'Sec-Fetch-Site': 'cross-site'}]:
            self.assertEqual(self.request('PUT', endpoint, {'demo_mode': True}, header)[0], 403)
        self.assertFalse(self.request('GET', endpoint)[1]['settings']['demo_mode'])
        self.assertEqual(self.request('POST', '/api/hmi/v1/actions', {'action': 'brightness', 'params': {'value': 200}, 'confirmed': False})[0], 400)

    def test_raw_http_ambiguities_and_bounded_headers(self):
        for suffix in [b'Content-Length: 0\r\nContent-Length: 0\r\n\r\n', b'Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n',
                       b'Content-Length: 9999999\r\n\r\n', b'X-Large: ' + b'a' * 17000 + b'\r\n\r\n']:
            with socket.create_connection(('127.0.0.1', self.port), timeout=3) as s:
                s.sendall(b'POST /api/hmi/v1/settings HTTP/1.1\r\nHost: localhost\r\n' + suffix)
                status = s.recv(4096).split(b' ')[1]
                self.assertIn(status, (b'400', b'413', b'431'))
        self.assertEqual(self.request('GET', '/api/health')[0], 200)

    def test_unavailable_hardware_never_reports_success(self):
        code, result, _ = self.request('POST', '/api/hmi/v1/actions', {'action': 'brightness', 'params': {'value': 200}, 'confirmed': True})
        self.assertEqual(code, 400); self.assertFalse(result['ok'])
        self.assertEqual(self.request('GET', '/api/hmi/v1/audit')[1]['audit'][0]['result'], 'failed')

    def test_eds_upload_binding_and_invalid_replacement_preserves_dictionary(self):
        self.login()
        eds=b'[FileInfo]\nFileName=test.eds\n[DeviceInfo]\nVendorName=fixture\n[2000]\nParameterName=Controlword\nDataType=0x0006\n'
        def upload(data, name='test.eds'):
            raw=(b'--fixture-boundary\r\nContent-Disposition: form-data; name="file"; filename="'+name.encode()+
                 b'"\r\nContent-Type: application/octet-stream\r\n\r\n'+data+
                 b'\r\n--fixture-boundary\r\nContent-Disposition: form-data; name="node_id"\r\n\r\n1\r\n--fixture-boundary--\r\n')
            return self.request('POST','/api/canopen/eds',raw,{'Content-Type':'multipart/form-data; boundary=fixture-boundary'})
        self.assertEqual(upload(eds)[0],200)
        self.assertEqual((self.root/'eds/test.eds').read_bytes(),eds)
        result=self.request('GET','/api/canopen/eds')[1]
        self.assertEqual(result['files'],['test.eds'])
        self.assertEqual(result['nodes']['1']['eds'],str(self.root/'eds/test.eds'))
        for bad in [b'not an EDS',eds+b'DataType=0x0005\n']:
            self.assertEqual(upload(bad)[0],400)
            self.assertEqual((self.root/'eds/test.eds').read_bytes(),eds)
        self.assertEqual(upload(eds,'../escape.eds')[0],400)
        self.assertFalse((self.root/'escape.eds').exists())
        self.assertEqual(self.request('GET','/')[0],200)

if __name__ == '__main__': unittest.main()
