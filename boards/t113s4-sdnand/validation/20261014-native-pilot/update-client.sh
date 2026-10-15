#!/bin/sh
ROOT=/mnt/UDISK/dshancontrol/native-ota-pilot-20261014
if [ "${1:-}" = rpc ]; then
    exec "$ROOT/dshancontrol-runtime" "$@"
fi
exec "$ROOT/dshancontrol-runtime" --config "$ROOT/updater.json" "$@"
