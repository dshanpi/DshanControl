#!/usr/bin/env python3
"""Export a clean committed source tree; never copy the working directory."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root)] + list(args))

def package(root, output):
    root = Path(root).resolve()
    output = Path(output).resolve()
    commit = git(root, "rev-parse", "HEAD").decode().strip()
    if git(root, "status", "--porcelain", "--untracked-files=no").strip():
        raise ValueError("Tracked source has uncommitted changes; commit the reviewed source first")
    version = git(root, "show", commit + ":platform/VERSION").decode().strip()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,95}", version):
        raise ValueError("Invalid committed source version")
    name = "dshancontrol-" + version
    output.mkdir(parents=True, exist_ok=True)
    archive = output / (name + ".tar.gz")
    checksum = output / (name + ".tar.gz.sha256")
    metadata = output / (name + ".source.json")
    destinations = [archive, checksum, metadata]
    if any(p.exists() or p.is_symlink() for p in destinations):
        raise ValueError("Output already exists; use a new directory to preserve previous evidence")
    with tempfile.TemporaryDirectory(prefix=".source-package-", dir=str(output)) as temp:
        temp = Path(temp)
        raw = temp / "source.tar"
        subprocess.run(["git", "-C", str(root), "archive", "--format=tar",
                        "--prefix=" + name + "/", "-o", str(raw), commit], check=True)
        packed = temp / archive.name
        with raw.open("rb") as source, packed.open("wb") as target:
            with gzip.GzipFile(filename="", mode="wb", fileobj=target, mtime=0) as compressed:
                while True:
                    block = source.read(1024 * 1024)
                    if not block:
                        break
                    compressed.write(block)
        digest = hashlib.sha256(packed.read_bytes()).hexdigest()
        (temp / checksum.name).write_text(digest + "  " + archive.name + "\n")
        (temp / metadata.name).write_text(json.dumps({
            "schema": 1, "kind": "committed-source-preview", "version": version,
            "source_commit": commit, "archive": archive.name, "sha256": digest,
            "installable": False, "production_qualified": False,
            "scope": "Source export only; no board installation or hardware qualification"
        }, ensure_ascii=False, indent=2) + "\n")
        # Hard links create each destination exclusively, without replacing an old artifact.
        created = []
        try:
            for destination in destinations:
                os.link(str(temp / destination.name), str(destination))
                created.append(destination)
        except Exception:
            for destination in created:
                destination.unlink()
            raise
    return destinations

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", default=None)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        paths = package(root, args.output or root / "dist")
    except (ValueError, subprocess.CalledProcessError, OSError) as error:
        parser.exit(2, "Source packaging refused: " + str(error) + "\n")
    for path in paths:
        print(path)

if __name__ == "__main__":
    main()
