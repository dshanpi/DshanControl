#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PYTHON=${TEST_PYTHON:-$ROOT/.test-venv/bin/python}
CORE=$ROOT/components/core/src
BYTECODE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/dshancontrol-pycache.XXXXXX")
trap 'rm -rf "$BYTECODE_DIR"' EXIT HUP INT TERM
export PYTHONDONTWRITEBYTECODE=1

if [ ! -x "$PYTHON" ]; then
	echo "Missing test interpreter: $PYTHON" >&2
	echo "Run 'make test-bootstrap' first." >&2
	exit 2
fi

"$PYTHON" -c 'import flask; assert flask.__version__ == "2.1.2"'
PYTHONWARNINGS=ignore PYTHONPATH="$CORE" "$PYTHON" -m unittest discover \
	-s "$ROOT/components/core/tests" -v
PYTHONWARNINGS=ignore "$PYTHON" -m unittest discover -s "$ROOT/tests" -v

PYTHONPYCACHEPREFIX="$BYTECODE_DIR" "$PYTHON" -m py_compile \
	"$ROOT"/components/core/src/omnigate_core/*.py \
	"$ROOT"/components/web/*.py \
	"$ROOT"/components/supervisor/*.py

sh -n "$ROOT/platform/apps/node-red/init-data.sh"
sh -n "$ROOT/components/hmi/omnigate-hmi-launcher"
sh -n "$ROOT/components/hmi/S75omnigate-hmi"
sh -n "$ROOT/deploy/t153-lite/init/S71omnigate-web"
sh -n "$ROOT/deploy/t153-lite/init/S72omnigate-supervisor"

for value in "$ROOT/platform/components.json" \
	"$ROOT/platform/release-status.json" \
	"$ROOT"/platform/profiles/*.json \
	"$ROOT"/platform/boards/*.json \
	"$ROOT"/platform/schemas/*.json \
	"$ROOT/deploy/t153-lite/platform.json"; do
	"$PYTHON" -m json.tool "$value" >/dev/null
done
"$PYTHON" -m json.tool "$ROOT/platform/apps/node-red/flows.json" >/dev/null
if command -v node >/dev/null 2>&1; then
	node --check "$ROOT/platform/apps/node-red/settings.js"
fi

echo "All DshanControl host-side tests passed."
