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

## 2026-11-24 i.MX6ULL Pro maintenance authorization

The user explicitly identified the connected board as i.MX6ULL Pro and authorized
this session to reboot it into a supported programming mode, flash the prepared
DshanControl firmware, boot it, and validate the result. The user also authorized
board Wi-Fi access to SSID `Programmers`; its credential was supplied privately in
the session and must not be placed in tracked source or public delivery artifacts.

The observed LYNX MCP endpoint is `http://127.0.0.1:8771/mcp`, device/project ID 9,
serial binding COM51 (CH9102), and saved USB binding `1:102`. The saved project name
`t736 SDK` is not the board identity. Serial output confirmed i.MX6ULL rev1.1,
512 MiB RAM, and a 1024x600 display. Revalidate the exact target at each transition.

This record documents user authorization, not a maintenance interlock or a passed
hardware gate. Respect LYNX's live hardware guard, supported vendor workflows and
current programming-mode policy. Preserve the observed independent `/data`
partition; obtain concrete storage-layout and backup evidence before writes.
