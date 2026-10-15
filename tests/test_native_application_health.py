"""Real native health CLI, real process ownership and read-only SQLite/API probes."""
import contextlib
import hashlib
import http.server
import json
import os
from pathlib import Path
import signal
import sqlite3
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('DSHANCONTROL_NATIVE_UPDATER',
    str(ROOT / 'dist/native-runtime/dshancontrol-runtime'))).resolve()


class NativeApplicationHealthTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.apps = self.root / 'apps'
        self.factory = self.apps / 'releases/factory'
        self.factory.mkdir(parents=True)
        (self.apps / 'current').symlink_to('releases/factory')
        self.commit = 'a' * 40
        (self.factory / 'installed.json').write_text(json.dumps(dict(commit=self.commit, data_schema=1)))
        self.data = self.root / 'data'
        self.data.mkdir()
        for name, statements in [('edge.db', ['CREATE TABLE metadata(key TEXT,value TEXT)',
            "INSERT INTO metadata VALUES('schema_version','1')", 'CREATE TABLE devices(id TEXT)',
            'CREATE TABLE points(id TEXT)', 'CREATE TABLE audit(id INT)']),
            ('hmi.db', ['CREATE TABLE alarms(id INT)', 'CREATE TABLE audit(id INT)',
                        'CREATE TABLE telemetry(bucket INT)'])]:
            with contextlib.closing(sqlite3.connect(str(self.data / name))) as db:
                for statement in statements: db.execute(statement)
                db.commit()
        self.pids = self.root / 'run'
        self.pids.mkdir()
        source = self.root / 'process.c'
        source.write_text('#include <unistd.h>\nint main(void){for(;;)pause();}\n')
        self.exe = self.root / 'application'
        process_fixture = os.environ.get('DSHANCONTROL_HEALTH_PROCESS')
        if process_fixture:
            self.exe.write_bytes(Path(process_fixture).read_bytes())
            self.exe.chmod(0o755)
        else:
            subprocess.run(['cc', '-Wall', '-Werror', str(source), '-o', str(self.exe)], check=True)
        self.children = {}
        for name in ['web', 'hmi', 'supervisor']:
            child = subprocess.Popen([str(self.exe), name])
            self.children[name] = child
            (self.pids / (name + '.pid')).write_text(str(child.pid) + '\n')
            (self.pids / (name + '.pid')).chmod(0o600)
        self.addCleanup(self.stop_children)
        test = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_GET(self):
                if test.delay: time.sleep(test.delay)
                body = test.response if isinstance(test.response, bytes) else json.dumps(test.response).encode()
                self.send_response(test.http_status)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try: self.wfile.write(body)
                except OSError: pass
        self.response = dict(ok=True, service='omnigate-web', application_commit=self.commit)
        self.delay = 0
        self.http_status = 200
        self.server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)
        self.contract = self.root / 'contract.json'
        self.contract.write_text(json.dumps(dict(application_root=str(self.apps),
            data_root=str(self.data), data_schema=1)))
        self.config = self.root / 'updater.json'
        self.settings = dict(application_root=str(self.apps), system_contract=str(self.contract),
            development_mode=True, application_health=dict(pid_root=str(self.pids),
            http_url='http://127.0.0.1:%d/api/health' % self.server.server_port,
            fixture_processes={n: [str(self.exe), n] for n in self.children}))
        self.save()

    def stop_children(self):
        for child in self.children.values():
            if child.poll() is None:
                child.send_signal(signal.SIGCONT)
                child.terminate()
                child.wait(timeout=5)

    def stop_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)

    def save(self): self.config.write_text(json.dumps(self.settings))

    def run_health(self, command='application-health'):
        return subprocess.run([str(BINARY), '--config', str(self.config), command],
            capture_output=True, text=True, timeout=8)

    def failure(self, code):
        r = self.run_health()
        self.assertEqual(1, r.returncode, r.stdout)
        self.assertEqual(code, json.loads(r.stderr)['code'], r.stderr)
        return r

    def test_owned_processes_backend_identity_and_database_schema_are_all_required(self):
        hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in self.data.iterdir()}
        r = self.run_health()
        self.assertEqual(0, r.returncode, r.stderr)
        value = json.loads(r.stdout)
        self.assertTrue(value['ok'])
        self.assertEqual(2, value['databases'])
        self.assertEqual(self.children['hmi'].pid, value['processes']['hmi'])
        self.assertFalse(value['hmi_functional_heartbeat'])
        self.assertEqual(hashes, {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in self.data.iterdir()})
        r = self.run_health('application-commit')
        self.assertEqual(0, r.returncode)
        self.assertEqual(self.commit, r.stdout.strip())

    def test_foreign_pid_and_same_executable_different_invocation_are_rejected(self):
        pid = self.pids / 'hmi.pid'
        pid.write_text(str(os.getpid()))
        self.failure('health-process')
        pid.write_text(str(self.children['web'].pid))
        self.failure('health-process')
        self.assertIsNone(self.children['web'].poll())

    def test_linked_or_writable_pid_file_is_not_trusted(self):
        pid = self.pids / 'web.pid'
        pid.chmod(0o666)
        self.failure('health-process')
        pid.chmod(0o600)
        target = self.root / 'external.pid'
        pid.rename(target)
        pid.symlink_to(target)
        self.failure('health-process')
        self.assertIsNone(self.children['web'].poll())

    def test_wrong_backend_commit_or_http_error_never_reports_healthy_or_echoes_body(self):
        self.response['application_commit'] = 'b' * 40
        self.response['private'] = 'must-never-appear-in-error'
        r = self.failure('health-web-identity')
        self.assertNotIn('must-never-appear', r.stderr + r.stdout)
        self.http_status = 503
        self.failure('health-web-http')

    def test_redirect_duplicate_json_and_invalid_optional_policy_are_rejected(self):
        self.http_status = 302
        self.failure('health-web-http')
        self.http_status = 200
        self.response = b'{"private-diagnostic-value":1,"private-diagnostic-value":2}'
        r = self.failure('health-web-identity')
        self.assertNotIn('private-diagnostic-value', r.stdout + r.stderr)
        self.response = dict(ok=True, service='omnigate-web', application_commit=self.commit)
        for value in [None, 'false', 1, {}, []]:
            self.settings['application_health']['require_hmi_heartbeat'] = value
            self.save()
            self.failure('health-contract')

    def test_web_timeout_and_oversized_response_are_bounded(self):
        self.delay = 4
        started = time.monotonic()
        self.failure('health-web-timeout')
        self.assertLess(time.monotonic() - started, 5)
        self.delay = 0
        self.response = b'x' * 100000
        self.failure('health-web-size')

    def test_corrupt_or_empty_database_fails_without_repairing_it(self):
        path = self.data / 'hmi.db'
        path.write_bytes(b'preserve corrupt input')
        self.failure('health-database')
        self.assertEqual(b'preserve corrupt input', path.read_bytes())
        path.unlink()
        with contextlib.closing(sqlite3.connect(str(path))) as db: db.execute('CREATE TABLE wrong(id INT)')
        self.failure('health-database-schema')

    def test_frozen_hmi_process_needs_fresh_matching_event_loop_heartbeat(self):
        path = self.root / 'heartbeat.json'
        self.settings['application_health'].update(require_hmi_heartbeat=True, hmi_heartbeat=str(path))
        self.save()
        self.failure('health-hmi-heartbeat')
        value = dict(schema_version=1, pid=self.children['hmi'].pid, commit=self.commit,
            boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            gui_event_loop=True, backend_identity_verified=True,
            monotonic_seconds=time.clock_gettime(time.CLOCK_BOOTTIME))
        path.write_text(json.dumps(value))
        path.chmod(0o600)
        self.assertEqual(0, self.run_health().returncode)
        self.children['hmi'].send_signal(signal.SIGSTOP)
        value['monotonic_seconds'] -= 7
        path.write_text(json.dumps(value))
        self.failure('health-hmi-heartbeat')
        self.assertIsNone(self.children['hmi'].poll())
        value.update(monotonic_seconds=time.clock_gettime(time.CLOCK_BOOTTIME), commit='b' * 40)
        path.write_text(json.dumps(value))
        self.failure('health-hmi-heartbeat')

    def test_missing_or_production_storage_refuses_without_creating_application_state(self):
        self.settings['development_mode'] = False
        self.save()
        self.failure('storage-contract-missing')
        self.assertFalse((self.apps / 'update-state.json').exists())
        self.assertFalse((self.apps / 'downloads').exists())
        self.settings['development_mode'] = 'true'
        self.save()
        self.failure('health-contract')

    def test_platform_two_requires_heartbeat_and_absent_fixture_policy_is_not_an_override(self):
        contract = json.loads(self.contract.read_text())
        contract['contract_id'] = 'dshancontrol-platform-2'
        self.contract.write_text(json.dumps(contract))
        self.failure('health-contract')
        self.settings['application_health']['require_hmi_heartbeat'] = True
        self.save()
        self.failure('health-hmi-heartbeat')
        del self.settings['application_health']['fixture_processes']
        self.save()
        self.failure('health-process')
