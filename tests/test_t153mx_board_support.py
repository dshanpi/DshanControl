import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
BOARD = ROOT / "boards" / "t153mx-tina5"
OMNIGATE = (
    BOARD
    / "overlay/device/config/chips/t153/configs/omnigate"
)


class T153mxBoardSupportTests(unittest.TestCase):
    def test_snapshot_metadata_and_required_sources(self):
        metadata = json.loads((BOARD / "source-snapshot.json").read_text())
        self.assertEqual(metadata["runtime_profile"], "t153-lite")
        self.assertEqual(metadata["production_security"], "not-certified")
        required = [
            OMNIGATE / "linux-5.10-origin/board.dts",
            OMNIGATE / "buildroot/sys_partition.fex",
            OMNIGATE / "buildroot/swupdate/sw-description-ab-emmc",
            BOARD
            / "overlay/buildroot/buildroot-202205/configs/sun8iw22p1_t153_mmc_defconfig",
            BOARD / "overlay/bsp/drivers/can/sunxi_can_plat.c",
            BOARD / "overlay/bsp/drivers/mmc/sunxi-mmc.c",
            BOARD
            / "overlay/brandy/brandy-2.0/u-boot-bsp/drivers/video/drm/sunxi_device/hardware/lowlevel_lcd/dsi_v1.c",
            BOARD / "scripts/t153_board_acceptance.py",
        ]
        for path in required:
            self.assertTrue(path.is_file(), str(path))

    def test_goodix_matches_validated_board_wiring(self):
        dts = (OMNIGATE / "linux-5.10-origin/board.dts").read_text()
        start = dts.index("goodix_ts: touchscreen@14")
        node = dts[start : dts.index("\n\t};", start)]
        self.assertIn('compatible = "goodix,gt911"', node)
        self.assertIn("reg = <0x14>", node)
        self.assertIn("reset-gpios = <&pio PJ 6 GPIO_ACTIVE_LOW>", node)
        self.assertIn("interrupts = <9 7 IRQ_TYPE_EDGE_FALLING>", node)
        self.assertNotIn("irq-gpios", node)

    def test_snapshot_contains_no_known_binary_delivery_types(self):
        forbidden_suffixes = {
            ".bin", ".dtbo", ".fex.bin", ".img", ".ko", ".o", ".so",
            ".tar", ".gz", ".xz", ".bz2", ".deb",
        }
        offenders = []
        for path in BOARD.rglob("*"):
            if path.is_file() and any(
                path.name.endswith(suffix) for suffix in forbidden_suffixes
            ):
                offenders.append(str(path.relative_to(ROOT)))
        self.assertEqual(offenders, [])

    def test_canonical_runtime_sources_match_snapshot(self):
        runtime = OMNIGATE / "buildroot/overlay"
        pairs = [
            (ROOT / "components/web/app.py", runtime / "usr/lib/omnigate-web/app.py"),
            (ROOT / "components/web/hmi_api.py", runtime / "usr/lib/omnigate-web/hmi_api.py"),
            (ROOT / "components/web/assets/index.html", runtime / "usr/share/omnigate-web/index.html"),
            (ROOT / "components/supervisor/supervisor.py", runtime / "usr/lib/omnigate-supervisor/supervisor.py"),
        ]
        for canonical, snapshot in pairs:
            self.assertEqual(
                canonical.read_bytes().rstrip(),
                snapshot.read_bytes().rstrip(),
                f"{snapshot} is not synchronized with {canonical}",
            )


if __name__ == "__main__":
    unittest.main()
