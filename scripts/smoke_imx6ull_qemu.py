#!/usr/bin/env python3
"""Run the built ARM desktop/API under qemu-user; never accesses a framebuffer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def arm_hardfloat(path):
    with path.open("rb") as stream:
        header = stream.read(52)
    if (header[:6] != b"\x7fELF\x01\x01" or len(header) < 52
            or struct.unpack_from("<H", header, 18)[0] != 40
            or not struct.unpack_from("<I", header, 36)[0] & 0x400):
        raise RuntimeError("Expected ARM32 little-endian hard-float ELF: " + str(path))


def wait_until(check, processes, seconds=40):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if any(process.poll() is not None for process in processes):
            raise RuntimeError("A guest process exited; inspect guest.log")
        if check():
            return
        time.sleep(.2)
    raise RuntimeError("Guest readiness timed out; inspect guest.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--qemu", default="qemu-arm-static")
    args = parser.parse_args()
    target, output = args.target.resolve(), args.output.resolve()
    qemu = shutil.which(args.qemu)
    if not qemu:
        parser.error("qemu-arm-static is required")
    if output.exists():
        parser.error("Use a new evidence directory")
    bins = {name: target / "usr/bin" / name for name in (
        "omnigate-hmi", "dshancontrol-desktop-service", "dshancontrol-media-worker",
        "dshancontrol-runtime", "dshancontrol-api")}
    for path in bins.values():
        arm_hardfloat(path)
    output.mkdir(parents=True)
    command = [qemu, "-cpu", "cortex-a7", "-L", str(target), "-E",
               "LD_LIBRARY_PATH=" + str(target / "lib") + ":" + str(target / "usr/lib")]
    processes = []
    report = {"scope": "ARM userspace emulation on x86_64; no board, kernel, display or touch qualification",
              "hardware_validation": "not-run",
              "binaries_sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in bins.items()}}
    with tempfile.TemporaryDirectory(prefix="dshan-arm-") as temporary, (output / "guest.log").open("wb") as log:
        run = Path(temporary)
        for name in ("web", "hmi", "cache"):
            (run / name).mkdir(mode=0o700)
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        env = dict(os.environ,
            OMNIGATE_PLATFORM_MANIFEST=str(target / "usr/share/dshancontrol/platform.json"),
            OMNIGATE_CORE_PATH=str(target / "usr/lib/dshancontrol/core"),
            OMNIGATE_WEB_ASSETS=str(target / "usr/share/dshancontrol/web"),
            OMNIGATE_WEB_CONFIG=str(run / "web/config.json"), OMNIGATE_CONFIG=str(run / "web/config.json"),
            OMNIGATE_WEB_SECRET=str(run / "web/session.key"), OMNIGATE_WEB_LOG=str(output / "web.log"),
            OMNIGATE_EDGE_DB=str(run / "web/edge.db"), OMNIGATE_HMI_DB=str(run / "web/hmi.db"),
            OMNIGATE_HMI_SETTINGS=str(run / "web/settings.json"), OMNIGATE_EDS_DIR=str(run / "web/eds"),
            OMNIGATE_LISTEN_ADDRESS="127.0.0.1", OMNIGATE_LISTEN_PORT=str(port),
            QT_QPA_PLATFORM="offscreen", QT_PLUGIN_PATH=str(target / "usr/lib/qt/plugins"),
            QT_QPA_PLATFORM_PLUGIN_PATH=str(target / "usr/lib/qt/plugins/platforms"),
            XDG_RUNTIME_DIR=str(run), XDG_CACHE_HOME=str(run / "cache"), DSHANCONTROL_HMI_API_PORT=str(port),
            DSHANCONTROL_DESKTOP_STATE_DIR=str(run / "hmi"),
            DSHANCONTROL_DESKTOP_SOCKET=str(run / "ipc/desktop.sock"),
            DSHANCONTROL_DESKTOP_UID=str(os.getuid()))
        env.pop("PYTHONPATH", None)

        def start(binary, *arguments):
            process = subprocess.Popen(command + [str(binary)] + list(arguments),
                                       env=env, stdout=log, stderr=log)
            processes.append(process)
            return process

        def api_ready():
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{port}/api/hmi/v1/snapshot", timeout=3) as response:
                    snapshot = json.load(response)
                (output / "hmi-snapshot.json").write_text(json.dumps(snapshot, ensure_ascii=False, indent=2))
                return True
            except OSError:
                return False

        def check_http_contract():
            checks = []

            def request(path, method="GET", body=None, expected=200):
                data = None if body is None else json.dumps(body).encode()
                req = urllib.request.Request(f"http://127.0.0.1:{port}" + path,
                    data=data, method=method, headers={"Content-Type": "application/json"})
                try:
                    response = urllib.request.urlopen(req, timeout=5)
                except urllib.error.HTTPError as error:
                    response = error
                with response:
                    status, value = response.code, json.load(response)
                checks.append({"method": method, "path": path, "status": status})
                if status != expected or value.get("ok") is not (expected == 200):
                    raise RuntimeError("HTTP contract mismatch: " + method + " " + path)
                if expected != 200 and not isinstance(value.get("error"), str):
                    raise RuntimeError("Expected a readable API error")
                return value

            prefix = "/api/hmi/v1"
            health = request("/api/health")
            if health.get("service") != "omnigate-web":
                raise RuntimeError("Unexpected health service")
            original = request(prefix + "/settings")["settings"]
            try:
                saved = request(prefix + "/settings", "PUT", {"refresh_ms": 1500})["settings"]
                if saved != dict(original, refresh_ms=1500):
                    raise RuntimeError("Partial settings update changed unrelated fields")
                if request(prefix + "/snapshot")["settings"] != saved:
                    raise RuntimeError("Snapshot/settings contract mismatch")
                for path, field in (("history?metric=cpu&hours=1", "points"), ("alarms?limit=500", "alarms"), ("audit", "audit")):
                    if not isinstance(request(prefix + "/" + path).get(field), list):
                        raise RuntimeError("Expected API array: " + field)
                request(prefix + "/settings", "PUT", [], expected=400)
                request(prefix + "/settings", "PUT", {"refresh_milliseconds": 1500}, expected=400)
                # No confirmed hardware operation is sent by this smoke check.
                request(prefix + "/actions", "POST", {"action": "brightness", "params": {"value": 120},
                                                        "confirmed": False}, expected=400)
                if request(prefix + "/settings")["settings"] != saved:
                    raise RuntimeError("Rejected request changed settings")
            finally:
                request(prefix + "/settings", "PUT", original)
                (output / "http-contract.json").write_text(json.dumps(checks, indent=2) + "\n")
            report["http_contract"] = True

        try:
            start(bins["dshancontrol-api"])
            wait_until(api_ready, processes)
            check_http_contract()
            start(bins["dshancontrol-desktop-service"], "--socket", str(run / "ipc/desktop.sock"),
                  "--config", str(run / "absent-private-config.json"))
            wait_until(lambda: (run / "ipc/desktop.sock").exists(), processes)
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(5)
                client.connect(str(run / "ipc/desktop.sock"))
                client.sendall(b'{"protocol":1,"method":"desktop.snapshot","params":{}}\n')
                data = b""
                while not data.endswith(b"\n"):
                    chunk = client.recv(65536)
                    if not chunk or len(data) > 524288:
                        raise RuntimeError("Invalid desktop RPC response")
                    data += chunk
            reply = json.loads(data)
            if not reply.get("ok"):
                raise RuntimeError("Desktop RPC failed")
            (output / "desktop-snapshot.json").write_text(json.dumps(reply, ensure_ascii=False, indent=2))
            start(bins["omnigate-hmi"], "--windowed", "--desktop-capture", str(output / "desktop-arm.png"))
            wait_until(lambda: (output / "desktop-arm.png").exists(), processes)
            time.sleep(3)
            if any(process.poll() is not None for process in processes):
                raise RuntimeError("Guest process exited after startup")
            if not (run / "hmi/layout-v1.json").is_file():
                raise RuntimeError("Layout was not initialized")
            with (output / "desktop-arm.png").open("rb") as screenshot:
                header = screenshot.read(24)
            if header[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", header[16:24]) != (1024, 600):
                raise RuntimeError("Expected a complete 1024x600 PNG capture")
            report.update(passed=True, local_api=True, desktop_rpc=True, layout_created=True,
                          screenshot_sha256=hashlib.sha256((output / "desktop-arm.png").read_bytes()).hexdigest())
            print(json.dumps(report, indent=2))
        except Exception as error:
            report.update(passed=False, error=str(error))
            raise
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
