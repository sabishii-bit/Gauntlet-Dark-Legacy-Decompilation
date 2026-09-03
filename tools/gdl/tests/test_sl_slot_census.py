"""sl_slot_census's REAL column is the ARBITER's real (run-45 item 5).

THE OBSERVATION. The census computed the `fndiff --clean` flavour of `real`
(diff rows over reloc-NORMALIZED text) and printed it under a column header a
lane reads as the number probe.py and `fndiff --count` report -- which is the
other computation, raw diff rows minus reloc lines. fndiff.real_reconciliation
already names that confusion; this census was inside it, and the column
mis-ranked rows against the arbiter a lane actually uses.

These tests pin BOTH flavours on synthetic line lists, including the two
directions the gap goes: a reloc-only difference (clean 0, arbiter 0 -- the
reloc rows are dropped by one and normalized away by the other) and a
relocated-instruction difference where the two disagree.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import fndiff  # noqa: E402
import sl_slot_census as census  # noqa: E402


class RealCountsTests(unittest.TestCase):
    def test_identical_bodies_score_zero_both_ways(self):
        lines = ["lwz r3,0(r4)", "blr"]
        self.assertEqual(census.real_counts(lines, lines), (0, 0))

    def test_a_pool_name_only_difference_is_zero_both_ways(self):
        """@N against lbl_ is the same datum under both computations: the
        clean view normalizes it, the arbiter drops every reloc row."""
        target = ["lis r3,0", "    R_PPC_ADDR16_HA lbl_80345D40", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA @48", "blr"]
        self.assertEqual(census.real_counts(target, ours), (0, 0))

    def test_an_instruction_difference_counts_once_per_side(self):
        target = ["lwz r3,0(r4)", "blr"]
        ours = ["lwz r5,0(r4)", "blr"]
        self.assertEqual(census.real_counts(target, ours), (2, 2))

    def test_the_two_flavours_disagree_on_a_differing_reloc_symbol(self):
        """The arbiter drops reloc rows outright; the clean view keeps the
        row when the two symbols do NOT normalize together."""
        target = ["lis r3,0", "    R_PPC_ADDR16_HA gPlayers", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA gEnemies", "blr"]
        arbiter, clean = census.real_counts(target, ours)
        self.assertEqual(arbiter, 0)
        self.assertEqual(clean, 2)
        self.assertNotEqual(arbiter, clean)

    def test_the_arbiter_column_is_fndiff_count_real_itself(self):
        """Not a re-implementation: the same function --count calls, so the
        two cannot drift apart again."""
        target = ["lwz r3,0(r4)", "    R_PPC_ADDR16_HA gPlayers", "blr"]
        ours = ["lwz r5,0(r4)", "    R_PPC_ADDR16_HA gEnemies", "blr"]
        rows = census._rows(target, ours)
        self.assertEqual(census.real_counts(target, ours)[0],
                         fndiff.count_real(rows))


if __name__ == "__main__":
    unittest.main()
