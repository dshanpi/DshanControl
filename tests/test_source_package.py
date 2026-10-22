import importlib.util
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/package_source_release.py"
spec = importlib.util.spec_from_file_location("source_package", str(SCRIPT))
source_package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_package)

class SourcePackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Source Package Test")
        (self.root / "platform").mkdir()
        (self.root / "platform/VERSION").write_text("0.1.0-preview\n")
        (self.root / "README").write_text("reviewed source\n")
        (self.root / ".gitignore").write_text("private/\n")
        self.commit()

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root)] + list(args))

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "Reviewed fixture")

    def export(self, suffix="out"):
        return source_package.package(self.root, Path(self.temp.name) / suffix)

    def test_exports_only_committed_members_and_bound_metadata(self):
        (self.root / "untracked-secret").write_text("do not export")
        (self.root / "private").mkdir()
        (self.root / "private/password").write_text("do not export")
        paths = self.export()
        with tarfile.open(str(paths[0])) as archive:
            names = archive.getnames()
            self.assertTrue(any(name.endswith("/README") for name in names))
            self.assertFalse(any("secret" in name or "password" in name or "/.git/" in name for name in names))
        info = json.loads(paths[2].read_text())
        self.assertEqual(info["source_commit"], self.git("rev-parse", "HEAD").decode().strip())
        self.assertFalse(info["installable"])
        self.assertFalse(info["production_qualified"])

    def test_dirty_and_staged_changes_refuse_export(self):
        (self.root / "README").write_text("unreviewed")
        with self.assertRaises(ValueError):
            self.export()
        self.git("add", "README")
        with self.assertRaises(ValueError):
            self.export()
        self.assertFalse((Path(self.temp.name) / "out").exists())

    def test_export_is_reproducible_across_output_directories(self):
        first, second = self.export("one"), self.export("two")
        self.assertEqual([p.read_bytes() for p in first], [p.read_bytes() for p in second])

    def test_existing_artifact_is_preserved(self):
        paths = self.export()
        content = paths[0].read_bytes()
        with self.assertRaises(ValueError):
            self.export()
        self.assertEqual(paths[0].read_bytes(), content)

    def test_unsafe_committed_version_is_rejected(self):
        (self.root / "platform/VERSION").write_text("../../unsafe\n")
        self.commit()
        with self.assertRaises(ValueError):
            self.export()
