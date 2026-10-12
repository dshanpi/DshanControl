#!/usr/bin/env python3
"""Periodic GitHub checks; automatic installation is an explicit system policy."""
import json
import signal
import subprocess
import time
from pathlib import Path
from updater import atomic_json
from update_protocol import read_json

RUNNING = True


def stop(*_):
    global RUNNING
    RUNNING = False


def poll(config_path):
    config = read_json(config_path)
    cli = ['/usr/bin/dshancontrol-update', '--config', str(config_path)]
    result = subprocess.run(cli + ['check'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=600)
    if result.returncode:
        return {'status': 'check-failed', 'detail': result.stderr[-1024:]}
    candidate = json.loads(result.stdout)
    if not candidate:
        return {'status': 'up-to-date'}
    if not config.get('automatic_install', False):
        return {'status': 'available', 'candidate': candidate}
    # Installer independently verifies the current contract, signature, sequence
    # and maintenance interlock. The watcher grants no bypass.
    result = subprocess.run(cli + ['download'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=1800)
    if result.returncode:
        return {'status': 'download-failed', 'detail': result.stderr[-1024:]}
    directory = Path(result.stdout.strip())
    manifest = read_json(directory / 'application-release.json')
    contract = read_json(config['system_contract'])
    from update_protocol import match_target
    current = read_json(Path(config['application_root']) / 'current/installed.json')
    target = match_target(manifest, contract, current)
    result = subprocess.run(cli + ['install', str(directory / 'application-release.json'),
                             str(directory / 'application-release.json.sig'),
                             str(directory / target['filename'])], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, universal_newlines=True, timeout=600)
    return {'status': 'installed' if result.returncode == 0 else 'install-refused-or-recovered',
            'detail': (result.stdout + result.stderr)[-2048:]}


def main():
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    config_path = Path('/etc/dshancontrol/updater.json')
    while RUNNING:
        try:
            status = poll(config_path)
        except Exception as error:
            status = {'status': 'check-failed', 'detail': str(error)}
        status['checked_at'] = int(time.time())
        atomic_json(Path('/var/run/dshancontrol-update.json'), status)
        interval = max(600, int(read_json(config_path).get('poll_interval_seconds', 3600)))
        deadline = time.monotonic() + interval
        while RUNNING and time.monotonic() < deadline:
            time.sleep(1)


if __name__ == '__main__':
    main()
