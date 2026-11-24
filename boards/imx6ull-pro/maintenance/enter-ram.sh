#!/bin/sh
set -eu
RAM=/mnt/dshan-maintenance
export PATH="$RAM/bin"
[ "$$" = 1 ] || exit 90
echo DSHAN_ENTERING_RAM_MAINTENANCE
# The original on-disk inittab was never modified.
umount /etc/inittab
for point in run tmp data dev sys proc; do
    mount --move "/$point" "$RAM/$point"
done
cd "$RAM"
./bin/busybox pivot_root . oldroot
exec ./bin/busybox chroot . /bin/sh /ram-init.sh
