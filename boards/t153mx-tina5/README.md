# T153MX Tina Linux 5 board support

This directory is the source-only T153MX integration snapshot used by the
hardware shown at the top of the repository README. It is intentionally kept
separate from the reusable DshanControl components.

## Snapshot

- Board: DshanPI OmniGate / mCore-T153MX
- SoC: Allwinner T153, ARMv7-A
- SDK family: Tina Linux 5
- Runtime profile: `t153-lite`
- Validated image: Linux build `#69`, 2026-09-16
- Source baseline: `dshanpi/T153MX-Tina5SDK_OmniGate` commit `11fdc2b`
- Snapshot state: validated working tree, including the post-baseline Goodix,
  ADB, Web/HMI, supervisor and acceptance changes

The machine-readable provenance and exclusions are in
[`source-snapshot.json`](source-snapshot.json).

## Contents

```text
overlay/   SDK-relative T153MX DTS, CAN/MMC/AIC8800/DSI source changes,
           kernel/Buildroot configuration, rootfs services, A/B partition
           data and protocol package recipes
scripts/   board/API acceptance and integration verification tools
docs/      bring-up, Goodix recovery and software verification records
```

The reusable Web, HMI, supervisor and core sources live under the repository
top-level [`components/`](../../components/) directory. Copies under the board
overlay record the exact rootfs state used by the validated firmware; the
top-level component sources are canonical for continued development.

## Apply to a compatible SDK

From the DshanControl repository root:

```sh
./scripts/apply_t153mx_tina5.sh /path/to/TinaSDK
```

The script checks the expected SDK layout, copies the board overlay, then
installs the current canonical DshanControl core, HMI, Web and supervisor
sources. It does not build or flash a device.

After applying, reload the Buildroot defconfig before building:

```sh
make -C buildroot/buildroot-202205 \
  O="$PWD/out/t153/omnigate/buildroot/buildroot" \
  sun8iw22p1_t153_mmc_defconfig
./build.sh
./build.sh pack
```

## Important boundary

This is not a complete Tina SDK and is not a production-security-certified
firmware. The included BSP source files retain their individual GPL/SPDX terms.
Vendor boot binaries, AIC8800 firmware, compiled DTBO files, flashing tools,
cached upstream archives, build output and device credentials are not stored
here. Obtain the base SDK and redistributable vendor files under their own
terms.

Protocol drivers and interface enumeration were tested without external
CANopen, Modbus or EtherCAT slaves. That does not claim interoperability.
