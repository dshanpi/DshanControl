#!/usr/bin/env python3
"""Signed application-only installer. System-owned hooks, no package scripts."""
import argparse
import contextlib
import fcntl
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
from pathlib import Path

from update_protocol import (GATES, REQUIRED, REPOSITORY, Rejected, digest, identifier,
                             match_target, payload_name, read_json, require,
                             validate_manifest, verify_signature)


def sync_directory(path):
    fd = os.open(str(path), os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def atomic_json(path, value):
    path = Path(path)
    fd, tmp = tempfile.mkstemp(prefix='.' + path.name, dir=str(path.parent))
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(value, stream, sort_keys=True)
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(tmp, 0o600)
        os.replace(tmp, str(path))
        sync_directory(path.parent)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def extract_payload(archive, destination, max_bytes):
    total = 0
    seen = set()
    with tarfile.open(str(archive), 'r:gz') as tar:
        for member in tar:
            p = payload_name(member.name)
            require(str(p) not in seen, 'duplicate archive entry')
            seen.add(str(p))
            require(len(seen) <= 50000, 'too many archive entries')
            require(member.isdir() or member.isfile(), 'links and special files forbidden')
            require(member.mode & 0o7000 == 0, 'special permission bits forbidden')
            target = destination.joinpath(*p.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            total += member.size
            require(total <= max_bytes, 'expanded package exceeds declared limit')
            target.parent.mkdir(parents=True, exist_ok=True)
            with tar.extractfile(member) as source, open(str(target), 'xb') as output:
                shutil.copyfileobj(source, output)
                output.flush()
                os.fsync(output.fileno())
            os.chmod(str(target), 0o755 if member.mode & 0o111 else 0o644)
    require(total == max_bytes, 'expanded size mismatch')
    require(all((destination / p).is_file() for p in REQUIRED), 'incomplete application')
    require(os.access(str(destination / 'bin/omnigate-hmi'), os.X_OK), 'HMI not executable')
    for base, dirs, files in os.walk(str(destination), topdown=False):
        sync_directory(base)


class Updater:
    def __init__(self, config):
        require(type(config.get('development_mode', False)) is bool,
                'development_mode must be a boolean')
        self.config = config
        self.root = Path(config['application_root'])
        self.contract = read_json(config['system_contract'])
        require(self.root.is_absolute() and str(self.root) != '/' and
                str(self.root) == self.contract.get('application_root'),
                'application root differs from system contract')
        self.root.mkdir(parents=True, exist_ok=True)
        self.releases = self.root / 'releases'
        self.releases.mkdir(exist_ok=True)

    @contextlib.contextmanager
    def locked(self):
        with open(str(self.root / '.update.lock'), 'a') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise Rejected('another update is running')
            yield

    def current_path(self):
        link = self.root / 'current'
        require(link.is_symlink(), 'factory application not initialized')
        path = link.resolve()
        require(path.parent == self.releases.resolve(), 'current outside release directory')
        return path

    def state(self):
        path = self.root / 'update-state.json'
        return read_json(path) if path.exists() else {'high_water': 0, 'failed': []}

    def hook(self, operation, check=True):
        # This executable is installed by the system; never supplied in an app.
        result = subprocess.run([self.config['service_hook'], operation],
                                stdin=subprocess.DEVNULL, timeout=120)
        if check:
            require(result.returncode == 0, 'system hook failed: ' + operation)
        return result.returncode == 0

    def switch(self, name):
        require(identifier(name), 'invalid release directory')
        link = self.root / '.current.next'
        if link.is_symlink():
            link.unlink()
        os.symlink('releases/' + name, str(link))
        os.replace(str(link), str(self.root / 'current'))
        sync_directory(self.root)

    def healthy(self):
        # Background services need time to bind IPC and initialize their database.
        startup_deadline = time.monotonic() + self.config.get('startup_timeout_seconds', 30)
        while not self.hook('health', check=False):
            if time.monotonic() >= startup_deadline:
                return False
            time.sleep(self.config.get('health_interval_seconds', 2))
        deadline = time.monotonic() + self.config.get('health_observation_seconds', 30)
        while time.monotonic() < deadline:
            time.sleep(self.config.get('health_interval_seconds', 2))
            if not self.hook('health', check=False):
                return False
        return True

    def rollback(self, transaction):
        # Journal remains until old application is running and checked.
        self.hook('stop')
        self.switch(transaction['old'])
        self.hook('start')
        require(self.healthy(), 'rollback health failed; journal retained')
        state = self.state()
        state['failed'] = sorted(set(state['failed'] + [transaction['new']]))
        atomic_json(self.root / 'update-state.json', state)
        (self.root / 'transaction.json').unlink()
        sync_directory(self.root)

    def recover(self):
        journal = self.root / 'transaction.json'
        if journal.exists():
            tx = read_json(journal)
            if tx.get('phase') == 'committed':
                journal.unlink()
                sync_directory(self.root)
            else:
                self.rollback(tx)

    def install(self, manifest_path, signature_path, package_path):
        with self.locked():
            self.recover()
            # Snapshot caller files into private storage to avoid verify/use races.
            with tempfile.TemporaryDirectory(prefix='.download-', dir=str(self.root)) as tmp:
                tmp = Path(tmp)
                for src, name, limit in ((manifest_path, 'manifest.json', 1024*1024),
                                         (signature_path, 'signature', 16384)):
                    require(Path(src).is_file(), 'missing update input')
                    with open(str(src), 'rb') as stream:
                        body = stream.read(limit + 1)
                    require(len(body) <= limit, 'metadata too large')
                    (tmp / name).write_bytes(body)
                verify_signature(tmp / 'manifest.json', tmp / 'signature', self.config['public_key'])
                manifest = validate_manifest(read_json(tmp / 'manifest.json'),
                                             public=not self.config.get('development_mode', False))
                current = self.current_path()
                installed = read_json(current / 'installed.json')
                state = self.state()
                item = match_target(manifest, self.contract, installed, state['high_water'])
                require(Path(package_path).is_file() and Path(package_path).stat().st_size == item['size'],
                        'package size mismatch')
                require(shutil.disk_usage(str(self.root)).free >
                        item['size'] + item['unpacked_size'] + 16*1024*1024, 'insufficient free space')
                total = 0
                with open(str(package_path), 'rb') as source, open(str(tmp / 'package.tar.gz'), 'wb') as output:
                    while True:
                        block = source.read(65536)
                        if not block:
                            break
                        total += len(block)
                        require(total <= item['size'], 'package changed while copying')
                        output.write(block)
                require((tmp / 'package.tar.gz').stat().st_size == item['size'] and
                        digest(tmp / 'package.tar.gz') == item['sha256'], 'package hash/size mismatch')
                name = str(manifest['sequence']) + '-' + item['sha256'][:16]
                require(name not in state['failed'], 'known failed release quarantined')
                require(shutil.disk_usage(str(self.root)).free > item['unpacked_size'] + 16*1024*1024,
                        'insufficient free space')
                stage = Path(tempfile.mkdtemp(prefix='.stage-', dir=str(self.releases)))
                try:
                    extract_payload(tmp / 'package.tar.gz', stage, item['unpacked_size'])
                    atomic_json(stage / 'installed.json', {
                        'version': manifest['version'], 'commit': manifest['commit'],
                        'sequence': manifest['sequence'], 'data_schema': manifest['data_schema'],
                        'package_sha256': item['sha256']})
                    destination = self.releases / name
                    # An interrupted pre-activation attempt is safe to re-stage.
                    require(destination != current, 'already active')
                    if destination.exists():
                        shutil.rmtree(str(destination))
                    os.rename(str(stage), str(destination))
                    sync_directory(self.releases)
                finally:
                    if stage.exists():
                        shutil.rmtree(str(stage))
                self.hook('maintenance')
                tx = {'old': current.name, 'new': name, 'phase': 'prepared'}
                atomic_json(self.root / 'transaction.json', tx)
                # Persist anti-replay state BEFORE touching the running version.
                state['high_water'] = manifest['sequence']
                atomic_json(self.root / 'update-state.json', state)
                try:
                    self.hook('stop')
                    self.switch(name)
                    tx['phase'] = 'switched'
                    atomic_json(self.root / 'transaction.json', tx)
                    self.hook('start')
                    require(self.healthy(), 'new application health failed')
                    tx['phase'] = 'committed'
                    atomic_json(self.root / 'transaction.json', tx)
                    (self.root / 'transaction.json').unlink()
                    sync_directory(self.root)
                except Exception:
                    self.rollback(tx)
                    raise
                return name

    def check_github(self):
        current = read_json(self.current_path() / 'installed.json')
        candidates = []
        # Paginate instead of choosing GitHub's possibly incompatible Latest.
        for page in range(1, 11):
            url = 'https://api.github.com/repos/' + REPOSITORY + '/releases?per_page=100&page=' + str(page)
            releases = json.loads(fetch_bytes(url, 4*1024*1024).decode('utf-8'))
            require(isinstance(releases, list), 'invalid GitHub release response')
            for release in releases:
                if release.get('draft') or not release.get('tag_name', '').startswith('app-v'):
                    continue
                assets = {a['name']: a['browser_download_url'] for a in release.get('assets', [])}
                if not all(a in assets for a in ('application-release.json', 'application-release.json.sig')):
                    continue
                try:
                    with tempfile.TemporaryDirectory(prefix='.check-', dir=str(self.root)) as tmp:
                        mpath, sig = Path(tmp) / 'manifest', Path(tmp) / 'signature'
                        mpath.write_bytes(fetch_asset(assets['application-release.json'], 1024*1024))
                        sig.write_bytes(fetch_asset(assets['application-release.json.sig'], 16384))
                        verify_signature(mpath, sig, self.config['public_key'])
                        manifest = validate_manifest(read_json(mpath), public=True)
                        require(manifest['tag'] == release['tag_name'], 'GitHub tag mismatch')
                        require((manifest['channel'] == 'prerelease') == bool(release.get('prerelease')),
                                'GitHub channel mismatch')
                        target = match_target(manifest, self.contract, current, self.state()['high_water'])
                        require(target['filename'] in assets, 'target asset missing')
                        name = str(manifest['sequence']) + '-' + target['sha256'][:16]
                        require(name not in self.state()['failed'], 'failed release')
                        candidates.append({'version': manifest['version'], 'sequence': manifest['sequence'],
                                           'manifest_url': assets['application-release.json'],
                                           'signature_url': assets['application-release.json.sig'],
                                           'package_url': assets[target['filename']]})
                except (Rejected, ValueError, OSError, subprocess.SubprocessError):
                    continue
            if len(releases) < 100:
                break
        return max(candidates, key=lambda x: x['sequence']) if candidates else None


def fetch_bytes(url, limit):
    require(url.startswith('https://'), 'HTTPS required')
    request = urllib.request.Request(url, headers={'User-Agent': 'DshanControl-Updater/1'})
    with urllib.request.urlopen(request, timeout=30) as response:
        require(response.geturl().startswith('https://'), 'insecure redirect')
        body = response.read(limit + 1)
    require(len(body) <= limit, 'download exceeds size limit')
    return body


def fetch_asset(url, limit):
    require(url.startswith('https://github.com/' + REPOSITORY + '/releases/download/'),
            'asset outside configured repository')
    return fetch_bytes(url, limit)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', default='/etc/dshancontrol/updater.json')
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('check')
    sub.add_parser('recover')
    p = sub.add_parser('install')
    p.add_argument('manifest')
    p.add_argument('signature')
    p.add_argument('package')
    sub.add_parser('download')
    args = parser.parse_args()
    updater = Updater(read_json(args.config))
    if args.command == 'recover':
        with updater.locked():
            updater.recover()
    elif args.command == 'check':
        print(json.dumps(updater.check_github(), indent=2))
    elif args.command == 'download':
        candidate = updater.check_github()
        require(candidate is not None, 'no compatible qualified update')
        with updater.locked():
            destination = updater.root / 'downloads'
            destination.mkdir(exist_ok=True)
            manifest = fetch_asset(candidate['manifest_url'], 1024*1024)
            (destination / 'application-release.json').write_bytes(manifest)
            (destination / 'application-release.json.sig').write_bytes(fetch_asset(candidate['signature_url'], 16384))
            verify_signature(destination / 'application-release.json', destination / 'application-release.json.sig',
                             updater.config['public_key'])
            m = validate_manifest(read_json(destination / 'application-release.json'), public=True)
            target = match_target(m, updater.contract, read_json(updater.current_path() / 'installed.json'),
                                  updater.state()['high_water'])
            # Stream bounded packages to disk; never keep the application archive in RAM.
            require(candidate['package_url'].endswith('/' + target['filename']), 'asset selection changed')
            require(shutil.disk_usage(str(destination)).free > target['size'] + 16*1024*1024, 'no download space')
            request = urllib.request.Request(candidate['package_url'], headers={'User-Agent': 'DshanControl-Updater/1'})
            partial = destination / '.package.part'
            try:
                with urllib.request.urlopen(request, timeout=30) as response, open(str(partial), 'wb') as output:
                    require(response.geturl().startswith('https://'), 'insecure redirect')
                    total = 0
                    while True:
                        block = response.read(65536)
                        if not block:
                            break
                        total += len(block)
                        require(total <= target['size'], 'download size exceeded')
                        output.write(block)
                    output.flush()
                    os.fsync(output.fileno())
                require(total == target['size'] and digest(partial) == target['sha256'], 'download hash mismatch')
                os.replace(str(partial), str(destination / target['filename']))
                sync_directory(destination)
            finally:
                if partial.exists():
                    partial.unlink()
            print(str(destination))
    else:
        print(updater.install(args.manifest, args.signature, args.package))


if __name__ == '__main__':
    try:
        main()
    except (Rejected, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Update rejected: ' + str(error), file=sys.stderr)
        sys.exit(1)
