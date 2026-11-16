#!/usr/bin/env bash
# Re-run a release run's macOS leg when GitHub never gave it a runner (#1655).
#
# Called by .github/workflows/release-macos-retry.yml once a release run of
# generate-builds (a v* tag push, a release/v* branch push or a dispatch) has
# completed with conclusion failure. On 2026-11-16 the v0.5.1-alpha run
# (43905101860) lost its "macOS build" job three times: attempts 1, 2 and 3
# were each cancelled exactly 15 minutes after the job started, with no step
# run and no runner named, and attempt 4 (19:40 UTC, more than three hours
# after the first) got a runner and built in 8 minutes. Each unacquired job's
# check run carries the failure annotation "The job was not acquired by Runner
# of type hosted even after multiple attempts" beside the notice "Due to
# capacity constraints, jobs targeting macOS arm64 runners may experience
# longer queue times". That 15 minutes is GitHub's own limit on finding a
# hosted runner; no line in this repository sets it, and no timeout-minutes
# can lengthen it. The release itself was made (create-release does not wait
# for macOS, #1501), and "Attach macOS release" was skipped, so the release
# had no dmg.
#
# The manual remedy is to re-run the macOS job of that run later: it rebuilds
# "macOS build" from the same commit and, through the jobs that need it
# (required-build-macos and release-macos), attaches the dmg with --clobber.
# This script does that by itself, with an escalating pause before each try.
#
# It re-runs only when ALL of these hold for the newest attempt, which must
# have completed:
#   * the attempt's conclusion is failure (a person's or a concurrency
#     group's cancel ends it cancelled);
#   * its "macOS build" job was cancelled with no step and no runner;
#   * that job's check run has the failure annotation "... was not acquired
#     by Runner ..." (a job cancelled on purpose while queued has the same
#     shape but no annotation);
#   * every job outside the macOS leg ("macOS build", "build-macos",
#     "Attach macOS release") ended success or skipped, so nothing else of
#     the run is a failure a person should look at;
#   * for a tag or release-branch run, the release tag still points at the
#     run's commit (a re-run always builds the run's own commit);
#   * the run is still at that attempt and completed when the pause ends (a
#     person may have re-run it meanwhile).
# It re-runs only the "macOS build" job (gh run rerun --job), which also
# re-runs the jobs that depend on it and nothing else. Anything else is
# printed and left for a person. Once a re-run's macOS job has a runner, the
# script exits: that attempt attaches the dmg itself, and a real build
# failure is a person's to read.
#
# Wait schedule: WAIT_SCHEDULE lists the pause in seconds before each re-run,
# indexed by the absolute attempt number (the pause before attempt N+1 is
# entry N), so a second retrier started for a later attempt resumes the
# schedule instead of restarting it. The default "600 1800 3600 5400" allows
# four re-runs. The pauses count from the attempt's COMPLETION (the whole
# run, Windows and Linux included, which can end long after the macOS job was
# cancelled), and each unacquired re-run adds about 16 minutes, so the last
# re-run starts about 3.6 hours after the first attempt completed. After the
# last entry the script gives up with an error and the manual command.
# release-macos-retry.yml's `run_attempt <` guard must equal the number of
# entries plus one (tools/tests/test_retry_unacquired_macos.py locks this).
#
# Environment: GH_TOKEN (actions: write, checks: read), REPO (owner/name),
# RUN_ID, RUN_ATTEMPT (the attempt that just completed), and optionally
# WAIT_SCHEDULE, POLL_SECONDS (default 60, the poll interval while a re-run
# is queued) and MAX_BAD_POLLS (default 30, consecutive failed polls before
# giving up).
set -uo pipefail

WAIT_SCHEDULE="${WAIT_SCHEDULE:-600 1800 3600 5400}"
POLL_SECONDS="${POLL_SECONDS:-60}"
MAX_BAD_POLLS="${MAX_BAD_POLLS:-30}"
JOB_NAME="macOS build"
UNACQUIRED_PATTERN='^failure: .*was not acquired by Runner'

# jq programs for gh api --jq (gh's built-in jq; no jq binary is needed).
JQ_RUN='"\(.run_attempt) \(.status) \(.event) \(.head_sha) \(.head_branch)"'
JQ_CONCLUSION='.conclusion // "none"'
JQ_JOBS='.jobs[] | "\(.id)|\(.conclusion // "none")|\(.steps | length)|\(.runner_name // "")|\(.name)"'
JQ_ANNOTATIONS='.[] | "\(.annotation_level): \(.message)"'
JQ_SHA='.sha'

is_uint() {
    case "$1" in
        '' | *[!0-9]*) return 1 ;;
    esac
    return 0
}

read -r -a waits <<<"${WAIT_SCHEDULE}"
if [ "${#waits[@]}" -eq 0 ]; then
    echo "::error::WAIT_SCHEDULE is empty"
    exit 1
fi
for value in "${waits[@]}" "${POLL_SECONDS}" "${MAX_BAD_POLLS}" "${RUN_ID}" "${RUN_ATTEMPT}"; do
    if ! is_uint "${value}"; then
        echo "::error::'${value}' is not a non-negative integer (WAIT_SCHEDULE, POLL_SECONDS, MAX_BAD_POLLS, RUN_ID, RUN_ATTEMPT)"
        exit 1
    fi
done
# Attempt 1 plus one re-run per schedule entry.
last_attempt=$((${#waits[@]} + 1))

# Prints "<attempt> <status> <event> <head sha> <head branch>" for the run.
run_state() {
    gh api "repos/${REPO}/actions/runs/${RUN_ID}" --jq "${JQ_RUN}"
}

# Prints one "<id>|<conclusion>|<steps>|<runner>|<name>" line per job of one
# attempt; carried-over jobs are listed with their earlier conclusions.
attempt_jobs() {
    gh api "repos/${REPO}/actions/runs/${RUN_ID}/attempts/$1/jobs?per_page=100" --jq "${JQ_JOBS}"
}

# Sets macos_id, macos_conclusion, macos_steps and macos_runner ("-" for none)
# from a jobs listing; macos_id is empty when the attempt has no macOS job.
find_macos_job() {
    macos_id=""
    macos_conclusion=""
    macos_steps=""
    macos_runner=""
    local id conclusion steps runner name
    while IFS='|' read -r id conclusion steps runner name; do
        if [ "${name}" = "${JOB_NAME}" ]; then
            macos_id="${id}"
            macos_conclusion="${conclusion}"
            macos_steps="${steps}"
            macos_runner="${runner:--}"
            return
        fi
    done <<<"$1"
}

# Prints text read from the API without letting a line that starts with "::"
# act as a workflow command.
print_inert() {
    local token="inert-${RANDOM}${RANDOM}${RANDOM}"
    echo "::stop-commands::${token}"
    printf '%s\n' "$1"
    echo "::${token}::"
}

attempt="${RUN_ATTEMPT}"

# Act only on the run's newest attempt, and only once it has completed. If the
# completion of a re-run started below also fires workflow_run, its retrier
# queues behind this one (per-run concurrency group) and must not start a
# second chain of re-runs from an attempt that is no longer the newest.
if ! current="$(run_state)"; then
    echo "::error::could not read run ${RUN_ID}"
    exit 1
fi
read -r current_attempt current_status event head_sha head_branch <<<"${current}"
if [ "${current_attempt}" != "${attempt}" ] || [ "${current_status}" != "completed" ]; then
    echo "Run ${RUN_ID} is now at attempt ${current_attempt} (${current_status}), not completed attempt ${attempt}; nothing to do."
    exit 0
fi

# The release tag a re-run would attach the dmg to, when the run names it (a
# dispatch's tag is an input the run object does not carry).
release_tag=""
case "${event}:${head_branch}" in
    push:release/v*) release_tag="${head_branch#release/}" ;;
    push:v*) release_tag="${head_branch}" ;;
    workflow_dispatch:*) ;;
    *)
        echo "Run ${RUN_ID} (${event} of '${head_branch}') is not a release run; nothing to do."
        exit 0
        ;;
esac

while :; do
    if ! jobs="$(attempt_jobs "${attempt}")"; then
        echo "::error::could not read the jobs of run ${RUN_ID} attempt ${attempt}"
        exit 1
    fi
    find_macos_job "${jobs}"
    if [ -z "${macos_id}" ]; then
        echo "Run ${RUN_ID} attempt ${attempt} has no '${JOB_NAME}' job; nothing to do."
        exit 0
    fi
    echo "Run ${RUN_ID} attempt ${attempt}: '${JOB_NAME}' ${macos_conclusion}, ${macos_steps} steps, runner ${macos_runner}"
    if [ "${macos_conclusion}" = "success" ]; then
        echo "The macOS build succeeded; release-macos attaches the dmg in the same attempt."
        exit 0
    fi
    if [ "${macos_conclusion}" != "cancelled" ] || [ "${macos_steps}" != "0" ] || [ "${macos_runner}" != "-" ]; then
        echo "::warning::the macOS build of run ${RUN_ID} attempt ${attempt} ended '${macos_conclusion}' (${macos_steps} steps, runner ${macos_runner}); that is not a runner shortage, so it is not re-run automatically"
        exit 0
    fi

    if ! attempt_conclusion="$(gh api "repos/${REPO}/actions/runs/${RUN_ID}/attempts/${attempt}" --jq "${JQ_CONCLUSION}")"; then
        echo "::error::could not read the conclusion of run ${RUN_ID} attempt ${attempt}; not re-run"
        exit 1
    fi
    if [ "${attempt_conclusion}" != "failure" ]; then
        echo "::warning::run ${RUN_ID} attempt ${attempt} ended '${attempt_conclusion}', not 'failure': a person or a concurrency group stopped it, so it is not re-run automatically"
        exit 0
    fi

    if ! annotations="$(gh api "repos/${REPO}/check-runs/${macos_id}/annotations?per_page=100" --jq "${JQ_ANNOTATIONS}")"; then
        echo "::error::could not read the annotations of the macOS job ${macos_id}; not re-run"
        exit 1
    fi
    if ! grep -q -E "${UNACQUIRED_PATTERN}" <<<"${annotations}"; then
        echo "::warning::the macOS job of run ${RUN_ID} attempt ${attempt} was cancelled before it started, but GitHub did not report a runner shortage, so it is not re-run automatically. Its annotations:"
        print_inert "${annotations:-(none)}"
        exit 0
    fi

    others=""
    while IFS='|' read -r id conclusion steps runner name; do
        [ -n "${id}" ] || continue
        case "${name}" in
            "${JOB_NAME}" | "build-macos" | "Attach macOS release") continue ;;
        esac
        case "${conclusion}" in
            success | skipped) ;;
            *) others="${others}${others:+, }${name} (${conclusion})" ;;
        esac
    done <<<"${jobs}"
    if [ -n "${others}" ]; then
        echo "::warning::run ${RUN_ID} attempt ${attempt} has other jobs that did not succeed: ${others}. It is not re-run automatically; a person should look at it."
        exit 0
    fi

    if [ -n "${release_tag}" ]; then
        if ! tag_sha="$(gh api "repos/${REPO}/commits/${release_tag}" --jq "${JQ_SHA}")"; then
            echo "::error::could not resolve the release tag ${release_tag} (deleted?); not re-run"
            exit 1
        fi
        if [ "${tag_sha}" != "${head_sha}" ]; then
            echo "::warning::the tag ${release_tag} now points at ${tag_sha}, not this run's commit ${head_sha}; not re-run"
            exit 0
        fi
    fi

    if [ "${attempt}" -ge "${last_attempt}" ]; then
        echo "::error::no hosted macOS runner was acquired by attempt ${attempt}, the last the schedule allows. Re-run the macOS job of run ${RUN_ID} later: gh run rerun ${RUN_ID} --job ${macos_id} -R ${REPO}"
        exit 1
    fi
    wait_seconds="${waits[$((attempt - 1))]}"
    echo "No hosted macOS runner picked the job up (GitHub reported a runner shortage). Re-running it in ${wait_seconds} s."
    sleep "${wait_seconds}"

    # A person may have re-run the run during the pause.
    if ! current="$(run_state)"; then
        echo "::error::could not read run ${RUN_ID} after the pause; not re-run"
        exit 1
    fi
    read -r current_attempt current_status _ <<<"${current}"
    if [ "${current_attempt}" != "${attempt}" ] || [ "${current_status}" != "completed" ]; then
        echo "Run ${RUN_ID} moved to attempt ${current_attempt} (${current_status}) during the pause; someone else re-ran it, so this retrier stops."
        exit 0
    fi
    if ! gh run rerun "${RUN_ID}" --job "${macos_id}" -R "${REPO}"; then
        echo "::error::gh run rerun ${RUN_ID} --job ${macos_id} was refused"
        exit 1
    fi

    # Wait until the new attempt's macOS job has a runner (then it is no longer
    # this script's business) or the attempt completes without one.
    bad_polls=0
    while :; do
        sleep "${POLL_SECONDS}"
        if ! now="$(run_state)"; then
            now=""
        fi
        read -r now_attempt now_status _ <<<"${now}"
        if ! is_uint "${now_attempt:-}"; then
            bad_polls=$((bad_polls + 1))
            if [ "${bad_polls}" -ge "${MAX_BAD_POLLS}" ]; then
                echo "::error::could not read run ${RUN_ID} in ${bad_polls} polls in a row; giving up"
                exit 1
            fi
            continue
        fi
        bad_polls=0
        if [ "${now_attempt}" -le "${attempt}" ]; then
            continue
        fi
        if [ "${now_status}" = "completed" ]; then
            attempt="${now_attempt}"
            break
        fi
        if new_jobs="$(attempt_jobs "${now_attempt}")"; then
            find_macos_job "${new_jobs}"
            if [ -n "${macos_id}" ] && { [ "${macos_steps}" != "0" ] || [ "${macos_runner}" != "-" ]; }; then
                echo "Run ${RUN_ID} attempt ${now_attempt}: the macOS job has a runner (${macos_runner}); release-macos attaches the dmg when it succeeds."
                exit 0
            fi
        fi
    done
done
