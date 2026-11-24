"""Port invariants that prevent accidental use of the old Allwinner platform."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_imx6ull", ROOT / "scripts/prepare_imx6ull.py")
port = importlib.util.module_from_spec(spec)
spec.loader.exec_module(port)


class Imx6ullPortTest(unittest.TestCase):
    def test_config_merge_replaces_choices_without_duplicate_symbols(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".config"
            path.write_text('BR2_PACKAGE_LVGL9_DEMO=y\n# BR2_INSTALL_LIBSTDCPP is not set\nBR2_arm=y\n')
            fragment = (ROOT / "boards/imx6ull-pro/buildroot/dshancontrol.fragment").read_text()
            port.merge_config(path, fragment)
            result = path.read_text()
            self.assertNotIn('BR2_PACKAGE_LVGL9_DEMO=y', result)
            self.assertEqual(result.count('BR2_INSTALL_LIBSTDCPP'), 1)
            self.assertIn('BR2_arm=y', result)
            port.merge_config(path, fragment)
            self.assertEqual(path.read_text(), result)

    def test_build_does_not_use_allwinner_codecs_or_updater_identity(self):
        recipe = (ROOT / "boards/imx6ull-pro/buildroot/package/dshancontrol-imx6ull/dshancontrol-imx6ull.mk").read_text()
        for forbidden in ('CONFIG+=cedar', 'components/imageformats', 'install_s4_runtime', 'system-contract.json'):
            self.assertNotIn(forbidden, recipe)
        self.assertIn('components/media/media.pro'.replace('components/media/', ''), recipe)


if __name__ == "__main__":
    unittest.main()
