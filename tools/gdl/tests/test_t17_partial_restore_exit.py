#!/usr/bin/env python3
"""T17 run-47 item 3: a partial --revert-best must refuse in the EXIT CODE too.

THE OBSERVATION, and what reproducing it actually showed. The item reported a
"silent partial restore" that re-scored the wrong state twice. Reproduced at
65348245e on game/anim/atree::AtreeMatch, in both shapes a partial restore
comes in:

  helper-lift (a static helper hoisted to file scope, the shape the item
  names): probe printed THREE loud blocks -- COUPLED FILE-SCOPE HALF, REVERT
  IS PARTIAL, REFUSED TO BANK -- and exited 0.
  sibling-body edit only: two blocks (no coupled warning, correctly, since no
  file-scope item moved), REFUSED TO BANK, and exited 0.

So the restore was never SILENT -- run-45 item 6 had already closed that half.
What survived is the pair of things a reader and a script actually go by:

  (a) exit 0. Every OTHER probe refusal carries in the exit code -- the
      straddling-hunk ValueError in `scoped_revert` exits 1, an unknown flag
      or a contradictory --function/--whole-file pair exits 2, a fuzzy readout
      with no number exits 4. This one alone was indistinguishable from a
      banked probe.
  (b) the headline read `READOUT   real 0 (insns T44/O44, multiset 0t)`,
      printed ABOVE the refusal -- the same shape, column and width as
      `BASELINE  real 0 (insns T44/O44, multiset 0t)`. A worker scanning for
      the score found something that looked exactly like one.

Two-sided calibration, all four cases run live against the same bank:

  POSITIVE  helper-lift partial       -> refuses, exit 5
  POSITIVE  sibling-body partial      -> refuses, exit 5
  NEGATIVE  complete restore          -> banks, exit 0 ("0 hunk(s) elsewhere")
  NEGATIVE  straddling hunk           -> refuses BEFORE the build, exit 1
                                         (unchanged; this is the shape copied)
"""

import re
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class ExitCode(unittest.TestCase):
    def test_the_partial_restore_code_is_its_own(self):
        self.assertEqual(probe.PARTIAL_RESTORE_EXIT, 5)
        self.assertNotIn(probe.PARTIAL_RESTORE_EXIT,
                         (0, 1, 2, 3, probe.FUZZY_NO_NUMBER_EXIT))

    def test_the_franken_site_returns_it(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        after = text[text.index("if franken_restore is not None:"):]
        body = after[:after.index("\n    state = {}")]
        self.assertIn("return PARTIAL_RESTORE_EXIT", body)
        self.assertNotIn("return 0", body)

    def test_the_straddling_hunk_refusal_still_exits_1(self):
        """The shape this item copies -- it must not have moved."""
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        marker = 'print(f"REFUSED (function-scoped revert): {err}")'
        self.assertIn(marker, text)
        self.assertIn(marker + "\n                return 1", text)


class RefusalText(unittest.TestCase):
    def setUp(self):
        self.text = probe.franken_readout_refusal(
            "AtreeMatch", "the BEST-scoring banked state",
            "1 hunk(s) inside AtreeMatch reverted; 1 hunk(s) elsewhere in the"
            " TU left untouched", 0, "T44/O44", ", multiset 0t")

    def test_the_refusal_leads(self):
        self.assertTrue(self.text.startswith("REFUSED (partial restore):"))

    def test_the_headline_is_not_shaped_like_a_verdict(self):
        """`READOUT   real 0 (insns T44/O44...)` was one column off a
        BASELINE line. Nothing in the refusal may match that shape."""
        verdict_shape = re.compile(r"^\w+\s+real \d+ \(insns ")
        for line in self.text.splitlines():
            self.assertIsNone(verdict_shape.match(line), line)

    def test_the_number_is_still_reported_and_labelled(self):
        self.assertIn("NOT A VERDICT — real 0 (insns T44/O44, multiset 0t)",
                      self.text)
        self.assertIn("exists in no bank and in no commit", self.text)

    def test_the_exit_code_is_named_in_the_text(self):
        self.assertIn(f"Exit code {probe.PARTIAL_RESTORE_EXIT}", self.text)

    def test_the_escape_hatches_are_named(self):
        self.assertIn("--whole-file", self.text)
        self.assertIn("no restore flag", self.text)


class PartialityDiscriminant(unittest.TestCase):
    """`restore_is_partial` is the trigger; both sides of it are measured."""

    SNAP = ("static int helper(int i) { return i; }\n"
            "int a(void) { return helper(1); }\n"
            "int b(void) { return 2; }\n")

    def test_NEGATIVE_a_complete_restore_is_not_partial(self):
        self.assertFalse(probe.restore_is_partial(self.SNAP, self.SNAP))

    def test_POSITIVE_a_helper_lift_leaves_a_file_scope_hunk(self):
        lifted = self.SNAP.replace("static int helper(int i) { return i; }",
                                   "static int helper(int i, int j)"
                                   " { return i + j; }")
        self.assertTrue(probe.restore_is_partial(self.SNAP, lifted))

    def test_POSITIVE_a_sibling_body_edit_is_partial_too(self):
        sibling = self.SNAP.replace("return 2", "return 22")
        self.assertTrue(probe.restore_is_partial(self.SNAP, sibling))

    def test_only_the_helper_lift_is_a_COUPLED_file_scope_survivor(self):
        """The coupled warning must not fire on a plain sibling-body edit --
        confirmed live on atree::AtreeListLock, which produced REVERT IS
        PARTIAL with no coupled block."""
        sibling = self.SNAP.replace("return 2", "return 22")
        self.assertEqual(probe.coupled_scope_survivors(self.SNAP, sibling), [])
        lifted = self.SNAP.replace("static int helper(int i)",
                                   "static int helper(int i, int j)")
        self.assertNotEqual(probe.coupled_scope_survivors(self.SNAP, lifted),
                            [])


class Documentation(unittest.TestCase):
    def test_the_usage_text_states_the_exit_code(self):
        self.assertIn("EXITS 5", probe.__doc__)


if __name__ == "__main__":
    unittest.main()
