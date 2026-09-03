#!/usr/bin/env python3
"""T17 run-47 item 6: the STRICT/EQUIVALENT split is stamped, so a lane's
delta is MEASURED rather than attributed.

Mandatory policy: "Progress reporting always publishes the STRICT/EQUIVALENT
split; never quote the combined matched% alone." The computation existed ONLY
inline inside `configure.py progress` -- reproduced at 4419968c4, a
`defake_gate.py baseline game/anim/atree --at-head` wrote a meta of exactly
four keys (unit, head, source_sha1, taken_at) and no split anywhere. So a lane
reporting its own STRICT delta had to ATTRIBUTE one: read two printed
percentages from two different moments and assert the difference was its work.

The fix is one function (`progress.postprocessor_split`) that both callers
use, stamped into the baseline meta, with `check` printing the delta between
the two stamped ends.

TWO-SIDED, and the second side is the one that matters:

  POSITIVE  a baseline taken now carries `meta["progress"]`, and `check`
            prints `PROGRESS SPLIT since this baseline: STRICT a -> b`.
  NEGATIVE  a baseline taken BEFORE this change carries no split, and the
            delta must be ABSENT, never rendered as +0.00 -- "no movement"
            and "nothing to compare against" are the two readings this whole
            item exists to separate. Verified live on atree's pre-run-47
            baseline: check printed the explicit "would be ATTRIBUTED, not
            measured" note and no numbers.
  NEGATIVE  the second-copy hazard: `progress.py --split` and
            `configure.py progress` must agree. Measured at 4419968c4, both
            print STRICT 56.91% (2588 fns) + EQUIVALENT 10.99% (151 fns).
"""

import json
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import defake_gate  # noqa: E402
import progress  # noqa: E402

SPLIT_A = {"strict_percent": 56.45, "equivalent_percent": 9.38,
           "strict_functions": 2574, "equivalent_functions": 140,
           "strict_bytes": 1, "equivalent_bytes": 1, "total_bytes": 2}
SPLIT_B = {"strict_percent": 56.91, "equivalent_percent": 10.99,
           "strict_functions": 2588, "equivalent_functions": 151,
           "strict_bytes": 1, "equivalent_bytes": 1, "total_bytes": 2}


class Computation(unittest.TestCase):
    def test_pins_are_read_from_the_units_key_not_the_root(self):
        """A parser iterating the ROOT finds 0 pins, which reads exactly like
        'no pins exist' and reports every matched function as STRICT."""
        report = {"units": [{"name": "main/game/x/y.c", "functions": [
            {"name": "a", "size": 100, "fuzzy_match_percent": 100.0},
            {"name": "b", "size": 100, "fuzzy_match_percent": 100.0},
            {"name": "c", "size": 200, "fuzzy_match_percent": 50.0}]}]}
        rules = {"version": 1, "units": {
            "game/x/y.c": [{"function": "b", "rule": "..."}]}}
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            rp = Path(tmp) / "report.json"
            kp = Path(tmp) / "webfrank.json"
            rp.write_text(json.dumps(report), encoding="utf-8")
            kp.write_text(json.dumps(rules), encoding="utf-8")
            split = progress.postprocessor_split(rp, kp)
        self.assertEqual(split["total_bytes"], 400)
        self.assertEqual(split["strict_functions"], 1)
        self.assertEqual(split["equivalent_functions"], 1)
        self.assertAlmostEqual(split["strict_percent"], 25.0)
        self.assertAlmostEqual(split["equivalent_percent"], 25.0)

    def test_an_empty_report_does_not_divide_by_zero(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            rp = Path(tmp) / "report.json"
            rp.write_text(json.dumps({"units": []}), encoding="utf-8")
            split = progress.postprocessor_split(rp, Path(tmp) / "none.json")
        self.assertEqual(split["strict_percent"], 0.0)
        self.assertEqual(split["total_bytes"], 0)


class Delta(unittest.TestCase):
    def test_a_measured_delta_names_both_ends(self):
        line = defake_gate.progress_delta_line(SPLIT_A, SPLIT_B)
        self.assertIn("56.45% -> 56.91%", line)
        self.assertIn("+0.46", line)
        self.assertIn("+14 fns", line)
        self.assertIn("9.38% -> 10.99%", line)

    def test_a_regression_shows_its_sign(self):
        line = defake_gate.progress_delta_line(SPLIT_B, SPLIT_A)
        self.assertIn("-0.46", line)
        self.assertIn("-14 fns", line)

    def test_a_missing_end_is_ABSENT_never_zero(self):
        """The whole point: 'nothing to compare against' must not render as
        '+0.00'."""
        self.assertIsNone(defake_gate.progress_delta_line(None, SPLIT_B))
        self.assertIsNone(defake_gate.progress_delta_line(SPLIT_A, None))
        self.assertIsNone(defake_gate.progress_delta_line({}, SPLIT_B))

    def test_the_line_says_the_number_is_image_wide(self):
        line = defake_gate.progress_delta_line(SPLIT_A, SPLIT_B)
        self.assertIn("IMAGE-WIDE", line)
        self.assertIn("not attributed", line)


class Wiring(unittest.TestCase):
    def test_the_baseline_meta_stamps_the_split(self):
        text = (REPO / "tools" / "gdl" / "defake_gate.py").read_text(
            encoding="utf-8")
        self.assertIn('meta["progress"] = split', text)

    def test_check_prints_the_absence_when_the_baseline_predates_it(self):
        text = (REPO / "tools" / "gdl" / "defake_gate.py").read_text(
            encoding="utf-8")
        self.assertIn("no PROGRESS SPLIT stamped in this baseline", text)
        self.assertIn("ATTRIBUTED, not measured", text)

    def test_there_is_ONE_implementation(self):
        """defake_gate delegates; a second copy is how two lanes end up
        quoting two numbers for one discriminator."""
        text = (REPO / "tools" / "gdl" / "defake_gate.py").read_text(
            encoding="utf-8")
        self.assertIn("progress.postprocessor_split()", text)
        # The PIN table -- the input that decides STRICT vs EQUIVALENT -- is
        # read in exactly one place. (defake_gate does read
        # `fuzzy_match_percent` for its own fuzzy anchor; that is a different
        # measurement and not a second copy of this one.)
        self.assertNotIn('.get("units", {})', text)


class LiveAgreement(unittest.TestCase):
    def test_progress_split_agrees_with_configure_py(self):
        """The second-copy hazard, measured rather than asserted."""
        if not (REPO / "build" / "GUNE5D" / "report.json").exists():
            self.skipTest("no report.json in this tree")
        mine = progress.postprocessor_split()
        done = subprocess.run([sys.executable, "configure.py", "progress"],
                              cwd=str(REPO), capture_output=True, text=True)
        line = next((row for row in done.stdout.splitlines()
                     if "Postprocessor split" in row), None)
        if line is None:
            self.skipTest("configure.py progress printed no split line")
        self.assertIn(f"STRICT matched {mine['strict_percent']:.2f}%", line)
        self.assertIn(f"({mine['strict_functions']} fns", line)
        self.assertIn(f"EQUIVALENT {mine['equivalent_percent']:.2f}%", line)
        self.assertIn(f"({mine['equivalent_functions']} fns", line)


if __name__ == "__main__":
    unittest.main()
