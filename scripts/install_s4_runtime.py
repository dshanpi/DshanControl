#!/usr/bin/env python3
"""Buildroot target installation. Never runs services or touches host /etc."""
import argparse
import json
import shutil
from pathlib import Path
from check_hmi_resources import verify as verify_hmi_resources


from application_payload import payload


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', required=True, type=Path)
    p.add_argument('--target', required=True, type=Path)
    p.add_argument('--hmi', required=True, type=Path)
    p.add_argument('--runtime', type=Path,
                   help='target C++ system runtime; omitted only for legacy image reproduction')
    args = p.parse_args()
    src, target = args.source, args.target.resolve()
    if str(target) == '/':
        p.error('refusing host root')
    if args.runtime and (not args.runtime.is_file() or not args.runtime.stat().st_mode & 0o111):
        p.error('native runtime must be an executable build artifact')
    verify_hmi_resources(src)
    board = src / 'boards/t113s4-sdnand'
    factory = target / 'usr/share/dshancontrol/factory'
    provenance = json.loads((src / 'build-provenance.json').read_text())
    installed = {'version': provenance['version'], 'commit': provenance['application_commit'],
                 'sequence': 0, 'data_schema': 1}
    files = [(board / 'runtime/S71dshancontrol-update', 'etc/init.d/S71dshancontrol-update', 0o755),
             (board / 'platform.json', 'etc/omnigate/platform.json', 0o644),
             (board / 'system-contract.json', 'etc/dshancontrol/system-release.json', 0o644),
             (src / 'build-provenance.json', 'etc/dshancontrol/build-info.json', 0o644),
             (board / 'runtime/S70dshancontrol', 'etc/init.d/S70dshancontrol', 0o755),
             (board / 'runtime/dshancontrol-services', 'usr/libexec/dshancontrol-services', 0o755),
             (board / 'runtime/display.env', 'etc/dshancontrol/display.env', 0o644),
             (src / 'components/web/config/config.json', 'usr/share/dshancontrol/defaults/web.json', 0o600),
             (src / 'components/hmi/hmi-config.json', 'usr/share/dshancontrol/defaults/hmi.json', 0o600)]
    files.append((board / 'runtime/S60mount_udisk', 'etc/init.d/S60mount_udisk', 0o755))
    files.append((board / 'runtime/S72dshancontrol-wifi', 'etc/init.d/S72dshancontrol-wifi', 0o755))
    files.append((board / 'runtime/S73dshancontrol-time', 'etc/init.d/S73dshancontrol-time', 0o755))
    for name in ('health.py', 'initialize.py', 'storage.py'):
        files.append((board / 'runtime' / name, 'usr/lib/dshancontrol-update/' + name, 0o644))
    for path in (src / 'components/updater').glob('*.py'):
        files.append((path, 'usr/lib/dshancontrol-update/' + path.name, 0o644))
    if args.runtime:
        files += [(args.runtime, 'usr/bin/dshancontrol-runtime', 0o755),
                  (board / 'runtime/S69dshancontrol-runtime',
                   'etc/init.d/S69dshancontrol-runtime', 0o755)]
    # Check every leaf and parent before replacing the old factory or writing
    # any files. In particular, config symlinks must not escape the staging root.
    destinations = [name for _, name, _ in files]
    destinations += ['usr/share/dshancontrol/factory', 'usr/bin/dshancontrol-update',
                     'usr/bin/dshancontrol-runtime', 'etc/dshancontrol/updater.json',
                     'etc/dshancontrol/platform-service.json']
    for relative in destinations:
        path = target / relative
        for part in (path,) + tuple(path.parents):
            if part == target:
                break
            if part.is_symlink():
                p.error('refusing symlink staging destination: ' + relative)
    if not args.runtime and (target / 'usr/bin/dshancontrol-runtime').exists():
        p.error('native target requires --runtime; refusing mixed legacy installation')
    if factory.exists():
        shutil.rmtree(str(factory))
    payload(src, factory, args.hmi)
    (factory / 'installed.json').write_text(json.dumps(installed, indent=2) + '\n')
    for source, name, mode in files:
        destination = target / name
        if destination.is_symlink():
            p.error('refusing symlink staging destination: ' + name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(source), str(destination))
        destination.chmod(mode)
    config = {'application_root': '/mnt/UDISK/dshancontrol/apps',
              'system_contract': '/etc/dshancontrol/system-release.json',
              'public_key': '/etc/dshancontrol/update-public.pem',
              'service_hook': '/usr/libexec/dshancontrol-services',
              'development_mode': False, 'automatic_install': False, 'poll_interval_seconds': 3600,
              'health_observation_seconds': 30, 'startup_timeout_seconds': 30,
              'health_interval_seconds': 2}
    if args.runtime:
        config.update(application_health={'pid_root': '/var/run/dshancontrol/processes',
                                          'http_url': 'http://127.0.0.1/api/health',
                                          'require_hmi_heartbeat': True,
                                          'hmi_heartbeat': '/var/run/dshancontrol/hmi/heartbeat.json',
                                          'launch_receipts_required': True,
                                          'native_supervisor': True},
                      task_service=True,
                      control_lock='/mnt/UDISK/dshancontrol/apps/.control.lock')
        service = {'socket': '/var/run/dshancontrol/platform.sock',
                   'updater_config': '/etc/dshancontrol/updater.json',
                   'task_root': '/mnt/UDISK/dshancontrol/update-tasks',
                   'client_gid': 400, 'allowed_uids': [0, 400, 401],
                   'peer_policy': 's4-roles-v1',
                   'supervision': {'enabled': True, 'interval_seconds': 10,
                                   'failure_threshold': 3, 'timeout_seconds': 10},
                   'platform_manifest': '/etc/omnigate/platform.json',
                   'display': {'channel': 'display0', 'backend': 'sunxi-disp2', 'screen': 0}}
        (target / 'etc/dshancontrol/platform-service.json').write_text(json.dumps(service, indent=2) + '\n')
        (target / 'etc/dshancontrol/platform-service.json').chmod(0o600)
    (target / 'etc/dshancontrol/updater.json').write_text(json.dumps(config, indent=2) + '\n')
    (target / 'etc/dshancontrol/updater.json').chmod(0o600)
    executable = target / 'usr/bin/dshancontrol-update'
    executable.parent.mkdir(parents=True, exist_ok=True)
    command = ('/usr/bin/dshancontrol-runtime' if args.runtime else
               'python3 /usr/lib/dshancontrol-update/updater.py')
    executable.write_text('#!/bin/sh\nexec ' + command + ' "$@"\n')
    executable.chmod(0o755)
    # S4 has no verified CAN/cellular mapping; do not inherit T153 physical defaults.
    defaults = target / 'usr/share/dshancontrol/defaults/web.json'
    value = json.loads(defaults.read_text())
    value['can'] = {}
    value['canopen'] = {'nodes': {}}
    value['ec20'] = {'profiles': {}}
    defaults.write_text(json.dumps(value, indent=2) + '\n')
    # No production key, invented board revision, or guessed runtime ABI is provisioned.
    # Until system contract/key/maintenance hook are qualified, installation fails closed.


if __name__ == '__main__':
    main()
