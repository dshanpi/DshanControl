#!/bin/sh
# Explicit maintenance entry for the network/dd workflow, not a normal init script.
set -eu
set -o pipefail
TASK=/data/dshan-maintenance-20261124
RAM=/mnt/dshan-maintenance
case "${1:-}" in check|apply) MODE=$1 ;; *) echo 'Expected check or apply' >&2; exit 2 ;; esac
cd "$TASK"
sha256sum -c SHA256SUMS
. ./manifest.env
[ "$(cat /sys/class/block/mmcblk1/device/cid)" = "$EXPECTED_CID" ]
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(cat /sys/class/block/mmcblk1p2/start)" = 81920 ]
[ "$(cat /sys/class/block/mmcblk1p2/size)" = 1572864 ]
[ "$(xzcat rootfs-preserve.ext4.xz | sha256sum | cut -d ' ' -f1)" = "$ROOTFS_SHA256" ]
[ "$(xzcat boot-preserve.vfat.xz | sha256sum | cut -d ' ' -f1)" = "$BOOT_SHA256" ]
mkdir /run/dshan-netflash.lock
[ ! -L "$RAM" ]
! mountpoint -q "$RAM"
[ -z "$(ls -A "$RAM" 2>/dev/null || true)" ]
mkdir -p "$RAM"
mount -t tmpfs -o size=24m,mode=0755 tmpfs "$RAM"
for directory in bin dev proc sys run tmp data oldroot etc; do mkdir "$RAM/$directory"; done
cp busybox "$RAM/bin/busybox"
chmod 0755 "$RAM/bin/busybox"
for applet in $("$RAM/bin/busybox" --list); do
    [ "$applet" = busybox ] || ln -s busybox "$RAM/bin/$applet"
done
cp enter-ram.sh ram-init.sh manifest.env "$RAM/"
printf 'MAINTENANCE_MODE=%s\n' "$MODE" >> "$RAM/manifest.env"
# Preserve the console/getty entry until init performs the controlled restart.
awk '!/::shutdown:/' /etc/inittab > "$RAM/maintenance.inittab"
printf 'ttymxc0::restart:%s/bin/busybox sh %s/enter-ram.sh\n' "$RAM" "$RAM" >> "$RAM/maintenance.inittab"
"$RAM/bin/busybox" sh -n "$RAM/enter-ram.sh"
"$RAM/bin/busybox" sh -n "$RAM/ram-init.sh"
if [ -x /etc/init.d/S80dshancontrol ]; then
    /etc/init.d/S80dshancontrol stop
else
    echo 'Unexpected running-system startup script; stop before changing init' >&2
    exit 1
fi
sync
mount -o bind "$RAM/maintenance.inittab" /etc/inittab
kill -HUP 1
sleep 2
echo "DSHAN_RAM_REEXEC_REQUESTED mode=$MODE"
kill -QUIT 1
