import json
import pathlib
import unittest
import hashlib
import subprocess
import tempfile


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

    def test_snapshot_is_immutable_and_apply_installs_current_sources(self):
        runtime = OMNIGATE / "buildroot/overlay"
        pairs = [
            (ROOT / "components/web/app.py", runtime / "usr/lib/omnigate-web/app.py"),
            (ROOT / "components/web/hmi_api.py", runtime / "usr/lib/omnigate-web/hmi_api.py"),
            (ROOT / "components/web/assets/index.html", runtime / "usr/share/omnigate-web/index.html"),
            (ROOT / "components/supervisor/supervisor.py", runtime / "usr/lib/omnigate-supervisor/supervisor.py"),
        ]
        # Historical Linux #69 evidence remains immutable as canonical code evolves.
        hashes = ['f3f1b3cfcf4eadfe72b7ebb97f2a3be79659e46d25c26f331bec7619e5904ae2', '34569cbbaa95c5d66f77c72db3395876030f9347b7f3a5e19476bcf64fcef78f', '41b27265234a8435f81febcf65d12e49c96d5ff85be0b4eb787c7880616d94a5', '5546977f36eb3636a45719b01c14cd97070a77c89831b5607cc7cf5067a3c449']
        for (_, snapshot), expected in zip(pairs, hashes):
            self.assertEqual(hashlib.sha256(snapshot.read_bytes()).hexdigest(), expected)
        with tempfile.TemporaryDirectory() as temp:
            sdk = pathlib.Path(temp)
            (sdk / "buildroot/buildroot-202205").mkdir(parents=True)
            (sdk / "device/config/chips/t153").mkdir(parents=True)
            subprocess.run([str(ROOT / "scripts/apply_t153mx_tina5.sh"), str(sdk)],
                           check=True, stdout=subprocess.DEVNULL)
            for canonical, snapshot in pairs:
                destination = sdk / snapshot.relative_to(BOARD / "overlay")
                self.assertEqual(canonical.read_bytes(), destination.read_bytes())
            package = sdk / 'buildroot/buildroot-202205/package/omnigate-hmi'
            source = package / 'src'
            self.assertEqual((ROOT / 'components/hmi/LICENSE').read_bytes(),
                             (source / 'LICENSE').read_bytes())
            (source / 'omnigate-hmi').write_text('test-only native placeholder')
            target = sdk / 'test-target'
            makefile = sdk / 'test-install.mk'
            makefile.write_text('TOPDIR := ' + str(sdk / 'buildroot/buildroot-202205') + '\n'
                                'TARGET_DIR := ' + str(target) + '\nINSTALL := install\n'
                                'include ' + str(package / 'omnigate-hmi.mk') + '\n'
                                'all: ' + str(source / 'install') + '\n'
                                + str(source / 'install') + ':\n\t$(OMNIGATE_HMI_INSTALL_TARGET_CMDS)\n')
            subprocess.run(['make', '-f', str(makefile), 'all'], check=True, stdout=subprocess.DEVNULL)
            for original, installed in [('OFL.txt', 'DshanSans-OFL.txt'),
                                        ('manifest.json', 'DshanSans-manifest.json')]:
                self.assertEqual((source / 'fonts' / original).read_bytes(),
                                 (target / 'usr/share/licenses/dshancontrol' / installed).read_bytes())



if __name__ == "__main__":
    unittest.main()
