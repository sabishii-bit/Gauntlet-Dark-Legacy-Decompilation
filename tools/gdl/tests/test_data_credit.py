"""Two-sided tests for tools/gdl/data_credit.py.

Positive: representative valid inputs reconcile and classify as documented.
Negative: a deliberately inconsistent report is REPORTED, not averaged away;
an unknown flag is refused; `--help` does no work; a missing report exits 2.

The decisive regression guard is `LiveReport`, which asserts that the live
report.json reconciles for every unit. If the partition in CODE_SECTIONS
ever stops matching objdiff, that test fails instead of the tool quietly
mis-crediting a unit -- which is exactly the defect the first draft had.
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "gdl"))

import data_credit  # noqa: E402


def unit(name, sections, matched=None, total=None):
    rows = [{"name": n, "size": str(s), "fuzzy_match_percent": f}
            for n, s, f in sections]
    data = [r for r in rows if data_credit.is_data_section(r["name"])]
    if total is None:
        total = sum(int(r["size"]) for r in data)
    if matched is None:
        matched = sum(int(r["size"]) for r in data
                      if r["fuzzy_match_percent"] == 100.0)
    return {"name": name,
            "measures": {"total_data": str(total), "matched_data": str(matched)},
            "sections": rows}


class Partition(unittest.TestCase):
    def test_text_and_init_are_code(self):
        self.assertFalse(data_credit.is_data_section(".text"))
        self.assertFalse(data_credit.is_data_section(".init"))

    def test_every_other_section_is_data(self):
        for name in (".bss", ".data", ".rodata", ".sbss", ".sdata", ".sdata2",
                     "extab", "extabindex", ".ctors"):
            self.assertTrue(data_credit.is_data_section(name), name)


class CreditLaw(unittest.TestCase):
    def test_only_sections_at_exactly_100_are_credited(self):
        u = unit("main/x", [(".text", 900, 50.0), (".bss", 100, 100.0),
                            (".data", 40, 99.99), ("extab", 8, 100.0)])
        self.assertEqual(data_credit.model_matched(u), 108)
        self.assertEqual(data_credit.model_total_data(u), 148)

    def test_a_section_one_hundredth_short_is_paid_nothing(self):
        """The whole point of the tool: 99.99% credits ZERO, not 99.99%."""
        u = unit("main/x", [(".bss", 26508, 99.99)])
        self.assertEqual(data_credit.model_matched(u), 0)

    def test_code_sections_never_enter_the_data_total(self):
        u = unit("main/x", [(".init", 32, 100.0), (".data", 60, 100.0)])
        self.assertEqual(data_credit.model_total_data(u), 60)
        self.assertEqual(data_credit.model_matched(u), 60)


class Verify(unittest.TestCase):
    def test_a_consistent_report_has_no_unexplained_unit(self):
        report = {"units": [unit("main/a", [(".bss", 64, 100.0)]),
                            unit("main/b", [(".data", 32, 12.5)])]}
        rows = data_credit.verify_rows(report)
        text = "\n".join(data_credit.format_verify(rows))
        self.assertIn("2 of 2", text)
        self.assertIn("no unexplained unit", text)

    def test_a_wrong_matched_figure_is_reported(self):
        u = unit("main/bad", [(".bss", 64, 100.0)], matched=8)
        rows = data_credit.verify_rows({"units": [u]})
        text = "\n".join(data_credit.format_verify(rows))
        self.assertIn("UNEXPLAINED", text)
        self.assertIn("main/bad", text)

    def test_a_wrong_total_is_reported_even_when_matched_agrees(self):
        """The .init defect shape: matched agrees, the total does not."""
        u = unit("main/bad2", [(".bss", 64, 100.0)], matched=64, total=96)
        rows = data_credit.verify_rows({"units": [u]})
        self.assertIn("UNEXPLAINED", "\n".join(data_credit.format_verify(rows)))

    def test_units_without_data_are_not_counted(self):
        rows = data_credit.verify_rows(
            {"units": [unit("main/codeonly", [(".text", 100, 100.0)])]})
        self.assertEqual(rows, [])


class Rank(unittest.TestCase):
    REPORT = {"units": [
        unit("main/game/a", [(".bss", 500, 90.0), (".data", 10, 100.0)]),
        unit("main/auto_08_1234_bss", [(".bss", 9000, 0.0)]),
        unit("main/game/b", [(".rodata", 700, 99.5)]),
    ]}

    def test_credited_sections_are_excluded_and_order_is_by_size(self):
        rows = data_credit.rank_rows(self.REPORT)
        self.assertEqual([r[0] for r in rows], [9000, 700, 500])
        self.assertNotIn(".data", [r[2] for r in rows])

    def test_real_only_drops_auto_units(self):
        rows = data_credit.rank_rows(self.REPORT, real_only=True)
        self.assertEqual([r[1] for r in rows], ["main/game/b", "main/game/a"])

    def test_band_filters_by_fuzzy_percent(self):
        rows = data_credit.rank_rows(self.REPORT, real_only=True, band=(95, 100))
        self.assertEqual([r[1] for r in rows], ["main/game/b"])

    def test_base_of_and_is_real_tu(self):
        self.assertEqual(data_credit.base_of("main/game/game/player"),
                         "game/game/player")
        self.assertTrue(data_credit.is_real_tu("main/game/game/player"))
        self.assertFalse(data_credit.is_real_tu("main/auto_08_80296450_bss"))


class ClassifyGap(unittest.TestCase):
    def call(self, tgt, our, section=".bss"):
        return data_credit.classify_gap("main/x", section,
                                        target_map={section: tgt},
                                        ours_map={section: our})

    def test_identical_maps_are_a_byte_difference(self):
        got = self.call({"a": 4, "b": 8}, {"a": 4, "b": 8})
        self.assertEqual(got["verdict"], "BYTE")

    def test_a_differing_named_symbol_is_a_boundary_candidate(self):
        got = self.call({"lbl_80250E40": 776}, {"lbl_80250E40": 692,
                                                "lbl_802510F4": 12})
        self.assertEqual(got["verdict"], "BOUNDARY")
        self.assertEqual(got["only_ours"], ["lbl_802510F4"])
        self.assertEqual(got["resized"], ["lbl_80250E40"])

    def test_compiler_generated_names_are_anonymous_not_boundary(self):
        got = self.call({"jumptable_8011FB40": 52}, {"@447": 52})
        self.assertEqual(got["verdict"], "ANONYMOUS")

    def test_a_missing_section_on_one_side_is_still_classified(self):
        got = data_credit.classify_gap("main/x", ".sdata2",
                                       target_map={}, ours_map={".sdata2": {"z": 4}})
        self.assertEqual(got["verdict"], "BOUNDARY")
        self.assertEqual(got["only_ours"], ["z"])


class Cli(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, "tools/gdl/data_credit.py", *args],
                              cwd=str(ROOT), capture_output=True, text=True)

    def test_help_exits_zero_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "must_not_exist.json"
            done = self.run_tool("--help", "--out", str(out))
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertFalse(out.exists(), "--help had a side effect on disk")

    def test_an_unknown_flag_is_refused(self):
        done = self.run_tool("--definitely-not-a-flag")
        self.assertNotEqual(done.returncode, 0)

    def test_a_missing_report_exits_two_and_says_so(self):
        done = self.run_tool("--report", "build/f_lane/no_such_report.json",
                             "--verify")
        self.assertEqual(done.returncode, 2)
        self.assertIn("missing report", done.stderr)

    def test_json_out_is_written_and_parses(self):
        report = {"units": [unit("main/a", [(".bss", 64, 100.0)])]}
        with tempfile.TemporaryDirectory() as tmp:
            rpath = Path(tmp) / "r.json"
            opath = Path(tmp) / "o.json"
            rpath.write_text(json.dumps(report), encoding="utf-8")
            done = self.run_tool("--report", str(rpath), "--verify",
                                 "--out", str(opath))
            self.assertEqual(done.returncode, 0, done.stderr)
            payload = json.loads(opath.read_text(encoding="utf-8"))
            self.assertEqual(payload["verify"][0]["unit"], "main/a")


LIVE = (ROOT / "build" / "GUNE5D" / "report.json").is_file()


@unittest.skipUnless(LIVE, "needs a built report.json")
class LiveReport(unittest.TestCase):
    def test_every_live_unit_reconciles(self):
        report = data_credit.load_report(ROOT / "build" / "GUNE5D" / "report.json")
        rows = data_credit.verify_rows(report)
        self.assertGreater(len(rows), 100, "report looks empty")
        bad = [r for r in rows if r[2] != r[3] or r[1] != r[4]]
        self.assertEqual(bad, [], "credit partition no longer matches objdiff")

    def test_ranking_the_live_frontier_is_nonempty_and_ordered(self):
        report = data_credit.load_report(ROOT / "build" / "GUNE5D" / "report.json")
        rows = data_credit.rank_rows(report)
        self.assertTrue(rows)
        self.assertEqual([r[0] for r in rows], sorted((r[0] for r in rows),
                                                      reverse=True))


if __name__ == "__main__":
    unittest.main()
