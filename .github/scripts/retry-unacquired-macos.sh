#!/usr/bin/env bash
# Re-run a release run's macOS leg when GitHub never gave it a runner (#1655).
#
# Called by .github/workflows/release-macos-retry.yml once a release run of
# generate-builds (a v* tag push, a release/v* branch push or a dispatch) has
# completed. On 2026-11-16 the v0.5.1-alpha run's "macOS build" job was
# cancelled after exactly 15 minutes, twice, with no step run and no runner
# named. Its check annotations read "The job was not acquired by Runner of type
# hosted even after multiple attempts" beside "Due to capacity constraints,
# jobs targeting macOS arm64 runners may experience longer queue times". That
# 15 minutes is GitHub's own limit on finding a hosted runner; no line in this
# repository sets it, and no timeout-minutes can lengthen it. The release
# itself was made (create-release does not wait for macOS, #1501), and
# "Attach macOS release" was skipped, so the release had no dmg.
#
# The remedy that needs no workflow change is "Re-run failed jobs" on that run
# later: it re-runs "macOS build" from the same commit and, through the
# release-macos job that needs it, attaches the dmg with --clobber. Only the
# macOS leg runs again; the Windows and Linux assets are not rebuilt. This
# script does that by itself, a few times, with a pause between tries.
#
# It re-runs ONLY when the macOS job of the latest attempt was never acquired
# (conclusion cancelled, no step, no runner). A macOS build that ran and failed
# is a real failure and is left for a person: re-running it would only fail
# again.
#
# Environment: GH_TOKEN (actions: write), REPO (owner/name), RUN_ID,
# RUN_ATTEMPT (the attempt that just completed), and optionally MAX_RERUNS
# (default 3) and WAIT_SECONDS (pause before each re-run, default 600).
set -uo pipefail

MAX_RERUNS="${MAX_RERUNS:-3}"
WAIT_SECONDS="${WAIT_SECONDS:-600}"
JOB_NAME="macOS build"

# Prints "<conclusion> <step count> <runner name or ->" for the macOS job of
# one attempt, or nothing when the attempt has no such job.
macos_job_state() {
    gh api "repos/${REPO}/actions/runs/${RUN_ID}/attempts/$1/jobs?per_page=100" \
        --jq "[.jobs[] | select(.name == \"${JOB_NAME}\")][0] // empty
              | \"\(.conclusion // \"none\") \(.steps | length) \(if (.runner_name // \"\") == \"\" then \"-\" else .runner_name end)\""
}

attempt="${RUN_ATTEMPT}"
reruns=0
while :; do
    if ! state="$(macos_job_state "${attempt}")"; then
        echo "::error::could not read the jobs of run ${RUN_ID} attempt ${attempt}"
        exit 1
    fi
    if [ -z "${state}" ]; then
        echo "Run ${RUN_ID} attempt ${attempt} has no '${JOB_NAME}' job; nothing to do."
        exit 0
    fi
    read -r conclusion steps runner <<<"${state}"
    echo "Run ${RUN_ID} attempt ${attempt}: '${JOB_NAME}' ${conclusion}, ${steps} steps, runner ${runner}"
    if [ "${conclusion}" = "success" ]; then
        echo "The macOS build succeeded; release-macos attaches the dmg in the same attempt."
        exit 0
    fi
    if [ "${conclusion}" != "cancelled" ] || [ "${steps}" != "0" ] || [ "${runner}" != "-" ]; then
        echo "::warning::the macOS build of run ${RUN_ID} ran and ended '${conclusion}'; it was not a runner shortage, so it is not re-run automatically"
        exit 0
    fi
    if [ "${reruns}" -ge "${MAX_RERUNS}" ]; then
        echo "::error::no macOS runner was acquired after ${reruns} automatic re-run(s). Re-run the failed jobs of run ${RUN_ID} later: gh run rerun ${RUN_ID} --failed -R ${REPO}"
        exit 1
    fi
    echo "No hosted macOS runner picked the job up. Re-running the failed jobs in ${WAIT_SECONDS} s."
    sleep "${WAIT_SECONDS}"
    if ! gh run rerun "${RUN_ID}" --failed -R "${REPO}"; then
        echo "::error::gh run rerun ${RUN_ID} --failed was refused"
        exit 1
    fi
    reruns=$((reruns + 1))
    # Wait for the new attempt to finish. The macOS build itself can take hours;
    # if this job's own time limit ends first, the re-run carries on without it.
    while :; do
        sleep 60
        now="$(gh api "repos/${REPO}/actions/runs/${RUN_ID}" --jq '"\(.run_attempt) \(.status)"')" || continue
        read -r now_attempt now_status <<<"${now}"
        if [ "${now_attempt}" -gt "${attempt}" ] && [ "${now_status}" = "completed" ]; then
            attempt="${now_attempt}"
            break
        fi
    done
done
