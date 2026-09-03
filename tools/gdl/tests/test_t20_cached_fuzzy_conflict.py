"""Run-50 item 2: the gate reads the fuzzy already on disk, and a CONFLICT
stops calling itself a FAILURE.

THE OBSERVATION, from the run-50 tool queue: a keep that
claim.law.EN-equal-count-opcode-respell-must-be-arbitrated-on-fuzzy-not-real
.20260901.v1 makes CORRECT printed `GATE FAILED: 1 regression(s)`.  The
fuzzy arbiter existed but only behind `--arbiter fuzzy`, which costs a
report build, so lanes did not pass it -- while the deciding number was
already sitting in build/GUNE5D/report.json.

TWO-SIDED CALIBRATION of the freshness guard, measured at run-50 HEAD
(scratch scripts t20_fresh_census.py / t20_stale_sim.py):

  POSITIVE  after a full `ninja`, all 310 built objects are older than
            report.json and 256 of them get a per-function fuzzy from it
            (the other 54 units carry no `functions` entry).
  NEGATIVE  move ONE object's mtime past the report -- exactly the state
            `check --rebuild` produces, since it rebuilds the object and
            not the report -- and the guard returns 0 rows, so the row
            keeps the `--arbiter fuzzy` advice instead of quoting a stale
            number.  Measured: memcard 30 rows -> 0 rows -> 30 rows across
            the bump and its exact restore.

That negative is why the cached read is GUARDED rather than unconditional:
the most common gate invocation in this project is the one where a cached
fuzzy would be wrong.
"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import defake_gate                                             # noqa: E402
from defake_gate import (arbitrate_regressions, cached_report_fuzzy,
                         conflict_summary)                     # noqa: E402


def differing_ops(_unit, _name):
    """A DIFFERING multiset: the naming-churn CONFLICT route is off, so the
    fuzzy arbiter is the only thing that can move this row."""
    return "  opcode multiset: DIFFERS  target-only: +1 b  ours-only: -1 beq"


class CachedFuzzyArbiter(unittest.TestCase):
    VERDICTS = [("get_vmu_directory", "REGRESSION", "real 48 -> 65")]

    def arb(self, baseline, genuine_now, cached=None):
        return arbitrate_regressions(
            list(self.VERDICTS), "game/sys/memcard", baseline,
            genuine_fn=lambda unit, names: genuine_now,
            ops_fn=differing_ops,
            cached_fuzzy_fn=(None if cached is None
                             else (lambda unit, names: cached)))

    def test_cached_rising_fuzzy_is_a_CONFLICT_without_arbiter_fuzzy(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 92.72})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("fuzzy 90.0400 -> 92.7200", out[0][2])
        self.assertIn("CACHED report.json", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_cached_falling_fuzzy_stays_a_REGRESSION_and_says_they_agree(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 92.72}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 90.04})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("-2.6800", out[0][2])
        self.assertIn("real and fuzzy AGREE", out[0][2])

    def test_no_cached_reader_keeps_the_old_unmeasured_advice(self):
        # The IMPORTABLE-CORE default: arbitrate_regressions performs no
        # file read of its own, so with nothing injected the row is exactly
        # what it was before this item.
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("fuzzy delta UNMEASURED", out[0][2])
        self.assertIn("--arbiter fuzzy", out[0][2])

    def test_an_empty_cached_read_is_not_a_number(self):
        # A stale report yields {} -- that must read as "no number", never
        # as a fuzzy of 0.
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5}, {})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("fuzzy delta UNMEASURED", out[0][2])

    def test_cached_fuzzy_never_rescues_a_byte_exact_function(self):
        out = arbitrate_regressions(
            [("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
            {"f": {"genuine": 5, "fuzzy": 100.0}},
            genuine_fn=lambda unit, names: {"f": 5},
            ops_fn=differing_ops,
            cached_fuzzy_fn=lambda unit, names: {"f": 100.0})
        self.assertEqual(out[0][1], "REGRESSION")


class FreshnessGuard(unittest.TestCase):
    """The guard is the soundness argument: no stale fuzzy, ever."""

    REPORT = {"units": [{"name": "main/game/sys/memcard",
                         "functions": [{"name": "get_vmu_directory",
                                        "fuzzy_match_percent": 92.72}]}]}

    def setUp(self):
        self.dir = Path(tempfile.mkdtemp(prefix="t20_fresh_"))
        self.report = self.dir / "report.json"
        self.report.write_text(json.dumps(self.REPORT), encoding="utf-8")
        self.obj = self.dir / "build" / "GUNE5D" / "src" / "game" / "sys"
        self.obj.mkdir(parents=True)
        self.obj = self.obj / "memcard.o"
        self.obj.write_bytes(b"\x00")
        self.cwd = os.getcwd()
        os.chdir(self.dir)
        self._report = defake_gate.REPORT
        defake_gate.REPORT = self.report

    def tearDown(self):
        defake_gate.REPORT = self._report
        os.chdir(self.cwd)

    def test_report_newer_than_the_object_serves_the_cached_fuzzy(self):
        os.utime(self.obj, (0, 1000))
        os.utime(self.report, (0, 2000))
        self.assertEqual(cached_report_fuzzy("game/sys/memcard"),
                         {"get_vmu_directory": 92.72})

    def test_report_older_than_the_object_serves_nothing(self):
        # This IS the `check --rebuild` state: the object was just rebuilt
        # and the report was not.
        os.utime(self.obj, (0, 3000))
        os.utime(self.report, (0, 2000))
        self.assertEqual(cached_report_fuzzy("game/sys/memcard"), {})

    def test_a_missing_report_serves_nothing(self):
        self.report.unlink()
        self.assertEqual(cached_report_fuzzy("game/sys/memcard"), {})


class ConflictSummaryWording(unittest.TestCase):

    def test_a_conflict_only_result_is_not_announced_as_a_failure(self):
        line = conflict_summary(1)
        self.assertTrue(line.startswith("GATE CONFLICT:"))
        self.assertNotIn("GATE FAILED", line)
        self.assertIn("DISAGREE", line)
        # The exit code did NOT change; the line must say so, because a
        # reader who sees a softer word must not think the gate passed.
        self.assertIn("Exit is still 1", line)
        self.assertIn("--arbitrate", line)


if __name__ == "__main__":
    unittest.main()
