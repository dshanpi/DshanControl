# DshanControl engineering gates

## Replacement board and development acceptance (2026-10-15)

The user confirmed a replacement board and authorized reflashing it, then asked
the primary agent to define and execute a sustained development acceptance task.
The sanitized native image `dc62a4b5...` was flashed once with vendor verification.
Fresh UART confirmed a changed boot ID, automatic writable ext4 UDISK mounting,
factory source `12359ec...`, and native/application health exit 0. The user
confirmed normal display and physical touch. This supersedes the earlier
unavailable-Gateway/no-new-board-operation state; the previous sequence-6 pilot
belongs to the old board. Complete electrical power removal was not measured.

WiFi diagnosis found the selected modules expect `/lib/firmware/aic8800d80`,
while the vendor package installs `lib/firmware/sdio/aic8800d80`. All 13 files
matched the frozen target; adding the relative alias restored driver loading
and wlan0. Keep this as a documented system overlay, separate from the immutable
uploaded image. The post-build preflight must validate firmware before creating
the alias and reject missing/empty/linked files or conflicting aliases.
Keep site WiFi credentials out of images/Git; the S4 startup configuration is a
private root-owned `system/wifi.conf` under the persistent DshanControl root.
Network association, DHCP and reboot persistence require fresh physical evidence.
See `docs/S4-DEVELOPMENT-ACCEPTANCE-20261015.md`. Signed synthetic CLI fixtures
must remain isolated from the live application, contract, trust root and data;
process interruption tests do not establish physical power-loss qualification.

## Native S4 system build continuation (2026-10-15)

New S4 package builds include the C++ runtime and the single S69 system service;
S70 waits for its real RPC readiness, and native-mode S71 does not run the old
Python watcher. Keep these system-owned files outside application payloads.
Graceful shutdown must refuse active task/update/control locks and verify PID
ownership; do not add kill/relaunch fallbacks. The default template still has
pending production identity, no signing key, root-only peers and automatic
installation disabled. This source integration is not physical baseline, least
privilege, complete Python migration or OTA qualification. Preserve the separate
factory-image and sequence-6 pilot evidence. See
`docs/validation/2026-10-15-native-system-integration.json` for tested inputs.
S69 must initialize vendor partition aliases using only do_device_node_switch,
then validate UDISK with the boot map before initialization/native startup. Never
call the vendor formatting helpers. busybox-init-base-files is an explicit S4
dependency; do not assume its alias creation survived replacing vendor S60.
Bind startup readiness to the actual owned daemon PID with service.ready, and
use the monotonic bounded recovery wait. IPC readiness is not application or
industrial health qualification. Keep the required BusyBox applets/fancy
start-stop-daemon options in the S4 fragment.
The MMC environment explicitly sets init=/init. A fresh ext4 build installs
only /sbin/init; the S4 post-build hook must create /init -> bin/busybox after
validating the staged executable. Refuse unexpected init files/links and symlinked
BusyBox destinations before writing. Keep this boot entry in the source recipe
and audit; do not depend on a previous target directory's leftover link.

## Native development delivery (2026-10-15)

New pinned draft `system-v0.1.0-dev.20261015--t113s4-sdnand` uses clean factory source
`12359ec3ceaec23640fa64764c4c09dc94ce56ef`. See
`docs/S4-NATIVE-SYSTEM-DELIVERY-20261015.md` and its structured receipt.
All 17 remote assets were re-downloaded and verified. New selected BSP
and Buildroot target outputs passed static/container/privacy checks; no full
122-component fresh checkout or complete SDK reproduction was performed.
Compute DTB relocation from this build's raw/compressed kernel ELF and BSS
inputs, then compare the unmodified vendor function and actual boot header.
The old image's fixed 0x4113f100 expectation is not reusable; this image uses
0x4113f000 with its smaller compressed kernel. Do not bypass the relocation gate.
No new board flash/acceptance occurred. Preserve sequence-6/data and verify a
network backup plus live device identity before any full factory flash.
Formal identity/signing/maintenance, physical OTA, least privilege and remaining
Python migration remain pending; automatic_updates_qualified=false, Draft only.

## Latest upload authorization (2026-10-14, compressed system delivery)

The user explicitly instructed: upload the S4 SDK differences and images,
compress them, and complete the delivery workflow. This supersedes earlier
source-only upload restrictions. The primary agent may commit/push reviewed
source, gates and sanitized evidence to the existing feature branch, create a
pinned development tag and GitHub **draft** Release, and upload compressed SDK
input deltas, matching frozen factory source, sanitized factory firmware and
version/checksum metadata. Re-download every remote asset and verify its hash.
Keep firmware/vendor binaries out of application Git. Never upload device
credentials, operational private keys, previews or raw lab logs. Remove the
factory FreeRDP shared key on an image copy before uploading; keep the original
image identity and its physical evidence distinct from the sanitized derivative.
Do not reflash the board or overwrite its sequence-6 prototype/data for this
upload. The sanitized derivative has not been physically flashed. This delivery
is not a production C++ OTA baseline; qualification remains false, and no public
Release promotion is authorized by incomplete board acceptance.

## Product scope (user-confirmed 2026-10-12)

DshanControl is the industrial-control desktop application system. Extend its
existing T153 implementation to T113s4 SD NAND + Buildroot. This is not an EinkOS
port and not a shared desktop product with SmartDesktopMimi. EinkOS supplies the
release/separation reference only. SmartDesktopMimi is a separate future project.
Preserve existing T153 support. Future A133/T527/RK3576 ports reuse DshanControl's
application contracts and update protocol, with separate board adapters and tested
artifacts; never infer support from another board's successful test.

User added IMX6ULL-PRO, STM32MP157 and RK3506 on 2026-10-14.
Track these with A133/T527/RK3576 in `docs/BOARD-PORTING-QUEUE.md` as pending;
reuse contracts, maintain separate board inputs/build artifacts and physical evidence.

UI direction: the user requests an industrial blue/white redesign with RGB50
1024x600 as the baseline. Follow `docs/design/industrial-blue-white-v1/README.md`.
Reflow layouts for actual screen geometry; preserve readable text and touch
targets instead of shrinking the entire UI. Generated previews are not runtime
or board evidence. The latest mention of T113s3 has not resolved its relationship
to the existing T113s4 target; do not silently change BSP identity.

Visual acceptance: preserve the reference composition, typography, icon language,
spacing and component hierarchy; a palette change is not a faithful implementation.
Review actual Qt screenshots at 1024x600 against the reference before describing
visual work as finished. Cover every navigation page and child tab, including CAN NMT/SDO,
RS485, EtherCAT, wired/wireless/cellular, service management, audit and app updates.
Check the controls and empty/offline states as well as the overview. Runtime values must remain truthful, including empty states.
For the current UI iteration, keep generated screenshots and comparison artifacts in
ignored `dist/`. The user objected to uploading previews: do not push this iteration
without a new explicit instruction to do so.

The S4 hardware source of truth is
https://github.com/DongshanPI/100ASK_T113s4-SdNand_TinaSDK5
(`t113_s4/100ask`, SD NAND, Buildroot). Do not substitute
`t113/evb1_auto_nand` SPI NAND or copy T153 DTS/partition tables into S4.

## Required gates

- User runtime-language decision (2026-10-14): implement the new production
  platform service and application updater in C++; reuse Qt and the existing
  target toolchain. Do not extend the Python runtime prototype. Existing Python
  implementation/tests remain a migration reference until replacement passes
  compatibility, signature, recovery and board gates. Host build/test tooling may
  continue using the repository's existing tools; do not claim migration complete.
- The user confirmed the 2026-10-14 layered application-update direction in
  `docs/APPLICATION-UPDATE-PLAN.md`. Follow `docs/HARDWARE-AND-UPDATE-GATES.md`
  for the supplied BL-04/BL-05 references and REQ-APP-UPDATE-001..010 requirements.
  LYNX/Workbench requirement IDs are external integration gates, not claims that
  this repository implements those components. Fixed scope, resource locks,
  truthful task projection, strict SSH trust, original stage/errors and no
  automatic retry of failed hardware actions are mandatory. Existing authorized
  scope remains valid; do not add repeated confirmations for the same action.
  Durable update recovery/known-good rollback remains enabled and is distinct
  from automatically reinstalling a failed version. Preserve and reuse the
  updater; structural implementation and board qualification are still pending.
  Temporary HMI trials are not signed application-update acceptance.
- Follow `docs/RELEASING.md` and `platform/contracts/application-update-v1.json`.
- Keep kernel, DTS, bootloader, partitioning, base libraries, board identity,
  updater and trust roots in the system release. Application packages must not
  change these. Board integration examples may live here, but their origin and
  exact hardware inputs must be recorded.
- Classify releases as `application` or `system`. Compatible application releases
  do not require a new hardware commit, SDK rebuild or system image.
- Preserve immutable system/preinstalled application identity; record the active
  application separately. Match product, board revision, ISA/float ABI, libc,
  runtime and contract versions before installation. Unknown/missing values fail.
- Verify signatures anchored in the system and every payload hash. Enforce package
  boundaries, installation locking, durable transactions, startup recovery,
  health checks and known-good rollback. Keep user data outside version trees.
- Industrial application updates must occur in a board-approved maintenance
  state. Never stop live control services merely because a newer release exists.
- S4 persistent application/data storage requires the boot-mapped writable ext4
  UDISK volume. Never format or convert storage on startup or on mount failure.
  Apply the system post-build override after vendor init files are installed.
- Before image packing, verify the selected Buildroot partition table, MMC root
  index, raw rootfs size, actual media capacity, and all SDK component inputs.
  A list of commits or hashes alone is not a complete reproduction bundle.
- Run `make test` for logic/tool changes. New updater/port tests must exercise
  mismatch, tampering, interruption/recovery and existing T153 behavior.
  Cross-compile native changes with the target sysroot; record unavailable checks.
- Physical tests are required for any supported board claim. No hardware, no
  evidence, unfinished signing/rollback, unknown ABI or incomplete source input
  means draft only. Host tests do not establish board readiness.
- Public release tags/assets are immutable. Re-download and verify remote assets
  before promotion. Do not publish releases or label missing tests as passed.
- Do not include device secrets, signing private keys, SDK build caches or vendor
  binaries in this application repository. Run `git diff --check` before delivery.

## Hardware access and flashing (user update 2026-10-13)

- User supplied and authorized hardware operations, including a trial firmware flash,
  via `lynx-t153mx-ominigate-sdk` at `http://127.0.0.1:38926/mcp`.
  Codex registration is persistent locally. The channel name is historical: the
  bound COM44 console reported `allwinner,t113_s4` on 2026-10-13. Re-identify the
  actual bound board before each destructive run; old T153 task history is not S4 evidence.
- Use the Tina vendor system and board-matched vendor firmware package. Do not
  substitute a pure-mainline image, mainline RAM installer or other-board loader.
- User specifies AIC8800D80 WiFi6 and RGB50 1024x600. Consult T113s3 Pro records
  for peripheral reference, preserving S4 SD NAND storage/SoC identity.
- Transfer files over the network (including SDK-host to LYNX-host transfer).
  Never transfer file payloads over UART. UART is for commands, identification and
  boot logs; a supported fixed FEL-entry recipe may use UART. Vendor flashing uses
  the dedicated USB FEL/FES tool, not UART file transfer.
- Observed storage: SD, mmcblk0 3932160 sectors of 512 bytes (2013265920 bytes),
  root mmcblk0p5, UDISK mmcblk0p9 currently vfat. This is a running old system,
  not proof that the new ext4 application/data contract has been installed.
- Current hardware owner is the primary interactive agent. Ralph continues local
  compilation; it must not concurrently reboot, flash, power-cycle or open UART.
  This ownership prevents races, not a new user approval requirement. The user's
  flashing authorization supersedes earlier blanket no-flash text for the primary
  agent; matching, firmware inspection, verification and boot evidence still apply.
- No new authorization to commit, push, upload previews or publish was given.

### Private lab WiFi (user supplied 2026-10-13)

- Network SSID: `Programmers7`. Credentials are stored only in the local private
  file `/home/ubuntu/.config/dshancontrol/hardware-wifi.json` (0600, directory 0700).
- When the AIC8800D80 interface is ready, use that private configuration for the
  authorized board connection. Never print/cat the secret file into tool output,
  Ralph logs, task descriptions or transcripts. Never embed the PSK in tracked
  files, firmware, screenshots or release evidence. Record connection results only.
- Hardware remains owned by the primary interactive agent; do not race its UART,
  power, networking or flashing operations from the build loop.

### MCP channel rename (latest user update 2026-10-13)

The active name is now `lynx-t113-pro-sdk`, still at
`http://127.0.0.1:38926/mcp`. Workbench confirmed project `T113-pro SDK`,
projectId/deviceId 5, COM44 and the same bound USB location. The old Codex alias
`lynx-t153mx-ominigate-sdk` was removed to avoid duplicate connections. This
rename does not resolve the SDK SSH `HOST_KEY_CHANGED` transfer failure; update
LYNX's saved host trust before transferring the package. Do not bypass checking.

### MCP port and project replacement (latest user update 2026-10-13)

Current endpoint: `lynx-t113-pro-sdk` at `http://127.0.0.1:38928/mcp`.
Workbench now reports projectId/deviceId 7 (`T113-Pro SDK`), COM44, but no USB
binding (`state=unbound`). This supersedes port 38926/project 5 above. Do not
reuse old project IDs or assume its former USB binding applies to project 7.
The new project still rejects firmware SFTP with HOST_KEY_CHANGED. LYNX must
update the SDK SSH trust record and bind this board's USB location before flash.

### Current hardware connection recovery

The project 7 USB binding is now configured at
`location:pciroot(0)#pci(1400)#usbroot(0)#usb(9)#usb(3)`.
On the latest retry, SFTP began transferring successfully; the earlier SSH and
unbound reports above are historical. Read `dist/hardware-mcp/trial-flash-status.json`
and the current MCP responses for live state. The primary agent is performing
an authorized vendor-package trial; do not concurrently operate the board.

### Latest physical retry: changed USB port

Project 7 now binds `location:pciroot(0)#pci(1400)#usbroot(0)#usb(10)#usb(2)`.
User physically entered FEL and authorized another vendor flash. Task
`flash-1791948657068341400` completed package writing and verification, superseding
the earlier FES reconnect block. The subsequent boot panicked in OP-TEE
`check_hardware_info` / `plat_init`, before Linux. Do not describe this as a
bootable or accepted system. Current evidence is in
`dist/hardware-mcp/trial-flash-status.json` and `port-retry-uart.log`.

### Boot0 hardware-information gate

The first S4 image omitted the vendor buildserver/update_chip output and failed
OP-TEE hardware checks after flashing. Start the matching vendor buildserver;
require update_chip success and validate hardware information plus checksum from
the FINAL IMAGEWTY container with `scripts/check_s4_boot_header.py`. See
`boards/t113s4-sdnand/hardware-inputs.md` for the exact S4 provenance and pack patch.
Never accept a source .fex check alone or substitute an OP-TEE check bypass.
The original image SHA256 `2da05c73...` is rejected; corrected trial SHA256
`535918fe...` has distinct transfer/flash/boot evidence under `bootfix-*`.

The corrected Boot0 image has now flashed and booted Linux successfully (task
`flash-1791950677798869600`). OP-TEE remains unchanged. DshanControl health passes
with web/HMI/supervisor running after manual ext4 UDISK mounting. Do not claim
cold-boot autostart: BusyBox lacks CONFIG_FEATURE_BLKID_TYPE, so its blkid omits
TYPE and S60 refuses the otherwise valid ext4 volume. Fix that Buildroot/BusyBox
configuration and verify a cold boot before accepting automatic startup.


### RGB50 touch correction in progress (2026-10-14)

The first touch trial proved automatic ext4 UDISK mounting after enabling BusyBox
filesystem type reporting, but did not register Goodix: inherited MDIO PHY reset
claimed PE13, and OTG ID detection claimed PE12. The RGB50 wiring patch now frees
both I2C2 pins, moves the inherited PHY reset to the board's PE10, uses USB0 fixed
device role, and selects the named Goodix evdev device for Qt. Do not confuse the
camera's disabled-node typo with the observed PHY GPIO owner. See
`boards/t113s4-sdnand/hardware-inputs.md` for limitations and exact image hashes.
The primary agent owns the active flash; Ralph is stopped. Require real driver,
event and UI evidence before marking touch accepted. No new push authorization.


GT911 follow-up: the actual controller identifies at I2C2 address 0x5d. Vendor
I2C drv/DMA mode fails its long config read; engine mode succeeds and reports
1024x600. Persist 0x5d + twi_drv_used=0 in the RGB50 patch. Runtime Qt has opened
the correct Goodix evdev device and health passes, but no physical input has yet
been observed/confirmed. Do not equate registration or correct ranges with UI
acceptance. The current trial image hash begins ac593abd; consult live evidence.


V1.3 physical touch gate: J14 must route PB2/PB3 to CTP_RST/CTP_INT (shorts
1–2, 4–5), not CAN0 (2–3, 5–6). Read the actual board revision and pin-1 mark.
Do not claim native CAN0 and touch simultaneously on these shared jumper routes.
Earlier clicks failed with zero PB3 interrupts. After the user's power-off and
reconnection, they confirmed touch works. Recheck recorded 5103 real evdev events,
PB3 interrupts, Qt's Goodix event4 descriptor and application health=0. The former
wait-for-reconnection requirement is resolved. See hardware-inputs.md for evidence.

Latest user scope: only the brightness slider is awkward. Improve its local touch
target, absolute tap/drag behavior and fine adjustment; do not change global touch
calibration. Validate host interaction, target compilation and physical usability
separately. A temporary network-delivered HMI trial is not a signed application
release or proof of full updater/board qualification. Keep trial identity separate
from immutable installed application metadata. No new push authorization.
