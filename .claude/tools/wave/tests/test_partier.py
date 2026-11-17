"""partier.py's pure helpers and verify.py's rando job count (#1692)."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from wavetest import load_script

verify = load_script("verify.py")
partier = load_script("partier.py")


def _row(name, labels=("rando",), home=True, from_=True, fixture=True, timeout=300):
    env = ["SDL_AUDIODRIVER=dummy"]
    if home:
        env.insert(0, f"RSBS_TEST_HOME=/b/rsbs-rows/{name}")
    if from_:
        env.insert(0, "RSBS_TEST_HOME_FROM=/b")
    props = [{"name": "LABELS", "value": list(labels)}, {"name": "ENVIRONMENT", "value": env},
             {"name": "TIMEOUT", "value": timeout}]
    if fixture:
        props.append({"name": "FIXTURES_REQUIRED", "value": ["rsbs_row_homes"]})
    return {"name": name, "properties": props}


def _listing(rows):
    return json.dumps({"kind": "ctestInfo", "tests": rows})


class RandoJobsTest(unittest.TestCase):
    def test_default_is_one(self):
        self.assertEqual(verify.rando_jobs({}), 1)
        self.assertEqual(verify.rando_jobs({"WAVE_RANDO_JOBS": ""}), 1)

    def test_one_to_four(self):
        for n in (1, 2, 3, 4):
            self.assertEqual(verify.rando_jobs({"WAVE_RANDO_JOBS": str(n)}), n)

    def test_outside_the_four_thread_rule_is_refused(self):
        for bad in ("0", "5", "-1", "two", "2.5"):
            with self.assertRaises(verify.Usage):
                verify.rando_jobs({"WAVE_RANDO_JOBS": bad})

    def test_only_the_rando_tier_runs_wide(self):
        env = {"WAVE_RANDO_JOBS": "3"}
        self.assertEqual(verify.tier_jobs("rando", env), 3)
        for label in ("redship", "ui", "integration", "integration-soak"):
            self.assertEqual(verify.tier_jobs(label, env), 1)


class HomesTest(unittest.TestCase):
    def rows(self, *extra):
        base = [_row(f"R{i}") for i in range(100)] + [_row("RowHomesReset", home=False, from_=False, fixture=False)]
        return partier.parse_show_only(_listing(base + list(extra)))

    def test_clean_listing_passes(self):
        self.assertEqual(partier.check_homes(self.rows()), [])

    def test_each_missing_piece_is_named(self):
        for row, text in ((_row("NoHome", home=False), "NoHome: no RSBS_TEST_HOME"),
                          (_row("NoFrom", from_=False), "NoFrom: no RSBS_TEST_HOME_FROM"),
                          (_row("NoFix", fixture=False), "NoFix: no rsbs_row_homes")):
            problems = partier.check_homes(self.rows(row))
            self.assertTrue(any(text in p for p in problems), problems)

    def test_case_folded_collision(self):
        dup = _row("r5")
        problems = partier.check_homes(self.rows(dup))
        self.assertTrue(any("r5" in p and "R5" in p for p in problems), problems)

    def test_redship_rows_are_ignored(self):
        self.assertEqual(partier.check_homes(self.rows(_row("Unit", labels=("redship",), home=False))), [])

    def test_vacuous_listing_is_refused(self):
        rows = partier.parse_show_only(_listing([_row("A")]))
        self.assertTrue(any("vacuous" in p for p in partier.check_homes(rows)))


class JunitTest(unittest.TestCase):
    def test_parse(self):
        xml = ('<testsuite><testcase name="A" classname="A" time="1.5" status="run"></testcase>'
               '<testcase name="B" classname="B" time="20" status="fail"/></testsuite>')
        self.assertEqual(partier.parse_junit(xml), [("A", "run", 1.5), ("B", "fail", 20.0)])


class CompareTest(unittest.TestCase):
    def test_cr_is_ignored_and_differences_named(self):
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp) / "a", Path(tmp) / "b"
            a.mkdir()
            b.mkdir()
            (a / "same.txt").write_bytes(b"x\r\ny\r\n")
            (b / "same.txt").write_bytes(b"x\ny\n")
            (a / "moved.txt").write_bytes(b"1\n")
            (b / "moved.txt").write_bytes(b"2\n")
            (a / "lonely.txt").write_bytes(b"z\n")
            diffs = partier.compare_dirs(a, b)
        self.assertEqual(diffs, ["lonely.txt: only in serial", "moved.txt: differs"])

    def test_nothing_collected_is_a_difference(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertTrue(partier.compare_dirs(Path(tmp) / "a", Path(tmp) / "b"))


class TailTest(unittest.TestCase):
    def test_tail_starts_when_a_slot_goes_idle_after_the_last_start(self):
        events = [
            (0.0, "    Start 1: A"),
            (0.0, "    Start 2: B"),
            (10.0, "1/3 Test #1: A ................   Passed   10.00 sec"),
            (10.0, "    Start 3: C"),
            (15.0, "2/3 Test #2: B ................   Passed   15.00 sec"),
            (100.0, "3/3 Test #3: C ................   Passed   90.00 sec"),
        ]
        tail, rows = partier.tail_report(events, 2, 100.0)
        self.assertEqual(tail, 85.0)
        self.assertEqual(rows, ["C"])


class CliTest(unittest.TestCase):
    def test_usage_errors_exit_2(self):
        self.assertEqual(partier.main(["partier.py"]), 2)
        self.assertEqual(partier.main(["partier.py", "K9", "-j", "1"]), 2)
        self.assertEqual(partier.main(["partier.py", "K9", "-j", "5"]), 2)
        self.assertEqual(partier.main(["partier.py", "K9", "--bogus"]), 2)


if __name__ == "__main__":
    unittest.main()
