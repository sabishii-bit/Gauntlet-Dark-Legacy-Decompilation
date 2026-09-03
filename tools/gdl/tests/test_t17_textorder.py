#!/usr/bin/env python3
"""T17 run-47 item 1: the .text function-ORDER screen and its finish_tu gate.

Fingerprint #4: a TU can read 36/36 functions at `real 0`, carry byte-verified
data in every section, and be UNFLIPPABLE because .text emits the functions in
the wrong order -- and no project score sees it, because they all pair by name
(claim.law.MF_every-function-at-real-0-does-not-mean-the-text-order-is-right-
and-no-project-score-sees-it.20260903.v1).

Two-sided calibration, measured at 4726b33ca over the 252 unit pairs built in
this tree and asserted live below where the objects exist:

    naive full-sequence compare   53 of 200 Matching units flagged (all FP)
    intersection compare           0 of 200 Matching units flagged
    intersection compare          23 of  52 NonMatching units flagged

The Matching units are the negative set BY CONSTRUCTION: they link
byte-identically today, so any hit among them is a false positive. That 53 is
what the shared-name intersection removes -- our objects define static helpers
the linker dead-strips (PPCArch alone has 22), and none of them occupies a
position in the linked image.
"""

import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import textorder  # noqa: E402


class Discriminant(unittest.TestCase):
    def test_identical_order_is_clean(self):
        seq = ["a", "b", "c"]
        self.assertEqual(textorder.misordered_groups(seq, list(seq)), [])

    def test_a_transposition_is_ONE_group_not_two(self):
        """difflib reports a permutation as an unpaired insert AND delete."""
        groups = textorder.misordered_groups(
            ["a", "b", "c", "d"], ["a", "c", "b", "d"])
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0]["target"], ["b", "c"])
        self.assertEqual(groups[0]["ours"], ["c", "b"])
        self.assertEqual(groups[0]["at"], 1)

    def test_a_rotation_resyncs_at_the_next_function(self):
        target = ["x", "InitLists", "ListLock", "Alloc", "SetEmpty", "y"]
        ours = ["x", "SetEmpty", "Alloc", "InitLists", "ListLock", "y"]
        groups = textorder.misordered_groups(target, ours)
        self.assertEqual(len(groups), 1)
        self.assertEqual(len(groups[0]["target"]), 4)

    def test_two_independent_groups_stay_separate(self):
        target = ["a", "b", "c", "d", "e", "f"]
        ours = ["b", "a", "c", "d", "f", "e"]
        groups = textorder.misordered_groups(target, ours)
        self.assertEqual([g["at"] for g in groups], [0, 4])

    def test_dead_stripped_helpers_are_intersected_OUT(self):
        """The false-positive class: 53 Matching units without this."""
        target = [(0x0, 4, "a"), (0x4, 4, "b")]
        ours = [(0x0, 4, "helper"), (0x4, 4, "a"), (0x8, 4, "b")]
        t_seq, o_seq, t_only, o_only = textorder.common_sequences(target, ours)
        self.assertEqual(t_seq, ["a", "b"])
        self.assertEqual(o_seq, ["a", "b"])
        self.assertEqual(o_only, ["helper"])
        self.assertEqual(t_only, [])
        self.assertEqual(textorder.misordered_groups(t_seq, o_seq), [])

    def test_intersection_does_not_mask_an_order_error(self):
        target = [(0x0, 4, "a"), (0x4, 4, "b"), (0x8, 4, "c")]
        ours = [(0x0, 4, "helper"), (0x4, 4, "c"), (0x8, 4, "b"), (0xc, 4, "a")]
        t_seq, o_seq, _, _ = textorder.common_sequences(target, ours)
        self.assertTrue(textorder.misordered_groups(t_seq, o_seq))


class LiveCalibration(unittest.TestCase):
    """The corpus halves. Skipped when the objects are not built."""

    @classmethod
    def setUpClass(cls):
        cls.units = textorder.all_units()
        if not cls.units:
            raise unittest.SkipTest("no unit object pairs built")
        cls.states = textorder.configure_states()

    def test_NEGATIVE_side_no_Matching_unit_is_flagged(self):
        flagged = [u for u in self.units
                   if self.states.get(u) == "Matching"
                   and textorder.check_unit(u)["verdict"] == "MISORDERED"]
        self.assertEqual(flagged, [], "false positives on the linked set")

    def test_POSITIVE_side_the_screen_is_not_vacuous(self):
        flagged = [u for u in self.units
                   if textorder.check_unit(u)["verdict"] == "MISORDERED"]
        self.assertGreater(len(flagged), 0,
                           "a screen that never fires proves nothing")

    def test_atree_is_the_recorded_positive(self):
        unit = "game/anim/atree"
        if unit not in self.units:
            self.skipTest("atree objects not built")
        result = textorder.check_unit(unit)
        if result["verdict"] == "ORDER-OK":
            self.skipTest("atree's order was fixed since the law was recorded")
        names = {n for g in result["groups"] for n in g["target"]}
        self.assertIn("AtreeFindNodeIdx", names)
        self.assertIn("AtreeSetEmpty", names)


class FinishTuPrecondition(unittest.TestCase):
    def test_finish_tu_runs_the_screen_and_blocks_on_it(self):
        text = (REPO / "tools" / "gdl" / "finish_tu.py").read_text(
            encoding="utf-8")
        self.assertIn("tools/gdl/textorder.py", text)
        self.assertIn("returncode == 1", text)

    def test_a_NO_PAIR_unit_does_not_block_a_flip(self):
        """Assembly-only units have no shared .text symbols; exit 2, not 1."""
        result = subprocess.run(
            [sys.executable, "tools/gdl/textorder.py",
             "TRK_MINNOW_DOLPHIN/ppc/Generic/exception"],
            cwd=str(REPO), capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 1, result.stdout)


if __name__ == "__main__":
    unittest.main()
