"""`--no-bank` must not move the BEST anchor; `--discard` must not rank
`--function` above `--whole-file` when outside hunks exist (run-53 item 7).

Item 7 carried three sub-observations, all cited to
claim.law.BG_arbitrate-leaves-a-stale-object-and-no-bank-still-moves-the-best-
anchor.20260904.v1. Reproduced one at a time at c7b741799 on
game/ps2/fakelib::sceOpen with a `volatile int` local added (real 24 -> 44),
the file restored byte-identical afterwards:

(a) `--arbitrate` LEAVING A STALE OBJECT: **does not reproduce.** The rebuild
    landed with --arbitrate itself (commit cd480723c). The object after
    --arbitrate is byte-identical to the one built from the EDITED source and
    the following `ninja` correctly prints `no work to do`. What DOES hold is
    the law's own second sentence: the rebuild was SILENT on success, so the
    only line printed was `[working tree restored to your edited state]`,
    which says nothing about the object — and a whole run applied a manual
    touch-and-rebuild before every inspection because of it. A guarantee that
    prints nothing cannot be told from one that did not run.

(b) `--no-bank` MOVING THE BEST ANCHOR: **reproduces, with a precondition
    neither the law nor the queue item states.** The verdict must be
    BASELINE:
      anchor present -> `REGRESSED vs best 24`, best_real stays 24   (correct)
      after --reset  -> `BASELINE real 44`,     best_real None -> 44 (the bug)
    The first half was measured before the second and does NOT reproduce; a
    cure written to "stop --no-bank banking" without that precondition would
    have been aimed at a path that already behaves.

(c) `--discard`'s refusal offering `--function` first: reproduces by reading
    the refusal text, and the cure is a REORDERING plus the ambiguity said
    out loud — see DiscardRefusalTests.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import probe  # noqa: E402


class NoBankAnchorTests(unittest.TestCase):
    def classify(self, state, real, **kwargs):
        return probe.classify(dict(state), real, "T99/O99", 0, **kwargs)

    def test_a_baseline_with_no_bank_leaves_no_anchor(self):
        """The reproduced case: no prior anchor, so the verdict is BASELINE
        and bank_best() used to run despite the flag."""
        verdict, state = self.classify({}, 44, no_bank=True)
        self.assertTrue(verdict.startswith("BASELINE"), verdict)
        self.assertIsNone(state.get("best_real"))

    def test_a_baseline_without_no_bank_still_anchors(self):
        """The NEGATIVE side: the ordinary path is untouched, because the
        BASELINE anchor is the only revert point a session has."""
        verdict, state = self.classify({}, 44)
        self.assertTrue(verdict.startswith("BASELINE"), verdict)
        self.assertEqual(state.get("best_real"), 44)

    def test_an_improvement_under_no_bank_does_not_move_the_anchor(self):
        _verdict, state = self.classify(
            {"best_real": 44, "best_multiset": 0, "best_insns": "T99/O99"},
            24, no_bank=True)
        self.assertEqual(state.get("best_real"), 44)

    def test_a_worse_state_never_banked_either_way(self):
        """Measured first and it did NOT reproduce: with an anchor present a
        worse state classifies REGRESSED and banks nothing already."""
        for flag in (True, False):
            _verdict, state = self.classify(
                {"best_real": 24, "best_multiset": 0, "best_insns": "T99/O99"},
                44, no_bank=flag)
            self.assertEqual(state.get("best_real"), 24)


class DiscardRefusalTests(unittest.TestCase):
    def refusal(self, entangled):
        inside = 1
        outside = len([r for r in entangled if r[0] == "outside"])
        return probe.discard_refusal("alpha", "game/x/y.c", inside, outside,
                                     entangled)

    def test_whole_file_is_offered_before_function_when_work_is_outside(self):
        """The helper-lift shape: an edit that lifts a helper OUT of the
        function leaves the new definition outside it, so `--function`
        restores the call site and STRANDS the helper — half the edit, and a
        tree that is neither the probe nor HEAD."""
        text = self.refusal([("outside", 40, 52)])
        self.assertLess(text.index("--discard --whole-file"),
                        text.index("--discard --function"))
        self.assertIn("helper lifted", text)
        self.assertIn("neither the probe nor HEAD", text)

    def test_the_ambiguity_is_stated_rather_than_decided(self):
        """probe knows WHERE the hunks are, never WHOSE they are."""
        text = self.refusal([("outside", 40, 52)])
        self.assertIn("probe CANNOT see", text)
        self.assertIn("git diff", text)
        self.assertIn("SIBLING", text)

    def test_a_straddling_hunk_still_says_function_will_refuse(self):
        text = self.refusal([("straddling", 12, 13)])
        self.assertIn("WILL REFUSE while a straddling hunk", text)

    def test_a_straddling_only_refusal_does_not_gain_the_ambiguity_note(self):
        """The NEGATIVE side: only the outside-hunk shape changes wording."""
        text = self.refusal([("straddling", 12, 13)])
        self.assertNotIn("probe CANNOT see", text)

    def test_both_classes_are_still_counted_from_their_own_list(self):
        text = self.refusal([("outside", 40, 52), ("straddling", 12, 13)])
        self.assertIn("1 uncommitted hunk(s) lie OUTSIDE alpha", text)
        self.assertIn("1 hunk(s) STRADDLE", text)


if __name__ == "__main__":
    unittest.main()
