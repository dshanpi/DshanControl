#!/usr/bin/env python3
"""Run Qt alarm confirmation against the real API on a private loopback database."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    with tempfile.TemporaryDirectory(prefix='dshan-alarm-integration-') as folder:
        state = Path(folder)
        os.environ.update(OMNIGATE_HMI_DB=str(state / 'hmi.db'),
                          OMNIGATE_HMI_SETTINGS=str(state / 'settings.json'),
                          OMNIGATE_CONFIG=str(state / 'config.json'),
                          OMNIGATE_MODBUS_CONFIG=str(state / 'modbus.json'),
                          OMNIGATE_PLATFORM_MANIFEST=str(ROOT / 'boards/t113s4-sdnand/platform.json'))
        (state / 'settings.json').write_text('{"demo_mode":false}')
        (state / 'config.json').write_text('{}')
        sys.path[:0] = [str(ROOT / 'components/web'), str(ROOT / 'components/core/src')]
        from flask import Flask, request
        from werkzeug.serving import make_server
        import hmi_api
        # This endpoint exercises alarm SQL and audit only; no hardware command may run.
        def forbidden(*args, **kwargs):
            raise AssertionError('Hardware operation outside isolated alarm test')
        hmi_api._run = forbidden
        app = Flask(__name__)
        allowed = {('GET', '/api/hmi/v1/alarms'), ('GET', '/api/hmi/v1/audit'),
                   ('POST', '/api/hmi/v1/alarms/2147483655/ack')}
        requests = []

        @app.before_request
        def restrict():
            entry = (request.method, request.path)
            requests.append(entry)
            if request.method == 'GET' and request.content_length:
                return {'ok': False, 'error': 'GET must have no request body'}, 400
            if entry not in allowed:
                return {'ok': False, 'error': 'outside alarm fixture scope'}, 403

        app.register_blueprint(hmi_api.create_blueprint())
        with hmi_api._db() as db:
            for key in (22, 2147483655):
                db.execute('INSERT INTO alarms(id,ts,severity,source,code,message,state) VALUES(?,?,?,?,?,?,?)',
                           (key, int(time.time()), 'error', 'owned-test', 'fixture', 'test event', 'active'))
        server = make_server('127.0.0.1', 0, app)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
                       HMI_ALARM_HTTP_PORT=str(server.server_port),
                       DSHANCONTROL_DESKTOP_ANIMATIONS='0')
            with (output / 'qt-test.log').open('xb') as log:
                result = subprocess.run([str(binary), 'alarmAckUsesSelectedIdAndRecoversFromFailure'],
                                        cwd=str(state), env=env, stdin=subprocess.DEVNULL,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=60)
            with hmi_api._db() as db:
                alarms = [dict(row) for row in db.execute('SELECT id,state FROM alarms ORDER BY id')]
                audit = [dict(row) for row in db.execute('SELECT action,target,result FROM audit')]
            passed = (result.returncode == 0 and
                      requests.count(('POST', '/api/hmi/v1/alarms/2147483655/ack')) == 1 and
                      alarms == [{'id': 22, 'state': 'active'}, {'id': 2147483655, 'state': 'acknowledged'}] and
                      audit == [{'action': 'alarm_ack', 'target': '2147483655', 'result': 'success'}])
            report = dict(passed=passed, qt_exit_code=result.returncode, alarms=alarms, audit=audit,
                          requests=requests, scope='Private loopback API/SQLite; no production alarm or hardware write')
            (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
            print(json.dumps(report, ensure_ascii=False))
            return 0 if passed else 1
        finally:
            server.shutdown()
            thread.join(timeout=5)
            server.server_close()
            hmi_api._close_db()


if __name__ == '__main__':
    raise SystemExit(main())
