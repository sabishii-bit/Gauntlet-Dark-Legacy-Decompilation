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


class HiddenRowAccountingTests(unittest.TestCase):
    """Run-43 item 3: the "dropped row" was `--parked skip`, silently.

    Reproduced before the fix: `nearmiss --min 90` prints all 221 in-band
    rows; `--parked skip` prints 79 and says nothing about the other 142,
    which included MBCameraUpdate at 99.97% and do_enemy_move at 99.05%. An
    independent enumeration straight out of report.json found 221 in band
    and 221 printed by default — so no row is lost in the report-reading
    path, and the queue tool was not dropping anything.
    """

    def test_the_footer_states_the_band_and_what_was_hidden(self):
        text = nearmiss.summary_line(79, 142, 258, 90.0)
        self.assertIn("79 near-miss fns", text)
        self.assertIn("221 in band", text)
        self.assertIn("142 hidden by --parked skip", text)

    def test_the_footer_no_longer_cites_a_file_it_does_not_read(self):
        text = nearmiss.summary_line(79, 142, 258, 90.0)
        self.assertNotIn("PARKED.txt", text)
        self.assertIn("memory graph", text)

    def test_nothing_hidden_still_reports_zero_rather_than_going_quiet(self):
        text = nearmiss.summary_line(221, 0, 258, 90.0)
        self.assertIn("221 in band", text)
        self.assertIn("0 hidden", text)

    def test_every_row_carries_its_record_count(self):
        row = nearmiss.format_row(99.97, 636, "", 7, "MBCameraUpdate",
                                  "game/mb/mb_camera", "  [PARKED]")
        self.assertIn("rec=7", row)
        self.assertTrue(row.endswith("[PARKED]"))

    def test_a_function_with_no_records_reads_zero_not_blank(self):
        self.assertIn("rec=0 ", nearmiss.format_row(
            99.0, 100, "", 0, "fn_80001000", "game/test/foo", ""))


class RecordQualityTests(unittest.TestCase):
    """Run-45 item 9: the record COUNT ranked a zero-probe park as the
    best-explored function in the image (`rec=3` on a function whose three
    records never say what was probed). The letter after the count is the
    strongest EVIDENCE those records carry.

    Measured over the live corpus at 56067bfae: of 466 functions carrying
    attempt records, 52 reach D (a typed denial), 140 reach P (a literal
    probed_form) and 274 -- 59% -- are prose only, including nine functions
    holding FIVE prose-only records each.
    """

    def test_a_typed_denial_outranks_a_probed_form(self):
        self.assertEqual(nearmiss.evidence_tier(1, 3), "D")
        self.assertEqual(nearmiss.evidence_tier(2, 0), "D")

    def test_a_probed_form_alone_is_p(self):
        self.assertEqual(nearmiss.evidence_tier(0, 1), "P")

    def test_records_with_neither_are_prose(self):
        self.assertEqual(nearmiss.evidence_tier(0, 0), "-")

    def test_the_defect_row_is_visibly_different_from_a_probed_one(self):
        prose = nearmiss.format_row(99.17, 2732, "", 3, "game_main",
                                    "game/game/gamemain", "", "-")
        probed = nearmiss.format_row(99.38, 1164, "", 3, "EnemyStartMissile",
                                     "game/game/combat", "", "P")
        self.assertIn("rec=3-", prose)
        self.assertIn("rec=3P", probed)

    def test_a_zero_record_row_carries_no_tier_letter(self):
        row = nearmiss.format_row(99.0, 100, "", 0, "fn_80001000",
                                  "game/test/foo", "", "-")
        self.assertIn("rec=0 ", row)
        self.assertNotIn("rec=0-", row)

    def test_the_footer_explains_the_letter(self):
        text = nearmiss.summary_line(11, 0, 260, 99.0)
        self.assertIn("typed denial", text)
        self.assertIn("prose only", text)


if __name__ == "__main__":
    unittest.main()
