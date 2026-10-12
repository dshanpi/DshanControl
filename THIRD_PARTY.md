# Third-party components

DshanControl source code is GPL-3.0. Runtime images may combine it with the
following independently licensed projects. Their licenses and source-offer
requirements must be collected by each board integration:

- Python, Flask and Werkzeug
- Qt and Qt Charts
- SOEM
- python-can and CANopen libraries
- Node-RED and its npm dependencies
- Eclipse Mosquitto or another MQTT broker
- Linux, BusyBox and Buildroot packages

This repository intentionally excludes SoC vendor BSP archives, proprietary
wireless firmware, modem firmware, board boot binaries and flashing tools.
Presence in a downstream firmware does not change those components' licenses.

The source-only T153MX board integration under `boards/t153mx-tina5/` includes
device-tree and build integration material that retains any SPDX license stated
in the individual file. See its `NOTICE.md` and `source-snapshot.json` for the
precise inclusion and exclusion boundary.

## T113s4 Buildroot integration

`boards/t113s4-sdnand/buildroot/package/wqy-zenhei` is a Buildroot recipe backport
from the SDK's `buildroot-mainline` package, preserving the upstream font hash and
GPL-2.0 font exception license. Fonts are downloaded at build time, not stored here.
`python-runtime.lock.json` pins PyPI artifacts for the Python 3.7 runtime (Flask,
Werkzeug, Jinja2, itsdangerous, click, importlib-metadata, zipp, typing_extensions,
MarkupSafe); wheel license files and MarkupSafe LICENSE.rst are installed alongside
the runtime. MarkupSafe uses its pure Python fallback. Release source/license
review must include these runtime sources, not only the DshanControl source archive.
