#!/usr/bin/env python3
"""T16 run-46 item 6: the branch-pair census's profitability column.

Two-sided by construction: profit == 0 is tested next to both signs of
nonzero, and the live calibration in the census docstring reports the
positives AND the negatives (50% of profit-0 rows carry a branch-only opcode
multiset delta against 3% of the 31 nonzero rows). The inherited premise
that the column predicts EXACT CLOSURE did not survive that measurement --
the eight profit-0 rows carry `real` 45..627 -- so the tool advertises a
ranking, and this test pins that wording so it cannot quietly become a
promise again.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import nm_branchpair_census as bp  # noqa: E402


class Profitability(unittest.TestCase):
    def test_pairs_that_explain_the_whole_count_residual(self):
        # one missing pair, one extra target instruction
        self.assertEqual(bp.profitability(1, 0, 1), (0, "WHOLE-RESIDUAL"))
        # five missing pairs, five extra instructions (gauntworld fn_8005BA1C)
        self.assertEqual(bp.profitability(6, 1, 5), (0, "WHOLE-RESIDUAL"))

    def test_something_else_is_also_missing(self):
        profit, verdict = bp.profitability(1, 0, 3)
        self.assertEqual(profit, 2)
        self.assertEqual(verdict, "partial(+2)")

    def test_our_function_is_longer_than_the_pairs_explain(self):
        profit, verdict = bp.profitability(2, 0, 0)
        self.assertEqual(profit, -2)
        self.assertEqual(verdict, "overshoot(-2)")

    def test_a_negative_insn_delta_is_still_classified(self):
        # init_enemy_vars in the live queue: target is SHORTER than ours
        profit, verdict = bp.profitability(1, 0, -1)
        self.assertEqual((profit, verdict), (-2, "overshoot(-2)"))

    def test_equal_sites_and_equal_counts_are_neutral(self):
        self.assertEqual(bp.profitability(3, 3, 0), (0, "WHOLE-RESIDUAL"))


class CountsSideFile(unittest.TestCase):
    def test_missing_or_corrupt_counts_degrade_to_empty(self):
        self.assertEqual(bp.load_counts("no/such/file.json"), {})

    def test_a_real_counts_file_round_trips(self):
        import json
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "c.json"
            path.write_text(json.dumps({"game/a\tfn": 12}), encoding="ascii")
            self.assertEqual(bp.load_counts(path), {"game/a\tfn": 12})

    def test_a_non_dict_payload_is_rejected(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "c.json"
            path.write_text("[1, 2, 3]", encoding="ascii")
            self.assertEqual(bp.load_counts(path), {})


class HonestWording(unittest.TestCase):
    def test_the_tool_does_not_promise_exact_closure(self):
        text = (REPO / "tools" / "gdl" / "composed_census"
                / "nm_branchpair_census.py").read_text(encoding="utf-8")
        self.assertIn("RANKING, not a promise of exact closure", text)
        self.assertIn("Rank on it; do not promise on it.", text)

    def test_the_calibration_reports_both_sides(self):
        text = (REPO / "tools" / "gdl" / "composed_census"
                / "nm_branchpair_census.py").read_text(encoding="utf-8")
        for fragment in ("profit == 0   n=8", "partial(+N)   n=9",
                         "overshoot(-N) n=22"):
            self.assertIn(fragment, text)


if __name__ == "__main__":
    unittest.main()
