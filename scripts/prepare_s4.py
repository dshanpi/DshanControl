#!/usr/bin/env python3
"""Prepare an isolated S4 Buildroot output from the specified hardware checkout."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path)] + list(args), universal_newlines=True).strip()


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', required=True, type=Path)
    p.add_argument('--hardware', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    sdk, hw, out = a.sdk.resolve(), a.hardware.resolve(), a.output.resolve()
    br = sdk / 'buildroot/buildroot-201902'
    baseline = hw / 'device/config/chips/t113_s4/configs/100ask'
    if not (br / 'Makefile').is_file() or not baseline.is_dir():
        p.error('requires Tina Buildroot 201902 and the S4 100ask hardware checkout')
    import re
    if not re.search(r'^storage_type\s*=\s*2\s*$', (baseline / 'sys_config.fex').read_text(), re.M):
        p.error('unexpected storage configuration; refusing SPI NAND/unknown baseline')
    remote = git(hw, 'remote', 'get-url', 'origin')
    if 'DongshanPI/100ASK_T113s4-SdNand_TinaSDK5' not in remote:
        p.error('hardware source must be the approved S4 SD NAND repository')
    if out.exists():
        p.error('output already exists; use a new isolated output')
    out.mkdir(parents=True)
    external = out / 'external'
    shutil.copytree(str(ROOT / 'boards/t113s4-sdnand/buildroot'), str(external))
    source = external / 'app-source'
    source.mkdir()
    for name in ('components', 'scripts', 'boards/t113s4-sdnand', 'platform'):
        shutil.copytree(str(ROOT / name), str(source / name),
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    shutil.copyfile(str(ROOT / 'LICENSE'), str(source / 'LICENSE'))
    shutil.copytree(str(baseline), str(out / 'board'))
    hardware_files = {str(f.relative_to(baseline)): file_hash(f)
                      for f in sorted(baseline.rglob('*')) if f.is_file()}
    source_files = {str(f.relative_to(source)): file_hash(f)
                    for f in sorted(source.rglob('*')) if f.is_file()}
    provenance = {'schema_version': 1, 'release_kind': 'system', 'status': 'development-draft',
                  'version': (ROOT / 'platform/VERSION').read_text().strip(),
                  'application_commit': git(ROOT, 'rev-parse', 'HEAD'),
                  'application_dirty': bool(git(ROOT, 'status', '--porcelain', '--untracked-files=normal')),
                  'application_snapshot_files': source_files,
                  'hardware_repository': remote, 'hardware_commit': git(hw, 'rev-parse', 'HEAD'),
                  'hardware_board_files': hardware_files,
                  'sdk_path': str(sdk), 'sdk_manifest_complete': False,
                  'board': 't113_s4/100ask', 'storage': 'sdnand', 'buildroot': '201902',
                  'kernel': 'linux-5.4', 'board_validation': 'not-run'}
    text = json.dumps(provenance, indent=2) + '\n'
    (out / 'build-provenance.json').write_text(text)
    (source / 'build-provenance.json').write_text(text)
    config = (hw / 'buildroot/buildroot-201902/configs/sun8iw20p1_t113_defconfig').read_text()
    # Merge by Kconfig key, avoiding duplicate assignments from a simple append.
    lines = config.splitlines()
    toolchain = sdk / 'out/toolchain/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabi'
    if not (toolchain / 'bin/arm-linux-gnueabi-gcc').is_file():
        p.error('SDK Linaro 5.3.1 toolchain missing; refusing an implicit toolchain download')
    fragments = (ROOT / 'boards/t113s4-sdnand/buildroot/dshancontrol.fragment').read_text().splitlines()
    fragments += ['BR2_TOOLCHAIN_EXTERNAL_CUSTOM=y', 'BR2_TOOLCHAIN_EXTERNAL_PREINSTALLED=y',
                  'BR2_TOOLCHAIN_EXTERNAL_PATH="' + str(toolchain) + '"',
                  'BR2_TOOLCHAIN_EXTERNAL_CUSTOM_PREFIX="arm-linux-gnueabi"',
                  'BR2_TOOLCHAIN_EXTERNAL_GCC_5=y', 'BR2_TOOLCHAIN_EXTERNAL_HEADERS_4_0=y',
                  'BR2_TOOLCHAIN_EXTERNAL_CUSTOM_GLIBC=y', 'BR2_TOOLCHAIN_EXTERNAL_CXX=y',
                  'BR2_TOOLCHAIN_EXTERNAL_HAS_SSP=y']
    for fragment in fragments:
        if not fragment.startswith('BR2_'):
            continue
        key = fragment.split('=', 1)[0]
        lines = [line for line in lines if not line.startswith(key + '=') and line != '# ' + key + ' is not set']
        lines.append(fragment)
    (out / 'dshancontrol_defconfig').write_text('\n'.join(lines) + '\n')
    # This command configures/builds userspace only. It does not change .buildconfig,
    # copy T153 BSP, flash, or pretend to build a complete board image.
    command = ['make', '-C', str(br), 'O=' + str(out / 'buildroot'),
               'BR2_EXTERNAL=' + str(external), 'BR2_DEFCONFIG=' + str(out / 'dshancontrol_defconfig')]
    import shlex
    (out / 'configure.sh').write_text('#!/bin/sh\nset -eu\n' + ' '.join(shlex.quote(x) for x in command + ['defconfig']) + '\n')
    (out / 'configure.sh').chmod(0o755)
    print(out)
    print('Configure: ' + str(out / 'configure.sh'))
    print('Build: ' + ' '.join(shlex.quote(x) for x in command + ['dshancontrol']))
    print('Full system image requires SDK manifest capture, S4 kernel/boot integration and board acceptance.')


if __name__ == '__main__':
    main()
