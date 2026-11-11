#!/usr/bin/env bash
# Stop a build of main that a newer push to main has superseded.
#
# generate-builds.yml queues main's builds per platform (MAIN'S BUILD QUEUES at
# the top of that file): every push to main waits its turn, and none is
# dropped from the queue. A build job runs this as soon as its turn comes and
# its checkout is done. If main has moved on by then, a later push already has
# its own build waiting, so this one stops here and gives the platform to it.
#
# Exit 0: build this commit. Exit 1: superseded, do not build.
#
# "Superseded" means main's tip differs from this commit in something that is
# not docs. A push that changes only docs starts no generate-builds run (the
# workflow's paths-ignore), so when the tip differs only in docs this commit is
# still the newest one with a build, and it must be built. The docs patterns
# are classify-docs-only.py's, which the tests keep equal to paths-ignore.
#
# Fails OPEN. Anything this script cannot work out (no network, no python, a
# git error) ends in exit 0: an extra build costs minutes, a build wrongly
# skipped leaves main without one and its caches stale.
set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
remote="${SUPERSEDED_REMOTE:-origin}"

build() {
    echo "$1 Building this commit."
    exit 0
}

this="$(git -c safe.directory='*' rev-parse --verify --quiet HEAD)" || build "Could not read HEAD."
git -c safe.directory='*' fetch --quiet --no-tags --depth=1 "${remote}" '+refs/heads/main' ||
    build "Could not fetch main's tip."
tip="$(git -c safe.directory='*' rev-parse --verify --quiet FETCH_HEAD)" || build "Could not read main's tip."

if [ "${tip}" = "${this}" ]; then
    build "${this} is main's tip."
fi

# The first python that RUNS: on Windows `python3` can be a Store alias that
# is found on PATH and only prints how to install Python.
python=""
for candidate in python3 python; do
    if "${candidate}" -c 'import sys' > /dev/null 2>&1; then
        python="${candidate}"
        break
    fi
done
[ -n "${python}" ] || build "No python to classify the difference to main's tip ${tip}."

changed="$(mktemp)" || build "Could not create a temporary file."
verdict="$(mktemp)" || build "Could not create a temporary file."
trap 'rm -f "${changed}" "${verdict}"' EXIT

# Trees, not history: both commits are shallow, and the question is only which
# paths differ between this commit and the tip.
git -c safe.directory='*' diff -z --name-only --no-renames "${this}" "${tip}" > "${changed}" ||
    build "Could not compare ${this} with main's tip ${tip}."
"${python}" "${here}/classify-docs-only.py" -z --github-output "${verdict}" < "${changed}" ||
    build "Could not classify the difference to main's tip ${tip}."

if [ ! -s "${changed}" ]; then
    build "Main's tip ${tip} has the same tree as ${this}."
fi
if grep -qx 'docs_only=true' "${verdict}"; then
    build "Main's tip ${tip} differs from ${this} in docs only, and a docs-only push starts no build."
fi

echo "::error::Superseded: main moved to ${tip} while this build of ${this} waited its turn. That push has its own build queued, so this one stops here. Nothing is wrong with ${this}."
exit 1
