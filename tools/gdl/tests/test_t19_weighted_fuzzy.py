#!/usr/bin/env python3
"""T19 run-49 item 1: the project-weighted fuzzy delta, and the arbiter's place.

TWO observations, one commit.

(1) probe prints a function-LOCAL fuzzy delta and nothing else, so a local
    -1.10 was read as a project-level loss when the image effect was ~0.001
    and a full ninja plus `configure.py progress` was spent settling it (NC,
    run 48). The projection is arithmetic, not a model: measured at
    c8a28c3bb against build/GUNE5D/report.json,

        sum(size * fuzzy) / sum(size) = 98.53066553650227
        measures.fuzzy_match_percent  = 98.53067
        sum(size) = 1,071,824         = measures.total_code

    so the project figure IS the size-weighted mean of every function's
    fuzzy and a local delta scales by size/total_code exactly.

    TWO-SIDED CALIBRATION at c8a28c3bb over all 2,990 sized functions,
    asking where a 1.00-point LOCAL delta lands on the project figure:

        >= 0.005  (the digit `configure.py progress` prints)   10  (0.3%)
        >= 0.0005 (the 4th decimal of the report figure)      469  (15.7%)
        below both                                          2,521

    Over the 246 functions that are not already 100% -- the only ones a
    probe ever runs on -- it is 6 / 152 / 94. Median function size is 164 B
    against a 1,071,824 B image, so the median local number overstates its
    image effect by about 6,500x. The clause is therefore an ANNOTATION, not
    a refusal: it never suppresses the local number (which remains the right
    arbiter for the function) and never gates a bank.

(2) The IMMEDIATE-row arbiter printed BELOW the whole verdict block. A
    REGRESSED verdict is three printed lines at c8a28c3bb -- the
    `[revert advised]` headline, `baseline_clause`'s SESSION BASELINE line,
    and the RE-RUN THIS NEGATIVE paragraph -- so the arbiter landed on line
    4, three lines under the advice it exists to overrule, which is what WF
    read on the probe that closed a literal.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402

REPORT = {
    "measures": {"fuzzy_match_percent": 98.53067, "total_code": "1071824"},
    "units": [
        {"name": "main/game/world/camera",
         "functions": [
             {"name": "camera_mode_dest", "size": "974",
              "fuzzy_match_percent": 97.5645},
             {"name": "CameraCollide", "size": "96",
              "fuzzy_match_percent": 100.0},
         ]},
    ],
}


class Weight(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "report.json"
        self.path.write_text(json.dumps(REPORT), encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def test_size_and_total_come_from_the_report(self):
        self.assertEqual(
            probe.function_weight("game/world/camera", "camera_mode_dest",
                                  "camera_mode_dest", self.path),
            (974, 1071824))

    def test_a_dot_c_spelling_resolves_the_same_unit(self):
        self.assertEqual(
            probe.function_weight("game/world/camera.c", "CameraCollide",
                                  "CameraCollide", self.path),
            (96, 1071824))

    def test_a_missing_report_is_None_not_a_guess(self):
        self.assertEqual(
            probe.function_weight("game/world/camera", "camera_mode_dest",
                                  "camera_mode_dest",
                                  Path(self.tmp.name) / "nope.json"),
            (None, None))

    def test_a_function_absent_from_the_report_has_no_size(self):
        size, total = probe.function_weight(
            "game/world/camera", "no_such_fn", "no_such_fn", self.path)
        self.assertIsNone(size)
        self.assertEqual(total, 1071824)


class Projection(unittest.TestCase):
    def test_NCs_case_a_local_1_point_10_is_about_0_001_of_the_image(self):
        got = probe.project_weighted_delta(-1.10, 974, 1071824)
        self.assertAlmostEqual(got, -0.001, places=4)

    def test_no_denominator_projects_nothing(self):
        self.assertIsNone(probe.project_weighted_delta(-1.10, None, 1071824))
        self.assertIsNone(probe.project_weighted_delta(-1.10, 974, None))
        self.assertIsNone(probe.project_weighted_delta(None, 974, 1071824))

    def test_the_clause_names_both_numbers_and_the_share(self):
        text = probe.weighted_fuzzy_clause(-1.10, 974, 1071824)
        self.assertIn("PROJECT-WEIGHTED", text)
        self.assertIn("-0.0010", text)
        self.assertIn("974 B of 1,071,824", text)

    def test_the_clause_is_EMPTY_not_a_guess_when_unweighable(self):
        self.assertEqual(probe.weighted_fuzzy_clause(-1.10, None, None), "")

    def test_a_large_function_keeps_a_visible_share(self):
        """PlayerMotion, 19,100 B: 1.78% of the image, the one function
        where a local point really is a project point-and-a-half."""
        text = probe.weighted_fuzzy_clause(1.00, 19100, 1071824)
        self.assertIn("+0.0178", text)
        self.assertIn("1.782% of the image", text)


class WiredIntoEveryFuzzyDelta(unittest.TestCase):
    def test_the_cached_anchor_note_carries_the_weighted_twin(self):
        note = probe.fuzzy_anchor_note(92.72, 90.04, 974, 1071824)
        self.assertIn("-2.6800", note)
        self.assertIn("PROJECT-WEIGHTED", note)

    def test_the_anchor_note_is_unchanged_without_a_weight(self):
        """The weight is optional everywhere: an old state file has none."""
        note = probe.fuzzy_anchor_note(92.72, 90.04)
        self.assertIn("-2.6800", note)
        self.assertNotIn("PROJECT-WEIGHTED", note)

    def test_the_arbitration_table_delta_row_carries_it(self):
        text = probe.arbitrate_table("rolling snapshot", 30, 80.85, 24, 71.89,
                                     size=974, total_code=1071824)
        self.assertIn("PROJECT-WEIGHTED", text)
        self.assertIn("-0.0081", text)

    def test_the_fuzzy_headline_carries_it_when_a_prev_exists(self):
        real, weight = probe.report_fuzzy, probe.function_weight
        probe.report_fuzzy = lambda *a: 96.4645
        probe.function_weight = lambda *a, **k: (974, 1071824)
        try:
            headline, _ = probe.measure_fuzzy(
                "game/world/camera", "camera_mode_dest", "camera_mode_dest",
                {"last_fuzzy": 97.5645})
        finally:
            probe.report_fuzzy, probe.function_weight = real, weight
        self.assertIn("-1.1000 local", headline)
        self.assertIn("PROJECT-WEIGHTED", headline)

    def test_a_first_fuzzy_readout_has_no_delta_and_so_no_clause(self):
        real, weight = probe.report_fuzzy, probe.function_weight
        probe.report_fuzzy = lambda *a: 96.4645
        probe.function_weight = lambda *a, **k: (974, 1071824)
        try:
            headline, _ = probe.measure_fuzzy(
                "game/world/camera", "camera_mode_dest", "camera_mode_dest",
                {})
        finally:
            probe.report_fuzzy, probe.function_weight = real, weight
        self.assertIn("96.4645%", headline)
        self.assertNotIn("PROJECT-WEIGHTED", headline)

    def test_measure_fuzzy_banks_the_weight_for_a_later_buildless_note(self):
        """A CONFLICT prints its anchor note WITHOUT a build, so the
        denominator has to already be in state."""
        real, weight = probe.report_fuzzy, probe.function_weight
        probe.report_fuzzy = lambda *a: 96.4645
        probe.function_weight = lambda *a, **k: (974, 1071824)
        state = {}
        try:
            probe.measure_fuzzy("game/world/camera", "camera_mode_dest",
                                "camera_mode_dest", state)
        finally:
            probe.report_fuzzy, probe.function_weight = real, weight
        self.assertEqual(state["fn_size"], 974)
        self.assertEqual(state["total_code"], 1071824)


class ArbiterPlacement(unittest.TestCase):
    REGRESSED = (
        "REGRESSED vs best 156: real 156 -> 160 (prev -> current; insns"
        " T682/O682, 0t)  [revert advised]\n"
        "[SESSION BASELINE real 156, insns T682/O682 — ...]\n"
        "RE-RUN THIS NEGATIVE FROM THE LAST COMMIT before recording it: ...")

    def test_the_arbiter_lands_on_line_2_not_line_4(self):
        arbiter = probe.immediate_arbiter_line(2, 5, 160, 156)
        out = probe.insert_after_headline(self.REGRESSED, arbiter)
        lines = out.split("\n")
        self.assertIn("[revert advised]", lines[0])
        self.assertIn("IMMEDIATE-ROW ARBITER", lines[1])
        self.assertIn("`real` ROSE while the IMMEDIATE count FELL", lines[1])

    def test_the_trailing_annotations_keep_their_order_below_it(self):
        out = probe.insert_after_headline(
            self.REGRESSED, probe.immediate_arbiter_line(2, 5, 160, 156))
        lines = out.split("\n")
        self.assertIn("SESSION BASELINE", lines[2])
        self.assertIn("RE-RUN THIS NEGATIVE", lines[3])

    def test_no_arbiter_leaves_the_verdict_byte_identical(self):
        self.assertEqual(
            probe.insert_after_headline(self.REGRESSED, None), self.REGRESSED)
        self.assertEqual(
            probe.insert_after_headline(self.REGRESSED, ""), self.REGRESSED)

    def test_a_single_line_verdict_still_gets_the_arbiter_under_it(self):
        one = "BASELINE  real 156 (insns T682/O682, multiset 0t)"
        out = probe.insert_after_headline(one, "ARB")
        self.assertEqual(out, one + "\nARB")


class Wiring(unittest.TestCase):
    SRC = (REPO / "tools" / "gdl" / "probe.py").read_text(encoding="utf-8")

    def test_the_arbiter_is_printed_THROUGH_insert_after_headline(self):
        self.assertIn("print(insert_after_headline(verdict, arbiter))",
                      self.SRC)

    def test_the_arbiter_is_still_gated_on_a_multiset_IDENTICAL_residual(self):
        """run-47 item 5's gate must survive the move."""
        self.assertIn("if multiset_tokens == 0 and real > 0:", self.SRC)

    def test_the_banked_verdict_excludes_the_arbiter(self):
        """`last_verdict` is REPLAYED verbatim by a RE-SCORE; banking the
        arbiter into it would print the line twice."""
        self.assertNotIn('state["last_verdict"] = insert_after_headline',
                         self.SRC)


if __name__ == "__main__":
    unittest.main()
