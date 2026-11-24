#!/bin/sh
# PID 1 in an independent tmpfs. No network or installed rootfs executable needed.
set -eu
set -o pipefail
export PATH=/bin HOME=/ TMPDIR=/tmp
TASK=/data/dshan-maintenance-20261124
. /manifest.env

log() {
    echo "$*"
    printf '%s\n' "$*" >> "$TASK/maintenance.log"
}
rescue() {
    echo "DSHAN_MAINTENANCE_STOPPED: $*"
    echo 'No automatic retry. Serial recovery shell remains available.'
    while :; do /bin/sh -i || true; done
}
trap 'rescue "unexpected failure at maintenance stage"' EXIT

[ "$$" = 1 ] || rescue 'maintenance must own PID 1'
[ "$(cat /sys/class/block/mmcblk1/device/cid)" = "$EXPECTED_CID" ] || rescue 'eMMC identity changed'
for check in '1 16384 65536' '2 81920 1572864' '3 1654784 524288'; do
    set -- $check
    [ "$(cat /sys/class/block/mmcblk1p$1/start)" = "$2" ] || rescue 'partition start changed'
    [ "$(cat /sys/class/block/mmcblk1p$1/size)" = "$3" ] || rescue 'partition size changed'
done
# Require a real unmount, never a lazy detach, before permitting block writes.
umount /oldroot || rescue 'old root still busy; block writes forbidden'
awk '$1 ~ /mmcblk1p[12]$/ || $1 == "/dev/root" { bad=1 } END { exit bad }' /proc/mounts || rescue 'target partition still mounted'
awk '$1 == "/dev/mmcblk1p3" && $2 == "/data" && $3 == "ext4" && $4 ~ /(^|,)rw(,|$)/ {ok=1} END {exit !ok}' /proc/mounts || rescue 'private data mount unavailable'
log 'RAM_INTERLOCK_PASSED: PID1 in tmpfs; target CID/layout matched; p1/p2 unmounted'

normal_reboot() {
    sync
    cd /
    umount /data || rescue 'cannot cleanly unmount persistent data'
    trap - EXIT
    reboot -f
    rescue 'normal reboot did not complete'
}

if [ "$MAINTENANCE_MODE" = check ]; then
    log RAM_REHEARSAL_PASSED_NO_BLOCK_WRITES
    normal_reboot
fi
[ "$MAINTENANCE_MODE" = apply ] || rescue 'unknown maintenance mode'
cd "$TASK"
sha256sum -c SHA256SUMS || rescue 'local transfer checksum mismatch'
[ "$(xzcat rootfs-preserve.ext4.xz | sha256sum | cut -d ' ' -f1)" = "$ROOTFS_SHA256" ] || rescue 'root image payload mismatch'
[ "$(xzcat boot-preserve.vfat.xz | sha256sum | cut -d ' ' -f1)" = "$BOOT_SHA256" ] || rescue 'boot image payload mismatch'

ROLLBACK="$TASK/rollback"
[ ! -e "$ROLLBACK" ] || rescue 'preserve previous rollback set; do not overwrite it'
umask 077
mkdir "$ROLLBACK"
prefix_hash() { dd if=/dev/mmcblk1 bs=1048576 count=8 2>/dev/null | sha256sum | cut -d ' ' -f1; }
data_hashes() { find /data -path "$TASK" -prune -o -type f -exec sha256sum '{}' ';' | sort; }
OLD_PREFIX=$(prefix_hash)
OLD_ROOT=$(sha256sum /dev/mmcblk1p2 | cut -d ' ' -f1)
OLD_BOOT=$(sha256sum /dev/mmcblk1p1 | cut -d ' ' -f1)
data_hashes > "$ROLLBACK/data-files-before.sha256"
log QUIESCED_BACKUP_STARTED
# These backups are taken after a real unmount, with all application writers gone.
gzip -1 -c /dev/mmcblk1p2 > "$ROLLBACK/rootfs.ext4.gz" || rescue 'root backup failed; no flash performed'
gzip -1 -c /dev/mmcblk1p1 > "$ROLLBACK/boot.vfat.gz" || rescue 'boot backup failed; no flash performed'
dd if=/dev/mmcblk1 bs=1048576 count=8 2>/dev/null | gzip -1 > "$ROLLBACK/emmc-prefix.gz" || rescue 'prefix backup failed'
tar -C /data --exclude=./dshan-maintenance-20261124 -cf - . | gzip -1 > "$ROLLBACK/data.tar.gz" || rescue 'data backup failed'
sync
[ "$(gzip -dc "$ROLLBACK/rootfs.ext4.gz" | sha256sum | cut -d ' ' -f1)" = "$OLD_ROOT" ] || rescue 'root backup verification failed'
[ "$(gzip -dc "$ROLLBACK/boot.vfat.gz" | sha256sum | cut -d ' ' -f1)" = "$OLD_BOOT" ] || rescue 'boot backup verification failed'
[ "$(gzip -dc "$ROLLBACK/emmc-prefix.gz" | sha256sum | cut -d ' ' -f1)" = "$OLD_PREFIX" ] || rescue 'prefix backup verification failed'
gzip -dc "$ROLLBACK/data.tar.gz" | tar -tf - > /tmp/data-archive-members || rescue 'data backup is unreadable'
printf '%s  original-rootfs\n%s  original-boot\n%s  original-prefix\n' "$OLD_ROOT" "$OLD_BOOT" "$OLD_PREFIX" > "$ROLLBACK/original-raw-hashes"
log QUIESCED_BACKUP_VERIFIED
df -k /data | tail -1 | awk '{exit !($4 >= 8192)}' || rescue 'insufficient free space after backups'

apply_images() {
    log WRITING_ROOT_PARTITION
    xzcat rootfs-preserve.ext4.xz | dd of=/dev/mmcblk1p2 bs=1048576 conv=fsync || return 1
    [ "$(sha256sum /dev/mmcblk1p2 | cut -d ' ' -f1)" = "$ROOTFS_SHA256" ] || return 1
    log ROOT_PARTITION_READBACK_PASSED
    xzcat boot-preserve.vfat.xz | dd of=/dev/mmcblk1p1 bs=1048576 conv=fsync || return 1
    [ "$(sha256sum /dev/mmcblk1p1 | cut -d ' ' -f1)" = "$BOOT_SHA256" ] || return 1
    log BOOT_PARTITION_READBACK_PASSED
}
if ! apply_images; then
    log WRITE_FAILED_RESTORING_PREVIOUS_SYSTEM
    gzip -dc "$ROLLBACK/rootfs.ext4.gz" | dd of=/dev/mmcblk1p2 bs=1048576 conv=fsync || rescue 'root rollback write failed'
    gzip -dc "$ROLLBACK/boot.vfat.gz" | dd of=/dev/mmcblk1p1 bs=1048576 conv=fsync || rescue 'boot rollback write failed'
    [ "$(sha256sum /dev/mmcblk1p2 | cut -d ' ' -f1)" = "$OLD_ROOT" ] || rescue 'root rollback hash mismatch'
    [ "$(sha256sum /dev/mmcblk1p1 | cut -d ' ' -f1)" = "$OLD_BOOT" ] || rescue 'boot rollback hash mismatch'
    log ROLLBACK_VERIFIED_REBOOTING_PREVIOUS_SYSTEM
    normal_reboot
fi
[ "$(prefix_hash)" = "$OLD_PREFIX" ] || rescue 'boot metadata changed unexpectedly'
data_hashes > "$ROLLBACK/data-files-after.sha256"
cmp "$ROLLBACK/data-files-before.sha256" "$ROLLBACK/data-files-after.sha256" || rescue 'retained user data changed'
log 'FLASH_AND_READBACK_PASSED: root, boot, preserved metadata and user data verified'
normal_reboot
