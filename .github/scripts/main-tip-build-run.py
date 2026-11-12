#!/usr/bin/env python3
"""Does a commit on main have a generate-builds push run that will build it, or has built it?

stop-superseded-main-build.sh asks this about main's tip before it stops an
older commit's build as superseded. A newer code commit on main normally has a
build run of its own, but not always: its message may carry a skip token
([skip ci] and friends), its run may have been cancelled, or the run may have
failed before it reached the build jobs. Stopping the older build in those
cases would leave main with no build at all, so the caller stops only on
exit 0.

Exit 0: the commit has a generate-builds push run that is still going, or that
        succeeded.
Exit 1: it has no such run (none at all, or only failed or cancelled ones).
Exit 2: could not tell (no network, an API error, an unexpected answer).

The caller builds on 1 and on 2.
"""

import argparse
import json
import os
import sys
import urllib.error
import urllib.request

WORKFLOW_FILE = "generate-builds.yml"


def is_build_run(run, sha):
    """One of the workflow's push runs for `sha`. `path` can carry an @ref suffix."""
    path = str(run.get("path", "")).split("@", 1)[0]
    return run.get("head_sha") == sha and run.get("event") == "push" and path.rsplit("/", 1)[-1] == WORKFLOW_FILE


def will_build(run):
    """Still going (any status but completed), or finished with success."""
    return run.get("status") != "completed" or run.get("conclusion") == "success"


def decide(payload, sha):
    """(exit code, sentence) for the API's answer to "list the workflow runs of this commit"."""
    runs = payload.get("workflow_runs") if isinstance(payload, dict) else None
    if not isinstance(runs, list):
        return 2, "the runs list has an unexpected shape"
    mine = [run for run in runs if isinstance(run, dict) and is_build_run(run, sha)]
    if not mine:
        return 1, f"{sha} has no {WORKFLOW_FILE} push run"
    for run in mine:
        if will_build(run):
            state = run.get("status") if run.get("status") != "completed" else "success"
            return 0, f"{sha} has {WORKFLOW_FILE} push run {run.get('id')} ({state})"
    states = ", ".join(f"{run.get('id')} {run.get('conclusion')}" for run in mine)
    return 1, f"every {WORKFLOW_FILE} push run of {sha} ended without a build ({states})"


def fetch(sha):
    api = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")
    repo = os.environ["GITHUB_REPOSITORY"]
    request = urllib.request.Request(
        f"{api}/repos/{repo}/actions/runs?head_sha={sha}&event=push&per_page=100",
        headers={"Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"},
    )
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument("sha", help="the commit to ask about (main's tip)")
    parser.add_argument("--runs-json", help="read the API's answer from this file instead of asking GitHub")
    args = parser.parse_args(argv)
    try:
        if args.runs_json:
            with open(args.runs_json, encoding="utf-8") as fh:
                payload = json.load(fh)
        else:
            payload = fetch(args.sha)
    except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
        print(f"Could not list the runs of {args.sha}: {error}")
        return 2
    code, sentence = decide(payload, args.sha)
    print(sentence[0].upper() + sentence[1:] + ".")
    return code


if __name__ == "__main__":
    sys.exit(main())
