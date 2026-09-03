"""Run-50 item 5: nearmiss' `pool=N` column and the codegen-remainder rank.

REPRODUCED at run-50 HEAD.  `python tools/gdl/nearmiss.py --residuals`
prints

    98.95%   1612B  real=  68  STRUCTURAL  rec=2D  write_health_and_items

and three of that residual's rows are `addi r7,r29,868 / 884 / 896` against
our `28 / 44 / 56` -- every one off by EXACTLY 840, base `lbl_80113AE0` in
the target against `@125` in ours (attempt.NC_write-health-and-items-real-
is-dominated-by-an-840-byte-rodata-pool-offset.20260903.v1).  The same data
in the same order at a different base: unreachable from inside the function.

THE DESIGN REVERSAL THE CALIBRATION FORCED.  The obvious rule -- a recurring
constant immediate delta -- fires on 79 of the 195 functions in the >=90%
band, and its heaviest rows are `-8` x49 (mb_particle::DrawPsysSub) and
`-4` x33 (critter::CritterCollidePlayers): struct-field and frame
displacements, which are ordinary codegen.  Adding the second condition --
the row's BASE REGISTER is relocated against DIFFERENT symbols in the two
streams -- takes it to 6 of 195, and all six were already named in the
corpus.

AND THE HEADLINE IT REFUTES: `real` is NOT dominated by these rows.  They
are 1.2%-16.3% of it (write_health_and_items 6 of 68 = 8.8%), and ranking on
`real - pool` moves 26 of 195 positions of which only TWO move for a reason
of their own.  The column's value is naming unreachable rows, not the
re-ordering.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import nearmiss                                                # noqa: E402


def stream(rows):
    """rows = [(instruction, reloc symbol or None)] -> fndiff-shaped lines."""
    out = []
    for text, symbol in rows:
        out.append(text)
        if symbol:
            out.append("    R_PPC_EMB_SDA21  " + symbol)
    return out


class PoolOffsetRows(unittest.TestCase):

    #  write_health_and_items, verbatim: same base register, same order,
    #  a constant +840, and the base relocates against different symbols.
    TARGET = stream([("addi    r29,r4,0", "lbl_80113AE0"),
                     ("addi    r7,r29,868", None),
                     ("addi    r7,r29,884", None),
                     ("addi    r7,r29,896", None)])
    OURS = stream([("addi    r29,r4,0", "@125"),
                   ("addi    r7,r29,28", None),
                   ("addi    r7,r29,44", None),
                   ("addi    r7,r29,56", None)])

    def test_the_recurring_offset_over_a_differing_base_is_a_pool_row(self):
        rows = nearmiss.pool_offset_rows(self.TARGET, self.OURS)
        self.assertEqual(len(rows), 3)
        self.assertEqual({row[2] for row in rows}, {840})
        self.assertEqual(rows[0][3], "lbl_80113AE0")
        self.assertEqual(rows[0][4], "@125")

    def test_lines_are_two_per_row_because_that_is_what_real_counts(self):
        self.assertEqual(nearmiss.pool_offset_lines(self.TARGET, self.OURS), 6)

    def test_the_same_base_symbol_is_NOT_a_pool_row(self):
        # The negative that took the census from 79 to 6: a recurring delta
        # over a base BOTH streams relocate identically is a struct-field or
        # frame displacement, i.e. ordinary codegen.
        ours = stream([("addi    r29,r4,0", "lbl_80113AE0"),
                       ("addi    r7,r29,28", None),
                       ("addi    r7,r29,44", None),
                       ("addi    r7,r29,56", None)])
        self.assertEqual(nearmiss.pool_offset_rows(self.TARGET, ours), [])

    def test_an_unrelocated_base_is_NOT_a_pool_row(self):
        # A stack base (r1) carries no relocation: nothing says these rows
        # are data position at all.
        target = stream([("addi    r7,r1,868", None),
                         ("addi    r7,r1,884", None)])
        ours = stream([("addi    r7,r1,28", None),
                       ("addi    r7,r1,44", None)])
        self.assertEqual(nearmiss.pool_offset_rows(target, ours), [])

    def test_a_ONE_OFF_delta_is_not_recurrence(self):
        target = stream([("addi    r29,r4,0", "lbl_80113AE0"),
                         ("addi    r7,r29,868", None)])
        ours = stream([("addi    r29,r4,0", "@125"),
                       ("addi    r7,r29,28", None)])
        self.assertEqual(nearmiss.pool_offset_rows(target, ours), [])

    def test_two_differing_immediates_in_one_row_are_not_a_base_offset(self):
        target = stream([("addi    r29,r4,0", "lbl_1"),
                         ("rlwinm  r7,r29,868,4", None),
                         ("rlwinm  r7,r29,884,4", None)])
        ours = stream([("addi    r29,r4,0", "@2"),
                       ("rlwinm  r7,r29,28,8", None),
                       ("rlwinm  r7,r29,44,8", None)])
        self.assertEqual(nearmiss.pool_offset_rows(target, ours), [])


class PoolColumnFormatting(unittest.TestCase):

    def test_the_column_appears_only_when_there_is_something_to_report(self):
        with_pool = nearmiss.format_residual(68, 68, "STRUCTURAL", True, 6)
        without = nearmiss.format_residual(68, 68, "STRUCTURAL", True, 0)
        self.assertIn("pool=6", with_pool)
        self.assertNotIn("pool=", without)
        # Columns must still line up when the field is empty.
        self.assertEqual(len(with_pool), len(without))

    def test_the_legacy_call_shape_still_works(self):
        # residual_columns stayed a 3-tuple and `pool` defaults to 0, so
        # every existing caller and test is untouched.
        self.assertEqual(nearmiss.format_residual(12, 12, "STRUCTURAL", True),
                         nearmiss.format_residual(12, 12, "STRUCTURAL", True,
                                                  0))


if __name__ == "__main__":
    unittest.main()
