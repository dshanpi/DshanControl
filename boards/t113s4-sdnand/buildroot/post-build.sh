#!/bin/sh
set -eu
EXTERNAL=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$HOST_DIR/bin/python3" "$EXTERNAL/app-source/scripts/finalize_s4_rootfs.py" "$1"
