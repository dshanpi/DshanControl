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
