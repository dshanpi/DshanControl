#!/bin/sh
# Bench-only rollback of the explicitly installed native OTA pilot boot overlay.
set -eu
ROOT=/mnt/UDISK/dshancontrol/native-ota-pilot-20261014
case "${1:-}" in --check|--restore) ;; *) echo 'usage: restore-original-system.sh --check|--restore' >&2; exit 2 ;; esac
python3 /usr/lib/dshancontrol-update/storage.py
python3 -c 'from pathlib import Path; assert b"allwinner,t113_s4" in Path("/proc/device-tree/compatible").read_bytes()'
test ! -e "$ROOT/apps/transaction.json"
for name in S70dshancontrol S71dshancontrol-update; do
    test ! -e "/etc/init.d/$name"
    cmp "$ROOT/system-backup/$name" "/etc/init.d/.$name.pre-native-pilot"
done
cmp "$ROOT/boot-recovery/S70native-pilot" /etc/init.d/S70native-pilot
python3 -c 'import json; from pathlib import Path; r=Path("/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/tasks"); assert all(json.loads(p.read_text()).get("status")!="running" for p in r.glob("*.json"))'
sha256sum -c "$ROOT/original-system.sha256" >/dev/null
sha256sum -c "$ROOT/original-data.sha256" >/dev/null
for name in native display-native; do
    pid=$(cat "$ROOT/$name.pid")
    case "$pid" in ''|*[!0-9]*|0|1) exit 82 ;; esac
    case "$name" in
      native) expected=$ROOT/dshancontrol-runtime ;;
      display-native) expected=/mnt/UDISK/dshancontrol/native-display-probe-20261014/dshancontrol-runtime ;;
    esac
    test "$(readlink "/proc/$pid/exe")" = "$expected"
done
printf 'ORIGINAL_SYSTEM_RESTORE_PREFLIGHT_PASS\n'
[ "$1" = --restore ] || exit 0
"$ROOT/lifecycle" stop
for name in native display-native; do
    pid=$(cat "$ROOT/$name.pid")
    kill "$pid"
    waited=0
    while kill -0 "$pid" 2>/dev/null; do
        waited=$((waited+1)); test "$waited" -le 10 || exit 83
        sleep 1
    done
done
for name in S70dshancontrol S71dshancontrol-update; do
    mv "/etc/init.d/.$name.pre-native-pilot" "/etc/init.d/$name"
done
rm /etc/init.d/S70native-pilot
sync
# Original factory startup reads the untouched original user-data directory.
/etc/init.d/S70dshancontrol start
/etc/init.d/S71dshancontrol-update start
/usr/libexec/dshancontrol-services health
printf 'ORIGINAL_BOOT_ENTRIES_AND_FACTORY_APPLICATION_RESTORED\n'
