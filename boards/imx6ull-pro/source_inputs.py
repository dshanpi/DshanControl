"""Identify a committed 6ULL source tree while preserving the validated patch base."""
import hashlib
import subprocess
from pathlib import Path, PurePosixPath


def git(app, *args):
    return subprocess.check_output(['git', '-C', str(app)] + list(args))


def application_identity(app, expected):
    app = Path(app).resolve()
    base = expected['application_commit']
    head = git(app, 'rev-parse', 'HEAD').decode().strip()
    lineage = subprocess.run(['git', '-C', str(app), 'merge-base',
                              '--is-ancestor', base, head],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if lineage.returncode:
        raise ValueError('Application HEAD does not descend from the recorded preview base')
    paths = expected['application_patch_paths']
    original = []
    added = []
    for name in paths:
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or str(path) != name:
            raise ValueError('Invalid recorded application path')
        exists = subprocess.run(['git', '-C', str(app), 'cat-file', '-e', base + ':' + name],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        (original if exists.returncode == 0 else added).append(name)
    # Preserve the order used by the original dirty-tree patch receipt, including
    # files which did not exist at the preview base but are now committed.
    patch = git(app, 'diff', '--binary', base, '--', *original) if original else b''
    for name in added:
        path = app / name
        if path.is_symlink() or not path.is_file():
            raise ValueError('Missing or linked recorded application input: ' + name)
        result = subprocess.run(['git', 'diff', '--no-index', '--binary', '--', '/dev/null', name],
                                cwd=str(app), stdout=subprocess.PIPE)
        if result.returncode != 1:
            raise ValueError('Cannot inspect recorded added input: ' + name)
        patch += result.stdout
    if hashlib.sha256(patch).hexdigest() != expected['application_patch_sha256']:
        raise ValueError('Application content differs from the recorded video patch')
    result = dict(expected)
    result.update(application_base_commit=base, application_commit=head,
                  application_branch=git(app, 'branch', '--show-current').decode().strip(),
                  application_dirty=bool(git(app, 'status', '--porcelain').strip()))
    return result
