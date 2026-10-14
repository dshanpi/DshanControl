# S4 hardware input reconciliation — 2026-10-13

This is a **system** integration record, not application-package content or board
qualification. Retain `t113_s4/100ask`, SD NAND and Buildroot from
`DongshanPI/100ASK_T113s4-SdNand_TinaSDK5` at
`0bbdbbabcfad8da15696e803f6714bfd1748a0a6`. Existing T153 support is unchanged.
The S3/S4 naming relationship and physical board revision remain unresolved.

## Observed hardware and execution ownership

The primary interactive agent's local evidence is in
`dist/hardware-mcp/user-handoff.json`, `board-identity.json`, `board-storage.json`,
`network-display.json`, `wifi-files.json`, `wifi-load.json` and `wifi-load-tail.json`.
The old running system reports S4, SD capacity 2013265920 bytes, root p5 and UDISK
p9 mounted as vfat. Display mode is 1024x600; framebuffer virtual size 1024x1200
is not the panel resolution. These observations are separate from new-image tests.

The primary interactive agent owns hardware operation and the user's trial-flash
authorization. Ralph only builds and inspects local artifacts. Transfer file
payloads over the network, including SDK-host to LYNX-host transfer. UART is for
commands/identification/logs; vendor flashing uses the dedicated USB FEL/FES tool.
Use a board-matched Tina vendor package. Do not substitute mainline or another
board's loader. Re-identify the bound board before any destructive operation.
WiFi credentials remain in the private local configuration owned by the hardware
operator; do not read them into build logs, firmware or evidence.

## RGB50 and touch

The reviewed S4 input is `configs/100ask/linux-5.4/board.dts` within the chip tree.
The local T113s3 Pro peripheral reference is
`/home/ubuntu/100ASK_T113-PRO_TinaSDK5/device/config/chips/t113/configs/evb1_auto_nand/linux-5.4/board.dts:1383`.
Its exact Git revision, dirty state and file hash are recorded in
`dist/iteration-hardware-inputs/input-review.json`. This reference does not supply
S4 storage, partition tables, SoC identity or bootloaders.

| Property | S4 source and compiled DTB | Pro reference |
| --- | --- | --- |
| Resolution | 1024x600 | 1024x600 |
| Pixel clock | 51 MHz | 51 MHz |
| Horizontal total / sync width | 1344 / 20 | 1344 / 20 |
| Vertical total / sync width | 635 / 3 | 635 / 3 |
| HBP / VBP (vendor definition includes sync width) | 140 / 20 | 160 / 23 |
| Configured physical size | 154x85 mm | 280x150 mm |
| Bus / pin group | parallel RGB / rgb18 | parallel RGB / rgb18 |
| Backlight PWM | channel 7, 500 Hz | channel 7, 500 Hz |

Neither configured physical size establishes the actual RGB50 dimensions. Retain
the approved S4 values pending panel timing/wiring evidence; copying the Pro block
would change both timing and geometry without that evidence. The compiled Goodix
touch node remains disabled and has an 800x480 range, with PB6/PB7 interrupt/wake
configuration. Actual controller, wiring, range and rotation need confirmation
before enabling touch. A host Qt screenshot is not that confirmation.

## AIC8800D80

The independent kernel config enables `CONFIG_AIC_WLAN_SUPPORT=y`,
`CONFIG_AIC8800_WLAN_SUPPORT=m`, `CONFIG_AIC8800_BTLPM_SUPPORT=m` and firmware path
`/lib/firmware/aic8800d80`. The compiled DTB enables SDIO1 and rfkill/WLAN, with
PG12 active-high WLAN enable and PG10 active-high host wake. The Pro reference
uses the same WLAN GPIOs; this agreement does not prove physical wiring or power.

The raw rootfs contains `aic8800_bsp.ko`, `aic8800_fdrv.ko` and
`aic8800_btlpm.ko`, ARM modules with kernel vermagic 5.4.61. Six approved D80 files
at the configured path match the S4 source: user configuration, fmac firmware,
RF firmware, ADID, patch and patch table. Exact file names, origins, sizes and
SHA256 values are in the local review. Other vendor firmware directories are not
substitutes for that configured path. Old-system probing failed to power on WiFi;
SDIO/power diagnosis and a real connection remain hardware work, not passed tests.

## Packing handoff

The round-20 independent build is at
`/home/ubuntu/dshancontrol-s4-prepared-20261013-h02`; its component paths and hashes
are in `dist/iteration-h02-system/build-handoff.json`. The round-21 review rereads
those artifacts, validates compiled peripheral inputs and extracts WiFi payloads
from the unchanged raw rootfs. Its handoff is
`dist/hardware-mcp/build-input-review.json`.

Round 22 produced a complete local Tina `IMAGEWTY` package at
`/home/ubuntu/dshancontrol-s4-vendor-pack-20261013/out/t113_s4/100ask/buildroot/t113_s4_linux_100ask_uart0.img`.
It is 1426076672 bytes, SHA256
`2da05c73f0a4f6e98bd0b6033871825f3a95d6d9aa35c3ba98401f18c783570e`.
The current handoff is `dist/hardware-mcp/vendor-package-handoff.json`; the round-21
null package field is historical. The new package remains a development draft.

The isolated pack uses restored vendor build/common/S4/tools inputs and the
round-20 kernel, DTB, U-Boot, MMC SPL and rootfs. FES and the generic container's
NAND boot0 entry were built from the same `t113_s4` sources; the boot environment
continues to use MMC p5. No other-board loader or mainline component was used.
Three local system overrides are recorded with origin and patches: replace the
vendor's hardcoded 61079552-sector GPT capacity with observed 3932160 sectors;
add an empty ext4 `udisk.fex` download to UDISK; omit the unused `arisc.fex` entry
because this board has no vendor `arisc.config`. DSP and `amp_rv0` remain present.

The final package's 41 entries match their staged payload hashes. GPT/MBR CRCs,
partition bounds, root p5/data p9, download map, boot checksums, TOC payloads,
exact kernel/DTB and MMC environment were inspected. The rootfs is 512 MiB in a
1 GiB partition. UDISK is a 768 MiB ext4 filesystem in an 845132288-byte partition;
it contains only root-owned `lost+found`. Both filesystems pass read-only fsck.
These are host checks, not evidence of successful USB writing or booting.

**This factory system image contains an empty UDISK payload. Writing it replaces
that volume's data.** It is not a data-preserving application update. Startup and
mount failure still never format or convert storage. The hardware operator must
verify the factory write, ext4 mapping/mount, and data policy on the actual board.
`flash_ready=false` requests primary pre-flash matching/inspection, not another
user authorization; the existing trial-flash authorization remains valid.

Packing inputs were archived outside the application repository under
`/home/ubuntu/dshancontrol-recovery-20261013-h02/vendor-pack-round22/`, recovered
with modes/symlinks verified, and used for a second successful vendor pack.
This supplements the prior SDK/source recovery set; it does not establish a
complete clean-system source rebuild or board qualification. Local evidence is
`dist/iteration-h02-pack/validation.json`. Original SDK components and old outputs
remain intact. Formal identity/trust roots, maintenance interlock, new-image boot,
RGB50/touch/WiFi/industrial peripherals, physical interruption/rollback and aging
remain pending. No hardware operation, upload, commit or publication occurred.

## Boot0 packaging correction — 2026-10-14

The round-22 image above must not be flashed again: its Boot0 hardware-information
field was zero. Full USB write/verification passed, but OP-TEE rejected the boot
chain with `check_hardware_info:114 hardware check error1` before Linux.
The isolated pack omitted the vendor buildserver; `update_chip` failed and the
vendor pack script did not stop. A valid additive checksum alone did not catch it.

The correction starts the vendor `tools/build/buildserver --path <isolated-sdk>`
with the selected S4 `.buildconfig` at the SDK root. Run `update_chip` from the
staging directory containing `optee.fex`, with a relative Boot0 filename and
`LICHEE_OUT_DIR` pointing to that SDK's `out`. Require exit 0 and verify the output.
Apply `tina/0001-reject-boot0-hardware-info-failure.patch` to the vendor `build`
component so a failed update cannot silently continue. Do not use another board's
Boot0, copy its chip bytes, or patch OP-TEE to skip the check.

For this S4 configuration the vendor tool generated hardware bytes `5241579be9`
at Boot0 offset `0xf1`. It changed only those five bytes and checksum bytes
`0x0c..0x0f`. OP-TEE, U-Boot, DTBs, kernel, rootfs and UDISK remain byte-identical.
Regenerate the IMAGEWTY container with Dragon, then inspect the actual final image:

```sh
python3 scripts/check_s4_boot_header.py <final-vendor-image.img> \
  --expected-hardware-info 5241579be9
```

This gate supplements the complete container/partition/source audit and physical
boot test. Expected bytes must match the selected vendor configuration; this
value is not a universal chip marker. The new gate rejects the old failed image.
Tests cover missing information with a valid checksum, another chip's information,
corruption and truncated final containers. `make test` passes (5 core + 121 tests).

Corrected development artifact:
`/home/ubuntu/dshancontrol-s4-bootfix-20261014/t113_s4_linux_100ask_uart0_boot0fixed.img`
(1426076672 bytes), SHA256
`535918fe291d8fe9c707f9afc75bdb1dfd0c3e48b77376048665c35e569bd205`.
Evidence: `dist/iteration-h02-bootfix/repack.json`, `package-inspection.json`,
`repack.py`, `make-test.log`; hardware progress: `dist/hardware-mcp/bootfix-*`.
This supersedes the original package for the authorized development trial.
Boot and peripheral qualification must still be reported separately.

## RGB50 touch profile — 2026-10-14

On the running board, `/proc/bus/input/devices` showed no Goodix device. The S4
baseline had both I2C2 and its misplaced I2C0 Goodix child disabled, with an
800x480 range. The enabled I2C3 failed to probe due to a PB6/PB7 UART conflict;
that unrelated bus is not the RGB50 touch bus.

The user-directed T113s3 Pro RGB50 reference supplies the touch wiring only:
I2C2 on PE12/PE13, Goodix at 0x14, PB3 interrupt, PB2 reset, 1024x600 range,
no axis reversal/swap. Patch `tina/0002-rgb50-goodix-touch.patch` moves the
Goodix child onto I2C2 and enables both. S4 SD NAND/SoC identity, boot chain and
partition layout remain S4. Use `scripts/prepare_s4.py --rgb50-touch` to record
this explicit wiring profile and retain both baseline and effective file hashes.
Do not infer that all S4 carrier revisions have this wiring.

`buildroot/busybox.fragment` enables CONFIG_FEATURE_BLKID_TYPE so the storage
init can identify the already provisioned ext4 volume. No startup formatting or
fallback that accepts an unknown filesystem is introduced.

Trial image SHA256: `bcb30723ee71ccd446d9e405bcb7b4e4d60ee24f9ee3ed2d4099e4c4e437f7b3`,
1426075648 bytes. Only final DTB/boot package, kernel boot image and rootfs plus
checksums differ from the bootfixed image. Boot0/OP-TEE/U-Boot and UDISK are
unchanged. Evidence: `dist/iteration-h03-touch/` and `dist/hardware-mcp/touch-*`.
Driver registration, real coordinates and Qt interaction require physical tests;
compiled nodes alone do not prove touch operation.


The first touch trial booted, and ext4 UDISK mounted automatically, but I2C2
failed because PE13 was already claimed. Debugfs identified the owner as
`PHY reset`, not the disabled camera. The inherited SoC MDIO node requests
PE13 despite the board's legacy `phy-rst` specifying PE10. This kernel uses
SUNXI_GMAC_NG and the MDIO reset property, so the RGB50 profile overrides
`gmac0_phy0/reset-gpios` to PE10 (active low). Ethernet link operation remains
unqualified; its existing NG driver also reports a missing `phy-clk-type`.
PE12 was claimed as `otg_id`; this profile uses a fixed USB0 device role and
removes ID/VBUS GPIO references, preserving the dedicated USB FEL flash path.
Automatic USB host/device switching on this connector is not part of this profile.
The disabled camera status typo is also corrected.

Qt's linuxfb backend is set to evdev because this Tina rootfs uses mdev and its
udev cannot start with the current kernel's inotify configuration. The board
runtime selects `gt9xxnew_ts` by device name instead of assuming an event number.

Combined pin/Qt trial image SHA256:
`f6ed9c2c60a643421d33b4db390e49f43688a61bbe09dd5413fddf771b241731`
(1426075648 bytes). Evidence: `dist/iteration-h03-touch/pins-image/` and
`dist/hardware-mcp/touchpins-*`. The final container inspection and Boot0 gate
passed; hardware touch validation is pending. Earlier touch images are diagnostic
artifacts, not accepted touch firmware.


The pin-conflict image booted and passed application health automatically, but
Goodix at 0x14 still NACKed. Direct GT911 product/version register reads at 0x5d
returned `39 31 31 00 60 10`. Short config reads reported 1024x600. The vendor
I2C drv/DMA mode failed the 240-byte configuration read (states 0xf9/0x50), leaving
the input driver on its 4096x4096 fallback when instantiated at 0x5d. Changing the
idle bus to engine mode made the full read succeed and driver registration then
reported X=0..1024, Y=0..600. The profile now uses address 0x5d and twi_drv_used=0.

An older T113s3 Pro tutorial describes PE0/PE1 reset/IRQ instead of PB2/PB3.
A reversible PE0 reset/PE1 address-selection diagnostic did NOT establish that
wiring on this board; those lines were returned to inputs. No PE0/PE1 substitution
is committed to the profile. PB2/PB3 remains the repository wiring reference,
pending real interrupt/event testing. The first 45-second event capture saw zero
events and there was no user confirmation of a tap; do not call it a touch pass.
Evidence: `touchpins-address.json`, `touchpins-engine-bind.json`,
`touchpins-events.json`, `touchpins-events-final.json`.


Durable engine-mode development image:
`/home/ubuntu/dshancontrol-s4-rgb50touch-engine-20261014/t113_s4_linux_100ask_uart0_rgb50touch_engine.img`
SHA256 `ac593abd7666393c2b7e554ea8d775b7255fb7c75e8868b8f6c505159ae233bc`.
Final container, compiled DTS, Boot0 gate and network-transfer SHA256 all pass.
Flash task: `flash-1791957654587823900`. Physical event/UI acceptance remains
separate from driver and image validation.


### V1.3 carrier jumper gate — latest physical blocker

The complete local V1.3 base schematic, page 7, confirms PB2/PB3, not PE0/PE1.
J14 routes PB2 between CTP_RST and CAN0_TX; PB3 between CTP_INT and CAN0_RX.
Touch requires shorts 1–2 and 4–5. CAN0 selects 2–3 and 5–6. Inspect the board's
pin-1 mark and revision before applying this mapping; power off before moving
jumpers. These shared routes cannot claim simultaneous native CAN0 and touch.

The engine-mode image flashed/verified successfully and booted with GT911 at
0x5d, correct 1024x600 ranges, automatically mounted UDISK, and health=0. However,
the user explicitly reports no response after tapping. The configured PB3 IRQ
count remains zero, so touch is NOT accepted. The schematic makes J14 routing a
specific item to check; its actual physical position has not yet been confirmed.
PE1's single edge on requesting observation is not evidence of a touch IRQ. PE0
held low does not suppress GT911 I2C reads, further rejecting the old tutorial's
PE0 reset mapping on the currently connected hardware. Diagnostic GPIO handles
and raw I2C capture were released; PE0 restored to input; desktop left running.
The user is powering off to reconnect jumpers; await power-on before more access.

Schematic source: `/home/ubuntu/T113-tina5v1.2-sdk/eink-sch/100ASK_T113-Pro_Base-SCH_V1.3.pdf`,
SHA256 `2100fa2decf074978107358d01c875081f9acd7233bcbf670380c54833234a4e`.
Reference URL: https://dl.100ask.net/Hardware/MPU/100ask_t113-pro/100ASK_T113-Pro_Base-SCH_V1.3.pdf
Evidence: `dist/iteration-h03-touch/j14-schematic.png`, `base-complete.txt`,
`dist/hardware-mcp/touchengine-probe.json`, `touchengine-diag-cleanup.json`.


### 2026-10-14 — touch recovered; brightness interaction follow-up

After power-off/reconnection the user confirmed the desktop is operable and
clarified that only the brightness slider is awkward. PB3 interrupts increased;
Qt opened Goodix event4, health=0, and capture recorded 5103 real press, motion and
release events. This supersedes the earlier zero-IRQ/J14-wait blocker; exact
physical jumper positions were not independently photographed or inspected.
Evidence: `dist/hardware-mcp/j14-recheck-probe.json`, `j14-capture-read.json`,
`j14-events-network.json`. Global touch calibration remains unchanged.

The brightness control now accepts absolute taps across a 48-pixel-high target,
retains the finger offset when grabbing its enlarged handle, clamps drags at the
ends, and offers 44x48 minimum-size +/- controls with a 3/255 adjustment step.
The existing save/confirmation flow remains in use. `make test-hmi`: 33 passed,
including touch, keyboard, dirty-state preservation and responsive layout tests.
`make test`: 5 core + 121 Python tests/gates passed. Cross-compilation with the
T113 Buildroot Qt/sysroot passed. Actual 1024x600 Qt screenshot reviewed locally;
artifacts under ignored `dist/iteration-h04-brightness/` and `dist/ui-test/`.
Physical comfort of the revised slider still needs user confirmation.

AIC8800D80 modules already present in the vendor image were loaded for this boot:
`aic8800_bsp`, then `aic8800_fdrv`. SDIO detection and firmware loading succeeded;
wlan0 associated with the authorized lab WiFi and DHCP assigned 192.168.1.64.
Credentials were configured with console echo disabled and are not in evidence.
Board-initiated HTTP downloads reset; a one-shot TCP push over WiFi succeeded
(observed peer 192.168.1.39, rather than SDK ens33 address 192.168.1.199).
The entire 7761920-byte trial archive was verified against a separately pinned
SHA256 before extraction: b17d12569d498131163da4ec04ad9bdead7121ffadeebf6980d5250c08f5e229.
No file payload was transferred over UART. Network startup/persistence is not
accepted by this one-boot connection test.


Brightness trial deployment completed: PID 2942 runs
`/tmp/dshancontrol-brightness-trial/omnigate-hmi`, SHA256
`f6653fe3bc1766b1a5b1684839c223d2a5455d3d4e1d6bb61609a14a5ef10298`.
The trial retained the old HMI environment, opened framebuffer and input, remained
running through the startup observation, and system health returned 0. Only the
HMI process was replaced; Web/supervisor were not stopped. Installed release
metadata and version-tree contents remain untouched. This is a temporary lab
trial, not a signed application update. Reboot restores the preinstalled HMI;
immediate restore command: `python3 /tmp/dshancontrol-brightness-trial/try-hmi.py restore`.
Physical slider comfort still requires user feedback. Evidence:
`dist/hardware-mcp/brightness-trial-health.json`. Nothing committed or pushed.

## Native brightness interface — 2026-10-14

The live vendor image has no /sys/class/backlight and no lcdbl attribute under
the display class. /dev/disp is the real display character device. The audited
S4 build's exported video/sunxi_display2.h defines GET_OUTPUT_TYPE=0x09,
LCD_SET_BRIGHTNESS=0x102 and LCD_GET_BRIGHTNESS=0x103 using four unsigned-long
arguments; LCD output type is 1. Header SHA256:
bc51a3dee3cf5529f3e6c98afdc9e8eb418236bf0e929db90e12c7509f52eeb8.
Source artifact: system-sources/bsp-out export recorded locally under
/home/ubuntu/dshancontrol-s4-prepared-20261013-h02/bsp-out/kernel/build/user_headers/include/video/sunxi_display2.h.

C++ platform-service testing read brightness 100, set/read 160 and restored/read
100. Holding the shared updater control lock rejected a second write without
changing 160. Product health=0 and original system/preinstalled identity hashes
remained unchanged. Test receiver/archive bytes crossed WiFi only; UART carried
commands and results. Evidence: dist/hardware-mcp/ota-native-display-physical-result.json.
Archive SHA256: be4d35794515429a345c5067a4f08ee17caa9cd5fb107b3616a64de0b1e847da;
ARM binary SHA256: 2ce162040811c28e8c18237cc8e2ea90d0b5a97759cd27fac9ba654c94e87cfd.
This used an explicitly temporary diagnostic service with the actual still-pending
system contract; it does not provision production update keys or qualify OTA.
