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

from defake_gate import (arbitrate_regressions, compare, load_baseline,
                         read_report_fuzzy)


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


class FuzzyArbiterTests(unittest.TestCase):
    """run-31 item 2: when the genuine structural rows are FLAT the
    structure arbiter has nothing to say, so the gate printed a bare
    REGRESSION and every keep of this shape had to be overridden by hand
    with a fuzzy the tool never showed. --arbiter fuzzy measures it."""

    VERDICTS = [("get_vmu_directory", "REGRESSION", "real 48 -> 65")]

    def arb(self, baseline, genuine_now, fuzzy_now=None, arbiter=None):
        return arbitrate_regressions(
            list(self.VERDICTS), "game/sys/memcard", baseline,
            genuine_fn=lambda unit, names: genuine_now,
            ops_fn=no_ops,
            fuzzy_fn=(None if fuzzy_now is None
                      else (lambda unit, names: fuzzy_now)),
            arbiter=arbiter)

    def test_flat_genuine_without_the_fuzzy_arbiter_says_it_is_unmeasured(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("fuzzy delta UNMEASURED", out[0][2])
        self.assertIn("--arbiter fuzzy", out[0][2])

    def test_flat_genuine_with_fuzzy_RISING_is_a_CONFLICT(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("fuzzy 90.0400 -> 92.7200", out[0][2])
        self.assertIn("+2.6800", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_flat_genuine_with_fuzzy_FALLING_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 92.72}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 90.04}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        # The delta is PRINTED either way — that is the point of the item.
        self.assertIn("fuzzy 92.7200 -> 90.0400", out[0][2])
        self.assertIn("-2.6800", out[0][2])

    def test_flat_genuine_with_fuzzy_EQUAL_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 90.04}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("+0.0000", out[0][2])

    def test_baseline_without_a_fuzzy_anchor_says_so(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("no fuzzy anchor", out[0][2])

    def test_fuzzy_arbiter_never_rescues_a_byte_exact_function(self):
        out = arbitrate_regressions(
            [("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
            {"f": {"genuine": 5, "fuzzy": 100.0}},
            genuine_fn=lambda unit, names: {"f": 5},
            ops_fn=no_ops, fuzzy_fn=lambda unit, names: {"f": 100.0},
            arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")

    def test_genuine_FELL_conflict_also_reports_the_measured_fuzzy(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 1},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("genuine structural rows 5 -> 1 FELL", out[0][2])
        self.assertIn("fuzzy 90.0400 -> 92.7200", out[0][2])


class ReportFuzzyReadTests(unittest.TestCase):
    REPORT = {
        "units": [
            {"name": "src/game/sys/memcard",
             "functions": [{"name": "get_vmu_directory",
                            "fuzzy_match_percent": 92.7155},
                           {"name": "saveLoad",
                            "fuzzy_match_percent": 100.0}]},
            {"name": "src/game/ui/select",
             "functions": [{"name": "serve_blits",
                            "fuzzy_match_percent": 50.0}]},
        ]
    }

    def _report(self, tmp):
        path = Path(tmp) / "report.json"
        path.write_text(json.dumps(self.REPORT), encoding="utf-8")
        return path

    def test_reads_the_named_units_functions(self):
        with tempfile.TemporaryDirectory() as tmp:
            got = read_report_fuzzy("game/sys/memcard", self._report(tmp))
            self.assertEqual(got, {"get_vmu_directory": 92.7155,
                                   "saveLoad": 100.0})

    def test_unknown_unit_reads_empty_not_wrong(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(
                read_report_fuzzy("game/nope/nope", self._report(tmp)), {})

    def test_missing_report_is_not_fatal(self):
        self.assertEqual(
            read_report_fuzzy("game/sys/memcard", Path("no/such.json")), {})


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
