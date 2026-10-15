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
/path/to/new-output/build.sh configure
/path/to/new-output/build.sh --jobs 4 application
# Full vendor rootfs, when the build volume has at least 16 GiB free:
/path/to/new-output/build.sh --jobs 4 rootfs
```

Preparation snapshots application sources and hashes, records hardware commit and
board-file hashes, and merges the application fragment into the approved hardware
defconfig. It selects the SDK's preinstalled compiler, not an arbitrary downloaded
Linaro version. Dirty sources are explicitly marked development-draft. It does not
change SDK `.buildconfig`, overwrite the existing board, or flash a device.
`board/` contains the exact hardware board files for review; it is not a built BSP.
The build runner validates snapshot hashes and explicitly sets S4 vendor recipe
variables, removing inherited `LICHEE_*`/`TINA_*` settings from previous SDK sessions.
It records commands, exit codes and logs. The 16 GiB free-space floor for a full
rootfs is a conservative build reserve, not a measured minimum or device capacity.

This builds the userspace integration target, **not a complete flashable image**.
Full-image integration still requires selecting/compiling the S4 BSP and bootloader,
applying the approved board inputs in the hardware SDK, capturing every SDK commit
and local patch, finalizing rootfs and packing the board image. Do not pack using
the workspace's old `t113/evb1_auto_nand` selection.

The separate round-22 local system build now has a statically checked Tina vendor
package and restored-input repack evidence; see [hardware-inputs.md](hardware-inputs.md#packing-handoff).
It includes an empty factory UDISK payload and remains unqualified on hardware.
The development helper above still builds only its documented userspace target.

Latest physical development status (2026-10-14): the corrected vendor image
`ac593abd7666393c2b7e554ea8d775b7255fb7c75e8868b8f6c505159ae233bc`
has flashed and booted, with writable ext4 UDISK, RGB50 1024x600 and user-confirmed
GT911 touch. The full signed application pilot is now sequence 6 / commit
`26088ef1acbc1bf8bf19e6c586d58e38c408a93e`. Native health-code/rollback,
prepared and switched physical interruption recovery, real Qt GitHub checking,
temporary-system cold startup, WiFi DHCP and native brightness have evidence.
This uses a fixture-only contract, copied data and a temporary system overlay;
hardware revision, formal baseline, GitHub download/install and full qualification
remain pending. See [current evidence](../../docs/validation/2026-10-14-board-native-ota-continuation.json).

## Porting choices

- Qt5 Widgets/Charts HMI, Web, core and supervisor use the canonical component sources.
- Buildroot 201902 lacks qmake-package, so the adapter invokes target qmake explicitly.
- Its Flask 0.12.2 is incompatible with the current application. A pinned private
  Python runtime supplies Flask 2.1.2 and dependencies; it does not replace other
  SDK applications' Flask. MarkupSafe uses its pure Python implementation.
- Python 3.7 compatibility fixes preserve behavior on T153's newer Python.
- WenQuanYi font recipe is backported with upstream source/license hashes.
- SD NAND UDISK is required for persistent app/data storage; boot refuses unmounted
  storage. It must be factory-provisioned ext4; FAT cannot support version symlinks.
  The post-build hook replaces the vendor S60 script, which otherwise tries FAT
  formatting on mount failure. DshanControl never formats/converts existing storage
  during startup. Unknown/FAT/damaged volumes require an explicit data-preserving
  provisioning/recovery process. No A/B system partition layout is introduced.
- Only the baseline display channel is configured. CAN, RS485, Ethernet fieldbus,
  and cellular are not copied from T153 or declared verified. GT911 has separate
  S4 driver/event/user confirmation evidence; synthetic UI taps do not qualify
  touch accuracy or simultaneous CAN0/touch operation.

## Historical handoff and current qualification boundary

The 2026-10-13 hardware handoff identifies the connected old system as
`allwinner,t113_s4`: SD media, 3932160 sectors of 512 bytes (2013265920 bytes),
root `mmcblk0p5`, and UDISK `mmcblk0p9` was **vfat** in that old-system observation. Hardware is available
to the primary interactive agent, who owns its UART, power, network and authorized
trial flashing. The local build loop must not access hardware concurrently.
These observations do not validate the new image or its ext4 data contract.

The requested peripherals are **AIC8800D80 WiFi6** and **RGB50 1024x600**.
The compiled S4 DTB and rootfs already carry the approved RGB/SDIO settings,
three AIC8800 modules and six source-matched D80 firmware files. Old-system WiFi
probing reported `set power on fail` and no WLAN interface; module/firmware presence
does not establish a working connection. Keep private credentials out of builds.

Use [the hardware input review](hardware-inputs.md) for the exact peripheral
reference, timing differences and remaining evidence. The original baseline touch
node was disabled with an 800x480 range. The approved S4 RGB50 correction now uses
GT911 0x5d/I2C2 engine and 1024x600; exact changes, pin-sharing limits and image
provenance remain in hardware-inputs.md. The old vfat/network/touch observations
above are historical. Current ext4, display/touch, network, pilot cold startup
and two physical interruption phases have development evidence; physical revision,
production integration, external industrial devices, complete power-loss matrix
and soak remain unqualified.

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

## System input and image preflight

```sh
python3 scripts/capture_sdk_inputs.py --sdk /path/to/sdk --output sdk-inputs.json
python3 scripts/audit_s4_system.py --hardware /path/to/hardware \
  --buildroot /path/to/output/buildroot --capacity-bytes ACTUAL_BYTES \
  --rootfs-image /path/to/output/buildroot/images/rootfs.ext4 --output preflight.json
```

The SDK inventory covers every manifest component, including local tracked changes
and untracked input identities. It does not archive those inputs or qualify a release.
The image preflight follows Tina pack's Buildroot override: the approved baseline
has a 1 GiB rootfs partition in `buildroot/sys_partition.fex`; its separate
`linux-5.4/sys_partition.fex` has only 104 MiB and no UDISK. Do not interchange them.
The current 512 MiB rootfs setting fits the Buildroot partition. The preflight also
checks the raw ext4 superblock and boot root index. Capacity budgets reserve 32 MiB
for boot/partition overhead and at least 256 MiB for data; final physical offsets
and actual app/data space still need checking against the packed image and board.


The preflight records hashes of the selected partition/environment/config inputs and
installed storage policy. Both S60 and storage.py must match the application
repository's system integration sources exactly; a marker comment is insufficient.
It follows versioned env-5.4.cfg selection and the final Buildroot env.cfg overwrite.
A fixed UDISK must itself meet the 256 MiB minimum; it is counted once in the capacity
budget. The post-build finalizer rejects symlink destinations and parent directories
before replacing either policy file. Run it after vendor installation, then rerun the
preflight against the final target. A zero preflight exit means no configuration
errors were found, **not** that packing or publication is approved: pending items and
publishable=false remain authoritative. Raw superblock inspection is not a filesystem
health check or proof that the image boots.

## Archived physical-pilot sources

[validation/20261014-native-pilot/manifest.json](validation/20261014-native-pilot/manifest.json)
indexes the exact public source bytes retained from the successful development
pilot: system startup readiness, lifecycle/health/storage checks, recovery,
C++ RPC adapter and fixed framebuffer/synthetic-input probe. These files are
non-executable reference copies, outside runtime/buildroot recipes and the
application payload mapping. Repository gates check their hashes and syntax.

The archived lifecycle deliberately contains sequence4 health failure and a
manually armed power-cut window. Do not deploy it as a production lifecycle,
execute it on another board, or infer that it provisions a complete pilot.
The original hardware/network session, test signing keys, fixture configuration,
packages, images, screenshots and raw logs are not part of this archive.
Legacy Python health/storage are frozen bench references; the production
platform/updater implementation remains C++.

The successful normal cold boot uses real display RPC readiness before launching
update recovery, rather than a fixed one-second sleep. The restore tool's
`--check` passed; `--restore` was not executed. Current source/asset hashes and
remaining qualification gates stay in the linked physical evidence record.
