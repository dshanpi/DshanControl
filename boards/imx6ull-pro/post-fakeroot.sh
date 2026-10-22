#!/bin/sh
set -eu
# Runs inside Buildroot fakeroot, after target ownership is normalized.
# Preserve private file ownership and modes in the independent data filesystem.
truncate -s 256M "$BINARIES_DIR/data.ext4"
"$HOST_DIR/sbin/mkfs.ext4" -q -F -L dshan-data -O ^metadata_csum,^64bit -E lazy_itable_init=0,lazy_journal_init=0 -d "$TARGET_DIR/data" "$BINARIES_DIR/data.ext4"
