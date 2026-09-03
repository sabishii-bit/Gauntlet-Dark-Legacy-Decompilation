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

import sys
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


if __name__ == "__main__":
    unittest.main()
