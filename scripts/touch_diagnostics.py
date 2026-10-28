"""Offline diagnostics only: no transport, device access, or command execution."""
import argparse
import ast
import base64
import hashlib
import json
import os
import stat
from pathlib import Path
import re
import shlex
import zlib

MAX_STREAM = 1000000
MAX_RAW = 262144
MAX_WIRE = 524288
PROMPT = re.compile(rb'^#(?:[ \t]*$|[ \t]+\[[ \t]*[0-9]+\.[0-9]+\])')
ERROR = re.compile(rb'^(?:[A-Za-z_][A-Za-z_0-9]*\.)*(?:[A-Za-z_][A-Za-z_0-9]*)?(?:Error|Exception):')

def public_bytes(data):
    """Preserve undecodable public proc/sysfs data without replacement/loss."""
    if len(data) > 32768:
        raise ValueError('public field exceeds bound')
    return {'b64': base64.b64encode(data).decode('ascii'),
            'sha256': hashlib.sha256(data).hexdigest()}

def checked_payload(meta, parts):
    raw_size, digest, wire_size = meta
    wire = b''.join(parts)
    if len(wire) != wire_size or not wire:
        raise ValueError('wire length mismatch')
    packed = base64.b64decode(wire, validate=True)
    obj = zlib.decompressobj()
    raw = obj.decompress(packed, raw_size + 1)
    if len(raw) != raw_size or not obj.eof or obj.unused_data or obj.unconsumed_tail:
        raise ValueError('compressed stream bounds or trailing data')
    if hashlib.sha256(raw).hexdigest() != digest:
        raise ValueError('payload SHA mismatch')
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError('payload must be an object')
    return value

def observe(stream, prefix, finalized=False):
    """Classify one saved receive buffer; a prompt is never execution permission.

    A completed frame describes transport integrity only, not board acceptance.
    A rejected command does not imply whether its source began executing.
    """
    if not isinstance(stream, bytes):
        raise TypeError('raw bytes required')
    if not re.fullmatch(r'[A-Z][A-Z0-9]{1,15}', prefix):
        raise ValueError('invalid marker prefix')
    result = {'status': 'waiting', 'root_prompt_seen': False,
              'retry_allowed': False, 'board_accepted': False}
    if len(stream) > MAX_STREAM:
        return dict(result, status='invalid', reason='receive bound exceeded')
    prefix = prefix.encode('ascii')
    meta = None
    parts = []
    count = 0
    ended = False
    terminal = False
    error = None
    invalid = None
    # Only complete lines are evidence; a split chunk is not a malformed line.
    lines = stream.replace(b'\r\n', b'\n').split(b'\n')
    complete_lines = lines[:-1]
    # A sealed archive can end without a newline; inspect its final bytes too.
    # Live/incremental buffers must not opt in without independent completion.
    if finalized and lines[-1]:
        complete_lines.append(lines[-1])
    for line in complete_lines:
        if PROMPT.match(line):
            result['root_prompt_seen'] = True
            if error is not None or ended:
                terminal = True
            continue
        if ERROR.match(line):
            error = line.decode('ascii', errors='backslashreplace')
            terminal = False
        if not line.startswith(prefix + b'_'):
            continue
        # Any extra marker following END invalidates the entire transaction.
        if ended:
            invalid = 'marker after END'
            continue
        if line.startswith(prefix + b'_META '):
            if meta is not None or parts:
                invalid = 'duplicate or late META'
                continue
            m = re.fullmatch(prefix + rb'_META ([0-9]{1,7}) ([0-9a-f]{64}) ([0-9]{1,7})', line)
            if not m:
                invalid = 'malformed META'
                continue
            meta = (int(m[1]), m[2].decode('ascii'), int(m[3]))
            if not 0 < meta[0] <= MAX_RAW or not 0 < meta[2] <= MAX_WIRE:
                invalid = 'declared frame bound exceeded'
        elif line.startswith(prefix + b'_DATA '):
            piece = line[len(prefix) + 6:]
            if meta is None or not piece or len(piece) > 512 or not re.fullmatch(rb'[A-Za-z0-9+/=]+', piece):
                invalid = 'malformed or out-of-order DATA'
                continue
            count += len(piece)
            if count > MAX_WIRE or count > meta[2]:
                invalid = 'DATA exceeds declared length'
                continue
            parts.append(piece)
        elif line == prefix + b'_END':
            if meta is None or not parts:
                invalid = 'END before frame'
            ended = True
            terminal = False
        else:
            invalid = 'unknown or damaged marker'
    if error is not None:
        return dict(result, status='rejected' if terminal else 'error_pending_terminal',
                    error=error, root_terminal=terminal)
    if invalid is not None:
        return dict(result, status='invalid', reason=invalid, root_terminal=terminal)
    if ended and terminal:
        try:
            value = checked_payload(meta, parts)
        except (ValueError, TypeError, zlib.error) as ex:
            return dict(result, status='invalid', reason=str(ex), root_terminal=True)
        return dict(result, status='complete', payload=value, root_terminal=True)
    return result

def inline_preflight(source, name):
    """Compile and round-trip a reviewed source locally; never execute it.

    No allowlist registration, intent, UART payload submission or retry occurs.
    Transport corruption remains a board-side uncertainty until fresh testing.
    """
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*\.py', name):
        raise ValueError('invalid source name')
    if not 0 < len(source) <= 65536:
        raise ValueError('source bound exceeded')
    compile(source, name, 'exec')
    ast.parse(source.decode('utf-8'), filename=name, feature_version=(3, 7))
    digest = hashlib.sha256(source).hexdigest()
    wire = base64.b64encode(zlib.compress(source, 9)).decode('ascii')
    decoded = zlib.decompress(base64.b64decode(wire, validate=True))
    if decoded != source:
        raise ValueError('source round-trip mismatch')
    code = ('import base64,zlib,hashlib;b=zlib.decompress(base64.b64decode(' + repr(wire)
            + '));assert hashlib.sha256(b).hexdigest()==' + repr(digest)
            + ';exec(compile(b,' + repr(name) + ",'exec'))")
    compile(code, '<wrapper>', 'exec')
    command = 'python3 -c ' + shlex.quote(code)
    if len(code.encode('ascii')) > 3500 or len(command.encode('ascii')) > 3500:
        raise ValueError('inline command exceeds 3500-byte bound')
    if shlex.split(command) != ['python3', '-c', code]:
        raise ValueError('shell round-trip mismatch')
    return {'source_sha256': digest, 'source_bytes': len(source),
            'wrapper_bytes': len(code), 'command_bytes': len(command),
            'python37_syntax': True, 'round_trip': True, 'executed': False}


def read_regular_file(path, limit):
    """Read a bounded local regular file; refuse devices, pipes and symlinks."""
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('input must be a regular file, not a symlink or device')
    fd = os.open(str(path), os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise ValueError('input type changed')
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            data = stream.read(limit + 1)
        if len(data) > limit:
            raise ValueError('input exceeds byte bound')
        return data
    finally:
        os.close(fd)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inspect = commands.add_parser('inspect', help='Inspect a saved UART log locally')
    inspect.add_argument('path', type=Path)
    inspect.add_argument('--prefix', required=True)
    inspect.add_argument('--finalized', action='store_true',
                         help='Only for a sealed log, never a live receive buffer')
    preflight = commands.add_parser('preflight', help='Check source without executing it')
    preflight.add_argument('path', type=Path)
    args = parser.parse_args(argv)
    try:
        limit = MAX_STREAM if args.command == 'inspect' else 65536
        data = read_regular_file(args.path, limit)
        if args.command == 'inspect':
            result = observe(data, args.prefix, finalized=args.finalized)
            result['input_sha256'] = hashlib.sha256(data).hexdigest()
            result['input_bytes'] = len(data)
            status = {'complete': 0, 'rejected': 2, 'waiting': 3,
                      'error_pending_terminal': 3, 'invalid': 4}[result['status']]
        else:
            result = inline_preflight(data, args.path.name)
            status = 0
    except (OSError, ValueError, SyntaxError) as error:
        # Do not echo source fragments or local paths from exception messages.
        result = {'status': 'invalid', 'reason': type(error).__name__,
                  'retry_allowed': False, 'board_accepted': False}
        status = 4
    print(json.dumps(result, ensure_ascii=True, sort_keys=True))
    return status


if __name__ == '__main__':
    raise SystemExit(main())
