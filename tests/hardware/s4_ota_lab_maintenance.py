#!/usr/bin/env python3
"""GUI-only development maintenance verifier. Not an industrial interlock."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import stat

from s4_ota_lab_bootstrap import PERSISTENT, RUNTIME, roots, mounts, require
from s4_gui_probe import lab_scope, device


def maintenance():
    context, original = roots()
    mounts()
    lab_scope()
    config = json.loads(Path('/etc/dshancontrol/updater.json').read_text())
    require(config.get('automatic_install') is False, 'Manual GUI-only installation required')
    # The updater owns both existing leases; never manufacture an authorization flag.
    for variable, filename in [('DSHANCONTROL_UPDATE_LEASE_FD', '.update.lock'),
                               ('DSHANCONTROL_CONTROL_LEASE_FD', '.control.lock')]:
        descriptor = int(os.environ[variable])
        require(descriptor >= 200, 'Inherited updater lease required')
        actual = os.fstat(descriptor)
        expected = (RUNTIME / 'apps' / filename).stat()
        require(stat.S_ISREG(actual.st_mode) and actual.st_uid == 0 and actual.st_nlink == 1 and
                (actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino),
                'Updater lease identity mismatch')
        observed = False
        for line in Path('/proc/locks').read_text().splitlines():
            fields = line.split()
            if len(fields) < 8 or fields[1:4] != ['FLOCK', 'ADVISORY', 'WRITE']:
                continue
            major, minor, inode = fields[5].split(':')
            if (int(major, 16), int(minor, 16), int(inode)) == (
                    os.major(actual.st_dev), os.minor(actual.st_dev), actual.st_ino):
                observed = True
        require(observed, 'Existing exclusive updater lease not observed')
        # Re-locking the inherited open file description proves it belongs to
        # that observed lock. A separately opened descriptor would refuse.
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
    for name in ['web.json', 'hmi.json']:
        body = (RUNTIME / 'data' / name).read_bytes()
        require(hashlib.sha256(body).hexdigest() == context['default_file_sha256'][name],
                'Lab default configuration changed')
    web_config = json.loads((RUNTIME / 'data/web.json').read_text())
    require(web_config.get('can') == {} and web_config.get('canopen') == {'nodes': {}} and
            web_config.get('ec20') == {'profiles': {}}, 'Nonempty lab transport configuration')
    database = RUNTIME / 'data/edge.db'
    with sqlite3.connect('file:' + str(database) + '?mode=ro', uri=True) as connection:
        require(connection.execute('PRAGMA quick_check').fetchone()[0] == 'ok', 'Lab database unhealthy')
        for table in ['devices', 'points']:
            require(connection.execute('SELECT count(*) FROM ' + table).fetchone()[0] == 0,
                    'Industrial devices/points configured in lab')
    allowed = {'/dev/null', '/dev/urandom', '/dev/fb0', device()}
    sockets = set()
    observations = []
    for role in ['web', 'hmi']:
        pid = int((RUNTIME / 'run/processes' / (role + '.pid')).read_text())
        devices = []
        for entry in Path('/proc/{}/fd'.format(pid)).iterdir():
            try:
                target = os.readlink(str(entry))
            except FileNotFoundError:
                continue
            if target.startswith('/dev/'):
                require(target in allowed, 'Unexpected application device handle')
                devices.append(target)
            if target.startswith('socket:['):
                sockets.add(target[8:-1])
        observations.append(dict(role=role, device_handles=sorted(devices)))
    # Only the web HTTP listener is allowed; do not infer socket ownership from
    # the per-process net directory, which lists the whole network namespace.
    for name in ['tcp', 'tcp6', 'udp', 'udp6', 'raw', 'raw6']:
        path = Path('/proc/net') / name
        if not path.exists():
            continue
        for line in path.read_text().splitlines()[1:]:
            fields = line.split()
            if len(fields) < 10 or fields[9] not in sockets:
                continue
            require(name.startswith('tcp'), 'Unexpected application datagram/raw socket')
            if fields[3] == '0A':
                require(int(fields[1].split(':')[1], 16) == 80, 'Unexpected application listener')
            else:
                require(fields[2].split(':')[0] in
                        ('0100007F', '00000000000000000000000001000000',
                         '0000000000000000FFFF00000100007F'),
                        'Application network connection outside local lab API')
    return dict(maintenance_scope='GUI-only development', leases_verified=True,
                default_configuration_verified=True, configured_industrial_devices=0,
                configured_points=0, role_observations=observations,
                industrial_interlock_qualified=False)


if __name__ == '__main__':
    os.umask(0o077)
    try:
        print(json.dumps(dict(ok=True, **maintenance())), flush=True)
    except (OSError, ValueError, KeyError, RuntimeError, sqlite3.Error) as error:
        print(json.dumps(dict(ok=False, error=str(error), state_retained=True)), flush=True)
        raise SystemExit(1)
