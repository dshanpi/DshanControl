import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("page_gate", Path(__file__).resolve().parents[1] / "scripts/check_ui_page_gates.py")
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


class PageGateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / "components/hmi/page.cpp"
        self.source.parent.mkdir(parents=True)
        self.source.write_text("original")
        self.folder = self.root / "docs/design/page-gates"
        self.folder.mkdir(parents=True)
        self.page = {
            "id": "P01", "name": "Example", "route": "example", "status": "design_review",
            "business_goal": "Observe", "prerequisites": "Read status", "primary_action": "Refresh",
            "confirmed_result": "Response", "failure_recovery": "Retain error",
            "operation_steps": ["Read", "Refresh"], "business_flow": ["Observe", "Confirm"],
            "state_coverage": {state: "Specific handling" for state in gate.STATES},
            "function_changes": {key: [] for key in ("retained", "added", "changed", "removed", "proposed_not_implemented")},
            "design_review": {"decision": "pending", "reference": "", "preview_id": "example",
                              "preview_sha256": "a" * 64, "known_gaps": []},
            "implementation": {"history": []}}
        self.manifest = {"pages": ["P01"], "baseline_hashes": gate.fingerprints(self.root), "impact_reviews": []}
        self.save()

    def save(self):
        (self.folder / "P01.json").write_text(json.dumps(self.page))
        (self.folder / "manifest.json").write_text(json.dumps(self.manifest))

    def completed(self, digest):
        self.page["status"] = "implemented"
        self.page["design_review"].update(decision="approved", reference="fixture review")
        self.page["implementation"] = {
            "summary": "Fixture change", "source_hashes": {"components/hmi/page.cpp": digest},
            "verification": ["fixture log"], "actual_screenshots": ["fixture screenshot"], "remaining_risks": []}
        self.page["implementation"]["history"] = [{
            "id": "fixture-change-1", "summary": "Fixture change",
            "business_flow": self.page["business_flow"], "operation_steps": self.page["operation_steps"],
            "function_changes": self.page["function_changes"],
            "source_hashes": {"components/hmi/page.cpp": digest},
            "verification": ["fixture log"], "actual_screenshots": ["fixture screenshot"]}]
        self.manifest["impact_reviews"] = [{"path": "components/hmi/page.cpp", "sha256": digest,
                                           "pages": ["P01"], "rationale": "Only fixture page"}]
        self.save()

    def test_unchanged_source_allows_draft(self):
        self.assertEqual(gate.check(self.root), [])

    def test_design_requires_review_and_no_gaps(self):
        self.assertTrue(gate.check(self.root, "design", ["P01"]))
        self.page["design_review"].update(decision="approved", reference="fixture review", known_gaps=["Missing navigation"])
        self.save()
        self.assertTrue(gate.check(self.root, "design", ["P01"]))
        self.page["design_review"]["known_gaps"] = []
        self.save()
        self.assertEqual(gate.check(self.root, "design", ["P01"]), [])

    def test_changed_source_requires_current_completed_record(self):
        self.source.write_text("changed")
        self.assertTrue(gate.check(self.root))
        self.completed(gate.fingerprints(self.root)["components/hmi/page.cpp"])
        self.assertEqual(gate.check(self.root), [])
        self.source.write_text("changed again")
        self.assertTrue(gate.check(self.root))

    def test_missing_workflow_and_function_lists(self):
        del self.page["operation_steps"]
        del self.page["function_changes"]["removed"]
        self.save()
        self.assertGreaterEqual(len(gate.check(self.root)), 2)

    def test_new_file_requires_coverage(self):
        (self.source.parent / "new.cpp").write_text("new")
        self.assertTrue(gate.check(self.root))

    def test_deleted_file_requires_binding(self):
        self.source.unlink()
        self.assertTrue(gate.check(self.root))
        self.completed(gate.DELETED)
        self.assertEqual(gate.check(self.root), [])

    def test_shared_change_requires_each_page(self):
        self.source.write_text("changed")
        self.completed(gate.fingerprints(self.root)["components/hmi/page.cpp"])
        self.manifest["impact_reviews"][0]["pages"].append("P02")
        self.save()
        self.assertTrue(gate.check(self.root))

    def test_asset_change_is_checked(self):
        (self.source.parent / "icon.png").write_bytes(b"fixture asset")
        self.assertTrue(gate.check(self.root))

    def test_change_without_history_rejected(self):
        self.source.write_text("changed")
        self.completed(gate.fingerprints(self.root)["components/hmi/page.cpp"])
        self.page["implementation"]["history"] = []
        self.save()
        self.assertTrue(gate.check(self.root))

    def test_missing_verification(self):
        self.source.write_text("changed")
        self.completed(gate.fingerprints(self.root)["components/hmi/page.cpp"])
        self.page["implementation"]["verification"] = []
        self.save()
        self.assertTrue(gate.check(self.root))


if __name__ == "__main__":
    unittest.main()
