import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORM = ROOT / "platform"


def load(relative):
    with (PLATFORM / relative).open(encoding="utf-8") as stream:
        return json.load(stream)


class PlatformLayoutTests(unittest.TestCase):
    def test_profiles_only_reference_declared_components(self):
        catalog = load("components.json")
        components = {item["id"]: item for item in catalog["components"]}
        self.assertEqual({"lite", "enhanced"}, set(catalog["profiles"]))
        for profile_id in catalog["profiles"]:
            profile = load("profiles/%s.json" % profile_id)
            self.assertEqual(profile_id, profile["id"])
            self.assertEqual(len(profile["components"]), len(set(profile["components"])))
            for component_id in profile["components"]:
                self.assertIn(component_id, components)
                self.assertIn(profile_id, components[component_id]["profiles"])
                self.assertTrue((ROOT / components[component_id]["path"]).exists())

    def test_lite_and_enhanced_boundaries(self):
        lite = load("profiles/lite.json")
        enhanced = load("profiles/enhanced.json")
        self.assertFalse(lite["containers"])
        self.assertFalse(lite["node_red"])
        self.assertNotIn("node-red-profile", lite["components"])
        self.assertTrue(enhanced["containers"])
        self.assertTrue(enhanced["node_red"])
        self.assertEqual([], enhanced["container_policy"]["direct_hardware_devices"])
        self.assertTrue(enhanced["container_policy"]["drop_all_capabilities"])

    def test_board_descriptor_matches_runtime_manifest(self):
        board = load("boards/t153mx.json")
        manifest_path = ROOT / board["runtime_manifest"]
        self.assertTrue(manifest_path.is_file())
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(board["id"], manifest["board_id"])
        self.assertEqual(board["soc"], manifest["soc"])
        self.assertIn(manifest["runtime_profile"], board["supported_profiles"])
        for name, channel in manifest["channels"].items():
            self.assertTrue(name)
            self.assertIn("kind", channel)
            self.assertIn("device", channel)
            self.assertIn("role", channel)

    def test_contracts_and_schemas_are_present_json(self):
        for name in ("runtime-profile", "board-descriptor", "telemetry", "command"):
            schema = load("schemas/%s.schema.json" % name)
            self.assertIn("$schema", schema)
            self.assertEqual("object", schema["type"])
        for relative in ("contracts/api-v1.md", "contracts/mqtt.md"):
            self.assertGreater((PLATFORM / relative).stat().st_size, 200)

    def test_release_status_does_not_claim_production(self):
        status = load("release-status.json")
        self.assertEqual("not-certified", status["production_security"])
        self.assertEqual("not-run", status["profiles"]["enhanced"]["rk3568_board_test"])
        self.assertEqual("not-run", status["profiles"]["enhanced"]["t536_board_test"])


if __name__ == "__main__":
    unittest.main()
