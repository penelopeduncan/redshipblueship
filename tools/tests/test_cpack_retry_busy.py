#!/usr/bin/env python3
"""The macOS packaging step retries cpack only on hdiutil "Resource busy" (#1713).

.github/scripts/cpack-retry-busy.sh is run under bash with a fake `cpack` and
a fake `sleep` first on PATH, so no test needs macOS, CPack or a real pause.
The fake cpack answers each call from a list of outcomes: "ok" packages,
"busy" fails the way run 44208707794 did (hdiutil's "create failed - Resource
busy" on stderr, exit 5), and "other" fails with a different error (exit 7).
Each scenario runs twice, once with no inherited shell options and once with
SHELLOPTS=errexit:pipefail exported, which bash turns on at startup, so the
script cannot depend on either.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / ".github" / "scripts" / "cpack-retry-busy.sh"
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "generate-builds.yml"

BUSY_EXIT = 5
OTHER_EXIT = 7
INSTALL_LOG_LINE = "fake install output from fixup_bundle"


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
needs_bash = pytest.mark.skipif(BASH is None, reason="no POSIX bash to run the script")

FAKE_CPACK = r"""#!/usr/bin/env bash
n=$(( $(cat "$FAKE_STATE/count" 2>/dev/null || echo 0) + 1 ))
echo "$n" > "$FAKE_STATE/count"
pwd >> "$FAKE_STATE/cwds"
outcome="$(sed -n "${n}p" "$FAKE_STATE/outcomes")"
echo "CPack: Create package using Bundle (fake call $n)"
case "$outcome" in
    ok)
        echo "CPack: - package: fake.dmg generated."
        exit 0 ;;
    busy)
        echo "CPack Error: Error generating temporary disk image."
        echo "hdiutil: create failed - Resource busy" >&2
        exit 5 ;;
    other)
        echo "CPack Error: Problem running install command" >&2
        exit 7 ;;
    *)
        echo "fake cpack: no outcome for call $n" >&2
        exit 99 ;;
esac
"""

FAKE_SLEEP = r"""#!/usr/bin/env bash
echo "$1" >> "$FAKE_STATE/sleeps"
"""


def run_script(tmp_path, outcomes, shellopts):
    fake_bin = tmp_path / "bin"
    fake_bin.mkdir()
    for name, body in (("cpack", FAKE_CPACK), ("sleep", FAKE_SLEEP)):
        tool = fake_bin / name
        tool.write_text(body, encoding="utf-8", newline="\n")
        tool.chmod(0o755)
    state = tmp_path / "state"
    state.mkdir()
    (state / "outcomes").write_text("".join(o + "\n" for o in outcomes), encoding="utf-8", newline="\n")
    install_log = tmp_path / "build" / "_CPack_Packages" / "Darwin" / "Bundle" / "InstallOutput.log"
    install_log.parent.mkdir(parents=True)
    install_log.write_text(INSTALL_LOG_LINE + "\n", encoding="utf-8", newline="\n")

    env = dict(os.environ)
    env.pop("SHELLOPTS", None)
    if shellopts:
        env["SHELLOPTS"] = shellopts
    env["FAKE_BIN"] = str(fake_bin)
    env["FAKE_STATE"] = state.as_posix()
    command = 'export PATH="$(cygpath -u "$FAKE_BIN" 2>/dev/null || echo "$FAKE_BIN"):$PATH"; exec bash "$0" build'
    result = subprocess.run(
        [BASH, "-c", command, SCRIPT.as_posix()],
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
        cwd=tmp_path,
    )

    def lines(name):
        path = state / name
        return path.read_text(encoding="utf-8").split() if path.exists() else []

    calls = int(lines("count")[0]) if lines("count") else 0
    return result, calls, lines("sleeps"), lines("cwds")


def retry_lines(stdout):
    return [line for line in stdout.splitlines() if line.startswith("::warning::cpack try ")]


SHELLOPTS_CASES = pytest.mark.parametrize("shellopts", ["", "errexit:pipefail"], ids=["plain", "errexit-pipefail"])


@needs_bash
@SHELLOPTS_CASES
def test_success_at_once_runs_cpack_once(tmp_path, shellopts):
    result, calls, sleeps, cwds = run_script(tmp_path, ["ok"], shellopts)
    assert result.returncode == 0, result.stdout + result.stderr
    assert calls == 1
    assert sleeps == []
    assert retry_lines(result.stdout) == []
    # cpack runs inside the build directory, and its output reaches the log.
    assert cwds[0].endswith("/build")
    assert "fake.dmg generated" in result.stdout
    assert INSTALL_LOG_LINE not in result.stdout


@needs_bash
@SHELLOPTS_CASES
def test_busy_then_success_retries_once_after_20s(tmp_path, shellopts):
    result, calls, sleeps, _ = run_script(tmp_path, ["busy", "ok"], shellopts)
    assert result.returncode == 0, result.stdout + result.stderr
    assert calls == 2
    assert sleeps == ["20"]
    retries = retry_lines(result.stdout)
    assert len(retries) == 1
    assert "try 2 of 3" in retries[0] and "Resource busy" in retries[0]
    # The failed try's output, stderr included, still streams to the log.
    assert "hdiutil: create failed - Resource busy" in result.stdout
    assert INSTALL_LOG_LINE not in result.stdout


@needs_bash
@SHELLOPTS_CASES
def test_busy_three_times_fails_with_cpack_status(tmp_path, shellopts):
    result, calls, sleeps, _ = run_script(tmp_path, ["busy", "busy", "busy", "ok"], shellopts)
    assert result.returncode == BUSY_EXIT, result.stdout + result.stderr
    assert calls == 3
    assert sleeps == ["20", "40"]
    retries = retry_lines(result.stdout)
    assert len(retries) == 2
    assert "try 2 of 3" in retries[0] and "try 3 of 3" in retries[1]
    assert re.search(r"^::error::cpack try 3 of 3 failed", result.stdout, re.M)
    assert INSTALL_LOG_LINE in result.stdout


@needs_bash
@SHELLOPTS_CASES
def test_other_error_fails_at_once(tmp_path, shellopts):
    result, calls, sleeps, _ = run_script(tmp_path, ["other", "ok"], shellopts)
    assert result.returncode == OTHER_EXIT, result.stdout + result.stderr
    assert calls == 1
    assert sleeps == []
    assert retry_lines(result.stdout) == []
    assert "Problem running install command" in result.stdout
    assert INSTALL_LOG_LINE in result.stdout


@needs_bash
@SHELLOPTS_CASES
def test_busy_then_other_error_stops_at_the_second_try(tmp_path, shellopts):
    result, calls, sleeps, _ = run_script(tmp_path, ["busy", "other", "ok"], shellopts)
    assert result.returncode == OTHER_EXIT, result.stdout + result.stderr
    assert calls == 2
    assert sleeps == ["20"]
    assert INSTALL_LOG_LINE in result.stdout


def test_script_uses_no_bash4_features():
    # macOS runners run steps with /bin/bash 3.2.
    code = "\n".join(
        line for line in SCRIPT.read_text(encoding="utf-8").splitlines() if not line.lstrip().startswith("#")
    )
    for needle in ("declare -A", "local -A", "mapfile", "readarray", "|&", ",,}", "^^}", "coproc", ";;&"):
        assert needle not in code, needle


def test_macos_build_step_packages_through_the_script():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    steps = workflow["jobs"]["build-macos"]["steps"]
    build = [s for s in steps if s.get("name") == "Build redship"]
    assert len(build) == 1
    run = build[0]["run"]
    assert "bash .github/scripts/cpack-retry-busy.sh build-cmake" in run
    # No second, unretried cpack call left in the step.
    assert not re.search(r"\bcpack\)|&& cpack\b|^\s*cpack\b", run, re.M)
