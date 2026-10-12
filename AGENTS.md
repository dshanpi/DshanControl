# DshanControl engineering gates

## Product scope (user-confirmed 2026-10-12)

DshanControl is the industrial-control desktop application system. Extend its
existing T153 implementation to T113s4 SD NAND + Buildroot. This is not an EinkOS
port and not a shared desktop product with SmartDesktopMimi. EinkOS supplies the
release/separation reference only. SmartDesktopMimi is a separate future project.
Preserve existing T153 support. Future A133/T527/RK3576 ports reuse DshanControl's
application contracts and update protocol, with separate board adapters and tested
artifacts; never infer support from another board's successful test.

The S4 hardware source of truth is
https://github.com/DongshanPI/100ASK_T113s4-SdNand_TinaSDK5
(`t113_s4/100ask`, SD NAND, Buildroot). Do not substitute
`t113/evb1_auto_nand` SPI NAND or copy T153 DTS/partition tables into S4.

## Required gates

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
