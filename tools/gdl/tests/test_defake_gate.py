"""defake_gate structure-arbiter and durable-baseline tests.

The gate scores only `real`, and `real` cannot see structure. Closing one
compensating error re-aligns every instruction after it, so a stream that
moved strictly NEARER target can score worse: get_vmu_directory went real
48 -> 65 while its opcode multiset went 4t -> 3t and its fresh fuzzy went
90.04 -> 92.72. The gate called that a REGRESSION and the lane had to
override it by hand.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from defake_gate import (arbitrate_regressions, compare, load_baseline)


def no_ops(_unit, _name):
    """--ops output with a DIFFERING multiset: the naming-churn route off."""
    return "  opcode multiset: DIFFERS  target-only: +1 b  ours-only: -1 beq"


def identical_ops(_unit, _name):
    return "  opcode multiset: IDENTICAL (116/116)"


class StructureArbiterTests(unittest.TestCase):
    VERDICTS = [("get_vmu_directory", "REGRESSION", "real 48 -> 65")]

    def arb(self, baseline, genuine_now, ops_fn=no_ops):
        return arbitrate_regressions(
            list(self.VERDICTS), "game/sys/memcard", baseline,
            genuine_fn=lambda unit, names: genuine_now, ops_fn=ops_fn)

    def test_real_rise_with_genuine_rows_falling_is_a_CONFLICT(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 1})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("genuine structural rows 5 -> 1 FELL", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_real_rise_with_genuine_rows_holding_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("genuine structural rows 5 -> 5", out[0][2])

    def test_real_rise_with_genuine_rows_rising_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 2}},
                       {"get_vmu_directory": 9})
        self.assertEqual(out[0][1], "REGRESSION")

    def test_legacy_baseline_without_genuine_counts_says_it_is_unavailable(self):
        out = self.arb({"get_vmu_directory": {}}, {"get_vmu_directory": 1})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("structure arbiter is UNAVAILABLE", out[0][2])

    def test_identical_multiset_route_still_wins_first(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5}, ops_fn=identical_ops)
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("opcode multiset IDENTICAL", out[0][2])

    def test_a_byte_exact_function_is_never_arbitrated(self):
        out = arbitrate_regressions(
            [("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
            {"f": {"genuine": 9}},
            genuine_fn=lambda unit, names: {"f": 0}, ops_fn=identical_ops)
        self.assertEqual(out[0][1], "REGRESSION")

    def test_improvements_pass_through_untouched(self):
        out = arbitrate_regressions(
            [("f", "IMPROVED", "real 9 -> 4")], "game/sys/memcard", {},
            genuine_fn=lambda unit, names: {}, ops_fn=no_ops)
        self.assertEqual(out, [("f", "IMPROVED", "real 9 -> 4")])


class BaselineFormatTests(unittest.TestCase):
    def test_reads_the_pre_run29_bare_dict(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "b.json"
            path.write_text(json.dumps({"f": {"real": 3}}), encoding="utf-8")
            functions, meta = load_baseline(path)
            self.assertEqual(functions, {"f": {"real": 3}})
            self.assertEqual(meta, {})

    def test_reads_the_commit_anchored_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "b.json"
            path.write_text(json.dumps({
                "meta": {"head": "abc123", "unit": "game/x/y"},
                "functions": {"f": {"real": 3, "genuine": 1}}}),
                encoding="utf-8")
            functions, meta = load_baseline(path)
            self.assertEqual(functions["f"]["genuine"], 1)
            self.assertEqual(meta["head"], "abc123")


class CompareRegressionTests(unittest.TestCase):
    """The pre-existing gate rules must be unaffected by the new field."""

    def test_byte_exact_demotion_is_still_a_regression(self):
        base = {"f": {"status": "EXACT", "real": 0}}
        cur = {"f": {"status": "RELOCATION_ONLY", "real": 0}}
        self.assertEqual(compare(base, cur)[0][1], "REGRESSION")

    def test_real_growth_is_still_a_regression(self):
        base = {"f": {"status": "STRUCTURAL", "real": 4}}
        cur = {"f": {"status": "STRUCTURAL", "real": 9}}
        self.assertEqual(compare(base, cur)[0][1], "REGRESSION")

    def test_real_fall_is_still_an_improvement(self):
        base = {"f": {"status": "STRUCTURAL", "real": 9}}
        cur = {"f": {"status": "STRUCTURAL", "real": 4}}
        self.assertEqual(compare(base, cur)[0][1], "IMPROVED")


if __name__ == "__main__":
    unittest.main()
