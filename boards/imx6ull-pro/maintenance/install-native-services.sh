#!/bin/sh
# File-level system migration for the already provisioned, isolated lab unit.
# The application is installed separately through the signed OTA transaction.
set -eu
export LC_ALL=C
umask 077
[ "$(id -u)" = 0 ] || exit 1
[ "$#" = 2 ] || { echo 'Usage: install-native-services.sh BUNDLE SHA256' >&2; exit 2; }
ARCHIVE=$1
EXPECTED=$2
case "$EXPECTED" in *[!0-9a-f]*|'') exit 2;; esac
[ "${#EXPECTED}" = 64 ] || exit 2
[ -f "$ARCHIVE" ] && [ ! -L "$ARCHIVE" ] || exit 1
printf '%s  %s\n' "$EXPECTED" "$ARCHIVE" | sha256sum -c -
/usr/libexec/dshan-imx6ull-services maintenance
/usr/libexec/dshan-imx6ull-services health
TASK=/var/lib/dshancontrol-maintenance/native-20261124
[ ! -e "$TASK" ] && [ ! -L "$TASK" ] || { echo 'Migration directory already exists; inspect its result before retrying' >&2; exit 1; }
mkdir -m 0700 "$TASK"
cat > "$TASK/expected-list" <<'LIST'
SHA256SUMS
system/etc/dshancontrol/system-release.json
system/etc/init.d/S41wifi
system/etc/init.d/S99dshancontrol
system/usr/bin/dshancontrol-fieldbus-worker
system/usr/bin/dshancontrol-runtime
system/usr/libexec/dshan-imx6ull-services
LIST
gzip -dc "$ARCHIVE" > "$TASK/system-bundle.tar"
tar -tf "$TASK/system-bundle.tar" > "$TASK/archive-list.unsorted"
sort "$TASK/archive-list.unsorted" > "$TASK/archive-list"
cmp "$TASK/archive-list" "$TASK/expected-list"
mkdir "$TASK/stage"
tar -xf "$TASK/system-bundle.tar" -C "$TASK/stage"
(cd "$TASK/stage" && sha256sum -c SHA256SUMS)
for file in etc/dshancontrol/system-release.json etc/init.d/S41wifi etc/init.d/S99dshancontrol usr/bin/dshancontrol-fieldbus-worker usr/bin/dshancontrol-runtime usr/libexec/dshan-imx6ull-services; do
    [ -f "$TASK/stage/system/$file" ] && [ ! -L "$TASK/stage/system/$file" ] || exit 1
done
# Verify the native interlock against the live old release before any stop/write.
"$TASK/stage/system/usr/libexec/dshan-imx6ull-services" maintenance
"$TASK/stage/system/usr/libexec/dshan-imx6ull-services" wifi-check
"$TASK/stage/system/usr/libexec/dshan-imx6ull-services" health
printf '%s  %s\n' '1c616fb245d1c879c8363c0e07488e84683f85a971ccd35ca422b6921a07cb22' /etc/dshancontrol/system-release.json | sha256sum -c -
for directory in /usr /usr/bin /usr/libexec /etc /etc/init.d /etc/dshancontrol; do
    [ -d "$directory" ] && [ ! -L "$directory" ] || exit 1
done
FILES='etc/dshancontrol/system-release.json etc/init.d/S41wifi etc/init.d/S99dshancontrol usr/bin/dshancontrol-runtime usr/libexec/dshan-imx6ull-services'
[ ! -e /usr/bin/dshancontrol-fieldbus-worker ] && [ ! -L /usr/bin/dshancontrol-fieldbus-worker ] || exit 1
tar -cf "$TASK/system-preimage.tar" -C / $FILES
mkdir "$TASK/preimage"
tar -xf "$TASK/system-preimage.tar" -C "$TASK/preimage"
changed=0
rollback() {
    status=$?
    trap - EXIT HUP INT TERM
    if [ "$changed" = 1 ]; then
        /etc/init.d/S99dshancontrol stop || true
        /etc/init.d/S98dshan-runtime stop || true
        for file in $FILES; do
            cp -p "$TASK/preimage/$file" "/$file.native-rollback"
            mv -f "/$file.native-rollback" "/$file"
        done
        rm -f /usr/bin/dshancontrol-fieldbus-worker
        sync
        /etc/init.d/S98dshan-runtime start || true
        /etc/init.d/S99dshancontrol start || true
        echo 'Migration failed; system preimages restored. Inspect service health.' >&2
    fi
    exit "$status"
}
trap rollback EXIT
trap 'exit 1' HUP INT TERM
changed=1
/usr/libexec/dshan-imx6ull-services stop
# Data backup is private and remains on this board; stopped SQLite files are consistent.
tar -cf "$TASK/user-data-preimage.tar" -C /data/dshancontrol web hmi provider
/etc/init.d/S98dshan-runtime stop
for file in $FILES usr/bin/dshancontrol-fieldbus-worker; do
    [ ! -e "/$file.native-new" ] && [ ! -L "/$file.native-new" ] || exit 1
    cp "$TASK/stage/system/$file" "/$file.native-new"
    case "$file" in *.json) chmod 0600 "/$file.native-new";; *) chmod 0755 "/$file.native-new";; esac
    mv -f "/$file.native-new" "/$file"
done
sync
/etc/init.d/S98dshan-runtime start
/usr/libexec/dshan-imx6ull-services start
sleep 3
/usr/libexec/dshan-imx6ull-services health
printf '%s\n' "$EXPECTED" > "$TASK/installed-system-bundle.sha256"
sync
changed=0
trap - EXIT HUP INT TERM
echo 'Native system services installed; retained application healthy. Proceed with signed application OTA.'
