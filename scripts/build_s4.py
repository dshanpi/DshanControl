#!/usr/bin/env python3
"""Build prepared S4 userspace with explicit board variables and input checks."""
import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path

from capture_sdk_inputs import capture, sha_file
from audit_s4_system import ext4_image_bytes


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



def build_policy(prepared, target, reuse_dependencies=False):
    if reuse_dependencies and target != 'rootfs':
        raise ValueError('--reuse-dependencies applies only to rootfs')
    policy = {'reused_target_dependencies': reuse_dependencies,
              'full_clean_sdk_rebuild': False}
    if target != 'rootfs':
        return policy
    reserve = 16 * 1024**3
    if reuse_dependencies:
        br = prepared / 'buildroot'
        qmake = br / 'host/bin/qmake'
        config = br / '.config'
        core = [x for x in (br / 'target/usr/lib').glob('libQt5Core.so.*')
                if x.is_file() and not x.is_symlink()]
        libc = list((br / 'target/lib').glob('libc-*.so'))
        stamps = list((br / 'build').glob('qt5base-*/.stamp_target_installed'))
        if (not qmake.is_file() or not os.access(str(qmake), os.X_OK)
                or not config.is_file() or len(core) != 1 or len(libc) != 1 or not stamps):
            raise ValueError('dependency reuse requires installed Qt/toolchain/libc baseline')
        image = br / 'images/rootfs.ext4'
        if br.resolve() not in image.resolve().parents:
            raise ValueError('baseline rootfs escapes prepared output')
        image_bytes = ext4_image_bytes(image)
        # Existing dependencies avoid a full SDK compile; retain room for the
        # new rootfs, its sanitized copy, vendor container and package objects.
        reserve = max(4 * 1024**3, 6 * image_bytes + 1024**3)
        policy.update(baseline_rootfs_bytes=image_bytes,
                      baseline_rootfs_sha256=sha_file(image),
                      reused_baseline_files={str(x.relative_to(br)): sha_file(x)
                                             for x in [qmake, config] + core + libc},
                      installed_qt_stamps=[str(x.relative_to(br)) for x in stamps])
    policy['minimum_free_bytes'] = reserve
    policy['free_bytes_at_preflight'] = shutil.disk_usage(prepared).free
    if policy['free_bytes_at_preflight'] < reserve:
        raise ValueError('rootfs build requires at least {} GiB free for selected mode'.format(
            (reserve + 1024**3 - 1) // 1024**3))
    return policy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepared', required=True, type=Path)
    parser.add_argument('--jobs', default=4, type=int)
    parser.add_argument('--reuse-dependencies', action='store_true',
                        help='incremental rootfs using a verified installed dependency baseline')
    parser.add_argument('target', choices=['configure', 'application', 'rootfs'])
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('jobs must be positive')
    out = args.prepared.resolve()
    argv, env, sdk = command(out, args.jobs, args.target)
    # Full builds retain the existing 16 GiB reserve. Explicit incremental mode
    # requires an existing dependency/image baseline and records exactly what it reuses.
    try:
        policy = build_policy(out, args.target, args.reuse_dependencies)
    except ValueError as error:
        parser.error(str(error))
    evidence = out / 'validation'
    evidence.mkdir(exist_ok=True)
    record = {'command': argv, 'target': args.target, 'status':'running', 'board_validation':'not-run', 'build_policy': policy}
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
