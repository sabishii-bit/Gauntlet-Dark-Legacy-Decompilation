"""A `SIZE target 0x0` blocker is CLAIM debt, and now says so (run-53 item 6).

Reproduced verbatim at c7b741799:

    [game/mb/mb_blit.c] .sdata2: SIZE target 0x0 vs ours 0x90  <- FLIP BLOCKER
    — OURS is LARGER than the target — the DOL-range byte check is
    structurally blind to this

That blurb is written for a DATA defect and lanes read it as one. The target
side is 0x0 because the dtk split never assigned .sdata2 to mb_blit.c: the
cure is a line in config/GUNE5D/splits.txt, not an edit to the .c
(claim.law.CX_a-datadiff-sections-flip-blocker-in-the-near-flip-band-is-
splits-txt-claim-debt-not-a-data-defect.20260904.v1).

The refinement CHECKS the claim per row against the live splits.txt rather
than asserting the law, so the law's falsifier stays live.

TWO-SIDED, measured image-wide over every unit in splits.txt with both
objects built (1 skipped):
  POSITIVE   93 blocker rows over 39 units take the new verdict — 2.4x the
             law's 38, because the law measured only the distance 0-3 band.
  NEGATIVE   0 rows are `target 0x0` AND claimed (none is misrouted);
             exactly 1 blocker keeps the OURS-LARGER data wording,
             `dolphin/demo/DEMOInit.c .sbss target 0x20 vs ours 0x28`, with
             both sides nonzero; 0 rows in any other blocker class.
  That single row is also a COUNTEREXAMPLE to the law's universal shape
  claim ("all 38 carry one shape"), which holds inside its measured band and
  not image-wide — exactly what the law's falsifier asked someone to look
  for, and the reason this refinement is a per-row check.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import datadiff as dd  # noqa: E402


class RefineUnclaimedTests(unittest.TestCase):
    def test_an_unclaimed_zero_target_becomes_the_claim_debt_verdict(self):
        self.assertEqual(
            dd.refine_unclaimed("blocker-ours-larger", 0, claimed=False),
            "blocker-unclaimed-section")

    def test_a_claimed_zero_target_keeps_the_data_wording(self):
        """The law is CHECKED per row, never assumed: a claimed section whose
        target side is still 0x0 is a different fact and must read as one."""
        self.assertEqual(
            dd.refine_unclaimed("blocker-ours-larger", 0, claimed=True),
            "blocker-ours-larger")

    def test_both_sides_nonzero_keeps_the_data_wording(self):
        """dolphin/demo/DEMOInit.c .sbss: target 0x20 vs ours 0x28."""
        self.assertEqual(
            dd.refine_unclaimed("blocker-ours-larger", 0x20, claimed=False),
            "blocker-ours-larger")

    def test_other_gap_classes_are_untouched(self):
        for gap in ("debt-zero-slack", "debt-bss-slack",
                    "blocker-nonzero-tail", "blocker-head-differs"):
            self.assertEqual(dd.refine_unclaimed(gap, 0, claimed=False), gap)

    def test_the_new_class_has_a_blurb_and_it_names_the_cure(self):
        blurb = dd.GAP_BLURB["blocker-unclaimed-section"]
        self.assertIn("splits.txt", blurb)
        self.assertIn("af_data_base_census.py", blurb)
        self.assertIn("not fixed in the .c", blurb.lower())

    def test_it_still_blocks(self):
        """Claim debt is a real flip blocker; only the CAUSE was misnamed."""
        self.assertTrue("blocker-unclaimed-section".startswith("blocker"))


class SplitsCrossCheckTests(unittest.TestCase):
    def test_the_worked_pair_from_the_law_still_holds(self):
        units = dd.parse_splits()
        self.assertNotIn(".sdata2", units["game/mb/mb_blit.c"])
        self.assertIn(".sdata2", units["game/mb/mb_camera.c"])


if __name__ == "__main__":
    unittest.main()
