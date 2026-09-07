"""nearmiss --residuals: the column formerly printed as `d=` (run 41 #6).

Two different computations are both called `real` in this project: raw diff
rows minus every relocation line (`fndiff --count`, and what probe.py prints
and every work order quotes) and rows over reloc-NORMALIZED text
(`fndiff --clean`). This queue printed and RANKED on the second under the
unexplained label `d=`.

Measured over the live 219-row queue before the change: the two columns
disagree on 140 rows, and 177 of the 219 queue positions move when ranked on
the arbiter every other tool quotes — AudioSetupBossStreams 1523 vs 1297,
PlayerMotion 4168 vs 3982, BossCamLimitAttn 187 vs 75.
"""

import io
import json
import sys
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import nearmiss  # noqa: E402


# One instruction word differs and one relocation SYMBOL differs. `--count`'s
# real drops the relocation rows and counts 2; `--clean` normalizes the pool
# names to nothing and counts 2 as well, so this pair agrees...
AGREEING_T = ["addi r3,r3,4", "    R_PPC_EMB_SDA21 @12"]
AGREEING_O = ["addi r3,r3,8", "    R_PPC_EMB_SDA21 @13"]

# ...while here the relocation TYPES differ, which survives normalization
# and inflates --clean's number while --count still drops the rows.
DIVERGING_T = ["lwz r3,0(r4)", "    R_PPC_EMB_SDA21 gFoo"]
DIVERGING_O = ["lwz r3,0(r4)", "    R_PPC_ADDR16_LO gBar"]


class ResidualColumnTests(unittest.TestCase):
    def test_real_is_count_reals_number_not_cleans(self):
        real, clean, _category = nearmiss.residual_columns(
            DIVERGING_T, DIVERGING_O)
        self.assertEqual(real, 0)      # every differing row is a reloc row
        self.assertEqual(clean, 2)     # --clean counts them
        self.assertNotEqual(real, clean)

    def test_an_instruction_difference_counts_in_both(self):
        real, clean, _category = nearmiss.residual_columns(
            AGREEING_T, AGREEING_O)
        self.assertEqual(real, 2)
        self.assertEqual(clean, 2)

    def test_an_identical_function_is_zero_in_both(self):
        real, clean, category = nearmiss.residual_columns(
            AGREEING_T, AGREEING_T)
        self.assertEqual((real, clean, category), (0, 0, "EXACT"))


class ResidualFormatTests(unittest.TestCase):
    def test_the_column_is_labelled_real_not_d(self):
        text = nearmiss.format_residual(12, 12, "STRUCTURAL", True)
        self.assertIn("real=", text)
        self.assertNotIn("d=", text)

    def test_clean_is_shown_only_when_the_two_disagree(self):
        self.assertIn("clean=18",
                      nearmiss.format_residual(2, 18, "STRUCTURAL", True))
        self.assertNotIn("clean=",
                         nearmiss.format_residual(2, 2, "STRUCTURAL", True))

    def test_columns_line_up_whether_or_not_clean_is_shown(self):
        with_clean = nearmiss.format_residual(2, 18, "STRUCTURAL", True)
        without = nearmiss.format_residual(2, 2, "STRUCTURAL", True)
        self.assertEqual(len(with_clean), len(without))

    def test_an_unmeasured_row_says_so_only_in_residual_mode(self):
        self.assertEqual(
            nearmiss.format_residual(None, None, None, True), "  real=???")
        self.assertEqual(
            nearmiss.format_residual(None, None, None, False), "")


class QueueWithoutHistoryTests(unittest.TestCase):
    def test_footer_explicitly_does_not_assess_history_or_ownership(self):
        text = nearmiss.summary_line(221, 90.0)
        self.assertIn("221 near-miss fns", text)
        self.assertIn("221 in band", text)
        self.assertIn("prior attempts and ownership not assessed", text)

    def test_rows_contain_only_measured_metadata(self):
        row = nearmiss.format_row(99.97, 636, "", "MBCameraUpdate",
                                  "game/mb/mb_camera")
        self.assertIn("99.97%", row)
        self.assertIn("MBCameraUpdate", row)
        self.assertNotIn("rec=", row)
        self.assertNotIn("PARKED", row)

    def test_queue_runs_without_knowledge_package(self):
        report = {"units": [
            {"name": "main/game/test/foo", "functions": [
                {"name": "near", "size": 100, "fuzzy_match_percent": 99},
                {"name": "low", "size": 80, "fuzzy_match_percent": 20},
                {"name": "exact", "size": 40, "fuzzy_match_percent": 100}]},
            {"name": "main/game/test/linked", "metadata": {"complete": True},
             "functions": [
                 {"name": "linked_noise", "size": 40,
                  "fuzzy_match_percent": 99}]}]}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "report.json"
            path.write_text(json.dumps(report), encoding="utf-8")
            output = io.StringIO()
            with patch.object(nearmiss, "REPORT", path), \
                 patch.object(sys, "argv", ["nearmiss", "--min", "90"]), \
                 patch.dict(sys.modules, {"memory_graph": None,
                                          "memory_graph.core": None}), \
                 redirect_stdout(output):
                self.assertEqual(nearmiss.main(), 0)
        text = output.getvalue()
        self.assertIn("near ", text)
        self.assertNotIn("low ", text)
        self.assertNotIn("exact ", text)
        self.assertNotIn("linked_noise", text)
        self.assertIn("1 in band", text)


if __name__ == "__main__":
    unittest.main()
