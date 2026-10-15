"""Real C++ daemon and independent jobs, with synthetic signed test packages."""
import hashlib
import fcntl
import json
import os
from pathlib import Path
import signal
import socket
import shutil
import subprocess
import sys
import time
import unittest
import test_application_update_cli as fixtures


class NativeServiceTests(fixtures.UpdateCliTests):
    # Only service-specific cases run here; CLI parity is covered in the other module.
    __unittest_skip__ = False

    def setUp(self):
        super().setUp()
        self.binary = Path(os.environ.get('DSHANCONTROL_NATIVE_UPDATER',
            str(fixtures.fixtures.ROOT / 'dist/native-runtime/dshancontrol-runtime')))
        self.tasks = self.f.root / 'tasks'; self.tasks.mkdir()
        self.socket_path = str(self.f.root / 'platform.sock')
        self.service_config = self.f.root / 'platform-service.json'
        self.service_config.write_text(json.dumps({'updater_config': str(self.config),
            'task_root': str(self.tasks), 'socket': self.socket_path,
            'client_gid': os.getgid(), 'allowed_uids': [os.getuid()]}))
        self.service = None
        self.addCleanup(self.stop)

    def stop(self):
        if self.service and self.service.poll() is None:
            self.service.terminate(); self.service.wait(timeout=5)
        if self.service:
            self.service.stdout.close(); self.service.stderr.close()

    def test_shutdown_refuses_busy_resources_and_preserves_service_until_idle(self):
        self.start()
        self.assertFalse(self.request('service.shutdown', {'force': True})['ok'])
        config = json.loads(self.config.read_text())
        for path in (self.tasks / '.task.lock',
                     Path(config['application_root']) / '.update.lock',
                     Path(config['application_root']) / '.control.lock'):
            with path.open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                response = self.request('service.shutdown')
                self.assertFalse(response['ok'], response)
                self.assertEqual('resource-busy', response['error']['code'])
                self.assertTrue(self.request('update.identity')['ok'])
                self.assertIsNone(self.service.poll())
        self.assertEqual([], self.operations())
        response = self.request('service.shutdown')
        self.assertTrue(response['ok'], response)
        self.assertEqual('stopping', response['result']['status'])
        self.assertEqual(0, self.service.wait(timeout=5))
        self.assertFalse(Path(self.socket_path).exists())

    def test_production_storage_failure_does_not_create_task_directory(self):
        config = json.loads(self.config.read_text())
        config['development_mode'] = False
        self.config.write_text(json.dumps(config))
        task_root = self.f.root / 'must-not-create/tasks'
        service = json.loads(self.service_config.read_text())
        service['task_root'] = str(task_root)
        self.service_config.write_text(json.dumps(service))
        result = subprocess.run([str(self.binary), 'serve', str(self.service_config)],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(1, result.returncode)
        self.assertEqual('storage-contract-missing', json.loads(result.stderr)['code'])
        self.assertFalse(task_root.parent.exists())
        self.assertFalse(Path(self.socket_path).exists())
        self.assertEqual([], self.operations())

    def test_readiness_binds_the_real_daemon_pid(self):
        self.start()
        response = self.request('service.ready', {'pid': self.service.pid})
        self.assertTrue(response['ok'], response)
        self.assertEqual(self.service.pid, response['result']['pid'])
        self.assertIn('installed', response['result'])
        wrong = self.request('service.ready', {'pid': self.service.pid + 1})
        self.assertFalse(wrong['ok'])
        self.assertEqual('service-instance-mismatch', wrong['error']['code'])
        for params in ({}, {'pid': True}, {'pid': 0}, {'pid': 1.5},
                       {'pid': self.service.pid, 'force': True}):
            self.assertFalse(self.request('service.ready', params)['ok'])

    def test_system_init_starts_once_waits_for_rpc_and_refuses_busy_stop(self):
        # Relocate only system-owned paths into this synthetic system fixture.
        # Execute the actual init script and daemon, not a mocked RPC client.
        root = self.f.root
        binary = root / 'dshancontrol-runtime'
        shutil.copy2(str(self.binary), str(binary))
        pidfile = root / 'native.pid'
        script = root / 'S69native'
        source = (fixtures.fixtures.ROOT / 'boards/t113s4-sdnand/runtime/S69dshancontrol-runtime').read_text()
        vendor = root / 'partition_op.sh'
        alias = root / 'dev/by-name/UDISK'
        for old, new in [('BIN=/usr/bin/dshancontrol-runtime', 'BIN=' + str(binary)),
                         ('CONFIG=/etc/dshancontrol/platform-service.json', 'CONFIG=' + str(self.service_config)),
                         ('PID=/var/run/dshancontrol-runtime.pid', 'PID=' + str(pidfile)),
                         ('SOCKET=/var/run/dshancontrol/platform.sock', 'SOCKET=' + self.socket_path),
                         ('LOG=/var/log/dshancontrol-runtime.log', 'LOG=' + str(root / 'runtime.log')),
                         ('/etc/partition_op.sh', str(vendor)),
                         ('/dev/by-name/UDISK', str(alias)),
                         ('mkdir -p /var/run/dshancontrol', 'mkdir -p ' + str(root / 'run'))]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        bootprobe = root / 'bootprobe.py'
        bootprobe.write_text("from pathlib import Path\np=Path(__file__).with_suffix('.calls')\n"
                             "with p.open('a') as f: f.write('called\\n')\n")
        for command in ('storage-check', 'initialize'):
            invocation = '"$BIN" ' + command
            self.assertIn(invocation, source)
            source = source.replace(invocation, 'python3 ' + str(bootprobe))
        script.write_text(source)
        script.chmod(0o755)

        def run(action):
            return subprocess.run([str(script), action], capture_output=True, text=True, timeout=50)

        def cleanup():
            if not pidfile.exists():
                return
            try:
                pid = int(pidfile.read_text())
                if Path('/proc/%d/exe' % pid).resolve() == binary:
                    os.kill(pid, signal.SIGTERM)
            except (OSError, ValueError):
                pass
        self.addCleanup(cleanup)
        # Missing vendor alias support refuses before initialization or launch.
        refused = run('start')
        self.assertNotEqual(0, refused.returncode)
        self.assertFalse(pidfile.exists())
        self.assertFalse(bootprobe.with_suffix('.calls').exists())
        device = root / 'device'
        device.write_text('synthetic boot-mapped device')
        vendor.write_text('do_device_node_switch() {\n'
            'mkdir -p ' + str(alias.parent) + '\n'
            'ln -s ' + str(device) + ' ' + str(alias) + '\n'
            'echo alias >> ' + str(root / 'alias.calls') + '\n}\n'
            'do_format_filesystem() { echo forbidden; return 99; }\n')
        # A real native cold-start recovery must finish before init reports ready.
        # The previous 30-second loop failed even the default health budget.
        recovery_root = Path(json.loads(self.config.read_text())['application_root'])
        (recovery_root / 'transaction.json').write_text(json.dumps({
            'old': 'factory', 'new': 'recovery-trial', 'phase': 'prepared'}))
        (recovery_root / 'update-state.json').write_text(json.dumps({'high_water': 1, 'failed': []}))
        self.hook.write_text(self.hook.read_text().replace(
            "operation = sys.argv[1]", "operation = sys.argv[1]\nif operation == 'stop':\n"
            "    import time; time.sleep(31)"))
        started = time.monotonic()
        result = run('start')
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertGreaterEqual(time.monotonic() - started, 31)
        self.assertFalse((recovery_root / 'transaction.json').exists())
        self.assertEqual(['recovery-trial'], json.loads(
            (recovery_root / 'update-state.json').read_text())['failed'])
        self.assertEqual(device, alias.resolve())
        self.assertEqual('alias\n', (root / 'alias.calls').read_text())
        first_pid = pidfile.read_text()
        self.assertTrue(self.request('update.identity')['ok'])
        self.assertEqual(0, run('status').returncode)
        self.assertEqual(0, run('start').returncode)
        self.assertEqual(first_pid, pidfile.read_text())
        self.assertEqual('alias\n', (root / 'alias.calls').read_text())
        self.assertEqual(['called', 'called'], bootprobe.with_suffix('.calls').read_text().splitlines())
        with (self.tasks / '.task.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            refused = run('stop')
            self.assertNotEqual(0, refused.returncode)
            self.assertIn('resource-busy', refused.stderr)
            self.assertEqual(first_pid, pidfile.read_text())
            self.assertTrue(self.request('update.identity')['ok'])
        self.assertEqual(0, run('stop').returncode)
        self.assertFalse(pidfile.exists())
        self.assertFalse(Path(self.socket_path).exists())
        self.assertNotEqual(0, run('status').returncode)
        config = json.loads(self.config.read_text())
        config['development_mode'] = False
        self.config.write_text(json.dumps(config))
        failed = run('start')
        self.assertNotEqual(0, failed.returncode)
        log = root / 'runtime.log'
        self.assertEqual(0o600, log.stat().st_mode & 0o777)
        self.assertEqual(1, log.read_text().count('storage-contract-missing'))
        self.assertEqual(['called'] * 4, bootprobe.with_suffix('.calls').read_text().splitlines())
        self.assertFalse(Path(self.socket_path).exists())
        # A reused PID never grants permission to stop an unrelated process.
        pidfile.write_text(str(os.getpid()) + '\n')
        self.assertNotEqual(0, run('stop').returncode)
        self.assertTrue(pidfile.exists())
        pidfile.unlink()
        child = subprocess.Popen([sys.executable, '-c', 'pass'])
        try:
            for _ in range(100):
                stat = Path('/proc/%d/stat' % child.pid)
                if stat.is_file() and stat.read_text().rsplit(') ', 1)[1].split()[0] == 'Z':
                    break
                time.sleep(0.01)
            pidfile.write_text(str(child.pid) + '\n')
            self.assertEqual(0, run('stop').returncode)
            self.assertFalse(pidfile.exists())
        finally:
            child.wait(timeout=5)

    def start(self):
        self.service = subprocess.Popen([str(self.binary), 'serve', str(self.service_config)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for _ in range(100):
            if self.service.poll() is not None:
                self.fail(self.service.communicate()[1].decode())
            if Path(self.socket_path).exists():
                try:
                    with socket.socket(socket.AF_UNIX) as probe:
                        probe.settimeout(0.1); probe.connect(self.socket_path)
                    return
                except OSError:
                    pass
            time.sleep(0.02)
        self.fail('Daemon did not open its socket')

    def request(self, method, params=None, raw=None):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5); connection.connect(self.socket_path)
            connection.sendall(raw or (json.dumps({'protocol': 1, 'method': method,
                'params': params or {}}).encode() + b'\n'))
            return json.loads(connection.makefile('rb').readline(1024*1024))

    def display_fixture(self, maximum=400, board=None):
        self.backlight = self.f.root / 'backlights/backlight0'
        self.backlight.mkdir(parents=True)
        (self.backlight / 'max_brightness').write_text(str(maximum) + '\n')
        (self.backlight / 'brightness').write_text('200\n')
        manifest = self.f.root / 'platform.json'
        manifest.write_text(json.dumps({'schema_version': 1,
            'board_id': board or fixtures.fixtures.TARGET['board_id'],
            'architecture': fixtures.fixtures.TARGET['architecture'],
            'channels': {'display0': {'kind': 'display', 'device': '/dev/fb0'}}}))
        config = json.loads(self.service_config.read_text())
        config.update(platform_manifest=str(manifest),
            display={'channel': 'display0', 'backlight': 'backlight0'},
            backlight_root=str(self.backlight.parent))
        self.service_config.write_text(json.dumps(config))

    def test_native_display_scales_and_reads_driver_without_client_paths(self):
        self.display_fixture(); self.start()
        params = {'channel': 'display0', 'value': 120, 'confirmed': True}
        response = self.request('display.brightness.set', params)
        self.assertTrue(response['ok'], response)
        self.assertEqual(188, response['result']['driver_brightness'])
        self.assertEqual(120, response['result']['brightness'])
        self.assertEqual('188\n', (self.backlight / 'brightness').read_text())
        self.assertEqual('driver-configured-brightness', response['result']['source'])
        self.assertEqual(response['result'],
            self.request('display.brightness.get', {'channel': 'display0'})['result'])
        for change in [{'channel': 'display1'}, {'value': 9}, {'value': 256},
                       {'value': True}, {'value': 12.5}, {'confirmed': False},
                       {'path': '/tmp/another'}, {'device_id': 'another-board'}]:
            self.assertFalse(self.request('display.brightness.set', dict(params, **change))['ok'])
            self.assertEqual('188\n', (self.backlight / 'brightness').read_text())
        self.assertEqual([], self.operations())

    def test_display_write_refuses_updater_control_lock_and_never_retries(self):
        self.display_fixture(); self.start()
        config = json.loads(self.config.read_text())
        root = Path(config['application_root'])
        with (root / '.control.lock').open('a') as ownership:
            fcntl.flock(ownership, fcntl.LOCK_EX | fcntl.LOCK_NB)
            response = self.request('display.brightness.set', {
                'channel': 'display0', 'value': 120, 'confirmed': True})
            self.assertFalse(response['ok'])
            self.assertEqual('resource-busy', response['error']['code'])
        time.sleep(0.1)
        self.assertEqual('200\n', (self.backlight / 'brightness').read_text())

    def test_display_board_mismatch_and_invalid_driver_refuse_before_write(self):
        self.display_fixture(board='wrong-board'); self.start()
        response = self.request('display.brightness.set', {
            'channel': 'display0', 'value': 120, 'confirmed': True})
        self.assertEqual('display-board-mismatch', response['error']['code'])
        self.assertEqual('200\n', (self.backlight / 'brightness').read_text())

        # Restore only this system fixture; a corrupt driver maximum remains a refusal.
        path = self.f.root / 'platform.json'
        manifest = json.loads(path.read_text())
        manifest['board_id'] = fixtures.fixtures.TARGET['board_id']
        path.write_text(json.dumps(manifest))
        (self.backlight / 'max_brightness').write_text('0\n')
        response = self.request('display.brightness.set', {
            'channel': 'display0', 'value': 120, 'confirmed': True})
        self.assertEqual('driver-value-invalid', response['error']['code'])
        self.assertEqual('200\n', (self.backlight / 'brightness').read_text())

    def test_sunxi_display_rejects_unaudited_screen_and_soc_before_device_open(self):
        self.display_fixture()
        config = json.loads(self.service_config.read_text())
        config['display'] = {'channel': 'display0', 'backend': 'sunxi-disp2', 'screen': 1}
        self.service_config.write_text(json.dumps(config))
        self.start()
        response = self.request('display.brightness.get', {'channel': 'display0'})
        self.assertFalse(response['ok'])
        self.assertEqual('display-backend-mismatch', response['error']['code'])
        self.assertEqual('200\n', (self.backlight / 'brightness').read_text())

    def source(self):
        task_id = 'b' * 32
        directory = self.tasks / task_id; directory.mkdir()
        for source, name in [(self.f.path, 'application-release.json'),
                             (self.f.signature, 'application-release.json.sig'),
                             (self.f.package, 'app.tar.gz')]:
            (directory / name).write_bytes(source.read_bytes())
        candidate = {'version': '0.2.0', 'sequence': 1,
            'manifest_sha256': hashlib.sha256(self.f.path.read_bytes()).hexdigest(),
            'package_sha256': fixtures.fixtures.digest(self.f.package)}
        record = {'task_id': task_id, 'operation': 'download', 'status': 'success',
            'identity': fixtures.fixtures.TARGET, 'system_contract_sha256': fixtures.fixtures.digest(self.f.root / 'contract.json'),
            'candidate': candidate, 'package_filename': 'app.tar.gz', 'created_at_ms': 0}
        fixtures.fixtures.atomic_json(self.tasks / (task_id + '.json'), record)
        return {'operation': 'install', 'source_task_id': task_id, 'confirmed': True,
                'version': candidate['version'], 'package_sha256': candidate['package_sha256']}

    def terminal(self, task_id):
        for _ in range(150):
            response = self.request('update.status', {'task_id': task_id})
            self.assertTrue(response['ok'], response)
            if response['result']['status'] != 'running': return response['result']
            time.sleep(0.02)
        self.fail('Task did not reach terminal state')

    def test_daemon_job_outlives_client_and_reports_true_success(self):
        params = self.source(); self.start()
        # Submitter disconnects after the acknowledgement; the daemon owns execution.
        response = self.request('update.start', params); self.assertTrue(response['ok'], response)
        task = self.terminal(response['result']['task_id'])
        self.assertEqual('success', task['status']); self.assertEqual('completed', task['stage'])
        self.assertTrue(task['stages']); self.assertGreaterEqual(task['elapsed_ms'], 0)
        self.assertEqual(task['release_directory'], self.f.updater.current_path().name)
        self.assertEqual(b'USER DATA MUST SURVIVE', self.f.data.read_bytes())

    def test_confirmation_scope_and_unknown_fields_refuse_before_hooks(self):
        params = self.source(); self.start()
        for field, value in [('confirmed', False), ('version', '9.9.9'), ('package_sha256', 'a'*64),
                             ('device_id', 'another-board'), ('source_task_id', '../outside')]:
            self.assertFalse(self.request('update.start', dict(params, **{field:value}))['ok'])
        self.assertEqual([], self.operations())

    def test_failed_install_keeps_raw_stage_code_and_does_not_retry(self):
        params = self.source()
        self.hook.write_text('#!/bin/sh\n[ "$1" != maintenance ] || exit 17\nexit 0\n')
        self.start(); response = self.request('update.start', params)
        task = self.terminal(response['result']['task_id'])
        self.assertEqual('failed', task['status']); self.assertEqual('maintenance', task['error']['stage'])
        self.assertEqual('17', task['error']['code'])
        self.assertEqual('factory', self.f.updater.current_path().name)
        time.sleep(0.1)
        self.assertEqual(task, self.request('update.status', {'task_id':task['task_id']})['result'])

    def test_restart_marks_interrupted_task_without_resubmission(self):
        self.source()
        task_id = 'c'*32
        record = fixtures.fixtures.read_json(self.tasks / ('b'*32+'.json'))
        record.update(task_id=task_id, status='running', stage='download-package')
        fixtures.fixtures.atomic_json(self.tasks / (task_id+'.json'), record)
        self.start(); response = self.request('update.status', {'task_id':task_id})
        self.assertEqual('failed', response['result']['status'])
        self.assertEqual('interrupted', response['result']['error']['code'])
        self.assertEqual([], self.operations())

    def test_actual_daemon_crash_recovers_transaction_and_stale_socket(self):
        params = self.source()
        self.hook.write_text(self.hook.read_text().replace(
            'if mode == \'kill-\' + operation:',
            'if mode == \'kill-\' + operation:\n    import time; time.sleep(0.2)'))
        self.mode('kill-start'); self.start()
        response = self.request('update.start', params)
        self.assertTrue(response['ok'], response)
        task_id = response['result']['task_id']
        self.assertEqual(-signal.SIGKILL, self.service.wait(timeout=5))
        self.service.stdout.close(); self.service.stderr.close()
        self.assertTrue((self.f.app / 'transaction.json').exists())
        self.mode('ok'); self.start()
        task = self.request('update.status', {'task_id':task_id})['result']
        self.assertEqual('interrupted', task['error']['code'])
        self.assertEqual('factory', self.f.updater.current_path().name)
        self.assertFalse((self.f.app / 'transaction.json').exists())
        self.assertEqual(1, self.f.updater.state()['high_water'])

    def test_peer_and_duplicate_request_are_rejected(self):
        config = json.loads(self.service_config.read_text()); config['allowed_uids'] = []
        self.service_config.write_text(json.dumps(config)); self.start()
        self.assertFalse(self.request('update.identity')['ok'])

    def test_duplicate_rpc_keys_rejected(self):
        self.start()
        response = self.request('', raw=b'{"protocol":1,"method":"update.identity","method":"update.start","params":{}}\n')
        self.assertFalse(response['ok']); self.assertIn('Duplicate JSON', response['error']['message'])


# Do not run the inherited CLI matrix a second time under this service suite.
for name in dir(fixtures.UpdateCliTests):
    if name.startswith('test_'):
        setattr(NativeServiceTests, name, None)
