#!/usr/bin/env python3
"""Check application identity and database integrity, not merely process existence."""
import json
import os
import sqlite3
import urllib.request
from pathlib import Path
from storage import ensure_storage

ensure_storage()

with urllib.request.urlopen('http://127.0.0.1/api/health', timeout=5) as response:
    status = json.loads(response.read(65536).decode('utf-8'))
assert status.get('ok') is True and status.get('service') == 'omnigate-web'
current = json.loads(Path('/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/apps/current/installed.json').read_text())
assert status.get('application_commit') == current['commit']
for env in ('OMNIGATE_EDGE_DB', 'OMNIGATE_HMI_DB'):
    path = Path(os.environ[env])
    assert path.is_file(), 'database not initialized: ' + env
    with sqlite3.connect('file:' + str(path) + '?mode=ro', uri=True) as db:
        assert db.execute('PRAGMA quick_check').fetchone()[0] == 'ok'

# Additional real device observations for this temporary board harness.
pid=int(Path('/var/run/dshancontrol/hmi.pid').read_text())
expected=Path("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/apps/current/bin/omnigate-hmi").resolve()
assert Path('/proc/%s/exe' % pid).resolve()==expected
fds=[]
for fd in Path('/proc/%s/fd' % pid).iterdir():
    try: fds.append(os.readlink(str(fd)))
    except FileNotFoundError: pass
assert '/dev/fb0' in fds
assert any(x.startswith('/dev/input/event') for x in fds)
