# DshanControl on T113s4 SD NAND / Buildroot

This is an additional industrial-desktop target, retaining the T153 implementation.
Hardware source: https://github.com/DongshanPI/100ASK_T113s4-SdNand_TinaSDK5
Baseline inspected: `0bbdbba` (full commit recorded by prepare_s4.py),
`t113_s4/100ask`, Linux 5.4, Buildroot 201902, Linaro 5.3.1, ARMv7-A softfp.
The toolchain sysroot reports glibc 2.21 and Linux headers 4.0. This is **SD NAND**;
it is not the workspace's currently selected T113 SPI NAND/EinkOS configuration.

## Isolated application build

```sh
python3 scripts/prepare_s4.py \
  --sdk /path/to/T113-tina5v1.2-sdk \
  --hardware /path/to/100ASK_T113s4-SdNand_TinaSDK5 \
  --output /path/to/new-output
/path/to/new-output/configure.sh
make -C /path/to/T113-tina5v1.2-sdk/buildroot/buildroot-201902 \
  O=/path/to/new-output/buildroot BR2_EXTERNAL=/path/to/new-output/external \
  BR2_JLEVEL=4 dshancontrol
```

Preparation snapshots application sources and hashes, records hardware commit and
board-file hashes, and merges the application fragment into the approved hardware
defconfig. It selects the SDK's preinstalled compiler, not an arbitrary downloaded
Linaro version. Dirty sources are explicitly marked development-draft. It does not
change SDK `.buildconfig`, overwrite the existing board, or flash a device.
`board/` contains the exact hardware board files for review; it is not a built BSP.

This builds the userspace integration target, **not a complete flashable image**.
Full-image integration still requires selecting/compiling the S4 BSP and bootloader,
applying the approved board inputs in the hardware SDK, capturing every SDK commit
and local patch, finalizing rootfs and packing the board image. Do not pack using
the workspace's old `t113/evb1_auto_nand` selection.

## Porting choices

- Qt5 Widgets/Charts HMI, Web, core and supervisor use the canonical component sources.
- Buildroot 201902 lacks qmake-package, so the adapter invokes target qmake explicitly.
- Its Flask 0.12.2 is incompatible with the current application. A pinned private
  Python runtime supplies Flask 2.1.2 and dependencies; it does not replace other
  SDK applications' Flask. MarkupSafe uses its pure Python implementation.
- Python 3.7 compatibility fixes preserve behavior on T153's newer Python.
- WenQuanYi font recipe is backported with upstream source/license hashes.
- SD NAND UDISK is required for persistent app/data storage; boot refuses unmounted
  storage. No A/B system partition layout is introduced by this application port.
- Only the baseline display channel is configured. CAN, RS485, Ethernet fieldbus,
  cellular and touch are not copied from T153 or declared verified.

## Pending board evidence

Baseline DTS configures 1024x600 RGB display and disables the touch node. Actual
panel, touch wiring, hardware revision, SD NAND capacity and physical device access
must be confirmed. Display layout at 600px, touch, network, external bus devices,
cold boot, storage persistence and power-loss recovery remain untested.

`system-contract.json` deliberately contains pending values. Do not publish it as
a qualified system identity. Before enabling upgrades, derive runtime/libc identity
from built artifacts, provision the real hardware revision and signing public key,
provide a verified maintenance interlock and board health probe, and test the full
factory-image → application-update → interruption/rollback chain. Until then,
packages are development drafts and the device refuses installation by default.

After the target rootfs is complete, derive its runtime identity (before image
packing) with:

```sh
python3 scripts/finalize_system_contract.py --target /path/to/buildroot/target \
  --hardware-revision ACTUAL_REVISION --public-key /path/to/approved-public.pem
```

This checks ARM32/soft-float calling convention, reads libc from the built target,
hashes base runtime files, and provisions the approved public key. It still marks
validation `not-run`; it neither creates a private key nor qualifies the board.

The external tree carries a local Qt 5.11.3 `SystemGenerator::result_type` patch
for GCC 9 host qmake compatibility. `prepare_s4.py` registers it through
`BR2_GLOBAL_PATCH_DIR`; the original SDK Qt recipe is not edited.
