#!/usr/bin/env python3
"""Snapshot a DshanControl development port into an isolated vendor BSP output."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

BSP_COMMIT = "e00afed02481332d2de44a498518b596651dcdef"


def command(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def merge_config(path, fragment):
    def key(line):
        match = re.match(r"(?:# )?(BR2_[A-Za-z0-9_]+)(?:=| is not set)", line)
        return match.group(1) if match else None
    additions = [line for line in fragment.splitlines() if key(line)]
    replaced = {key(line) for line in additions}
    existing = [line for line in path.read_text().splitlines() if key(line) not in replaced]
    path.write_text("\n".join(existing + additions) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bsp", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    bsp, output = args.bsp.resolve(), args.output.resolve()
    br = bsp / "buildroot-2026.02.1"
    if any(ch.isspace() for ch in str(bsp) + str(output)):
        parser.error("Buildroot paths must not contain whitespace")
    if output.exists() or output == source or source in output.parents or bsp in output.parents:
        parser.error("Use a NEW output directory outside the source and BSP trees")
    if command("git", "status", "--porcelain", cwd=source):
        parser.error("Release inputs must come from a clean committed source tree")
    if command("git", "rev-parse", "HEAD", cwd=bsp) != BSP_COMMIT:
        parser.error("BSP revision differs from the reviewed commit " + BSP_COMMIT)
    if command("git", "status", "--porcelain", "--untracked-files=normal", "--", "buildroot-2026.02.1", cwd=bsp):
        parser.error("BSP Buildroot inputs have local modifications; use the pinned clean checkout")
    if not (br / "configs/100ask_imx6ull-pro_defconfig").is_file():
        parser.error("BSP checkout is incomplete")
    output.mkdir(parents=True)
    external = output / "external"
    shutil.copytree(source / "boards/imx6ull-pro/buildroot", external)
    snapshot = external / "app-source"
    # Keep source, fonts, dependencies and their licenses; exclude generated files.
    snapshot.mkdir()
    with tempfile.TemporaryDirectory(prefix="dshan-source-") as temporary:
        archive = Path(temporary) / "source.tar"
        subprocess.run(["git", "archive", "--format=tar", "-o", str(archive), "HEAD"],
                       cwd=source, check=True)
        with tarfile.open(archive) as package:
            package.extractall(snapshot, filter="data")
    identity = {"application_commit": command("git", "rev-parse", "HEAD", cwd=source),
                "bsp_commit": BSP_COMMIT, "release_kind": "system",
                "qualification": "development-draft-not-qualified"}
    (external / "overlay/etc/dshancontrol/build-source.json").write_text(
        json.dumps(identity, indent=2) + "\n")
    for path in (external / "overlay/etc/init.d").iterdir():
        path.chmod(0o755)
    (snapshot / "boards/imx6ull-pro/runtime/S99dshancontrol").chmod(0o755)
    build = output / "buildroot"
    invocation = ["make", "-C", str(br), "O=" + str(build), "BR2_EXTERNAL=" + str(external)]
    subprocess.run(invocation + ["100ask_imx6ull-pro_defconfig"], check=True)
    overlay = "board/100ask/imx6ull-pro/rootfs-overlay " + str(external / "overlay")
    patches = "board/100ask/imx6ull-pro/patches " + str(external / "patches")
    merge_config(build / ".config", (external / "dshancontrol.fragment").read_text()
                 + '\nBR2_ROOTFS_OVERLAY="' + overlay + '"\n'
                 + 'BR2_GLOBAL_PATCH_DIR="' + patches + '"\n')
    subprocess.run(invocation + ["olddefconfig"], check=True)
    config = (build / ".config").read_text()
    for required in ("BR2_ARM_EABIHF=y", "BR2_PACKAGE_DSHANCONTROL_IMX6ULL=y",
                     "BR2_PACKAGE_QT5BASE_LINUXFB=y", "BR2_PACKAGE_QT5BASE_PNG=y",
                     "BR2_PACKAGE_QT5BASE_FONTCONFIG=y", "BR2_PACKAGE_DEJAVU=y",
                     "BR2_PACKAGE_QT5BASE_JPEG=y", "# BR2_PACKAGE_LVGL9_DEMO is not set"):
        if required not in config.splitlines():
            raise RuntimeError("Configuration did not retain " + required)
    files = {str(p.relative_to(snapshot)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(snapshot.rglob("*")) if p.is_file()}
    provenance = {
        "scope": "development-source-snapshot-not-firmware",
        "application_commit": command("git", "rev-parse", "HEAD", cwd=source),
        "application_status": command("git", "status", "--porcelain", cwd=source),
        "bsp_commit": BSP_COMMIT,
        "config_sha256": hashlib.sha256((build / ".config").read_bytes()).hexdigest(),
        "source_files_sha256": files,
        "hardware_validation": "not-run",
    }
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print("Prepared:", build)
    print("Application only: CCACHE=none CCACHE_DISABLE=1 make -C", build, "-j6 dshancontrol-imx6ull")
    print("Full image (100 GiB free recommended): CCACHE=none CCACHE_DISABLE=1 make -C", build, "-j6")
    print("No image has been built, installed or flashed by this command.")


if __name__ == "__main__":
    main()
