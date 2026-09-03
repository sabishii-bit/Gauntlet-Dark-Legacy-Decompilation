#!/usr/bin/env python3
"""T18 run-48 item 5: a first SESSION BASELINE over an edited tree is REFUSED.

THE DEFECT, third sighting. probe banks whatever state it FIRST sees per
function, and the session baseline is the one revert point nothing ever
overwrites -- its entire meaning is "the state before your edits". A first
probe run after an edit wrote the EDITED bytes into it and then WARNED about
having done so. Reproduced at c3f3aea99 on game/ui/btext::DrawGlowText with
`u8 unused[8]` -> `[16]` in the tree:

    [session baseline banked: probe.py --revert-baseline restores THIS state]
    WARNING: BASELINE BANKED FROM AN EDITED TREE - this unit's source differs
    from HEAD by 1 line(s), so the 'baseline' revert point is NOT the pre-edit
    state: --revert-baseline will restore YOUR EDITS. ...

Two true lines that contradict each other, the receipt first, and the bank
already done by the time the warning printed.

TWO-SIDED CALIBRATION at bb44ef4ab (T18_scratch/t18_calib_item5.py):

    NEGATIVES  11 first probes on a CLEAN tree across 11 different units
               (btext, zlib/inflate, gutil, camera, bosscam, message,
               pb_diag, enemy, action, worldcol, vsprintf):
               11 baselines CREATED, 0 refusals. One skipped as unscorable.
    POSITIVE   the same probe with the edit in the tree: baseline written =
               False, refusal printed = True; with --force-baseline, written
               = True. `--revert-baseline` after the refusal exits 1 with
               "no session baseline banked for this unit" rather than
               restoring the edit.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class FirstBaselineOverAnEditedTree(unittest.TestCase):
    def test_a_clean_tree_still_creates_the_baseline(self):
        action, note = probe.baseline_bank_decision(
            "BASELINE", False, changed_lines=0)
        self.assertEqual(action, "create")
        self.assertIn("session baseline banked", note)

    def test_a_divergent_tree_refuses(self):
        action, note = probe.baseline_bank_decision(
            "BASELINE", False, changed_lines=1,
            unit="game/ui/btext", fn="DrawGlowText")
        self.assertEqual(action, "refuse")
        self.assertIn("SESSION BASELINE REFUSED", note)
        self.assertIn("differs from HEAD by 1 line(s)", note)

    def test_the_refusal_names_both_ways_out(self):
        _action, note = probe.baseline_bank_decision(
            "BASELINE", False, changed_lines=3,
            unit="game/ui/btext", fn="DrawGlowText")
        self.assertIn("--discard", note)
        self.assertIn("--force-baseline", note)
        # And says what DID happen, so a lane is not left thinking the probe
        # banked nothing at all.
        self.assertIn("ROLLING revert point WAS banked", note)

    def test_force_baseline_creates_it(self):
        action, _note = probe.baseline_bank_decision(
            "BASELINE", False, changed_lines=1, force=True)
        self.assertEqual(action, "create")

    def test_rebaseline_counts_as_the_same_intent(self):
        self.assertIn("--rebaseline", probe.BASELINE_FORCE_FLAGS)
        self.assertIn("--force-baseline", probe.BASELINE_FORCE_FLAGS)

    def test_a_neutral_first_bank_is_refused_the_same_way(self):
        # The run-36 change made the FIRST bank create the baseline whatever
        # verdict caused it, so the NEUTRAL path reaches the same file and
        # needs the same guard.
        action, _note = probe.baseline_bank_decision(
            "NEUTRAL", False, changed_lines=2)
        self.assertEqual(action, "refuse")

    def test_an_unmeasurable_comparison_still_banks(self):
        # Deliberately narrow: a brand-new untracked TU has no HEAD bytes to
        # be the pre-edit state, and refusing there would deny a baseline to
        # work that never had the defect. It keeps the old warning.
        action, _note = probe.baseline_bank_decision(
            "BASELINE", False, changed_lines=None)
        self.assertEqual(action, "create")
        self.assertIn("UNMEASURED",
                      probe.bank_warning("BASELINE", None))

    def test_an_existing_baseline_is_untouched_by_the_guard(self):
        # The refusal is about CREATING the first one. An existing baseline
        # is kept (and --rebaseline still overwrites it) whatever the tree.
        self.assertEqual(
            probe.baseline_bank_decision("BASELINE", True,
                                         changed_lines=9)[0], "keep")
        self.assertEqual(
            probe.baseline_bank_decision("BASELINE", True, rebaseline=True,
                                         changed_lines=9)[0], "overwrite")

    def test_the_flag_is_known_to_the_typo_screen(self):
        # An unknown flag is refused by probe's own screen, so a new escape
        # that is not registered reads as a typo and the run stops.
        self.assertIn("--force-baseline", probe.KNOWN_FLAGS)
        self.assertEqual(probe.unknown_flags(["--force-baseline"]), [])


if __name__ == "__main__":
    unittest.main()
