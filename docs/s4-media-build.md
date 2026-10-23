# S4 media build and packaging check

The S4 component desktop uses the Cedar/TPlayer media backend. Building
`components/media/media.pro` without `CONFIG+=cedar` produces a worker that
reports `available=false`. That worker remains useful for host boundary tests,
but must not be packaged as the S4 playback implementation.

The Buildroot component-desktop recipe already passes `CONFIG+=cedar`. For an
independent target build, use the matching S4 Qt cross-qmake and an empty build
directory:

```sh
/path/to/s4/host/bin/qmake /path/to/source/components/media/media.pro CONFIG+=cedar
make
python3 /path/to/source/scripts/check_s4_media.py ./dshancontrol-media-worker
```

The cross-qmake's sysroot must contain the matching CedarX/CedarC headers and
soft-float libraries. If an existing dependency baseline keeps vendor headers
elsewhere, pass explicit include/library paths for that same baseline; do not
copy another board's headers into a shared sysroot. Missing headers or unresolved
TPlayer symbols are build failures, not a reason to omit `CONFIG+=cedar`.

`check_s4_media.py` examines the ARM ELF header, dynamic library requirements and
strong undefined playback entry points. It never executes the target binary.
`install_s4_runtime.py` runs the same check before replacing the target's existing
factory payload, and `build_application_release.py` checks before creating the
release output. Buildroot supplies its host-side `TARGET_READELF`; independent
staging can select a host ELF reader with `--readelf`.

A text marker or an appended `libtplayer.so` string is insufficient: the backend
must be present in the ELF dynamic linkage. The check records its scope as
`board_validation: not-run`. Passing it does not establish decoder operation,
visible Qt composition, physical LCD/audio, resource reclamation, field safety,
system-contract qualification, signing trust or update/rollback acceptance.
Keep those results separate from build/package evidence.

The default `make build-desktop` remains a host build and may deliberately lack
Cedar. Its media worker is rejected when supplied to the S4 factory installer.
Legacy payload reproduction without the optional component bundle is unchanged.
