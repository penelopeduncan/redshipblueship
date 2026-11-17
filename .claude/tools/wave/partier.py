#!/usr/bin/env python3
"""Serial against parallel: run a built tree's rando tier at -j 1 and at -j N and
require the two runs to agree (#1692).

    partier.cmd <tag> [--build-dir=<NAME>] [-j <N>] [--main=<DIR>] [--state-dir=<DIR>]

<tag> must hold the gate lock (gate-lock.cmd tag=<tag>), and the tree must
already be built (this builds nothing; it needs the one build slot all the
same, since the tier is CPU-bound). N is 2 by default, 2 to 4 allowed
(WAVE_RANDO_JOBS's limit). Steps:

  (a) preconditions: the staged config selects OpenGL; the manifest does not
      say homes are disabled; `ctest --show-only=json-v1 -L ^rando$` lists every
      row but RowHomesReset with RSBS_TEST_HOME and RSBS_TEST_HOME_FROM in its
      ENVIRONMENT and rsbs_row_homes in FIXTURES_REQUIRED, homes distinct after
      case-folding;
  (b) a snapshot of the build directory's shared files (CheckSharedDirUntouched);
  (c) the serial run, its `compare` outputs copied to <build>/partier/serial
      (CheckRowOutputs; optional ROM-mounted files required when oot.o2r and
      mm.o2r are staged), and the shared files verified unchanged;
  (d) the same at -j N into <build>/partier/par;
  (e) exit 0 only if every row passed in both runs, the per-row results agree,
      every compare file is byte-identical with CR stripped, and neither run
      touched the shared files.

It prints both wall times, the speed-up, the rows above 60% of their TIMEOUT in
the -j N run (CheckRowTimes) and the tail: the stretch at the end of the -j N
run in which fewer than N rows were running, with the rows running in it. Exit
0 = agree, 1 = disagree or a red row, 2 = usage or lock error. The full log is
<state>/logs/partier-<tag>.log.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import wavelib as w  # noqa: E402
import verify as v  # noqa: E402

RESET_ROW = "RowHomesReset"
WARN_PCT = 60


# ---------------------------------------------------------------------------
# Pure helpers (tests/test_partier.py)
# ---------------------------------------------------------------------------


def parse_show_only(text: str) -> list:
    """Rows of `ctest --show-only=json-v1`, in listing order: dicts with name,
    labels, environment, fixtures_required and timeout."""
    data = json.loads(text)
    rows = []
    for test in data.get("tests", []):
        row = {"name": test.get("name", ""), "labels": [], "environment": [], "fixtures_required": [], "timeout": 0.0}
        for prop in test.get("properties", []) or []:
            name, value = prop.get("name"), prop.get("value")
            values = value if isinstance(value, list) else [value]
            if name == "LABELS":
                row["labels"] = [str(x) for x in values]
            elif name == "ENVIRONMENT":
                row["environment"] = [str(x) for x in values]
            elif name == "FIXTURES_REQUIRED":
                row["fixtures_required"] = [str(x) for x in values]
            elif name == "TIMEOUT":
                try:
                    row["timeout"] = float(value)
                except (TypeError, ValueError):
                    pass
        rows.append(row)
    return rows


def env_value(row: dict, var: str) -> str | None:
    for entry in row["environment"]:
        if entry.startswith(var + "="):
            return entry[len(var) + 1:]
    return None


def check_homes(rows: list) -> list:
    """Problems with the rando rows' registered homes (the same rule as
    CMake/CheckTestRowHome.cmake)."""
    problems = []
    seen: dict = {}
    rando = [r for r in rows if "rando" in r["labels"] and r["name"] != RESET_ROW]
    if len(rando) < 100:
        problems.append(f"only {len(rando)} rando rows listed; the listing looks vacuous")
    if not any(r["fixtures_required"] for r in rows):
        problems.append("no listed row carries FIXTURES_REQUIRED; this ctest's json-v1 leaves it out")
    for row in rando:
        home = env_value(row, "RSBS_TEST_HOME")
        if not home:
            problems.append(f"{row['name']}: no RSBS_TEST_HOME")
            continue
        if not env_value(row, "RSBS_TEST_HOME_FROM"):
            problems.append(f"{row['name']}: no RSBS_TEST_HOME_FROM")
        if "rsbs_row_homes" not in row["fixtures_required"]:
            problems.append(f"{row['name']}: no rsbs_row_homes fixture")
        key = home.replace("\\", "/").casefold()
        if key in seen:
            problems.append(f"{row['name']}: home {home} is {seen[key]}'s too, after case-folding")
        else:
            seen[key] = row["name"]
    return problems


_CASE = re.compile(r"<testcase\b([^>]*)>")
_ATTR = re.compile(r'\b(name|status|time)="([^"]*)"')


def parse_junit(text: str) -> list:
    """(name, status, seconds) per <testcase>, in file order."""
    out = []
    for m in _CASE.finditer(text):
        attrs = dict(_ATTR.findall(m.group(1)))
        try:
            seconds = float(attrs.get("time", "0") or 0)
        except ValueError:
            seconds = 0.0
        out.append((attrs.get("name", ""), attrs.get("status", ""), seconds))
    return out


def compare_dirs(a: Path, b: Path) -> list:
    """Differences between two flat folders of text files, CR stripped."""
    diffs = []
    names_a = {p.name for p in a.iterdir() if p.is_file()} if a.is_dir() else set()
    names_b = {p.name for p in b.iterdir() if p.is_file()} if b.is_dir() else set()
    for name in sorted(names_a ^ names_b):
        diffs.append(f"{name}: only in {'serial' if name in names_a else 'parallel'}")
    for name in sorted(names_a & names_b):
        if (a / name).read_bytes().replace(b"\r", b"") != (b / name).read_bytes().replace(b"\r", b""):
            diffs.append(f"{name}: differs")
    if not names_a and not names_b:
        diffs.append("no compare files were collected in either run")
    return diffs


_START = re.compile(r"^\s*Start\s+\d+:\s+(\S+)")
_END = re.compile(r"^\s*\d+/\d+\s+Test\s+#\d+:\s+(\S+)\s")


def tail_report(events: list, jobs: int, end: float) -> tuple[float, list]:
    """From (time, ctest line) events: how long the run's end ran fewer than
    @p jobs rows after the last row started, and which rows ran then."""
    last_start = None
    for i, (_, line) in enumerate(events):
        if _START.match(line):
            last_start = i
    if last_start is None:
        return 0.0, []
    running: dict = {}
    tail_from = None
    in_tail: list = []
    for i, (t, line) in enumerate(events):
        m = _START.match(line)
        if m:
            running[m.group(1)] = t
            continue
        m = _END.match(line)
        if m:
            running.pop(m.group(1), None)
            if tail_from is None and i > last_start and len(running) < jobs:
                tail_from = t
                in_tail = sorted(running)
    if tail_from is None:
        return 0.0, []
    return max(0.0, end - tail_from), in_tail


# ---------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------


class Partier(v.Verify):
    # Verify's plumbing (lock, heartbeat, toolchain, run_cmd), WITHOUT its
    # constructor: that opens and rotates verify-<tag>.log, which is the gate's.
    def __init__(self, tag: str, jobs: int, opts: dict):  # noqa: super-init-not-called
        self.branch = "main"
        self.tag = tag
        self.flags = set()
        self.dry = False
        self.keep_going = False
        self.main = w.main_checkout(opts.get("main"))
        self.state = w.state_dir(opts.get("state-dir"), self.main)
        self.build_name = opts.get("build-dir") or "build-cmake"
        self.build = self.main / self.build_name
        self.allow_skips = False
        self.log = v.Log(w.logs_path(self.state) / f"partier-{tag}.log")
        self.env = dict(os.environ)
        self.env[v.NO_FOCUS_VAR] = "1"
        self.failures: list = []
        self.warnings: list = []
        self.results: list = []
        self.heartbeat = None
        self.touched_checkout = False
        self.tree = None
        self.head = None
        self.under_load = False
        self.started = time.time()
        self.jobs = jobs
        self.out = self.build / "partier"

    def timed_cmd(self, cmd: list) -> tuple[int | None, list, float]:
        """run_cmd with a timestamp per line, for the tail report."""
        events: list = []
        started = time.monotonic()
        self.log.step("$ " + " ".join(str(c) for c in cmd))
        proc = subprocess.Popen([str(c) for c in cmd], cwd=str(self.main), env=self.env, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                                errors="replace")

        def pump():
            for raw in proc.stdout:
                line = raw.rstrip("\r\n")
                events.append((time.monotonic() - started, line))
                self.log.raw(line)

        reader = threading.Thread(target=pump, daemon=True)
        reader.start()
        try:
            rc = proc.wait(timeout=v._env_minutes("WAVE_TIER_TIMEOUT_MIN", 90) * 60.0)
        except subprocess.TimeoutExpired:
            v.kill_tree(proc.pid)
            rc = None
        reader.join(timeout=10)
        return rc, events, time.monotonic() - started

    def cmake_script(self, script: str, *defs: str) -> int | None:
        rc, _ = self.run_cmd(["cmake", *defs, "-P", self.main / "CMake" / script], 30)
        return rc

    def shared(self, mode: str) -> None:
        rc = self.cmake_script("CheckSharedDirUntouched.cmake", f"-DMODE={mode}", f"-DBUILD_DIR={self.build}",
                               f"-DSTATE={self.out / 'shared-dir.txt'}")
        if rc != 0:
            self.failures.append(f"shared directory check ({mode}) exited {rc}")

    def one_run(self, jobs: int, name: str) -> tuple[list, float, list]:
        junit = self.out / f"partier-{name}.xml"
        rc, events, seconds = self.timed_cmd(
            ["ctest", "--test-dir", self.build, "-L", "^rando$", "-j", str(jobs), "--output-on-failure",
             "--no-tests=error", "--output-junit", junit])
        self.log.step(f"partier: {name} run (-j {jobs}) exited {rc} in {seconds:.0f} s")
        if rc != 0:
            self.failures.append(f"{name} run (-j {jobs}) exited {rc}")
        rows = parse_junit(junit.read_text(encoding="utf-8", errors="replace")) if junit.is_file() else []
        if not rows:
            self.failures.append(f"{name} run wrote no junit rows ({junit})")
        rom = all((self.build / n).is_file() for n in ("oot.o2r", "mm.o2r"))
        rc = self.cmake_script("CheckRowOutputs.cmake", f"-DMANIFEST={self.build / 'redship-test-manifest.txt'}",
                               f"-DBUILD_DIR={self.build}", "-DGROUP=compare", f"-DCOPY_TO={self.out / name}",
                               f"-DREQUIRE_OPTIONAL={'ON' if rom else 'OFF'}")
        if rc != 0:
            self.failures.append(f"{name} run: CheckRowOutputs exited {rc} (a compare output is missing or empty)")
        self.shared("verify")
        return rows, seconds, events

    def run(self) -> int:
        self.log.step(f"partier tag={self.tag} build={self.build} -j {self.jobs} pid={os.getpid()}")
        try:
            self.check_lock()
        except v.Usage as exc:
            self.log.step(f"FAIL lock: {exc}")
            self.log.close()
            return 2
        try:
            self.start_heartbeat()
            self.toolchain()
            if not self.build.is_dir():
                raise v.VerifyFail(f"precondition: {self.build} does not exist")
            gl = v.check_gl_backend(self.build, self.build_name)
            if gl:
                raise v.VerifyFail(gl)
            manifest = (self.build / "redship-test-manifest.txt").read_text(encoding="utf-8", errors="replace")
            if "\nhomes|disabled|" in manifest:
                raise v.VerifyFail("precondition: homes are disabled in this (multi-config) tree; -j > 1 is refused")
            proc = subprocess.run(["ctest", "--test-dir", str(self.build), "--show-only=json-v1", "-L", "^rando$"],
                                  capture_output=True, text=True, env=self.env, errors="replace")
            if proc.returncode != 0:
                raise v.VerifyFail(f"precondition: ctest --show-only=json-v1 exited {proc.returncode}")
            listed = parse_show_only(proc.stdout)
            problems = check_homes(listed)
            if problems:
                raise v.VerifyFail("precondition: " + "; ".join(problems[:10]))
            order = [r["name"] for r in listed]
            if self.out.exists():
                shutil.rmtree(self.out)
            self.out.mkdir(parents=True)
            self.shared("snapshot")
            serial, t1, _ = self.one_run(1, "serial")
            par, tn, events = self.one_run(self.jobs, "par")

            s_map = {n: st for n, st, _ in serial}
            p_map = {n: st for n, st, _ in par}
            red = sorted(n for n, st in list(s_map.items()) + list(p_map.items()) if st != "run")
            if red:
                self.failures.append(f"rows not passed: {', '.join(sorted(set(red)))}")
            if s_map != p_map:
                only = sorted(set(s_map) ^ set(p_map))
                moved = sorted(n for n in set(s_map) & set(p_map) if s_map[n] != p_map[n])
                self.failures.append(f"per-row results differ: only in one run {only}, status moved {moved}")
            serial_order = [n for n, _, _ in serial]
            if serial_order != [n for n in order if n in s_map]:
                # Not a failure: CTest's serial order follows COST (RSBS_TEST_COST=ON) and the
                # cost data of earlier runs (Testing/Temporary/CTestCostData.txt) as well.
                self.warn("the serial run's row order is not the registration order (COST or CTestCostData.txt)")
            for diff in compare_dirs(self.out / "serial", self.out / "par"):
                self.failures.append(f"compare: {diff}")
            junit = self.out / "partier-par.xml"
            self.cmake_script("CheckRowTimes.cmake", f"-DJUNIT={junit}", "-DCTEST_EXE=ctest",
                              f"-DBUILD_DIR={self.build}", "-DLABEL=^rando$", f"-DWARN_PCT={WARN_PCT}")
            tail, tail_rows = tail_report(events, self.jobs, tn)
            speed = (t1 / tn) if tn > 0 else 0.0
            self.log.step(f"partier: serial {t1:.0f} s, -j {self.jobs} {tn:.0f} s, speed-up {speed:.2f}x")
            self.log.step(f"partier: tail {tail:.0f} s with fewer than {self.jobs} rows running: "
                          f"{', '.join(tail_rows) or '(none)'}")
        except v.VerifyFail as exc:
            self.failures.append(str(exc))
        finally:
            self.stop_heartbeat()
            for msg in self.warnings:
                self.log.step(f"summary WARN {msg}")
            for msg in self.failures:
                self.log.step(f"FAIL {msg}")
            self.log.step(f"PARTIER {'PASS' if not self.failures else 'FAIL'} tag={self.tag}")
            self.log.close()
        return 0 if not self.failures else 1


def main(argv: list | None = None) -> int:
    argv = sys.argv if argv is None else argv
    args = list(argv[1:])
    jobs = 2
    if "-j" in args:
        at = args.index("-j")
        try:
            jobs = int(args[at + 1])
        except (IndexError, ValueError):
            print("FAIL partier: -j needs a number", file=sys.stderr)
            return 2
        del args[at:at + 2]
    if jobs < 2 or jobs > v.RANDO_JOBS_MAX:
        print(f"FAIL partier: -j {jobs} is outside 2 to {v.RANDO_JOBS_MAX}", file=sys.stderr)
        return 2
    pairs, flags, positional = w.split_args(args)
    if len(positional) != 1 or pairs:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    opts = {}
    for name in ("main", "state-dir", "build-dir"):
        value = w.flag_value(flags, name)
        if value is not None:
            opts[name] = value
    for flag in flags:
        if flag.split("=", 1)[0] not in ("--main", "--state-dir", "--build-dir"):
            print(f"FAIL partier: unknown option {flag}", file=sys.stderr)
            return 2
    return Partier(positional[0], jobs, opts).run()


if __name__ == "__main__":
    sys.exit(main())
