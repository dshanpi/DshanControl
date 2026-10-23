#!/usr/bin/env python3
"""Create a signed DshanControl application draft from a clean committed tree."""
import argparse
import json
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/updater'))
from update_protocol import TARGET_FIELDS, digest, read_json, require, validate_manifest, validate_target
sys.path.insert(0, str(ROOT / 'scripts'))
from application_payload import payload, component_inputs
from check_s4_media import verify as verify_s4_media


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hmi', required=True, type=Path)
    p.add_argument('--desktop-service', type=Path)
    p.add_argument('--media-worker', type=Path)
    p.add_argument('--imageformats', type=Path)
    p.add_argument('--target-contract', required=True, type=Path)
    p.add_argument('--build-info', required=True, type=Path)
    p.add_argument('--version', required=True)
    p.add_argument('--sequence', required=True, type=int)
    p.add_argument('--from-version', action='append', required=True)
    p.add_argument('--channel', choices=['stable', 'prerelease'], default='prerelease')
    p.add_argument('--signing-key', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    contract = read_json(a.target_contract)
    validate_target(contract)
    components = component_inputs(a.desktop_service, a.media_worker, a.imageformats)
    if components:
        verify_s4_media(a.media_worker)
    require(not components or contract.get('desktop_provider') is True,
            'desktop bundle requires an explicitly qualified desktop runtime contract')
    status = subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], universal_newlines=True)
    require(not status.strip(), 'commit reviewed changes before packaging; dirty source is not releasable')
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], universal_newlines=True).strip()
    require((ROOT / 'platform/VERSION').read_text().strip() == a.version, 'version differs from committed source')
    build = read_json(a.build_info)
    require(build.get('application_commit') == commit and build.get('application_dirty') is False,
            'native artifact must come from this clean application commit')
    require(build.get('hmi_sha256') == digest(a.hmi), 'native artifact differs from build record')
    require(build.get('component_sha256', {}) == {name: digest(path) for name, path in components.items()},
            'desktop artifacts differ from the clean target build record')
    require(build.get('target') == {k: contract[k] for k in TARGET_FIELDS}, 'native build target mismatch')
    # Native payload ABI must agree with its target contract (runtime dependencies
    # and exact sysroot are additionally validated in target-build evidence).
    for artifact in [a.hmi] + list(components.values()):
        elf = subprocess.check_output(['readelf', '-h', str(artifact)], universal_newlines=True)
        attrs = subprocess.check_output(['readelf', '-A', str(artifact)], universal_newlines=True)
        if contract['architecture'] == 'armv7-a':
            require('Machine:' in elf and 'ARM' in elf and 'ELF32' in elf, 'HMI architecture mismatch')
            hard = 'Tag_ABI_VFP_args: VFP registers' in attrs or 'hard-float ABI' in elf
            require(hard == (contract['float_abi'] == 'hard'), 'HMI float ABI mismatch')
        else:
            require(False, 'native ABI validator for this architecture is not implemented')
    a.output.mkdir(parents=True, exist_ok=False)
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp) / 'payload'
        stage.mkdir()
        payload(ROOT, stage, a.hmi, a.desktop_service, a.media_worker, a.imageformats)
        filename = 'DshanControl-app-' + a.version + '-' + contract['board_id'] + '.tar.gz'
        archive = a.output / filename
        with tarfile.open(str(archive), 'w:gz') as tar:
            for file in sorted(stage.rglob('*')):
                require(not file.is_symlink(), 'source symlink not allowed')
                tar.add(str(file), arcname=str(file.relative_to(stage)), recursive=False)
        unpacked = sum(f.stat().st_size for f in stage.rglob('*') if f.is_file())
    source = a.output / ('DshanControl-source-' + a.version + '.tar.gz')
    subprocess.run(['git', '-C', str(ROOT), 'archive', '--format=tar.gz', '--output=' + str(source.resolve()), commit], check=True)
    build_asset = a.output / 'application-build-info.json'
    build_asset.write_bytes(a.build_info.read_bytes())
    target = {k: contract[k] for k in TARGET_FIELDS}
    target.update(filename=filename, sha256=digest(archive), size=archive.stat().st_size,
                  unpacked_size=unpacked, validation={})
    manifest = dict(schema_version=1, app_id='dshancontrol', repository='dshanpi/DshanControl',
                    release_kind='application', release_state='draft', channel=a.channel,
                    minimum_updater_version='0.1.0',
                    version=a.version, tag='app-v' + a.version, commit=commit, sequence=a.sequence,
                    data_schema=contract['data_schema'], from_versions=a.from_version,
                    source={'filename': source.name, 'sha256': digest(source)},
                    build_info={'filename': build_asset.name, 'sha256': digest(build_asset)}, targets=[target])
    validate_manifest(manifest)
    path = a.output / 'application-release.json'
    path.write_text(json.dumps(manifest, indent=2) + '\n')
    subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(a.signing_key),
                    '-out', str(path) + '.sig', str(path)], check=True)
    (a.output / 'SHA256SUMS').write_text(''.join(digest(f) + '  ' + f.name + '\n'
                                                for f in sorted(a.output.iterdir()) if f.is_file()))
    print('Signed draft only: ' + str(a.output))


if __name__ == '__main__':
    main()
