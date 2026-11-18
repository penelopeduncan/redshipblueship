#!/usr/bin/env bash
# Run cpack for the macOS build, trying again only when hdiutil reports
# "Resource busy" (#1713).
#
# Called from the "Build redship" step of generate-builds' macOS build job as
#   bash .github/scripts/cpack-retry-busy.sh build-cmake
# On pull request #1707 (run 44208707794, job 134481514378) a clean compile
# and link failed in packaging with
#   CPack Error: Error executing: /usr/bin/hdiutil create -ov -srcfolder ... temp.dmg
#   CPack Error: Error generating temporary disk image.
#   hdiutil: create failed - Resource busy
# while the same tree packaged on the other pull requests of that hour:
# hdiutil create races with something on the hosted runner that still holds
# the staged folder (indexing, or the codesign CPack has just finished).
#
# cpack runs at most three times in all. It is tried again only when the
# failed try's output (stdout and stderr) contains "Resource busy", after a
# pause of 20 seconds before the second try and 40 before the third. Any
# other cpack failure, and a third busy failure, end the script at once with
# cpack's own exit status, after printing the install rules' and
# fixup_bundle's InstallOutput.log, which they write there and not to the
# console. Each retry prints one ::warning:: line naming the try and the
# reason, so a green job still shows that a retry happened.
#
# Nothing is removed between tries. CPack removes CPACK_TOPLEVEL_DIRECTORY
# (_CPack_Packages/Darwin/Bundle, which holds both the staged bundle and
# temp.dmg) at the start of every run, because CPACK_REMOVE_TOPLEVEL_DIRECTORY
# defaults to ON and this tree does not change it, and the Bundle generator
# creates temp.dmg with "hdiutil create -ov", which overwrites an old one. A
# try therefore starts from the same state as the first.
#
# cpack's output streams to the job log as it runs (tee), and the status
# examined is cpack's (PIPESTATUS[0]), not tee's. errexit is off around that
# pipeline, so the script behaves the same whether or not the caller's shell
# options (SHELLOPTS) carry errexit and pipefail. Written for macOS's
# /bin/bash 3.2: no bash 4 features.

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <build dir>" >&2
    exit 2
fi
build_dir="$1"
max_tries=3
pauses="20 40"

log="$(mktemp "${TMPDIR:-/tmp}/cpack-try.XXXXXX")"
trap 'rm -f "$log"' EXIT

print_install_logs() {
    find "$build_dir/_CPack_Packages" -name 'InstallOutput.log' \
        -exec sh -c 'echo "== $1"; tail -n 300 "$1"' _ {} \; || true
}

set -- $pauses
try=1
while :; do
    set +e
    (cd "$build_dir" && cpack) 2>&1 | tee "$log"
    status="${PIPESTATUS[0]}"
    set -e
    if [ "$status" -eq 0 ]; then
        exit 0
    fi
    if ! grep -q 'Resource busy' "$log"; then
        echo "cpack try ${try} of ${max_tries} failed (exit ${status}); not a Resource busy failure, so not retried"
        print_install_logs
        exit "$status"
    fi
    if [ "$try" -ge "$max_tries" ]; then
        echo "::error::cpack try ${try} of ${max_tries} failed with hdiutil Resource busy (exit ${status}); no tries left"
        print_install_logs
        exit "$status"
    fi
    pause="$1"
    shift
    try=$((try + 1))
    echo "::warning::cpack try ${try} of ${max_tries}: try $((try - 1)) failed with hdiutil Resource busy, retrying after ${pause}s (#1713)"
    sleep "$pause"
done
