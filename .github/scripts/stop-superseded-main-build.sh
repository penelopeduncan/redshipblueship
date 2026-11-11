#!/usr/bin/env bash
# Stop a build of main that a newer push to main has superseded.
#
# generate-builds.yml queues main's builds per job (MAIN'S BUILD QUEUES at the
# top of that file): every push to main waits its turn. A queued job runs this
# as soon as its turn comes and its checkout is done. If main has moved on by
# then and the newer commit has a build run of its own, this one stops here
# and gives its place to that one.
#
# Exit 0: build this commit. Exit 1: superseded, do not build.
#
# A build is superseded only when BOTH hold:
#   1. Main's tip differs from this commit in something that is not docs. A
#      push that changes only docs starts no generate-builds run (the
#      workflow's paths-ignore), so against such a tip this commit is still the
#      newest one with a build. The docs patterns are classify-docs-only.py's,
#      which the tests keep equal to paths-ignore.
#   2. The tip has a generate-builds push run that is still going or that
#      succeeded (main-tip-build-run.py). A code commit can be on main with no
#      such run: a skip token in its message, a run someone cancelled, a run
#      that failed before its build jobs. Then nothing else will build main,
#      so this commit is built.
#
# Fails OPEN. Anything this script cannot work out (no network, no python, a
# git or API error) ends in exit 0: an extra build costs minutes, a build
# wrongly skipped leaves main without one and its caches stale.
#
# What is left: a tip whose run is alive now and fails later, before its build
# jobs. The builds queued behind it have stopped by then, and main waits for
# the next push or a re-run.
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

# SUPERSEDED_RUNS_JSON: the tests' stand-in for GitHub's answer.
if [ -n "${SUPERSEDED_RUNS_JSON:-}" ]; then
    "${python}" "${here}/main-tip-build-run.py" --runs-json "${SUPERSEDED_RUNS_JSON}" "${tip}"
else
    "${python}" "${here}/main-tip-build-run.py" "${tip}"
fi
case "$?" in
    0) ;;
    1) build "Main's tip ${tip} differs in code but has no build run of its own." ;;
    *) build "Could not tell whether main's tip ${tip} has a build run." ;;
esac

echo "::error::Superseded: main moved to ${tip} while this build of ${this} waited its turn, and that commit has its own build run. This one stops here. Nothing is wrong with ${this}."
exit 1
