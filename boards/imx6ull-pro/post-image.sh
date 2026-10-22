#!/bin/sh
set -eu
GENIMAGE_TASK_TMP=$(mktemp -d "$BUILD_DIR/genimage-dshancontrol.XXXXXX")
genimage --rootpath "$TARGET_DIR" --tmppath "$GENIMAGE_TASK_TMP" --inputpath "$BINARIES_DIR" --outputpath "$BINARIES_DIR" --config "$(dirname "$0")/genimage.cfg"
cp "$(dirname "$0")/flash-sdp.uuu" "$BINARIES_DIR/flash-sdp.uuu"
cd "$BINARIES_DIR"
sha256sum u-boot-dtb.imx zImage imx6ull-14x14-evk.dtb rootfs.ext4 data.ext4 sdcard.img flash-sdp.uuu > SHA256SUMS
