#!/usr/bin/env python3
"""Package a verified native i.MX6ULL system as a development draft, never publish it."""
import argparse
import datetime
import gzip
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tarfile


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def run(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def archive(directory, output):
    with output.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w|') as tar:
            tar.add(directory, arcname=directory.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--integration', type=Path, required=True, help='Clean repository containing the pinned release recipe')
    parser.add_argument('--build', type=Path, required=True, help='prepare_imx6ull output directory')
    parser.add_argument('--bsp', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    require(re.fullmatch(r'[0-9][A-Za-z0-9.-]{0,80}', args.version), 'Invalid version')
    source, build, bsp, evidence, output = [p.resolve() for p in (
        args.source, args.build, args.bsp, args.evidence, args.output)]
    require(not output.exists(), 'Refusing to overwrite a delivery')
    require(not run('git', 'status', '--porcelain', cwd=source), 'Source tree is dirty')
    commit = run('git', 'rev-parse', 'HEAD', cwd=source)
    integration = args.integration.resolve()
    require(not run('git', 'status', '--porcelain', cwd=integration), 'Integration tree is dirty')
    integration_commit = run('git', 'rev-parse', 'HEAD', cwd=integration)
    pinned = json.loads((integration / 'dshancontrol/release-inputs.json').read_text())
    require(pinned['application_commit'] == commit, 'Recipe references another application')
    bsp_commit = run('git', 'rev-parse', 'HEAD', cwd=bsp)
    require(not run('git', 'status', '--porcelain', '--untracked-files=normal', cwd=bsp), 'BSP is dirty')
    br = build / 'buildroot'
    validation = json.loads((evidence / 'image-validation/result.json').read_text())
    require(validation['passed'], 'Image checks failed')
    require(validation['build_source']['application_commit'] == commit, 'Wrong packaged source identity')
    require(validation['build_source']['bsp_commit'] == bsp_commit, 'Wrong packaged BSP identity')
    require(validation['build_source']['hardware_commit'] == integration_commit, 'Wrong packaged hardware identity')
    for name, record in validation['artifacts'].items():
        require(sha(br / 'images' / name) == record['sha256'], 'Image changed after validation: ' + name)
    smoke = json.loads((evidence / 'arm-smoke/result.json').read_text())
    require(smoke['passed'], 'ARM userspace smoke failed')
    for check in ('build', 'legal-info', 'host-test', 'hmi-test', 'image-validation', 'arm-smoke'):
        require((evidence / (check + '.exit')).read_text().strip() == '0', check + ' failed')
    output.mkdir(parents=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    prefix = 'DshanControl-v' + args.version
    source_archive = output / (prefix + '-source.tar.gz')
    subprocess.run(['git', 'archive', '--format=tar.gz', '-o', str(source_archive), commit], cwd=source, check=True)
    require(sha(source_archive) == pinned['application_source_sha256'], 'Recipe source archive mismatch')
    require(bsp_commit == pinned['bsp_commit'], 'Recipe BSP mismatch')
    subprocess.run(['git', 'archive', '--format=tar.gz', '-o', str(output / 'hardware-source.tar.gz'), integration_commit], cwd=integration, check=True)
    subprocess.run(['git', 'archive', '--format=tar.gz', '-o', str(output / 'bsp-source.tar.gz'), bsp_commit], cwd=bsp, check=True)
    image_name = prefix + '-imx6ull-pro.img.gz'
    with (br / 'images/sdcard.img').open('rb') as src, (output / image_name).open('wb') as dst:
        with gzip.GzipFile(filename='', mode='wb', fileobj=dst, mtime=0) as compressed:
            shutil.copyfileobj(src, compressed)
    # Entire legal-info outputs carry matching third-party sources, manifests and licenses.
    legal = br / 'legal-info'
    require(legal.is_dir() and (legal / 'manifest.csv').is_file(), 'Missing Buildroot legal manifest')
    archive(legal, output / 'buildroot-corresponding-sources-and-licenses.tar.gz')
    info = run('make', '-s', '-C', str(br), 'show-info')
    write(output / 'buildroot-package-info.json', json.loads(info))
    configs = output / 'build-inputs'
    configs.mkdir()
    shutil.copyfile(br / '.config', configs / 'buildroot.config')
    for pattern, name in [('linux-*/.config', 'linux.config'), ('uboot-*/.config', 'uboot.config')]:
        files = list((br / 'build').glob(pattern))
        require(len(files) == 1, 'Ambiguous configuration: ' + pattern)
        shutil.copyfile(files[0], configs / name)
    shutil.copyfile(build / 'provenance.json', configs / 'source-provenance.json')
    shutil.copyfile(evidence / 'hardware-adoption.json', configs / 'hardware-adoption.json')
    shutil.copytree(build / 'external', configs / 'external', ignore=shutil.ignore_patterns('app-source'))
    recipe = {'application_commit': commit, 'application_version': (source / 'platform/VERSION').read_text().strip(),
              'integration_commit': integration_commit, 'source_archive': source_archive.name,
              'source_archive_sha256': sha(source_archive), 'bsp_commit': bsp_commit,
              'build_command': 'CCACHE=none CCACHE_DISABLE=1 make -C build/buildroot -j8',
              'prepare_command': 'python3 hardware/dshancontrol/prepare.py --application APPLICATION --output NEW_BUILD',
              'source_policy': 'git archive of the clean pinned commit; only verified source downloads reused'}
    write(configs / 'recipe-lock.json', recipe)
    archive(configs, output / 'build-inputs.tar.gz')
    shutil.rmtree(configs)
    logs = output / 'verification-evidence'
    logs.mkdir()
    for directory in ('image-validation', 'arm-smoke'):
        shutil.copytree(evidence / directory, logs / directory)
    shutil.copyfile(evidence / 'hardware-guard-tests.json', logs / 'hardware-guard-tests.json')
    for name in ('build', 'legal-info', 'host-test', 'hmi-test'):
        for suffix in ('.log', '.exit'):
            shutil.copyfile(evidence / (name + suffix), logs / (name + suffix))
    archive(logs, output / 'verification-evidence.tar.gz')
    shutil.rmtree(logs)
    image_hash = validation['artifacts']['sdcard.img']['sha256']
    profile = {'board_id': 'imx6ull-pro', 'profile_version': 1, 'status': 'development-draft',
               'soc': 'NXP i.MX6ULL', 'hardware_revision': 'unverified', 'ram_mib': 512,
               'architecture': 'armv7-a', 'float_abi': 'hard', 'display': [1024, 600],
               'touch': 'Goodix', 'storage': {'boot_device': 'eMMC mmc1', 'partitions': validation['partitions'],
               'data_policy': 'Fresh image has two partitions and rootfs /data fallback; existing p3 must be preserved separately; never dd the whole image over an installation to preserve data'},
               'installer': {'entrypoint': 'https://github.com/dshanpi/lynx-releases',
                             'nxp_software_download_mode': 'not-supported-by-current-MCP',
                             'this_image_installation': 'not_run'}}
    write(output / 'board-profile-imx6ull-pro.json', profile)
    source_files = {p.relative_to(legal).as_posix(): {'sha256': sha(p), 'bytes': p.stat().st_size}
                    for p in sorted(legal.rglob('*')) if p.is_file() and not p.is_symlink()}
    sdk = {'standard': 'DSP-LINUX-001/1.1.0', 'bsp_repository': 'https://github.com/100askTeam/aibsp-imx6ull-pro_linux5.4.47',
           'bsp_commit': bsp_commit, 'submodule_identities': run('git', 'submodule', 'status', cwd=bsp).splitlines(),
           'packages': 'buildroot-package-info.json', 'corresponding_sources': 'buildroot-corresponding-sources-and-licenses.tar.gz',
           'legal_files': source_files, 'overlays': {'repository': 'dshanpi/aibsp-imx6ull-pro_linux5.4.47', 'commit': integration_commit,
           'path': 'dshancontrol/board/buildroot', 'archive': 'build-inputs.tar.gz'},
           'source_redistribution_review': 'Buildroot legal-info captured; vendor binary redistribution audit pending',
           'excluded_inputs': ['previous build/host/target/images outputs', 'private laboratory configuration and signing keys']}
    write(output / 'sdk-manifest-imx6ull-pro.json', sdk)
    compiler = br / 'host/bin/arm-linux-gcc'
    build_info = dict(recipe, release_kind='system', date_utc=stamp, image_sha256=image_hash,
                      host=run('uname', '-a'), compiler=run(str(compiler), '--version'),
                      build_scope='Fresh output/toolchain extraction/target and full image; reused downloaded source archives; hardware source/recipe equivalence checked before image generation',
                      byte_reproducibility='not_tested', system_contract='unprovisioned',
                      auto_update_eligible=False, image_content=validation['installed_files_sha256'])
    write(output / 'build-info-imx6ull-pro.json', build_info)
    checks = {name: {'status': 'passed', 'tester': 'Codex automated checks', 'date_utc': stamp,
                    'evidence': 'verification-evidence.tar.gz'}
              for name in ('host-tests', 'hmi-tests', 'target-build', 'image-content', 'arm-userspace-smoke')}
    for name in ('new-image-flash-and-boot', 'new-image-touch', 'factory-to-ota-chain',
                 'physical-power-loss', 'external-fieldbus-slaves', 'cloud-tuya-production', 'voice-sdk', 'soak'):
        checks[name] = {'status': 'not_run', 'reason': 'Not qualified on this packaged image'}
    write(output / 'validation-imx6ull-pro.json', {'release_kind': 'system', 'image_sha256': image_hash,
          'checks': checks, 'stage': 'draft', 'auto_update_eligible': False,
          'previous_board_evidence': 'docs/validation/imx6ull-native-20261125 belongs to separately deployed laboratory files; not this complete image'})
    notes = '''# i.MX6ULL Pro native system development draft

This is a complete two-partition eMMC development image, not an application OTA package.
It includes the native C++ API, desktop, media, fieldbus and lifecycle binaries, Goodix discovery fix,
Qt linuxfb, Wi-Fi firmware and Asia/Shanghai timezone. No target Python runtime is installed.

The local desktop/API start from the factory /usr payload. The privileged runtime/OTA remains
unprovisioned: no production system contract, signing trust or per-device maintenance configuration
is embedded. Hardware RPC/OTA requiring that service is unavailable until separately provisioned.
The retained lifecycle adapter validates only the documented laboratory storage layout; it is not
a universal industrial maintenance interlock. Do not copy the old board CID/config to another unit.
Tuya gateway source is included; it is a separately deployed Go server, not a configured cloud service.

The new full image has NOT been flashed or booted. Earlier native-service board acceptance is separate.
No real fieldbus slaves, cloud/Tuya credentials, voice SDK, physical power-loss matrix or soak qualification.
This release stays Draft and is excluded from automatic update sources.

Verify SHA256SUMS before extracting the .img.gz. Installation/recovery tooling entrypoint:
https://github.com/dshanpi/lynx-releases . Current MCP has no NXP software-download transition.
The whole disk image replaces the partition table; it does not preserve existing p3/user data.
The existing authorized board network-maintenance procedure requires verified identity/layout/backups,
an independent RAM environment, unmounted p1/p2, bounded writes and readback. It is NOT automatically
applicable to this differently sized disk image. No board write was performed for this delivery.

Source inputs, patches, exact configurations, third-party sources/licenses and test logs accompany
this draft. Buildroot legal-info warnings and vendor redistribution audit remain review items.
No bit-for-bit reproducibility or production readiness is claimed.
'''
    (output / 'RELEASE_NOTES.md').write_text(notes)
    # Scan payload and new delivery for private material; upstream source tests may contain fixture keys,
    # so the stricter private-key test applies to the installed target rather than dependency test suites.
    findings = []
    for path in (br / 'target').rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(br / 'target').as_posix()
        if relative.startswith(('root/.ssh/', 'data/')) or 'imx6ull-maintenance.json' in relative:
            findings.append(relative)
        with path.open('rb') as stream:
            tail = b''
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                value = tail + block
                if re.search(rb'(?:\A|\n)-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----\r?\n', value):
                    findings.append(relative)
                tail = value[-64:]
    require(not findings, 'Private material found in target: ' + ', '.join(sorted(set(findings))))
    write(output / 'privacy-review.json', {'target_private_key_scan': 'passed',
          'target_data_and_ssh_scan': 'passed', 'lab_provisioning_embedded': False,
          'scope': 'Fresh source-only output; no live-board backup or configuration used'})
    artifacts = [{'filename': p.name, 'sha256': sha(p), 'bytes': p.stat().st_size}
                 for p in sorted(output.iterdir()) if p.is_file()]
    require(all(a['bytes'] < 2 * 1024 ** 3 for a in artifacts), 'GitHub asset exceeds 2 GiB')
    lock = {'schema': 'dshancontrol-imx6ull-system-draft-v1', 'standard': 'DSP-LINUX-001/1.1.0',
            'release_kind': 'system', 'version': args.version,
            'tag': 'system-v' + args.version + '--imx6ull-pro', 'application_commit': commit,
            'application_version': recipe['application_version'],
            'integration_commit': integration_commit, 'integration_repository': 'dshanpi/aibsp-imx6ull-pro_linux5.4.47',
            'integration_tag': 'imx6ull-pro-integration-v' + args.version,
            'integration_boundary': 'BSP and board overlay/recipe in hardware repository; application pinned independently',
            'bsp_commit': bsp_commit, 'image': {'filename': image_name, 'sha256': sha(output / image_name),
            'uncompressed_sha256': image_hash, 'uncompressed_bytes': (br / 'images/sdcard.img').stat().st_size},
            'auto_update_eligible': False, 'artifacts': artifacts}
    write(output / 'release-lock.json', lock)
    (output / 'SHA256SUMS').write_text(''.join(sha(p) + '  ' + p.name + '\n'
                                           for p in sorted(output.iterdir()) if p.is_file()))
    print(json.dumps({'output': str(output), 'tag': lock['tag'], 'files': len(artifacts) + 2,
                      'image_sha256': image_hash}))


if __name__ == '__main__':
    main()
