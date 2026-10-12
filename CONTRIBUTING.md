# Contributing

Run `make test-bootstrap` once, then `make test` before submitting changes.
Changes that affect a board image must also pass that board integration's
overlay verification, a clean image build, and the applicable non-destructive
board checks.

Keep board-independent behavior behind the platform manifest and `/api/v1`.
Do not hard-code `can0`, serial paths, Ethernet names or framebuffer paths in
new business logic.  Protocol writes and machine-control actions must require
authentication, CSRF protection, bounded input and explicit operator intent.

Do not report protocol interoperability without a real compatible slave and
captured evidence.  Never add credentials, private keys, production tokens or
customer data to examples, logs, fixtures or commits.

Follow [AGENTS.md](AGENTS.md) and [the dual release gates](docs/RELEASING.md).
DshanControl is the industrial desktop product; S4 SD NAND is an additional
Buildroot target. EinkOS is a release-standard reference and SmartDesktopMimi is
outside this product. Keep board-specific lifecycle/health adapters in `boards/`;
the application update engine and protocol stay portable. Unqualified targets are
listed as candidates, never as supported profiles.
