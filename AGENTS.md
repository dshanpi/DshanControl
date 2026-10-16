# DshanControl engineering gates

The current machine-readable S4 release status now references the replacement
board'sca98 image, actual Qt development OTA and verified r2 delivery; old-board
sequence6/power evidence is retained under historical_development_evidence.
See docs/S4-COMPLETION-AUDIT-20261016.md for G01–G12 scopes and remaining gates.
Metadata/layout6tests pass; no runtime/image/tag/asset changes for this correction.

## Publication-corrected full image and verified r2 delivery (2026-10-16 UTC)

Current board is complete imageca98f9da61b1f1fa298fc160152d9f547ed5808ddfc84bfa7f2158f5b86c144a,
factory5e268ac5a5ad6a31f63d7ae8f2a0a25a7a10ed4e/version.20261015.4/sequence0.
No runtime overlay or lab helpers/mounts/credentials remain on board. Original
user restore9files/hash checks and normal rebootc6d352a1-b8b4-4535-9491-79155fb8adcf
pass automatic WiFi/DHCP/UTC/defaultHTTPS200/native health and actual Qt401/Web400
role/device/heartbeat proof. All658 old telemetry rows preserved,660atcheck.
Immediate private backup7bc8b969…156files/4DBchecks retains all old lab state;
lab high_water2/quarantined sequence1 are archived, not new factory update state.
Original pre-lab backupA8 and every historical image/draft/evidence remain.
Flash1792111615839406300 write/verify/reboot success. Host248/9skip, actualARM17,
sourceCI38828856356/38828751115, rootfs104files/43ELFs/container41/Boot0/privacy
and1669 runtime equality pass. SDK122unchanged; no full clean reproduction claim.
Primary system Draft414421357/tag system-v0.1.0-dev.20261015.4--t113s4-sdnand-r2
has20assets all redownload/hash/Git/archive/SDK reference closure verified, fresh
Draft/tag metadata checked. Original Draft414420299 retained and superseded:
inherited SDK metadata referenced a missing report; r2 corrects paths, same image.
Do not mutate assets or reflash for an archive metadata fix. Draft metadata PATCH
must explicitly include tag_name/target_commitish: omission regenerated its
untagged association; initial verifier refused it, old association was restored
without changing Git tag object/assets. Primary r2 remains separately verified.
See docs/S4-PUBLICATION-SYSTEM-20261016.md and structured receipt. Formal PCB/
libc/runtime contract values stay pending and no production key is provisioned;
derived libc/runtime evidence exists separately. Relay6 is singleRelay/write-only
last-known state, no measured voltage/backfeed evidence and no power command used.
Whole long goal remains active pending actual PCB, production trust/industrial
maintenance and measured electrical power matrix; do not claim production.

## Actual Qt .4 installation and reboot (2026-10-15)

Private application Draft414397836, source5e268ac5a5ad6a31f63d7ae8f2a0a25a7a10ed4e,
version0.1.0-dev.20261015.4/sequence2: all9 assets redownload/signature/Git blobs
verified. Real production Qt UI check/download/install pass; download13.791s,
install56.065s. System runtime overlayf0f0da1b… replaces image's stripped739217f5…
after verified quiescent private backup84d96179…112files/4DBchecks. This is b20
plus explicit system overlay, not an unmodified image and not an application
payload system update. .3 failed sequence1 remains quarantined/high_water2.
Initial normal reboot16ed4f25… failed due lab contract on unmounted UDISK;
preserve failure and explicit manual storage/WiFi/UTC recovery, not an auto pass.
Lab-only S59 now mounts via native original rootfs contract before S60/S68.
Corrected normal reboot58f89141-9838-4103-bf9f-6d4ae7b3785f automatically runs
Web400/HMI401/native daemon, GUI heartbeat/fb/named touch, UDISK/WiFi/UTC/default
HTTPS200 without manual repair. Volatile token is gone; original user data7file
hashes preserved, formal identity and factoryea53 unchanged. Host251(9skips),
focused lab6 pass. No production/physical sensor/electrical power qualification.
See docs/validation/2026-10-15-real-qt-application-update.json. Complete system
imageca98f9da… from clean5e268ac passes static104files/43ELFs/Boot0/container/
privacy gates but has NOT yet been flashed. Preserve fresh lab/original evidence
and prepare current658-row restore before any flash. Whole long goal stays active.

## Real GUI installation failure and publication correction (2026-10-15)

Supersedes older "install not attempted" and "no actual token" entries. Actual
Qt check/download of Draft414377142/source4b5533f passed, download13.153s. Actual
install passed native maintenance then failed start: daemon umask077 published
new version/HMI0700, preventing UID401 execution. Automatic rollback completed:
factoryea53 functional health/GUI heartbeat pass, transaction cleared, failed
sequence1 quarantined and high_water1 retained. Never reinstall .3 or clear state.
Original user data remains separate; private pre-lab backupa8a5777e has86files,
2DBs/658telemetry. GitHub token is root-private tmpfs only; signing key SDK only.
New system-runtime source explicitly publishes fully verified version trees
0755 directories/executables and0644 ordinary files/installed metadata; private
inputs/locks/state keep077/0600. Host248(9skips) and extra root reduced-UID kernel
access case pass. Actual isolated ARM17 all pass, including reduced-UID kernel
access; evidencee04a9fe1d459914934dea365c2566d17705207ec82b2613b60bf6a43f2a52c98. Stage next application .4/sequence2 from a clean commit. System
runtime fixes stay outside application payloads; any board overlay needs explicit
source/binary identity and preserved old binary. See
docs/S4-UPDATE-PUBLICATION-PERMISSIONS-20261015.md. Whole goal stays active.

## Lab maintenance JSON representation correction (2026-10-15)

Lab is now active in separate persistent apps/data bound under /run; original
production data/config backups remain preserved (pre-lab86files/2DBs/658telemetry).
Real Qt GUI check/download of private Draft414377142 (.3/source4b5533f, sequence1)
passed actual API TLS/signature/target/size/SHA; download elapsed13.153s. Actual
credential reached only root-private tmpfs via mTLS; signing private key stays SDK.
Do not repeat claims from older entries that lab/token provisioning never occurred.
Before installation, Python inherited-lease fixture was refused because native
initialize reformatted default JSON. Actual parsed configurations are identical;
no different paths/transport settings. Preserve initial refusal evidence. Lab
maintenance now pins trusted default file SHA and compares semantic JSON; three
regressions prove formatting accepted, changed transport refused, changed trusted
defaults refused. Other lock/empty-device/role-I/O checks remain. This is a TEST
helper correction, not a production updater or signed .3 artifact change. Published
.3 stays pinned4b5533f and byte-identical. Install has NOT yet been attempted.
Continue patching only this root-private lab verifier with tracked hashes, rerun
preflight once conditions change, then actual GUI install/normal reboot/rollback.
Formal system-release.json stays pending; do not claim industrial or power safety.

## Qt OTA lab preparation source (2026-10-15)

Candidate application VERSION is0.1.0-dev.20261015.3; this is not yet installed.
Current board remains verified b20/ea53 with original user data and formal pending
contract. tests/hardware helpers are NOT in image recipes/application payloads.
Three actual production-scope refusal cases pass without input events/config/
mount/health changes. Reviewed GT9xx driver uses protocolA (GTP_ICS_SLOT_REPORT0),
1024x600 axes, ARM input_event16bytes. GUI software event tests require actual
isolated lab Web/HMI/process/data/mount scope; never claim physical sensor proof.
Bootstrap prepares actual devices with original fixed production policy, binds
separate persistent lab apps/data into volatile /run, and keeps role records in
tmpfs. Maintenance helper verifies inherited already-held leases, default empty
transport configuration, no configured devices/points and bounded role I/O;
it is GUI-only development evidence, NOT an industrial safety interlock.
See docs/S4-QT-OTA-LAB-20261015.md and production-negative receipt. No lab mounts,
root-config replacement, actual token transfer or positive maintenance test yet.
Formal PCB/trust and industrial/measured-power qualification stay pending.
Next compile clean-source ARM HMI, create signed private app Draft, prepare a
concrete root-private lab bundle, preserve fresh user state, then real Qt GUI
check/download/install/reboot/rollback. Do not add a production GUI test backdoor.
Whole long-task goal remains active.

## Actual native-role full-image delivery (2026-10-15)

Current board now boots corrected immutable imageb20c948f… / factoryea53f86a14283bd3a3b73b59b5224244eaec0a09,
not the older factory95 or failed image91. Vendor flash/verify/post-action succeed;
automatic native service, WebUID400/G400 with only NET_BIND_SERVICE and QtUID401/G400
with zero capabilities pass, both NNP1. Qt actually mapsfb0, opens named GT911event4
and advances GUI/backend identity heartbeat. Python supervisor absent. Original
user data restored9files; all598 original telemetry rows preserved (600atcheck),
other original rows and static config hashes preserved. Normal reboot bootUUID
00e70a3c-8318-4c65-8366-569bcd474fd3 automatically restores WiFi/DHCP/UTC/defaultHTTPS
and native functional health without manual repair. Private original backup0942205b…
(44files/2DBs), immediate preflash backup21ff7917… (53files/2DBs), failed image91
and all historical evidence retained. Build host244(9skips), CI38813089186 success;
rootfs104files/43ELFs/container41 and privacy checks pass. BSP/toolchain/dependencies
reused with122 inputs unchanged; no full clean SDK reproduction claim. Runtime1669
image files equal prior actual observation, libc2.21/runtimeIDs4-b5c814f0…; formal
hardware/trust contract remains pending. New system Draft414347622/tag.20261015.2
has18assets all redownload/hash/gzip/Git-source verified; fresh metadata stillDraft,
source tag pins ea53. See docs/S4-NATIVE-ROLE-SYSTEM-20261015.md and receipt.
Whole goal remains active. Continue real signed Qt app download/install/reboot/
rollback in explicit isolated development scope; keep original data and formal
contract intact. Actual PCB/industrial maintenance/measured electrical power and
physical touch-event qualification are still absent; never claim production.

## Actual sysfs permission preflight correction (2026-10-15)

Full image91193/source75cd7ed flashed, verified and rebooted successfully, but
native startup failed before role launch: readBytes rejected sysfs nominal
PAGE_SIZE against short attribute limits. Preserve that failed image/evidence;
never report its native-role startup as passed. A dedicated bounded sysfs reader
now verifies SYSFS_MAGIC and actual content length; ordinary-file bounds remain.
Host244 (9 skips), actual ARM NativeBootTests9 and a separate read-only production
permissions-plan pass. Raw runner's copied broader scope label is historical;
only the nine test IDs ran. See docs/S4-SYSFS-PERMISSION-READ-20261015.md and receipt.
Original user backup0942205b… (44 files/2DBs) remains verified on SDK; private
restore bundle is transferred to the board but has NOT replaced live data yet.
WiFi/DHCP/UTC work. No failed launch replay, actual role/device metadata changes
or production qualification. Continue a corrected full image, preserve current
board state before the next flash, then actual nonroot GUI and data restoration.
Whole goal remains active. Operational notes omit the private site network name.

## Pinned development Draft transport source (2026-10-15)

Explicit development mode with automatic installation disabled permits one fixed
Draft release ID and a root-private tmpfs token file. TLS verification remains
mandatory; Authorization is confined to the initial fixed GitHub API endpoint
and stripped from asset redirects. Signed metadata, candidate identity, asset
IDs, exact size and SHA256 are verified again before publishing a download.
Production configuration rejects this path. Test-only TLS driver is not shipped.
Use canonical /run/dshancontrol: /var/run is a symlink on the selected board and
must not be passed as an O_NOFOLLOW directory. Host243 (9 skips), Qt37 and final
actual isolated ARM46 pass. The new read-only supervisor also verified the live
system daemon PID without stopping it. See docs/S4-PINNED-DEVELOPMENT-RELEASE-20261015.md
and its structured receipt. No actual GitHub token or operational signing private
key was sent to the board. No new physical GUI, real upgrade or power acceptance
is implied. Continue the new native-role image, fresh backup, real nonroot Qt and
signed-update/reboot/rollback work. The whole goal remains active.

## Native reduced-role startup integration source (2026-10-15)

New native S4 templates enable managed records, native supervision and fixed-role
IPC. initialize applies permissions before daemon recovery; native application
start runs only web/HMI through the C++ launcher, without Python supervisor.
Private records are run/processes, HMI heartbeat is run/hmi/heartbeat.json and
launcher receives the configured path. Native restart-web refuses a maintenance
bypass. Legacy nonnative paths remain. Host237 (8 root-only skips), additional
root initialize/permissions5 and startup1, and final actual isolated ARM91 pass.
Initial ARM91 had9 failures/1error including subtests: zombies misclassified as
active roles, fixture prefix-replacement collision, and an8s pre-staging limit.
All initial evidence and missing-success-receipt records are retained. Final
64MiB case observed staging at8.51s and completed initialization29.21s under180s.
See docs/S4-NATIVE-STARTUP-20261015.md and receipt. No live factory95/imagef71,
account, data, device, startup or trust modifications occurred. Continue pinned
private Draft transport and a new reviewed native-role image with fresh backup,
actual nonroot Qt/device and signed-update/reboot/rollback/measured-power tests.
Whole goal remains active; source wiring and isolated tests are not deployment.

## Fixed-role native IPC authorization source (2026-10-15)

Explicit s4-roles-v1 requires root service owner, clientGID400 and exactly
UID0/400/401. Managed production roles require this policy. Web400 may only
use display/health methods; HMI401 additionally uses update identity/status/tasks.
Neither app controls service lifecycle. Real SO_PEERCRED identity precedes typed
method/parameter/maintenance/signature checks; policy is never client-selected.
Host235 (7 root-only skips), additional root2 and actual isolated ARM23 pass,
including12 role RPC observations. See docs/S4-NATIVE-ROLE-PEERS-20261015.md and
receipt. No live accounts, startup, data, device or trust changes. Next wire
native initialize-permissions/startup/heartbeat and validate actual nonroot Qt,
then signed update/reboot/rollback and measured physical acceptance. Whole goal
remains active; isolated numeric UID checks do not deploy the production policy.

## Native permission policy source (2026-10-15)

C++ permissions-plan/apply preflight immutable application versions, known data,
fixed role accounts and named S4 device/sysfs identities before metadata changes.
Apply refuses busy leases, PID/launch records and any surviving role UID process;
root-private journals retain initial metadata and original syscall stage/errno.
No broad recursive chown, unknown file deletion, app signals or content writes.
Preserve known database WAL/SHM after quiescence for SQLite's own crash recovery;
mere presence is not proof of an active process, orphan auxiliaries still refuse.
Web owns private data; HMI owns framebuffer/named touch/heartbeat; private records
stay root-only. /dev/disp and terminal nodes receive no app access. Actual Goodix
sysfs parent is virtual/input, not guessed I2C ancestry; event number is dynamic.
Buildroot fixed locked nonlogin accounts400/401:G400 pass the selected real
mkusers staging/collision/idempotence checks. Private updater config is600.
Host233 (5 root-only skips), additional root5 and final isolated ARM39 pass.
Initial ARM38/39 passed with one15s synthetic CLI timeout, retained separately;
software test deadline60s and WAL-preservation refinement are in the final build.
See docs/S4-NATIVE-PERMISSIONS-20261015.md and receipt. Actual board accounts,
data metadata, devices and startup remain unchanged. Role RPC and startup wiring,
real nonroot Qt/device operation, signed update and physical acceptance remain.
Whole goal stays active; these isolated fixtures do not qualify live deployment.

## Native read-only supervision source (2026-10-15)

Opt-in native supervision uses bounded asynchronous application-health helpers
and exposes service.health. It observes /proc/locks by file device/inode without
acquiring application leases, skips busy/changed-version samples and reports
consecutive failure alarms with original stage/code. No app signals/relaunch,
reinstall, database writes or hardware watchdog feeding. Status is in memory,
not historical alarm persistence. Production enablement requires managed roles.
Host228 (3 root-only skips), focused4 and final actual ARM21 pass; a separate
pre-refinement ARM21 also passed and retains distinct evidence/binary identity.
See docs/S4-NATIVE-SUPERVISION-20261015.md and structured receipt. Factory95/f71,
accounts, devices, startup and trust remain unchanged; supervision is not enabled
in live production. Continue typed role/data/device permissions, production
wiring and real Qt signed update/reboot/rollback/measured power acceptance.
The whole goal remains active; independent passing suites do not complete it.

## Public native update identity source (2026-10-15)

Native HMI discovery now uses public update.identity RPC and typed key-configuration
availability; it does not open private updater config or trust files. Host224
(3 root-only skips), Qt37 and an additional real-root/nonroot-peer case pass.
Actual ARM17 initially had15 service-fixture failures due to overlong Unix socket
paths; same binary passed all17 after using private /tmp fixture directories.
Preserve both receipts in docs/validation/2026-10-15-public-update-identity.json.
The real nonroot peer passed in both runs: reads identity/status, cannot read
private config or manage daemon lifecycle; another UID is denied. New runtime
and HMI cross-compile, but the new HMI has not run on the live display. Factory
application, accounts, device permissions, startup and trust remain unchanged.
Continue native supervision and role/data/device policy before startup wiring,
then real Qt signed update and physical acceptance. Whole goal remains active.

## Native launch and primary records source (2026-10-15)

Opt-in C++ application-start now forks fixed native roles, records private
birth/boot/inode/argv/credential identity before releasing the child, reduces
credentials and closes application-inherited descriptors at exec. Managed stop
uses the primary receipt even if current/installed is broken; missing/tampered
records refuse fallback. Update/control/lifecycle leases serialize transitions;
typed native hooks adopt verified inherited descriptors without leaking to apps.
Native system supervisor is health-only, never an application stop target.
Host222 (2 root-only skips) and actual isolated ARM58 all pass. See
docs/S4-NATIVE-LAUNCH-20261015.md and its receipt. Existing factory9594/imagef71,
accounts, device modes and startup policy remain unchanged. Production role
accounts/data/device policy, startup wiring, native supervision and public HMI
identity discovery remain necessary before deployment. Real Qt signed update,
reboot/rollback and measured power-loss acceptance remain unfinished. Whole goal
stays active; do not call an isolated launch suite production qualification.

## Native owned-stop source (2026-10-15)

The next native source step adds pidfd-bound TERM/exit waiting and shared process
identity checks. Host204 and one clean ARM44-case isolated suite pass; the live
factory application was only probed read-only, never stopped by the new command.
See docs/S4-NATIVE-OWNED-STOP-20261015.md and its receipt. Keep PID records on
timeout/change; no force-kill, bare-PID fallback or relaunch. Selected vendor ARM
kernel pidfd support was checked from source and on-device; cast pid_t to32-bit
for its variadic syscall ABI. Native system configs with missing/nonexecutable
runtime now refuse rather than enter legacy paths (host-tested script guards).
This step remains the same-UID/current-path transition: full native start,
independent launch receipts, nonroot roles, complete shared lifecycle leases and
supervision migration remain required before qualification. Live factory9594 and
imagef71 remain immutable; physical new-init/privilege/realQt OTA/power acceptance
is not implied by isolated tests. Continue the active goal, do not stop here.
The subsequent credential primitive has host208 (2 root-only skips), additional
host-root4 and actual ARM4 passing probes across exec. Web-shaped bit10-only and
HMI-shaped zero-cap profiles could not regain root or read the private test sample.
See docs/S4-NATIVE-CREDENTIALS-20261015.md. TestUID60000/60001 do not assign real
application accounts; the function is not yet wired to the production launcher.

## Native functional health and boot storage source (2026-10-15)

Native health source e28a78c adds read-only identity, owned process/argv checks,
bounded fixed-loopback identity, SQLite/schema checks and monotonic GUI heartbeat.
New S4 source uses software contract platform-2 and mandates heartbeat; T153 is
unchanged. This is not a PCB revision. Host186/Qt36 and isolated ARM27 passed.
Subsequent native boot/storage/init source has host194 and ARM35 distinct cases:
initial34 passed, one64MiB initialization hit the test's15s deadline; same binary
passed that case alone with a180s bound, measured31.63s. Keep both receipts, not a
fictional clean35-case run. New native storage/current-application read-only probes
passed on the board. See docs/S4-NATIVE-FUNCTIONAL-HEALTH-20261015.md and
docs/S4-NATIVE-BOOT-STORAGE-20261015.md. Live factory9594/imagef71 are unchanged;
new native boot scripts and HMI producer have not been installed/reboot-qualified.
Never format/repair storage, reset a broken current, overwrite user defaults or
delete unknown/interrupted initialization trees. Exact boot/device/mount checks
remain mandatory. Continue native owned lifecycle, least privilege, supervision,
real Qt signed-update and measured power acceptance; whole goal remains active.

## Netclock full system delivery and next phase (2026-10-15)

The subsequent sustained task's first stage is complete. New immutable source
`9594e8b105915bd450af69d48d4631e46031ca93` produced sanitized image `f71bcfff...`;
the bound replacement board was flashed once, private data restored, and a normal
reboot passed automatic WiFi/DHCP/UTC/default-verified HTTPS and application health.
All 157 original telemetry rows were preserved. New Draft 413927654 has 17 assets,
all re-downloaded and verified; older drafts and images remain unchanged.
See `docs/S4-NETCLOCK-SYSTEM-DELIVERY-20261015.md` and its structured receipt.
This supersedes earlier statements that network/time fixes are absent from the
latest image. BSP/toolchain and unchanged installed dependencies were reused;
this is not a completely fresh SDK reproduction. Do not mix later working-tree
health changes into that frozen image's identity or move its tag.
Continue stages 2–4 from `docs/S4-COMPLETION-PLAN-20261015.md`. Exact PCB revision,
production signing/maintenance/least privilege, real Qt signed update acceptance
and measured complete power removal remain unqualified. A relay command alone
does not prove 0 V or absence of USB backfeed. Keep private backups and keys local.
SQLite connection context managers do not close connections: explicitly close and
seal snapshots, verify logical rows, and exclude volatile auxiliary files from
static post-reboot preservation expectations.

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
The first normal reboot exposed a real wpa_supplicant -B/PID publication race:
WPA later associated but S72 had already refused, so DHCP was skipped. Preserve
that failure. Poll owned PID/executable/argv readiness for at most five seconds
without launching again; test both delayed PID publication and missing PID.
Manual completion of skipped DHCP does not qualify automatic startup.
A normal reboot also reset the unretained clock to 1970. System-owned S73 uses one
foreground client-only NTP attempt, bounded by timeout30, after S72; preserve
errors and reject an epoch clock. Plain NTP is not authenticated NTS or RTC
qualification. Require real post-reboot UTC and verified HTTPS evidence.
Corrected source 238d98d passed a normal reboot: automatic WPA/DHCP/route,
S73 UTC and verified HTTPS200, writable ext4, native/app health, immutable identity
and data marker. Host 171 and actual ARM native 15 / 45 CLI observations passed.
These are development overlays, absent from the immutable dc62 base image;
production identity/signing/maintenance/power-loss remain unqualified.
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

- Network SSID: `[site network name kept in local private configuration]`. Credentials are stored only in the local private
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
