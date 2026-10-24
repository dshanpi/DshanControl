"""Real Qt owner/session shutdown, with host children; not Cedar qualification."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "dist/application-shutdown-tests"

class ApplicationShutdownTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        hmi = ROOT / "components/hmi/src"
        pro = BUILD / "shutdown.pro"
        pro.write_text("QT += core gui widgets\nCONFIG += console c++11\n"
                       "TARGET = shutdown-fixture\nINCLUDEPATH += " + str(hmi) + "\n"
                       "SOURCES += " + str(ROOT / "tests/hmi/shutdown_fixture.cpp") +
                       " " + str(hmi / "mediasession.cpp") + "\nHEADERS += " +
                       str(hmi / "mediasession.h") + " " + str(hmi / "applicationshutdown.h") + "\n")
        subprocess.run(["qmake", str(pro)], cwd=BUILD, check=True, capture_output=True)
        subprocess.run(["make", "-j2"], cwd=BUILD, check=True, capture_output=True)

    def check_case(self, mode):
        with tempfile.TemporaryDirectory(dir=BUILD) as tmp:
            journal = Path(tmp) / "events.jsonl"
            worker = ROOT / "tests/hmi" / ("shutdown_wedged_worker.py" if mode == "wedged"
                                                   else "shutdown_cooperative_worker.py")
            env = dict(os.environ, QT_QPA_PLATFORM="offscreen", AO_JOURNAL=str(journal))
            before = time.monotonic()
            result = subprocess.run([str(BUILD / "shutdown-fixture"), mode, str(worker)],
                                    env=env, capture_output=True, text=True, timeout=10)
            elapsed = time.monotonic() - before
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            events = [json.loads(line) for line in journal.read_text().splitlines()] if journal.exists() else []
            count = 0 if mode == "empty" else 2 if mode == "multiple" else 1
            self.assertEqual(sum(e.get("command") == "close" for e in events), count)
            self.assertEqual(sum(e["event"] == "EOF" for e in events), count)
            for event in events:
                if event["event"] == "start":
                    self.assertFalse(Path("/proc/%d" % event["pid"]).exists())
            if mode == "wedged":
                self.assertTrue(any(e["event"] == "TERM" for e in events))
                self.assertGreater(elapsed, 2.3)
                self.assertLess(elapsed, 6.0)
                self.assertIn("normal=0", result.stdout)
            else:
                self.assertFalse(any(e["event"] == "TERM" for e in events))
                self.assertEqual(sum(e["event"] == "normal-exit" for e in events), count)

    def test_no_worker_exits(self): self.check_case("empty")
    def test_close_flushes_protocol_before_quit(self): self.check_case("cooperative")
    def test_multiple_sessions_retire(self): self.check_case("multiple")
    def test_ignored_close_keeps_worker(self): self.check_case("ignored")
    def test_hidden_page_keeps_worker(self): self.check_case("hidden")
    def test_wedged_child_uses_existing_escalation(self): self.check_case("wedged")
