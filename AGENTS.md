# DshanControl engineering gates

DshanControl targets the T113s4 SD NAND board. T153 material is historical.
SmartDesktopMimi and EinkOS are design references, not vendored private SDKs.

## Source preview scope
This branch is a source preview. Keep all unverified hardware and business gates
explicit; tests and rendering are not physical qualification. Do not change the
approved reference PNGs or original design requirements to conceal differences.
Keep source, third-party licenses and build recipes together. Never commit keys,
private configs, user databases, board backups, private SDKs or generated binaries.

## Field-connected devices
Source work does not authorize touching a connected installation. A real applicable
maintenance interlock is required before stopping applications, changing permissions,
installing, injecting input, controlling equipment, flashing or rebooting.
Never replace this with the GUI-only lab verifier, a true flag or an empty hook.
Do not invent clean build provenance, installed identities or passed contracts.
Maintain independent system/application rollback and user-data boundaries.

## Validation
Use make test and make test-hmi with Qt5 on Ubuntu. Record skips and exact scope.
Source packaging exports clean committed Git objects only. A source preview is not
an app update archive, firmware, production release or proof that a board was upgraded.

## Qt application interaction design
For Qt UI, application workflows, subpages or dialogs, read .agents/skills/qt-industrial-ui/SKILL.md and docs/design/qt-application-interaction.md. Exercise actual application flows and isolated host states; keep hardware acceptance separate.
