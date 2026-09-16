#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BOARD_OVERLAY="$ROOT/boards/t153mx-tina5/overlay"

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 /path/to/TinaSDK" >&2
	exit 2
fi

SDK_ROOT=$1
if [ ! -d "$SDK_ROOT/buildroot/buildroot-202205" ] || \
	[ ! -d "$SDK_ROOT/device/config/chips/t153" ]; then
	echo "Not a compatible T153 Tina SDK tree: $SDK_ROOT" >&2
	exit 2
fi

copy_tree()
{
	source_dir=$1
	target_dir=$2
	mkdir -p "$target_dir"
	(cd "$source_dir" && tar -cf - .) | (cd "$target_dir" && tar -xf -)
}

copy_tree "$BOARD_OVERLAY" "$SDK_ROOT"
copy_tree "$ROOT/components/core" \
	"$SDK_ROOT/buildroot/buildroot-202205/package/omnigate-core"
copy_tree "$ROOT/components/hmi" \
	"$SDK_ROOT/buildroot/buildroot-202205/package/omnigate-hmi"

RUNTIME="$SDK_ROOT/device/config/chips/t153/configs/omnigate/buildroot/overlay"
mkdir -p "$RUNTIME/usr/lib/omnigate-web" \
	"$RUNTIME/usr/share/omnigate-web" \
	"$RUNTIME/usr/lib/omnigate-supervisor" \
	"$RUNTIME/etc/default" \
	"$RUNTIME/etc/init.d" \
	"$RUNTIME/etc/omnigate-web" \
	"$RUNTIME/etc/omnigate"
cp "$ROOT/components/web/app.py" "$ROOT/components/web/hmi_api.py" \
	"$RUNTIME/usr/lib/omnigate-web/"
cp "$ROOT/components/web/assets/index.html" \
	"$RUNTIME/usr/share/omnigate-web/index.html"
cp "$ROOT/components/web/config/config.json" \
	"$RUNTIME/etc/omnigate-web/config.json"
cp "$ROOT/components/supervisor/supervisor.py" \
	"$RUNTIME/usr/lib/omnigate-supervisor/supervisor.py"
cp "$ROOT/deploy/t153-lite/defaults/omnigate-web" \
	"$RUNTIME/etc/default/omnigate-web"
cp "$ROOT/deploy/t153-lite/defaults/omnigate-supervisor" \
	"$RUNTIME/etc/default/omnigate-supervisor"
cp "$ROOT/deploy/t153-lite/init/S71omnigate-web" \
	"$RUNTIME/etc/init.d/S71omnigate-web"
cp "$ROOT/deploy/t153-lite/init/S72omnigate-supervisor" \
	"$RUNTIME/etc/init.d/S72omnigate-supervisor"
cp "$ROOT/deploy/t153-lite/platform.json" \
	"$RUNTIME/etc/omnigate/platform.json"

echo "Applied DshanControl T153MX source overlay to: $SDK_ROOT"
echo "Reload the Buildroot defconfig before compiling."
