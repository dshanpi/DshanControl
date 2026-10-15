#!/usr/bin/env python3
"""Private lab GUI probe. Never installed in images or application payloads.

Events follow the selected GT9xx driver (protocol A), not a physical sensor test.
Linux input_inject_event reference: https://www.kernel.org/doc/html/v5.14/driver-api/input.html
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys
import time

PERSISTENT = Path('/mnt/UDISK/dshancontrol-qt-ota-lab-20261015')
RUNTIME = Path('/run/dshancontrol-qt-ota-lab-20261015')
CID = '1212340050ffffb0001280096a0196ef'


def require(value, message):
    if not value:
        raise RuntimeError(message)


def lab_scope():
    require(os.geteuid() == 0, 'Root lab operator required')
    config_path = Path('/etc/dshancontrol/updater.json')
    metadata = config_path.lstat()
    require(stat.S_ISREG(metadata.st_mode) and metadata.st_uid == 0 and
            stat.S_IMODE(metadata.st_mode) == 0o600, 'Private root configuration required')
    config = json.loads(config_path.read_text())
    require(config.get('development_mode') is True and
            config.get('automatic_install') is False and
            config.get('application_root') == str(RUNTIME / 'apps'), 'Isolated lab scope required')
    require(config.get('system_contract') == str(PERSISTENT / 'system/contract.json'),
            'Fixed lab contract required')
    contract = json.loads(Path(config['system_contract']).read_text())
    require(contract.get('hardware_revision') == 'development-fixture-unqualified' and
            contract.get('data_root') == str(RUNTIME / 'data'), 'Explicit fixture identity required')
    require(Path('/sys/block/mmcblk0/device/cid').read_text().strip() == CID,
            'Bound board mismatch')
    device = os.stat('/dev/mmcblk0p9').st_rdev
    for path in [RUNTIME / 'apps', RUNTIME / 'data']:
        require(path.is_dir() and not path.is_symlink() and path.stat().st_dev == device,
                'Persistent lab mount required')
    require((RUNTIME / 'apps').stat().st_ino == (PERSISTENT / 'apps').stat().st_ino and
            (RUNTIME / 'data').stat().st_ino == (PERSISTENT / 'data').stat().st_ino,
            'Lab bind mount identity mismatch')
    policy = config['application_health']
    require(policy['pid_root'] == str(RUNTIME / 'run/processes'), 'Fixed lab role records required')
    pid = int((RUNTIME / 'run/processes/hmi.pid').read_text())
    fields = dict(line.split(':', 1) for line in Path('/proc/{}/status'.format(pid)).read_text().splitlines()
                  if ':' in line)
    require(fields['Uid'].split() == ['401'] * 4, 'Actual HMI identity mismatch')
    argv = Path('/proc/{}/cmdline'.format(pid)).read_bytes().split(bytes([0]))
    require(argv[:1] == [str(RUNTIME / 'apps/current/bin/omnigate-hmi').encode()],
            'Actual HMI lab command mismatch')
    web = int((RUNTIME / 'run/processes/web.pid').read_text())
    fields = dict(line.split(':', 1) for line in Path('/proc/{}/status'.format(web)).read_text().splitlines()
                  if ':' in line)
    require(fields['Uid'].split() == ['400'] * 4, 'Actual web identity mismatch')
    argv = Path('/proc/{}/cmdline'.format(web)).read_bytes().split(bytes([0]))
    require(argv[:2] == [b'/usr/bin/python3', str(RUNTIME / 'apps/current/lib/web/app.py').encode()],
            'Actual web lab command mismatch')
    environment = dict(value.split(b'=', 1) for value in
                       Path('/proc/{}/environ'.format(web)).read_bytes().split(bytes([0])) if b'=' in value)
    require(environment.get(b'OMNIGATE_CONFIG') == str(RUNTIME / 'data/web.json').encode(),
            'Actual web data scope mismatch')


def read_ioctl(fd, number, size):
    return fcntl.ioctl(fd, (2 << 30) | (size << 16) | (ord('E') << 8) | number, bytes(size))


def device():
    names = [path for path in Path('/sys/class/input').glob('event*/device/name')
             if path.read_text().strip() == 'gt9xxnew_ts']
    require(len(names) == 1, 'Unique named touchscreen required')
    return '/dev/input/' + names[0].parents[1].name


def framebuffer():
    fd = os.open('/dev/fb0', os.O_RDONLY | os.O_CLOEXEC)
    try:
        raw = bytearray(160)
        fcntl.ioctl(fd, 0x4600, raw, True)
        fields = struct.unpack('40I', raw)
        require(fields[0:2] == (1024, 600) and fields[6] == 32,
                'Reviewed framebuffer geometry required')
        return fields
    finally:
        os.close(fd)


def packet(events):
    # Selected ARM32/glibc2.21 input_event ABI: 16 bytes, including 32-bit timeval.
    return b''.join(struct.pack('<iiHHi', 0, 0, kind, code, value)
                    for kind, code, value in events)


def tap(x, y):
    lab_scope()  # Refuse production scope before opening either device.
    fields = framebuffer()
    require(0 <= x < fields[0] and 0 <= y < fields[1], 'Point outside framebuffer')
    node = device()
    fd = os.open(node, os.O_RDWR | os.O_NONBLOCK | os.O_CLOEXEC)
    down_written = False
    try:
        require(stat.S_ISCHR(os.fstat(fd).st_mode), 'Expected input character device')
        capabilities = read_ioctl(fd, 0x20 + 3, 8)
        supported = lambda code: bool(capabilities[code // 8] & (1 << (code % 8)))
        require(not supported(47) and all(supported(code) for code in [48, 50, 53, 54, 57]),
                'Reviewed GT9xx protocol A required')
        keys = read_ioctl(fd, 0x18, 96)
        require(not keys[330 // 8] & (1 << (330 % 8)), 'Touchscreen already active')
        coordinates = []
        for axis, point, length in [(53, x, fields[0]), (54, y, fields[1])]:
            values = struct.unpack('6i', read_ioctl(fd, 0x40 + axis, 24))
            require(values[2] > values[1], 'Invalid input axis')
            coordinates.append(values[1] + point * (values[2] - values[1]) // (length - 1))
        down = packet([(1, 330, 1), (3, 53, coordinates[0]), (3, 54, coordinates[1]),
                       (3, 48, 10), (3, 50, 10), (3, 57, 0), (0, 2, 0), (0, 0, 0)])
        up = packet([(1, 330, 0), (0, 0, 0)])
        written = os.write(fd, down)
        down_written = written > 0
        require(written == len(down), 'Incomplete touch press; no replay')
        time.sleep(0.08)
        require(os.write(fd, up) == len(up), 'Incomplete touch release; no replay')
        down_written = False
    finally:
        if down_written:
            # Release only our own accepted press if the observation interval failed.
            try:
                os.write(fd, packet([(1, 330, 0), (0, 0, 0)]))
            except OSError as error:
                print(json.dumps(dict(cleanup_release_failed=True, error=str(error))), file=sys.stderr)
        os.close(fd)
    return dict(action='tap', x=x, y=y, node=node, protocol='A', physical_sensor_test=False)


def snapshot(name):
    lab_scope()
    require(name.isalnum() and len(name) <= 48, 'Safe unique capture name required')
    fields = framebuffer()
    require(fields[4] == 0, 'Unsupported framebuffer horizontal offset')
    stride = int(Path('/sys/class/graphics/fb0/stride').read_text())
    require(stride >= fields[0] * 4 and stride <= 16384, 'Invalid framebuffer stride')
    count = stride * fields[1]
    fd = os.open('/dev/fb0', os.O_RDONLY | os.O_CLOEXEC)
    try:
        os.lseek(fd, fields[5] * stride, os.SEEK_SET)
        body = bytearray()
        while len(body) < count:
            part = os.read(fd, min(65536, count - len(body)))
            require(part, 'Incomplete framebuffer capture')
            body.extend(part)
    finally:
        os.close(fd)
    directory = PERSISTENT / 'screenshots'
    directory.mkdir(mode=0o700, exist_ok=True)
    path = directory / (name + '.raw')
    with path.open('xb') as stream:
        stream.write(body)
        stream.flush()
        os.fsync(stream.fileno())
    path.chmod(0o600)
    return dict(action='snapshot', path=str(path), bytes=len(body), sha256=hashlib.sha256(body).hexdigest(),
                width=fields[0], height=fields[1], stride=stride,
                red=list(fields[8:11]), green=list(fields[11:14]), blue=list(fields[14:17]),
                alpha=list(fields[17:20]), private=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    touch = sub.add_parser('tap')
    touch.add_argument('x', type=int)
    touch.add_argument('y', type=int)
    capture = sub.add_parser('snapshot')
    capture.add_argument('name')
    args = parser.parse_args()
    os.umask(0o077)
    try:
        result = tap(args.x, args.y) if args.action == 'tap' else snapshot(args.name)
        print(json.dumps(dict(ok=True, **result)), flush=True)
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(json.dumps(dict(ok=False, action=args.action, error=str(error), no_replay=True)), flush=True)
        raise SystemExit(1)
