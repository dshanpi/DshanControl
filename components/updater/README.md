# DshanControl application updater v1 (development)

This directory retains the legacy Python application updater as a protocol and
regression reference, independent of whole-system SWUpdate. New production
updater/platform work uses the [C++ runtime](../runtime/README.md).
The native S4 pilot has full-payload signed install, health rollback, two physical
power-interruption boundaries, real Qt checking and cold-start evidence; it uses
a fixture contract, copied data and temporary system hooks. No production S4/T153
OTA qualification is claimed. System identity, trust, maintenance, permissions
and health integration must still be provisioned and validated.

## Runtime layout

- `/usr/lib/dshancontrol-update`: system-owned updater, protocol, boot setup and checks.
- `/etc/dshancontrol/system-release.json`: immutable system compatibility contract.
- `/etc/dshancontrol/update-public.pem`: provisioned public key (never downloaded).
- `/etc/dshancontrol/maintenance-check`: system-owned site interlock; missing means refuse.
- `/mnt/UDISK/dshancontrol/apps/releases`: factory and installed application versions.
- `/mnt/UDISK/dshancontrol/apps/current`: atomic selection of active application.
- `/mnt/UDISK/dshancontrol/data`: configuration, databases, device session secret, EDS files.

S70 refuses to start without the persistent volume mounted. It initializes factory
data only when missing, recovers an unfinished update before starting the app, and
leaves the system's original build identity unchanged. S71 periodically checks
GitHub and writes `/var/run/dshancontrol-update.json`. Automatic installation is
**off by default**; enabling `automatic_install` still requires signed qualified
assets, exact compatibility and the system maintenance interlock.

## Device commands

The commands/layout below describe the legacy integration. Use the C++ runtime
documentation for native CLI/RPC operation; these instructions do not enable
production installation on the currently unqualified board.

```sh
dshancontrol-update check
dshancontrol-update download
dshancontrol-update install application-release.json application-release.json.sig TARGET.tar.gz
# Boot service invokes recovery independently of the desktop:
dshancontrol-update recover
```

Checks enumerate GitHub Releases (up to 1,000 entries), ignore system releases and
drafts, verify signatures, enforce selected channels, then select the highest
compatible sequence. Download does not install. Network errors leave the running
application untouched. Distribution does not require a device GitHub token.

Installation verifies a private snapshot of input files, checks target and data
schema, rejects unsafe tar members, stages and fsyncs files, checks maintenance,
journals, persists the sequence, stops services, switches, starts and observes
health. Recovery of uncommitted transactions restores the known-good version.
Failed sequences are not reinstalled automatically. Failed rollback retains the
journal. The updater never executes archive-provided hooks.

Version 1 rejects data-schema changes and arbitrary migrations. Matching schema
is a publisher contract; compatibility and data preservation still need actual
application regression tests. Whole-system recovery, signing-key rotation and
updater self-update belong to the system release line.

## Publisher commands

Use a clean committed application tree, target-built HMI, verified system contract
and an externally managed RSA signing key:

```sh
python3 scripts/build_application_release.py \
  --hmi /path/to/target/omnigate-hmi --target-contract /path/to/system-release.json \
  --build-info /path/to/native-build-info.json \
  --version 0.1.0-dev --sequence 1 --from-version FACTORY_VERSION \
  --signing-key /secure/path/key.pem --output dist/app-draft
python3 scripts/check_application_release.py dist/app-draft --public-key /path/to/public.pem
```

The builder emits a **draft**; it does not upload, mark tests passed or generate a
production key. Production devices reject draft manifests. `development_mode`
exists for controlled test systems and must remain false on qualified images.
Public qualification requires board evidence, archive/source correspondence,
native dependencies, system contract/image identity, re-signing the qualified
manifest and the release review in `docs/RELEASING.md`. There is intentionally no
command which promotes a package merely by changing a flag.
