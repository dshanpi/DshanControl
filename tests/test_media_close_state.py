"""Exercise the real Qt MediaWorker cancellation state without a Cedar device.

The generated translation unit only exposes private fields for setup and renames
main. It does not replace close/fail/command logic. Decoder teardown, callback
races and physical display retirement still require the separate board tests.
"""
from pathlib import Path
import shlex
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "dist/media-close-tests"


class MediaCloseStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        original = ROOT / "components/media/src/main.cpp"
        source = original.read_text()
        marker = "class MediaWorker : public QObject\n{"
        assert source.count(marker) == 1
        source = source.replace(marker, marker + "\npublic:", 1)
        source = source.replace("int main(int argc, char **argv)",
                                "int media_worker_entry(int argc, char **argv)", 1)
        for name in ("../../runtime/src/storage.h", "surfaceguardian.h"):
            source = source.replace('"' + name + '"',
                                    '"' + str((original.parent / name).resolve()) + '"')
        source += (ROOT / "tests/media/close_state_fixture.cpp").read_text()
        generated = BUILD / "worker_fixture.cpp"
        generated.write_text(source)
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "Qt5Core"], text=True))
        cls.binary = BUILD / "worker-close-state"
        subprocess.run(["g++", "-std=c++11", "-fPIC", "-O2", str(generated),
                        str(ROOT / "components/runtime/src/storage.cpp"),
                        str(ROOT / "components/media/src/surfaceguardian.cpp"),
                        "-o", str(cls.binary)] + flags + ["-lcrypto", "-lz"], check=True)

    def run_case(self, name):
        result = subprocess.run([str(self.binary), name], input="", text=True,
                                capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_close_clears_pending_seek_and_buffering(self):
        self.run_case("close")

    def test_error_cleanup_clears_pending_state(self):
        self.run_case("error")

    def test_repeated_close_keeps_cancellation_state_clear(self):
        self.run_case("repeat")

    def test_rejected_close_does_not_cancel_active_state(self):
        self.run_case("invalid")
