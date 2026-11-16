#!/usr/bin/env python3
"""The release macOS retrier re-runs only a runner shortage (#1655).

.github/scripts/retry-unacquired-macos.sh is run under bash with a fake `gh`
and a fake `sleep` first on PATH, so no test waits or talks to GitHub. The
fake answers `gh api` from a scenario modelled on the real run 43905101860
(tag v0.5.1-alpha): attempts 1-3 had "macOS build" cancelled with no step and
no runner, the run concluded failure, and the job's check run carried the
failure annotation "The job was not acquired by Runner of type hosted even
after multiple attempts". A job cancelled on purpose while queued has the
same job shape and no annotation, and its run concludes cancelled.

`gh api --jq` is evaluated by a real jq binary when one is on PATH (as on
ubuntu-latest, where CI runs this). Without one, the fake evaluates the
script's five jq programs itself, matched by their exact text, and fails
loudly on any other program, so an edited program is noticed here and its
real behaviour is checked in CI.
"""

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / ".github" / "scripts" / "retry-unacquired-macos.sh"
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "release-macos-retry.yml"

UNACQUIRED_MESSAGE = "The job was not acquired by Runner of type hosted even after multiple attempts"
CAPACITY_NOTICE = (
    "Due to capacity constraints, jobs targeting macOS arm64 runners may experience longer queue times."
)
DEFAULT_SCHEDULE = [600, 1800, 3600, 5400]
POLL = 7  # distinct from every schedule entry, so the two kinds of sleep are told apart


def find_bash():
    override = os.environ.get("RSBS_TEST_BASH")
    if override:
        return override
    if sys.platform == "win32":
        # System32\bash.exe is the WSL launcher, not a POSIX shell.
        for candidate in (r"C:\Program Files\Git\bin\bash.exe", r"C:\Program Files\Git\usr\bin\bash.exe"):
            if Path(candidate).exists():
                return candidate
        return None
    return shutil.which("bash")


BASH = find_bash()
pytestmark = pytest.mark.skipif(BASH is None, reason="no POSIX bash to run the script")


# ---------------------------------------------------------------- the fake gh

FAKE = r'''
import json, os, re, shutil, subprocess, sys

STATE = os.environ["FAKE_GH_STATE"]
# gh prints LF line ends on every platform.
sys.stdout.reconfigure(newline="\n")


def load():
    with open(STATE, encoding="utf-8") as f:
        return json.load(f)


def save(state):
    with open(STATE, "w", encoding="utf-8") as f:
        json.dump(state, f)


def js(value):
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def alt(value, default):
    return default if value is None or value is False else value


EMULATED = {
    '"\\(.run_attempt) \\(.status) \\(.event) \\(.head_sha) \\(.head_branch)"':
        lambda d: [" ".join(js(d.get(k)) for k in ("run_attempt", "status", "event", "head_sha", "head_branch"))],
    '.conclusion // "none"': lambda d: [js(alt(d.get("conclusion"), "none"))],
    '.jobs[] | "\\(.id)|\\(.conclusion // "none")|\\(.steps | length)|\\(.runner_name // "")|\\(.name)"':
        lambda d: ["%s|%s|%d|%s|%s" % (js(j["id"]), js(alt(j.get("conclusion"), "none")), len(j.get("steps") or []),
                                       js(alt(j.get("runner_name"), "")), js(j["name"])) for j in d["jobs"]],
    '.[] | "\\(.annotation_level): \\(.message)"':
        lambda d: ["%s: %s" % (js(a.get("annotation_level")), js(a.get("message"))) for a in d],
    '.sha': lambda d: [js(d.get("sha"))],
}


def apply_jq(program, data):
    jq = shutil.which("jq")
    if jq and not os.environ.get("FAKE_GH_EMULATE_JQ"):
        out = subprocess.run([jq, "-r", program], input=json.dumps(data), capture_output=True, text=True)
        sys.stdout.write(out.stdout)
        sys.stderr.write(out.stderr)
        return out.returncode
    if program not in EMULATED:
        sys.stderr.write("fake gh: no emulation for jq program %r\n" % program)
        return 3
    for line in EMULATED[program](data):
        print(line)
    return 0


def attempt_view(spec):
    """Jobs of an attempt as the API shows them in its current phase."""
    phase = spec.get("phase", "completed")
    if phase == "completed":
        return spec["jobs"]
    jobs = []
    for job in spec["jobs"]:
        job = dict(job)
        if job["name"] in ("macOS build", "build-macos", "Attach macOS release"):
            job["conclusion"] = None
            job["steps"] = []
            job["runner_name"] = ""
            if job["name"] == "macOS build" and phase == "running":
                job["steps"] = [{"name": "Set up job"}]
                job["runner_name"] = "GitHub Actions 1000000007"
        jobs.append(job)
    return jobs


def failing(state, path):
    # A key matches any path containing it, or with a leading "=" only that path;
    # its value is how many more calls fail (-1: all of them).
    for prefix, left in list(state.get("fail", {}).items()):
        hit = path == prefix[1:] if prefix.startswith("=") else prefix in path
        if hit and left != 0:
            state["fail"][prefix] = left - 1 if left > 0 else left
            return True
    return False


def api(state, path, program):
    path = path.split("?", 1)[0]
    if failing(state, path):
        sys.stderr.write("gh: Server Error (HTTP 502)\n")
        return 1
    run = state["run"]
    current = len(state["attempts"])
    m = re.fullmatch(r"repos/[^/]+/[^/]+/actions/runs/(\d+)(/attempts/(\d+)(/jobs)?)?", path)
    if m and int(m.group(1)) == run["id"]:
        if m.group(2) is None:
            spec = state["attempts"][-1]
            if spec.get("phase", "completed") != "completed":
                progress = spec.setdefault("progress", [])
                spec["phase"] = progress.pop(0) if progress else "completed"
            status = "completed" if spec.get("phase", "completed") == "completed" else "in_progress"
            return apply_jq(program, dict(run, run_attempt=current, status=status))
        n = int(m.group(3))
        if not 1 <= n <= current:
            sys.stderr.write("gh: Not Found (HTTP 404)\n")
            return 1
        spec = state["attempts"][n - 1]
        if m.group(4):
            return apply_jq(program, {"jobs": attempt_view(spec)})
        done = spec.get("phase", "completed") == "completed"
        return apply_jq(program, {"run_attempt": n, "conclusion": spec["conclusion"] if done else None})
    m = re.fullmatch(r"repos/[^/]+/[^/]+/check-runs/(\d+)/annotations", path)
    if m:
        return apply_jq(program, state.get("annotations", {}).get(m.group(1), []))
    m = re.fullmatch(r"repos/[^/]+/[^/]+/commits/(.+)", path)
    if m and m.group(1) in state.get("tags", {}):
        return apply_jq(program, {"sha": state["tags"][m.group(1)]})
    sys.stderr.write("gh: Not Found (HTTP 404) for %s\n" % path)
    return 1


def add_attempt(state, spec):
    spec = dict(spec)
    spec.setdefault("phase", "queued")
    state["attempts"].append(spec)
    for job_id, notes in spec.pop("annotations", {}).items():
        state.setdefault("annotations", {})[job_id] = notes


def rerun(state, args):
    run_id, job_id = args[0], args[args.index("--job") + 1]
    spec = state["attempts"][-1]
    if int(run_id) != state["run"]["id"] or spec.get("phase", "completed") != "completed":
        sys.stderr.write("fake gh: run %s cannot be re-run now\n" % run_id)
        return 1
    macos = [j for j in spec["jobs"] if j["name"] == "macOS build"]
    if not macos or str(macos[0]["id"]) != job_id:
        sys.stderr.write("fake gh: job %s is not the newest attempt's macOS job\n" % job_id)
        return 1
    if not state["reruns"]:
        sys.stderr.write("fake gh: unexpected re-run\n")
        return 1
    add_attempt(state, state["reruns"].pop(0))
    state.setdefault("fail", {}).update(state.pop("fail_after_rerun", {}))
    return 0


def main():
    state = load()
    tool, args = sys.argv[1], sys.argv[2:]
    state["calls"].append([tool] + args)
    if tool == "sleep":
        action = state.get("on_sleep", {}).pop(args[0], None)
        if action:
            add_attempt(state, action)
        if len([c for c in state["calls"] if c[0] == "sleep"]) > 200:
            save(state)
            sys.stderr.write("fake sleep: runaway loop\n")
            sys.exit(99)
        save(state)
        return 0
    if args[:1] == ["api"]:
        path = args[1]
        program = args[args.index("--jq") + 1] if "--jq" in args else "."
        code = api(state, path, program)
    elif args[:2] == ["run", "rerun"]:
        code = rerun(state, args[2:])
    else:
        sys.stderr.write("fake gh: unexpected call %r\n" % args)
        code = 2
    save(state)
    return code


sys.exit(main())
'''


# --------------------------------------------------------------- scenarios


class Ids:
    def __init__(self):
        self.next = 1000

    def __call__(self):
        self.next += 1
        return self.next


def attempt(ids, macos="unacquired", annotated=None, conclusion=None, others=None, progress=None):
    """One attempt of a release run, shaped like run 43905101860's."""
    macos_id = ids()
    job = {"id": macos_id, "name": "macOS build", "conclusion": "cancelled", "steps": [], "runner_name": ""}
    leg_conclusion = "failure"
    if macos == "success":
        job.update(conclusion="success", steps=[{"name": "Build"}] * 20, runner_name="GitHub Actions 1000000001")
        leg_conclusion = "success"
    elif macos == "failed":
        job.update(conclusion="failure", steps=[{"name": "Build"}] * 9, runner_name="GitHub Actions 1000000002")
    elif macos == "queued-cancel":
        job.update(runner_name=None)
    jobs = [
        {"id": ids(), "name": "generate-rsbs-otr", "conclusion": "success", "steps": [{}], "runner_name": "r"},
        {"id": ids(), "name": "Linux build", "conclusion": "success", "steps": [{}], "runner_name": "r"},
        {"id": ids(), "name": "Windows build", "conclusion": "success", "steps": [{}], "runner_name": "r"},
        {"id": ids(), "name": "docs-only", "conclusion": "skipped", "steps": [], "runner_name": None},
        job,
        {"id": ids(), "name": "create-release", "conclusion": "success", "steps": [{}], "runner_name": "r"},
        {"id": ids(), "name": "build-macos", "conclusion": leg_conclusion, "steps": [{}], "runner_name": "r"},
        {
            "id": ids(),
            "name": "Attach macOS release",
            "conclusion": "success" if macos == "success" else "skipped",
            "steps": [],
            "runner_name": None,
        },
    ]
    for job_ in jobs:
        if others and job_["name"] in others:
            job_["conclusion"] = others[job_["name"]]
    if annotated is None:
        annotated = macos == "unacquired"
    notes = [{"annotation_level": "notice", "message": CAPACITY_NOTICE}]
    if annotated:
        notes.insert(0, {"annotation_level": "failure", "message": UNACQUIRED_MESSAGE})
    if conclusion is None:
        conclusion = "success" if macos == "success" else "failure"
    spec = {"conclusion": conclusion, "jobs": jobs, "annotations": {str(macos_id): notes}}
    if progress is not None:
        spec["progress"] = progress
    return spec


def scenario(attempts, reruns=(), event="push", branch="v0.5.1-alpha", tags=None, **extra):
    state = {
        "run": {"id": 42, "event": event, "head_branch": branch, "head_sha": "abc123"},
        "attempts": [],
        "reruns": list(reruns),
        "tags": {"v0.5.1-alpha": "abc123"} if tags is None else tags,
        "calls": [],
    }
    for spec in attempts:
        spec = dict(spec)
        spec.setdefault("phase", "completed")
        state["attempts"].append(spec)
        for job_id, notes in spec.pop("annotations", {}).items():
            state.setdefault("annotations", {})[job_id] = notes
    state.update(extra)
    return state


def macos_id(spec):
    return [j["id"] for j in spec["jobs"] if j["name"] == "macOS build"][0]


def run_script(tmp_path, state, run_attempt=None, env=None, script=SCRIPT):
    fake_dir = tmp_path / "bin"
    fake_dir.mkdir(exist_ok=True)
    fake_py = tmp_path / "fake_gh.py"
    fake_py.write_text(FAKE, encoding="utf-8")
    python = Path(sys.executable).as_posix()
    for tool in ("gh", "sleep"):
        wrapper = fake_dir / tool
        wrapper.write_text(
            '#!/usr/bin/env bash\nexec "%s" "%s" %s "$@"\n' % (python, fake_py.as_posix(), tool),
            encoding="utf-8",
            newline="\n",
        )
        wrapper.chmod(0o755)
    state_file = tmp_path / "state.json"
    state_file.write_text(json.dumps(state), encoding="utf-8")
    full_env = dict(os.environ)
    full_env.update(
        {
            "FAKE_GH_STATE": str(state_file),
            "GH_TOKEN": "fake",
            "REPO": "o/r",
            "RUN_ID": "42",
            "RUN_ATTEMPT": str(run_attempt if run_attempt is not None else len(state["attempts"])),
            "POLL_SECONDS": str(POLL),
        }
    )
    for name in ("WAIT_SCHEDULE", "MAX_BAD_POLLS"):
        full_env.pop(name, None)
    full_env.update(env or {})
    bash_dir = Path(BASH).parent.as_posix()
    # The fakes come first; the script itself needs only bash builtins and grep.
    command = 'export PATH="$(cygpath -u "$FAKE_BIN" 2>/dev/null || echo "$FAKE_BIN"):$PATH"; exec bash "$0"'
    full_env["FAKE_BIN"] = str(fake_dir)
    result = subprocess.run(
        [BASH, "-c", command, Path(script).as_posix()],
        env=full_env,
        capture_output=True,
        text=True,
        timeout=120,
        cwd=bash_dir,
    )
    final = json.loads(state_file.read_text(encoding="utf-8"))
    return result, final


def reruns_of(final):
    return [c for c in final["calls"] if c[:3] == ["gh", "run", "rerun"]]


def pauses_of(final):
    return [int(c[1]) for c in final["calls"] if c[0] == "sleep" and c[1] != str(POLL)]


# -------------------------------------------------------------------- tests


def test_unacquired_with_annotation_is_rerun_and_a_later_success_ends_it(tmp_path):
    ids = Ids()
    first = attempt(ids)
    state = scenario([first], reruns=[attempt(ids, macos="success", progress=["queued"])])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == [["gh", "run", "rerun", "42", "--job", str(macos_id(first)), "-R", "o/r"]]
    assert pauses_of(final) == [600]
    assert "The macOS build succeeded" in result.stdout


def test_exits_once_the_rerun_has_a_runner(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], reruns=[attempt(ids, macos="success", progress=["queued", "running"] * 50)])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(reruns_of(final)) == 1
    assert "has a runner" in result.stdout
    # It stopped at the first poll that saw a runner instead of waiting for the build.
    assert final["attempts"][-1]["phase"] == "running"


def test_schedule_is_honoured_and_it_gives_up_after_the_last_entry(tmp_path):
    ids = Ids()
    unacquired = [attempt(ids, progress=["queued"]) for _ in DEFAULT_SCHEDULE]
    state = scenario([attempt(ids)], reruns=unacquired)
    result, final = run_script(tmp_path, state)
    assert result.returncode != 0
    assert pauses_of(final) == DEFAULT_SCHEDULE
    assert len(reruns_of(final)) == len(DEFAULT_SCHEDULE)
    assert len(final["attempts"]) == len(DEFAULT_SCHEDULE) + 1
    assert "::error::no hosted macOS runner was acquired by attempt 5" in result.stdout


def test_a_custom_schedule_sets_the_number_of_reruns(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], reruns=[attempt(ids, progress=[]) for _ in range(5)])
    result, final = run_script(tmp_path, state, env={"WAIT_SCHEDULE": "11 22"})
    assert result.returncode != 0
    assert pauses_of(final) == [11, 22]
    assert len(reruns_of(final)) == 2


def test_a_later_retrier_resumes_the_schedule_at_its_attempt(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids), attempt(ids), attempt(ids)], reruns=[attempt(ids, progress=[])] * 3)
    result, final = run_script(tmp_path, state, run_attempt=3)
    assert result.returncode != 0
    assert pauses_of(final) == [3600, 5400]
    assert len(reruns_of(final)) == 2


def test_cancelled_without_the_annotation_is_not_rerun(tmp_path):
    ids = Ids()
    # Even with a 'failure' run conclusion, the job shape alone is not enough.
    state = scenario([attempt(ids, macos="queued-cancel", annotated=False, conclusion="failure")])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "did not report a runner shortage" in result.stdout
    assert "::stop-commands::" in result.stdout and CAPACITY_NOTICE in result.stdout


def test_a_cancelled_run_is_not_rerun_even_with_the_annotation(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids, conclusion="cancelled")])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "ended 'cancelled'" in result.stdout


def test_a_rerun_someone_cancelled_ends_the_chain(tmp_path):
    ids = Ids()
    cancelled = attempt(ids, macos="queued-cancel", annotated=False, conclusion="cancelled", progress=[])
    state = scenario([attempt(ids)], reruns=[cancelled, attempt(ids)])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(reruns_of(final)) == 1


@pytest.mark.parametrize("name,conclusion", [("Windows build", "failure"), ("Linux build", "cancelled"),
                                             ("create-release", "timed_out"), ("generate-rsbs-otr", "neutral")])
def test_another_job_that_did_not_succeed_blocks_the_rerun(tmp_path, name, conclusion):
    ids = Ids()
    state = scenario([attempt(ids, others={name: conclusion})])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "%s (%s)" % (name, conclusion) in result.stdout


def test_an_acquired_and_failed_macos_build_is_not_rerun(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids, macos="failed", annotated=True)])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "not a runner shortage" in result.stdout


def test_a_stale_event_does_nothing(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids), attempt(ids)])
    result, final = run_script(tmp_path, state, run_attempt=1)
    assert result.returncode == 0, result.stdout + result.stderr
    assert [c[:2] for c in final["calls"]] == [["gh", "api"]]
    assert "nothing to do" in result.stdout


def test_a_manual_rerun_during_the_pause_stops_the_retrier(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], on_sleep={"600": attempt(ids, progress=["queued"])})
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "someone else re-ran it" in result.stdout


def test_an_unreadable_annotation_list_fails_closed(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], fail={"/annotations": -1})
    result, final = run_script(tmp_path, state)
    assert result.returncode != 0
    assert reruns_of(final) == []


def test_a_moved_release_tag_is_not_rerun(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], tags={"v0.5.1-alpha": "def456"})
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []
    assert "now points at def456" in result.stdout


def test_a_release_branch_run_checks_the_stripped_tag(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], branch="release/v0.5.1-alpha",
                     reruns=[attempt(ids, macos="success", progress=[])])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(reruns_of(final)) == 1
    assert any(c[:3] == ["gh", "api", "repos/o/r/commits/v0.5.1-alpha"] for c in final["calls"])


def test_a_dispatch_is_rerun_without_a_tag_lookup(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], event="workflow_dispatch", branch="main", tags={},
                     reruns=[attempt(ids, macos="success", progress=[])])
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert len(reruns_of(final)) == 1


def test_a_push_to_main_does_nothing(tmp_path):
    ids = Ids()
    state = scenario([attempt(ids)], branch="main")
    result, final = run_script(tmp_path, state)
    assert result.returncode == 0, result.stdout + result.stderr
    assert reruns_of(final) == []


def test_failing_polls_are_bounded(tmp_path):
    ids = Ids()
    # Every read of the run fails once the re-run is requested.
    state = scenario([attempt(ids)], reruns=[attempt(ids, progress=["queued"] * 100)],
                     fail_after_rerun={"=repos/o/r/actions/runs/42": -1})
    result, final = run_script(tmp_path, state, env={"MAX_BAD_POLLS": "3"})
    assert result.returncode != 0
    assert len(reruns_of(final)) == 1
    assert [c[1] for c in final["calls"] if c[0] == "sleep"] == ["600", str(POLL), str(POLL), str(POLL)]
    assert "in 3 polls in a row" in result.stdout


@pytest.mark.parametrize("name,value", [("WAIT_SCHEDULE", "600 soon"), ("WAIT_SCHEDULE", " "),
                                        ("POLL_SECONDS", "1m"), ("MAX_BAD_POLLS", "-1")])
def test_bad_settings_stop_before_any_call(tmp_path, name, value):
    ids = Ids()
    result, final = run_script(tmp_path, scenario([attempt(ids)]), env={name: value})
    assert result.returncode != 0
    assert final["calls"] == []


def test_workflow_guard_matches_the_schedule_length():
    script = SCRIPT.read_text(encoding="utf-8")
    m = re.search(r'^WAIT_SCHEDULE="\$\{WAIT_SCHEDULE:-([0-9 ]+)\}"$', script, re.M)
    assert m, "the script's default WAIT_SCHEDULE line moved"
    schedule = [int(x) for x in m.group(1).split()]
    assert schedule == DEFAULT_SCHEDULE
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    job = workflow["jobs"]["retry-macos"]
    guard = re.findall(r"github\.event\.workflow_run\.run_attempt < (\d+)", job["if"])
    assert guard == [str(len(schedule) + 1)], "run_attempt guard must be len(WAIT_SCHEDULE) + 1"
    assert "github.event.workflow_run.conclusion == 'failure'" in job["if"]
    for step in job["steps"]:
        assert "WAIT_SCHEDULE" not in (step.get("env") or {}), "the workflow must use the script's default"
    assert workflow["permissions"] == {"actions": "write", "checks": "read", "contents": "read"}
