#!/bin/sh
set -eu
SDK=${SDK:-/home/ubuntu/work/aibsp-imx6ull-pro_linux5.4.47}
OUTPUT=${OUTPUT:-/home/ubuntu/work/imx6ull-dshancontrol-build}
BOARD=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
python3 "$BOARD/prepare-sdk.py" "$SDK"
SOURCE_COMMIT=$(git -C "$BOARD/../.." rev-parse HEAD)
BUILD_IDENTITY="$SDK/sources/dshancontrol-build-identity.json"
make -C "$SDK/buildroot-2026.02.1" O="$OUTPUT" BR2_EXTERNAL="$BOARD/buildroot" DSHANCONTROL_IMX6ULL_VERSION="$SOURCE_COMMIT" DSHANCONTROL_IMX6ULL_BUILD_IDENTITY="$BUILD_IDENTITY" 100ask_imx6ull_dshancontrol_defconfig
make -C "$SDK/buildroot-2026.02.1" O="$OUTPUT" BR2_EXTERNAL="$BOARD/buildroot" DSHANCONTROL_IMX6ULL_VERSION="$SOURCE_COMMIT" DSHANCONTROL_IMX6ULL_BUILD_IDENTITY="$BUILD_IDENTITY" -j4 "$@"

if [ "$#" = 0 ] || [ "${1:-}" = all ]; then
    python3 "$BOARD/verify-images.py" "$OUTPUT"
fi
