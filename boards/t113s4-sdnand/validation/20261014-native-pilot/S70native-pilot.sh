#!/bin/sh
set -eu
ROOT=/mnt/UDISK/dshancontrol/native-ota-pilot-20261014
case "${1:-}" in
start)
    python3 /usr/lib/dshancontrol-update/storage.py
    export PYTHONDONTWRITEBYTECODE=1
    export DSHANCONTROL_PLATFORM_SOCKET=$ROOT/platform.sock
    # Explicit development clock seed; not a claim of RTC/NTP qualification.
    date -u -s "$(cat "$ROOT/boot-recovery/clock-seed")" >/dev/null
    if [ -z "$(lsmod | sed -n '/^aic8800_bsp /p')" ]; then
        insmod /lib/modules/5.4.61/kernel/drivers/net/wireless/aic8800/aic8800_bsp/aic8800_bsp.ko
        insmod /lib/modules/5.4.61/kernel/drivers/net/wireless/aic8800/aic8800_fdrv/aic8800_fdrv.ko
    fi
    ip link set wlan0 up
    wpa_supplicant -B -i wlan0 -D nl80211 -c "$ROOT/wpa.conf" -C /var/run/wpa_supplicant
    association_wait=0
    until [ "$(wpa_cli -i wlan0 status 2>/dev/null | sed -n 's/^wpa_state=//p')" = COMPLETED ]; do
        association_wait=$((association_wait+1))
        test "$association_wait" -lt 25 || break
        sleep 1
    done
    udhcpc -i wlan0 -q -n -t 3 -T 3 > "$ROOT/boot-dhcp.log" 2>&1 || true
    DISPLAY=/mnt/UDISK/dshancontrol/native-display-probe-20261014
    "$DISPLAY/dshancontrol-runtime" serve "$DISPLAY/platform-service.json" > "$ROOT/display-daemon.out" 2> "$ROOT/display-daemon.err" &
    printf '%s\n' "$!" > "$ROOT/display-native.pid"
    display_tries=0
    until DSHANCONTROL_PLATFORM_SOCKET=/tmp/dshan-native-display.sock "$DISPLAY/dshancontrol-runtime" rpc display.brightness.get '{"channel":"display0"}' >/dev/null 2>&1; do
        display_tries=$((display_tries+1)); test "$display_tries" -le 15 || exit 85
        kill -0 "$(cat "$ROOT/display-native.pid")"
        sleep 1
    done
    "$ROOT/dshancontrol-runtime" serve "$ROOT/platform-service.json" > "$ROOT/native.out" 2> "$ROOT/native.err" &
    printf '%s\n' "$!" > "$ROOT/native.pid"
    # Startup recovery may start the known-good application itself.
    tries=0
    until "$ROOT/dshancontrol-runtime" rpc update.identity '{}' >/dev/null 2>&1; do
        tries=$((tries+1)); test "$tries" -lt 45 || exit 81
        kill -0 "$(cat "$ROOT/native.pid")"
        sleep 1
    done
    if ! "$ROOT/lifecycle" health >/dev/null 2>&1; then
        "$ROOT/lifecycle" start
    fi
    ;;
stop) "$ROOT/lifecycle" stop ;;
*) exit 2 ;;
esac
