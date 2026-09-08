"""Two-sided tests for tools/gdl/progress_dims.py.

Positive side: a hand-built report/rules/edges triple whose every dimension is
computable by hand, plus the live checkout when its inputs exist -- there the
STRICT/EQUIVALENT line must still read exactly the way the retired
`configure.py progress` line read, because a restored report that quietly
changes convention is worse than no report.

Negative side: each refusal has its own test. A missing, malformed or
convention-breaking input must EXIT 2 and print REFUSED, never a zero
dimension -- "0.00% manual exceptions" and "the manual-exception file was
unreadable" are opposite facts and must not print the same way.
"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(GDL))

import progress_dims as pd  # noqa: E402


def report(units):
    return {"units": [{"name": name,
                       "functions": [{"name": f, "size": s,
                                      "fuzzy_match_percent": p}
                                     for f, s, p in fns]}
                      for name, fns in units]}


UNPROVEN = {"function": "manual_fn", "before_sha256": "a", "after_sha256": "b",
            "copy_register_fields": {"unproven_recolor_audit": "hand-checked"}}


def rules(units):
    return {"version": 1, "units": units}


def edges(units):
    return {"schema_version": 1,
            "units": [{"name": name, "module": "main", "code_size": size,
                       "linkage": linkage}
                      for name, size, linkage in units]}


class DimensionsTests(unittest.TestCase):
    """The arithmetic, on inputs small enough to check by hand."""

    def setUp(self):
        # 1000 bytes of code total.
        # strict:    stock_fn 400 @100
        # equivalent: pinned_fn 200 @100 + manual_fn 100 @100  (manual subset)
        # neither:   cold_fn 300 @ 55
        self.report = report([
            ("main/game/sys/thing", [("stock_fn", 400, 100.0),
                                     ("pinned_fn", 200, 100.0),
                                     ("manual_fn", 100, 100.0),
                                     ("cold_fn", 300, 55.5)]),
        ])
        self.rules = rules({"game/sys/thing": [
            {"function": "pinned_fn", "before_sha256": "a", "after_sha256": "b",
             "copy_register_fields": {}},
            UNPROVEN,
        ]})
        self.edges = edges([("game/sys/thing.c", 1000, "source")])

    def test_every_dimension_is_the_hand_computed_one(self):
        result = pd.dimensions(self.report, self.rules, self.edges)
        rows = result["dimensions"]
        self.assertEqual(result["total_code_bytes"], 1000)
        self.assertEqual((rows["strict"]["bytes"], rows["strict"]["functions"]), (400, 1))
        self.assertEqual((rows["equivalent"]["bytes"], rows["equivalent"]["functions"]), (300, 2))
        self.assertEqual((rows["manual"]["bytes"], rows["manual"]["functions"]), (100, 1))
        self.assertEqual(rows["strict"]["percent"], 40.0)
        self.assertEqual(rows["equivalent"]["percent"], 30.0)
        self.assertEqual(rows["manual"]["percent"], 10.0)

    def test_manual_is_a_disclosed_subset_not_a_deduction(self):
        """MANUAL must not be subtracted out of EQUIVALENT."""
        rows = pd.dimensions(self.report, self.rules, self.edges)["dimensions"]
        self.assertEqual(rows["manual"]["bytes"], 100)
        self.assertLess(rows["manual"]["bytes"], rows["equivalent"]["bytes"])
        self.assertEqual(rows["equivalent"]["functions"], 2)

    def test_source_linked_counts_all_code_and_its_matched_subset(self):
        rows = pd.dimensions(self.report, self.rules, self.edges)["dimensions"]
        self.assertEqual(rows["source_linked"]["bytes"], 1000)
        self.assertEqual(rows["source_linked_matched"]["bytes"], 700)

    def test_extracted_fallback_unit_earns_no_source_linked_credit(self):
        data = edges([("game/sys/thing.c", 1000, "extracted_fallback")])
        rows = pd.dimensions(self.report, self.rules, data)["dimensions"]
        self.assertEqual(rows["source_linked"]["bytes"], 0)
        self.assertEqual(rows["strict"]["bytes"], 400)

    def test_absent_edges_report_unavailable_not_zero(self):
        result = pd.dimensions(self.report, self.rules, None)
        self.assertFalse(result["source_linked_available"])
        self.assertIn("UNAVAILABLE", "\n".join(pd.format_lines(result)))
        self.assertNotIn("SOURCE-LINKED: 0.00%", "\n".join(pd.format_lines(result)))

    def test_pin_without_a_matched_report_function_is_named(self):
        data = rules({"game/sys/thing": [
            {"function": "ghost_fn", "before_sha256": "a", "after_sha256": "b"}]})
        result = pd.dimensions(self.report, data, self.edges)
        self.assertEqual(result["pins_without_matched_report_function"],
                         ["game/sys/thing::ghost_fn"])
        self.assertIn("game/sys/thing::ghost_fn", "\n".join(pd.format_lines(result)))

    def test_duplicate_rules_collapse_to_one_pinned_entry(self):
        """164 rules over 162 entries: a second rule is not a second function."""
        data = rules({"game/sys/thing": [
            {"function": "pinned_fn", "before_sha256": "a", "after_sha256": "b"},
            {"function": "pinned_fn", "before_sha256": "b", "after_sha256": "c"},
        ]})
        result = pd.dimensions(self.report, data, self.edges)
        self.assertEqual(result["configured_rules"], 2)
        self.assertEqual(result["pinned_entries"], 1)
        self.assertEqual(result["dimensions"]["equivalent"]["functions"], 1)

    def test_manual_flag_is_found_when_nested_deep_in_a_rule(self):
        deep = rules({"game/sys/thing": [
            {"function": "pinned_fn", "before_sha256": "a", "after_sha256": "b",
             "instruction_permutation": [{"windows": [{"unproven_recolor_audit": 1}]}]}]})
        rows = pd.dimensions(self.report, deep, self.edges)["dimensions"]
        self.assertEqual(rows["manual"]["functions"], 1)

    def test_dimensions_are_not_summed_in_the_printed_report(self):
        text = "\n".join(pd.format_lines(pd.dimensions(self.report, self.rules, self.edges)))
        self.assertIn("NOT summable", text)
        self.assertNotIn("= 70.00%", text)
        self.assertNotIn("matched;", text)


class RefusalTests(unittest.TestCase):
    """Deliberately invalid inputs: every one must refuse, none may score."""

    def setUp(self):
        self.good_report = report([("main/u", [("f", 4, 100.0)])])
        self.good_rules = rules({"u": []})

    def test_rules_without_units_key_refuses_rather_than_calling_it_all_strict(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            (folder / "report.json").write_text(json.dumps(self.good_report))
            # The exact shape that reads as "no rules exist" to a root iterator.
            (folder / "rules.json").write_text(json.dumps(
                {"game/sys/thing": [{"function": "pinned_fn"}]}))
            with self.assertRaises(pd.Refusal) as caught:
                pd.load_inputs(folder / "report.json", folder / "rules.json",
                               folder / "absent.json")
        self.assertIn("units", str(caught.exception))

    def test_missing_report_refuses(self):
        with self.assertRaises(pd.Refusal) as caught:
            pd.load_inputs(Path("nope-report.json"), Path("nope-rules.json"))
        self.assertIn("missing report.json", str(caught.exception))

    def test_unparseable_report_refuses(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "report.json"
            path.write_text("{not json")
            with self.assertRaises(pd.Refusal) as caught:
                pd.load_inputs(path, path)
        self.assertIn("unreadable", str(caught.exception))

    def test_out_of_range_score_refuses(self):
        bad = report([("main/u", [("f", 4, 150.0)])])
        with self.assertRaises(ValueError):
            pd.dimensions(bad, self.good_rules, None)

    def test_negative_size_refuses(self):
        bad = report([("main/u", [("f", -4, 100.0)])])
        with self.assertRaises(ValueError):
            pd.dimensions(bad, self.good_rules, None)

    def test_duplicate_report_function_refuses(self):
        bad = {"units": [{"name": "main/u", "functions": [
            {"name": "f", "size": 4, "fuzzy_match_percent": 100.0},
            {"name": "f", "size": 8, "fuzzy_match_percent": 100.0}]}]}
        with self.assertRaises(ValueError):
            pd.dimensions(bad, self.good_rules, None)

    def test_zero_code_refuses(self):
        with self.assertRaises(pd.Refusal):
            pd.dimensions({"units": []}, self.good_rules, None)

    def test_rule_without_a_function_refuses(self):
        bad = rules({"u": [{"before_sha256": "a"}]})
        with self.assertRaises(pd.Refusal):
            pd.dimensions(self.good_report, bad, None)

    def test_rule_list_that_is_not_a_list_refuses(self):
        bad = rules({"u": {"pinned_fn": {}}})
        with self.assertRaises(pd.Refusal):
            pd.dimensions(self.good_report, bad, None)

    def test_cli_exit_code_is_2_and_says_refused(self):
        proc = subprocess.run(
            [sys.executable, str(GDL / "progress_dims.py"),
             "--report", "nope-report.json", "--rules", "nope-rules.json"],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(proc.returncode, 2)
        self.assertIn("REFUSED", proc.stderr)
        self.assertEqual(proc.stdout, "")


class LiveCheckoutTests(unittest.TestCase):
    """The convention check, against this checkout's real inputs."""

    def setUp(self):
        if not (pd.REPORT.exists() and pd.EDGES.exists()):
            self.skipTest("report.json/build_edges.json not built in this checkout")
        self.result = pd.dimensions(*pd.load_inputs())

    def test_split_line_keeps_the_retired_progress_convention(self):
        line = pd.format_lines(self.result)[1]
        self.assertRegex(
            line,
            r"^  Postprocessor split: STRICT matched \d+\.\d\d% \(\d+ fns\)"
            r" \+ EQUIVALENT \d+\.\d\d% \(\d+ fns\)$")

    def test_split_agrees_with_the_shipped_progress_module(self):
        """Two implementations of one number must not drift apart."""
        import progress
        split = progress.postprocessor_split()
        rows = self.result["dimensions"]
        self.assertAlmostEqual(split["strict_percent"], rows["strict"]["percent"], places=6)
        self.assertAlmostEqual(split["equivalent_percent"], rows["equivalent"]["percent"], places=6)
        self.assertEqual(split["strict_functions"], rows["strict"]["functions"])
        self.assertEqual(split["equivalent_functions"], rows["equivalent"]["functions"])

    def test_manual_dimension_agrees_with_build_provenance_tier(self):
        path = ROOT / "build" / pd.VERSION / "build_provenance.json"
        if not path.exists():
            self.skipTest("build_provenance.json not present")
        tiers = json.loads(path.read_text())["report_accounting"]["tiers"]
        manual = tiers.get("manual_exception", {"functions": 0, "bytes": 0})
        rows = self.result["dimensions"]
        self.assertEqual(rows["manual"]["functions"], manual["functions"])
        self.assertEqual(rows["manual"]["bytes"], manual["bytes"])

    def test_equivalent_covers_both_guarded_and_manual_provenance_tiers(self):
        path = ROOT / "build" / pd.VERSION / "build_provenance.json"
        if not path.exists():
            self.skipTest("build_provenance.json not present")
        tiers = json.loads(path.read_text())["report_accounting"]["tiers"]
        guarded = tiers.get("webfrank_guarded_declared", {"functions": 0, "bytes": 0})
        manual = tiers.get("manual_exception", {"functions": 0, "bytes": 0})
        p6 = tiers.get("p6frank_guarded_declared", {"functions": 0, "bytes": 0})
        rows = self.result["dimensions"]
        # p6frank functions are NOT webfrank pins, so they belong to neither
        # the EQUIVALENT set nor STRICT-by-webfrank; state the arithmetic.
        self.assertEqual(rows["equivalent"]["functions"],
                         guarded["functions"] + manual["functions"])
        self.assertEqual(rows["strict"]["functions"] + rows["equivalent"]["functions"]
                         - p6["functions"] >= 0, True)


if __name__ == "__main__":
    unittest.main()
