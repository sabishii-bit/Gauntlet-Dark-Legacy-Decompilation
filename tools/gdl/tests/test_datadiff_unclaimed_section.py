"""A zero target size identifies missing ownership, not a proven data cure.

Reproduced verbatim at c7b741799:

    [game/mb/mb_blit.c] .sdata2: SIZE target 0x0 vs ours 0x90  <- FLIP BLOCKER
    — OURS is LARGER than the target — the DOL-range byte check is
    structurally blind to this

The target side is 0x0 because the dtk split never assigned .sdata2 to
mb_blit.c. That fact does not prove the emitted data correct, nor require
claiming it: unused surplus can be removed by the linker. R66 keeps the
conservative refusal and requires ownership/value/link-reachability review.

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

    def test_the_new_class_requires_review_not_an_automatic_cure(self):
        blurb = dd.GAP_BLURB["blocker-unclaimed-section"]
        self.assertIn("splits.txt", blurb)
        self.assertIn("af_data_base_census.py", blurb)
        self.assertIn("link-reachability review", blurb)
        self.assertIn("not proved correct or wrong", blurb)
        self.assertIn("Do not automatically add", blurb)

    def test_it_still_blocks(self):
        """Claim debt is a real flip blocker; only the CAUSE was misnamed."""
        self.assertTrue("blocker-unclaimed-section".startswith("blocker"))

    def test_the_blurb_points_at_the_scored_whole_image_census_first(self):
        # This IS the data campaign's queue, and the only tool that scores a
        # candidate extent against the DOL bytes is claimable_sections.py;
        # af_data_base_census derives bases for ONE unit. Naming only the
        # latter left a lane with candidate addresses and no discriminant.
        blurb = dd.GAP_BLURB["blocker-unclaimed-section"]
        self.assertIn("tools/gdl/claimable_sections.py", blurb)
        self.assertLess(blurb.index("claimable_sections.py"),
                        blurb.index("af_data_base_census.py"))

    def test_no_other_gap_class_advertises_a_census_tool(self):
        # A pointer printed on every row is not a pointer. Only the class
        # that HAS an unclaimed extent to derive may name these tools.
        for gap, blurb in dd.GAP_BLURB.items():
            if gap == "blocker-unclaimed-section":
                continue
            with self.subTest(gap=gap):
                self.assertNotIn("claimable_sections.py", blurb)
                self.assertNotIn("af_data_base_census.py", blurb)


class SplitsCrossCheckTests(unittest.TestCase):
    def test_the_worked_pair_from_the_law_still_holds(self):
        units = dd.parse_splits()
        self.assertNotIn(".sdata2", units["game/mb/mb_blit.c"])
        self.assertIn(".sdata2", units["game/mb/mb_camera.c"])


if __name__ == "__main__":
    unittest.main()
