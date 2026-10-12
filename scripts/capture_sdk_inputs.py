#!/usr/bin/env python3
"""Record all repo SDK component commits and local input hashes without changing them."""
import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path, PurePosixPath


def run(path, *args):
    return subprocess.check_output(['git', '-C', str(path)] + list(args), stderr=subprocess.PIPE)


def sha_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def project_paths(sdk):
    files, projects = {}, {}
    def parse(path):
        resolved = path.resolve()
        if resolved in files:
            raise ValueError('duplicate/recursive manifest include')
        files[resolved] = sha_file(resolved)
        for element in ET.parse(str(resolved)).getroot():
            if element.tag == 'include':
                include = PurePosixPath(element.attrib['name'])
                if include.is_absolute() or '..' in include.parts:
                    raise ValueError('unsafe manifest include')
                parse(resolved.parent / str(include))
            elif element.tag == 'project':
                name = element.get('path', element.get('name'))
                relative = PurePosixPath(name)
                if relative.is_absolute() or '..' in relative.parts or name in projects:
                    raise ValueError('unsafe/duplicate SDK project path')
                if list(element.iter('project')) != [element]:
                    raise ValueError('nested projects require explicit support')
                projects[name] = element.get('name')
            elif element.tag in ('remove-project', 'extend-project', 'submanifest'):
                raise ValueError('unsupported manifest transformation: ' + element.tag)
    parse(sdk / '.repo/manifest.xml')
    for path in sorted((sdk / '.repo/local_manifests').glob('*.xml')):
        parse(path)
    return projects, {str(p.relative_to(sdk)): h for p, h in files.items()}


def capture_project(sdk, path, name):
    directory = sdk / path
    result = dict(path=path, name=name)
    try:
        # Do not accidentally attribute a missing nested checkout to its parent.
        top = Path(os.fsdecode(run(directory, 'rev-parse', '--show-toplevel')).strip()).resolve()
        if top != directory.resolve():
            raise ValueError('project path is not its own checkout')
        result['commit'] = run(directory, 'rev-parse', 'HEAD').decode().strip()
        patch = run(directory, 'diff', '--binary', '--no-ext-diff', 'HEAD', '--')
        result['tracked_diff_sha256'] = hashlib.sha256(patch).hexdigest()
        result['tracked_dirty'] = bool(patch)
        untracked = run(directory, 'ls-files', '--others', '--exclude-standard', '-z')
        entries = {}
        for raw in filter(None, untracked.split(b'\0')):
            relative = os.fsdecode(raw)
            file = directory / relative
            if file.is_symlink():
                entries[relative] = {'symlink': os.readlink(str(file))}
            elif file.is_file():
                entries[relative] = {'sha256': sha_file(file), 'size': file.stat().st_size}
            elif file.is_dir():
                # Git reports an untracked embedded repository as a directory.
                # Do not silently drop it or claim its contents are archived.
                entries[relative] = {'kind': 'untracked-directory', 'archival_required': True}
            else:
                raise ValueError('untracked special/missing input: ' + relative)
        result['untracked'] = entries
        result['clean'] = not patch and not entries
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        result['unavailable'] = type(error).__name__
        if isinstance(error, ValueError):
            result['reason'] = str(error)
    return result


def capture(sdk):
    projects, manifests = project_paths(sdk)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(capture_project, sdk, path, name) for path, name in sorted(projects.items())]
        records = [f.result() for f in futures]
    complete = bool(records) and all('unavailable' not in p for p in records)
    return dict(schema_version=1, manifests=manifests, projects=records,
                component_count=len(records), commits_complete=complete,
                all_worktrees_clean=complete and all(p.get('clean') for p in records),
                # Hashes identify dirty inputs but do not distribute/restorably archive them.
                complete_reproduction_bundle=False,
                note='Capture is read-only. Dirty/untracked inputs need reviewed source archives; '
                     'ignored build outputs/toolchains are not covered. Not a release qualification.')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    value = capture(args.sdk.resolve())
    args.output.write_text(json.dumps(value, indent=2) + '\n')
    print('SDK components: {}; commits complete: {}; all clean: {}'.format(
        value['component_count'], value['commits_complete'], value['all_worktrees_clean']))
    raise SystemExit(0 if value['commits_complete'] else 1)
