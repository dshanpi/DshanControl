#!/usr/bin/env python3
"""Build prepared S4 userspace with explicit board variables and input checks."""
import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path

from capture_sdk_inputs import capture, sha_file


def command(prepared, jobs, target):
    provenance = json.loads((prepared / 'build-provenance.json').read_text())
    sdk = Path(provenance['sdk_path'])
    if (provenance['board'], provenance['storage'], provenance['buildroot']) != ('t113_s4/100ask', 'sdnand', '201902'):
        raise ValueError('not an approved S4 Buildroot preparation')
    for root, field in ((prepared / 'external/app-source', 'application_snapshot_files'),
                        (prepared / 'board', 'hardware_board_files')):
        for relative, expected in provenance[field].items():
            if sha_file(root / relative) != expected:
                raise ValueError('prepared input changed: ' + relative)
    env = {key: value for key, value in os.environ.items() if not key.startswith(('LICHEE_', 'TINA_'))}
    values = dict(LICHEE_TOP_DIR=str(sdk), LICHEE_ARCH='arm', LICHEE_IC='t113_s4',
                  LICHEE_CHIP='sun8iw20p1', LICHEE_BOARD='100ask', LICHEE_FLASH='default',
                  LICHEE_PLATFORM='linux', LICHEE_LINUX_DEV='buildroot',
                  LICHEE_BUILDING_SYSTEM='buildroot', LICHEE_KERN_VER='linux-5.4',
                  LICHEE_CROSS_COMPILER='arm-linux-gnueabi', LICHEE_USE_INDEPENDENT_BSP='',
                  LICHEE_CBBPKG_DIR=str(sdk / 'platform'), LICHEE_BSP_DIR=str(sdk / 'bsp'),
                  LICHEE_OUT_DIR=str(prepared / 'bsp-out'), LICHEE_BR_OUT=str(prepared / 'buildroot'))
    env.update(values)
    argv = ['make', '-C', str(sdk / 'buildroot/buildroot-201902'),
            'O=' + str(prepared / 'buildroot'), 'BR2_EXTERNAL=' + str(prepared / 'external'),
            'BR2_DEFCONFIG=' + str(prepared / 'dshancontrol_defconfig'), 'BR2_JLEVEL=' + str(jobs)]
    argv += [key + '=' + value for key, value in sorted(values.items())]
    argv += [{'configure':'defconfig', 'application':'dshancontrol', 'rootfs':'all'}[target]]
    return argv, env, sdk


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepared', required=True, type=Path)
    parser.add_argument('--jobs', default=4, type=int)
    parser.add_argument('target', choices=['configure', 'application', 'rootfs'])
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('jobs must be positive')
    out = args.prepared.resolve()
    argv, env, sdk = command(out, args.jobs, args.target)
    # Keep a conservative reserve before a full vendor build, which includes
    # multimedia packages and image packing intermediates. No automatic cleanup.
    if args.target == 'rootfs' and shutil.disk_usage(out).free < 16 * 1024**3:
        parser.error('full rootfs build requires at least 16 GiB free; select a larger output volume')
    evidence = out / 'validation'
    evidence.mkdir(exist_ok=True)
    record = {'command': argv, 'target': args.target, 'status':'running', 'board_validation':'not-run'}
    report = evidence / (args.target + '-build.json')
    report.write_text(json.dumps(record, indent=2) + '\n')
    if args.target == 'rootfs':
        (evidence / 'sdk-inputs-before.json').write_text(json.dumps(capture(sdk), indent=2) + '\n')
    with (evidence / (args.target + '-build.log')).open('w') as log:
        result = subprocess.run(argv, env=env, stdout=log, stderr=subprocess.STDOUT)
    record.update(exit_code=result.returncode, status='passed' if result.returncode == 0 else 'failed')
    if args.target == 'rootfs' and result.returncode == 0:
        image = out / 'buildroot/images/rootfs.ext4'
        if not image.is_file():
            record.update(status='failed', error='rootfs.ext4 not produced', exit_code=1)
        else:
            record.update(rootfs_sha256=sha_file(image), rootfs_size=image.stat().st_size)
        (evidence / 'sdk-inputs-after.json').write_text(json.dumps(capture(sdk), indent=2) + '\n')
    report.write_text(json.dumps(record, indent=2) + '\n')
    print('{}: {}; log: {}'.format(args.target, record['status'], evidence / (args.target + '-build.log')))
    raise SystemExit(record['exit_code'])


if __name__ == '__main__':
    main()
