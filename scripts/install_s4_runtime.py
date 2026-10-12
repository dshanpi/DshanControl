#!/usr/bin/env python3
"""Buildroot target installation. Never runs services or touches host /etc."""
import argparse
import json
import shutil
from pathlib import Path


def payload(source, destination, hmi):
    mapping = [('components/core/src/omnigate_core', 'lib/core/omnigate_core'),
               ('components/web/app.py', 'lib/web/app.py'),
               ('components/web/hmi_api.py', 'lib/web/hmi_api.py'),
               ('components/web/assets', 'share/web'),
               ('components/supervisor/supervisor.py', 'lib/supervisor/supervisor.py')]
    for src, dst in mapping:
        target = destination / dst
        target.parent.mkdir(parents=True, exist_ok=True)
        if (source / src).is_dir():
            shutil.copytree(str(source / src), str(target), ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        else:
            shutil.copy2(str(source / src), str(target))
    (destination / 'bin').mkdir(exist_ok=True)
    shutil.copy2(str(hmi), str(destination / 'bin/omnigate-hmi'))
    (destination / 'bin/omnigate-hmi').chmod(0o755)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', required=True, type=Path)
    p.add_argument('--target', required=True, type=Path)
    p.add_argument('--hmi', required=True, type=Path)
    args = p.parse_args()
    src, target = args.source, args.target.resolve()
    if str(target) == '/':
        p.error('refusing host root')
    board = src / 'boards/t113s4-sdnand'
    factory = target / 'usr/share/dshancontrol/factory'
    if factory.exists():
        shutil.rmtree(str(factory))
    payload(src, factory, args.hmi)
    provenance = json.loads((src / 'build-provenance.json').read_text())
    installed = {'version': provenance['version'], 'commit': provenance['application_commit'],
                 'sequence': 0, 'data_schema': 1}
    (factory / 'installed.json').write_text(json.dumps(installed, indent=2) + '\n')
    files = [(board / 'runtime/S71dshancontrol-update', 'etc/init.d/S71dshancontrol-update', 0o755),
             (board / 'platform.json', 'etc/omnigate/platform.json', 0o644),
             (board / 'system-contract.json', 'etc/dshancontrol/system-release.json', 0o644),
             (src / 'build-provenance.json', 'etc/dshancontrol/build-info.json', 0o644),
             (board / 'runtime/S70dshancontrol', 'etc/init.d/S70dshancontrol', 0o755),
             (board / 'runtime/dshancontrol-services', 'usr/libexec/dshancontrol-services', 0o755),
             (board / 'runtime/display.env', 'etc/dshancontrol/display.env', 0o644),
             (src / 'components/web/config/config.json', 'usr/share/dshancontrol/defaults/web.json', 0o600),
             (src / 'components/hmi/hmi-config.json', 'usr/share/dshancontrol/defaults/hmi.json', 0o600)]
    for path in (src / 'components/updater').glob('*.py'):
        files.append((path, 'usr/lib/dshancontrol-update/' + path.name, 0o644))
    for source, name, mode in files:
        destination = target / name
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
    (target / 'etc/dshancontrol/updater.json').write_text(json.dumps(config, indent=2) + '\n')
    executable = target / 'usr/bin/dshancontrol-update'
    executable.parent.mkdir(parents=True, exist_ok=True)
    executable.write_text('#!/bin/sh\nexec python3 /usr/lib/dshancontrol-update/updater.py "$@"\n')
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
