#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTPUT=${1:-$ROOT/dist}
VERSION=$(tr -d '\r\n' < "$ROOT/platform/VERSION")
case "$VERSION" in
	*[!A-Za-z0-9._-]*) echo "Invalid version: $VERSION" >&2; exit 2 ;;
esac

mkdir -p "$OUTPUT"
OUTPUT=$(CDPATH= cd -- "$OUTPUT" && pwd)
NAME="dshancontrol-$VERSION"
ARCHIVE="$OUTPUT/$NAME.tar.gz"

tar --sort=name --owner=0 --group=0 --numeric-owner \
	--exclude='./.git' --exclude='./.test-venv' --exclude='./dist' \
	--exclude='./showcase/node_modules' --exclude='./showcase/test-results' \
	--exclude='__pycache__' --exclude='*.pyc' \
	-C "$ROOT" -czf "$ARCHIVE" --transform "s,^\.,$NAME," .
(cd "$OUTPUT" && sha256sum "$NAME.tar.gz") > "$ARCHIVE.sha256"
printf '%s\n%s\n' "$ARCHIVE" "$ARCHIVE.sha256"
