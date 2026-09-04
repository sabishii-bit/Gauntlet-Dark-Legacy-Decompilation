"""probe says at PROBE time whether the revert loop can reach the edit.

Run-52 item 7. probe's restore family is function-scoped and says so
correctly — at RESTORE time. NC's lane matched a function whose body is
produced by static helpers defined outside its own span, so every edit
landed outside that span; `--revert-best` refused at the end of the lane,
correctly, and "the revert loop was silently unavailable all lane". The
information existed from the first probe.

DESIGN REVERSAL, forced by calibration. The first design was a BASELINE
warning keyed on "this function's body contains inlined static helpers".
Measured image-wide over 310 units: 479 (unit, function) pairs call a
static helper at all and 315 of them — 65.8% — would have fired it. That
is a constant line, not a warning, and the detector also mis-parsed
`static void (*fn)(...)` declarations as a helper literally named `void`.
The shipped trigger is the CONDITION itself (`restore_scope_counts`
against the snapshot a restore would use), so it fires only when a restore
would come up short and is silent on the body-only edit —
`coupled_scope_survivors` measures that case as four fifths of single-TU
edits.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from probe import restore_scope_counts, revert_reach_warning  # noqa: E402

BASE = """\
static int helper(int x)
{
    return x + 1;
}

void target(int *p)
{
    *p = helper(*p);
}

void sibling(void)
{
}
"""

EDIT_IN_HELPER = BASE.replace("return x + 1;", "return x + 2;")
EDIT_IN_TARGET = BASE.replace("*p = helper(*p);", "*p = helper(*p) + 1;")
EDIT_IN_BOTH = EDIT_IN_HELPER.replace("*p = helper(*p);",
                                      "*p = helper(*p) + 1;")
EDIT_IN_SIBLING = BASE.replace("void sibling(void)\n{\n}",
                               "void sibling(void)\n{\n    int q = 1;\n}")


class RevertReachTests(unittest.TestCase):
    def warn(self, current, fn="target"):
        return revert_reach_warning(
            fn, restore_scope_counts(BASE, current, fn))

    def test_an_edit_inside_the_function_says_nothing(self):
        self.assertEqual(self.warn(EDIT_IN_TARGET), "")

    def test_an_unchanged_tree_says_nothing(self):
        self.assertEqual(self.warn(BASE), "")

    def test_an_edit_only_in_the_inlined_helper_is_out_of_reach(self):
        text = self.warn(EDIT_IN_HELPER)
        self.assertIn("REVERT REACHES NONE OF THIS EDIT", text)
        self.assertIn("--revert --whole-file", text)
        self.assertIn("FUNCTION-SCOPED", text)

    def test_a_split_edit_reports_both_halves(self):
        text = self.warn(EDIT_IN_BOTH)
        self.assertIn("REVERT REACHES PART OF THIS EDIT", text)
        self.assertIn("1 hunk(s) are inside target", text)

    def test_a_sibling_edit_also_fires_because_a_restore_misses_it(self):
        # Same mechanism, different cause: the warning is about REACH, not
        # about inlining, so it is honest on any out-of-span hunk.
        self.assertIn("REVERT REACHES NONE",
                      self.warn(EDIT_IN_SIBLING))

    def test_an_unlocatable_function_produces_no_claim(self):
        # restore_scope_counts returns None; a warning would be a guess.
        self.assertIsNone(restore_scope_counts(BASE, EDIT_IN_HELPER, "nope"))
        self.assertEqual(revert_reach_warning("nope", None), "")

    def test_the_score_is_not_disputed_only_the_undo(self):
        text = self.warn(EDIT_IN_HELPER)
        self.assertIn("The score below is real", text)


if __name__ == "__main__":
    unittest.main()
