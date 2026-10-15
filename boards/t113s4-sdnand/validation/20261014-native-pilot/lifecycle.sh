#!/bin/sh
# System-owned lifecycle hook; application bundles cannot replace it.
set -eu
ROOT=/mnt/UDISK/dshancontrol/native-ota-pilot-20261014
CURRENT=$ROOT/apps/current
DATA=$ROOT/data
RUN=/var/run/dshancontrol
export DSHANCONTROL_APPLICATION_COMMIT=$(python3 -c 'import json; print(json.load(open("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/apps/current/installed.json"))["commit"])')
export OMNIGATE_CORE_PATH=$CURRENT/lib/core
export PYTHONPATH=/usr/lib/dshancontrol-python:$OMNIGATE_CORE_PATH
export OMNIGATE_WEB_ASSETS=$CURRENT/share/web
export OMNIGATE_WEB_CONFIG=$DATA/web.json
export OMNIGATE_CONFIG=$DATA/web.json
export OMNIGATE_WEB_SECRET=$DATA/session.key
export OMNIGATE_EDGE_DB=$DATA/edge.db
export OMNIGATE_HMI_DB=$DATA/hmi.db
export OMNIGATE_HMI_SETTINGS=$DATA/hmi.json
export OMNIGATE_EDS_DIR=$DATA/eds
export OMNIGATE_LISTEN_PORT=80
export DSHANCONTROL_UPDATER_CONFIG=$ROOT/updater.json
export DSHANCONTROL_UPDATER_EXECUTABLE=$ROOT/update-client
export DSHANCONTROL_PLATFORM_SOCKET=$ROOT/platform.sock
export DSHANCONTROL_NATIVE_PLATFORM_SOCKET=/tmp/dshan-native-display.sock
export HEALTH_URL=http://127.0.0.1/api/health
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/qt/plugins
export QT_QPA_FONTDIR=/usr/share/fonts/wqy-zenhei
[ ! -f /etc/dshancontrol/display.env ] || . /etc/dshancontrol/display.env

stop_one() {
    [ -f "$RUN/$1.pid" ] || return 0
    pid=$(cat "$RUN/$1.pid")
    case "$pid" in ''|*[!0-9]*|0|1) echo "Invalid owned service PID" >&2; return 1 ;; esac
    if kill -0 "$pid" 2>/dev/null; then
        cmdline=$(tr '\000' ' ' < "/proc/$pid/cmdline")
        case "$1:$cmdline" in
            web:*"/mnt/UDISK/dshancontrol/"*"/lib/web/app.py"*) ;;
            supervisor:*"/mnt/UDISK/dshancontrol/"*"/lib/supervisor/supervisor.py"*) ;;
            hmi:*"/mnt/UDISK/dshancontrol/"*"/bin/omnigate-hmi"*) ;;
            hmi:*"/tmp/dshancontrol-brightness-trial/omnigate-hmi"*) ;;
            *) echo "Refusing unrelated service PID" >&2; return 1 ;;
        esac
        start-stop-daemon -K -p "$RUN/$1.pid"
        waited=0
        while kill -0 "$pid" 2>/dev/null; do
            waited=$((waited + 1))
            test "$waited" -le 20 || return 1
            sleep 1
        done
    fi
    rm -f "$RUN/$1.pid"
}
printf '%s current=%s\n' "${1:-}" "$(readlink "$ROOT/apps/current")" >> "$ROOT/lifecycle-events.log"
case "${1:-}" in
maintenance)
    python3 /usr/lib/dshancontrol-update/storage.py
    # Temporary bench-only verifier: read the live saved platform/configuration.
    # This does not qualify a production industrial maintenance implementation.
    python3 -c 'import json; from pathlib import Path; p=json.load(open("/etc/omnigate/platform.json")); assert p["board_id"]=="100ask-t113s4-sdnand" and p["capabilities"]==[]; assert all(x["kind"]=="display" for x in p["channels"].values()); c=json.load(open("/mnt/UDISK/dshancontrol/data/web.json")); assert not c.get("can") and not c.get("canopen",{}).get("nodes") and not c.get("ec20",{}).get("profiles"); assert b"allwinner,t113_s4" in Path("/proc/device-tree/compatible").read_bytes()'
    ;;
start)
    python3 /usr/lib/dshancontrol-update/storage.py
    test -d "$CURRENT"
    mkdir -p "$RUN" "$DATA/eds" /var/log
    for name in web hmi supervisor; do
        if [ -s "$RUN/$name.pid" ] && kill -0 "$(cat "$RUN/$name.pid")" 2>/dev/null; then
            echo "$name already running" >&2; exit 1
        fi
    done
    start-stop-daemon -S -b -m -p "$RUN/web.pid" -x /usr/bin/python3 -- "$CURRENT/lib/web/app.py"
    start-stop-daemon -S -b -m -p "$RUN/hmi.pid" -x "$CURRENT/bin/omnigate-hmi"
    WEB_RESTART_HOOK=/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/lifecycle WEB_RESTART_ACTION=restart-web \
        start-stop-daemon -S -b -m -p "$RUN/supervisor.pid" -x /usr/bin/python3 -- "$CURRENT/lib/supervisor/supervisor.py"
    if [ "$(python3 -c 'import json; print(json.load(open("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/apps/current/installed.json"))["sequence"])')" = 3 ]; then
        printf 'POWER_CUT_WINDOW sequence=3\n' >> "$ROOT/lifecycle-events.log"
        sleep 45
    fi
    ;;
stop)
    if [ -f "$ROOT/armed-prepared-power-cut" ]; then
        rm "$ROOT/armed-prepared-power-cut"
        printf 'PREPARED_POWER_CUT_WINDOW\n' >> "$ROOT/lifecycle-events.log"
        sleep 45
    fi
    stop_one supervisor
    stop_one hmi
    stop_one web
    ;;
restart-web)
    stop_one web
    start-stop-daemon -S -b -m -p "$RUN/web.pid" -x /usr/bin/python3 -- "$CURRENT/lib/web/app.py"
    ;;
health)
    sequence=$(python3 -c 'import json; print(json.load(open("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/apps/current/installed.json"))["sequence"])')
    if [ "$sequence" = 4 ]; then
        printf 'injected_health_exit=73 sequence=4\n' >> "$ROOT/lifecycle-events.log"
        exit 73
    fi
    for name in web hmi supervisor; do
        test -s "$RUN/$name.pid"
        kill -0 "$(cat "$RUN/$name.pid")"
    done
    python3 /mnt/UDISK/dshancontrol/native-ota-pilot-20261014/health.py
    # Board validation may add a system-owned display/IPC/fieldbus health probe.
    [ ! -x /etc/dshancontrol/health-check ] || /etc/dshancontrol/health-check
    ;;
*) echo "usage: $0 {maintenance|start|stop|health}" >&2; exit 2 ;;
esac
