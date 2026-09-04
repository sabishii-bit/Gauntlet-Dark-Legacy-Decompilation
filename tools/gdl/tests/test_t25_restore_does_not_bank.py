"""T25 run-55 item 5: a restore re-scores but banks nothing.

REPORTED (MC, run 54): "probe.py --revert restored the EDITED state ...
re-banked the edited tree as BASELINE anyway - overwriting the clean revert
point I had correctly banked first".

REPRODUCED at a5215d1d3 on zlib/adler32, both halves separately:
  * the first half stands. A clean BASELINE probe banked the pristine tree;
    a comment edit probed NEUTRAL (which banks, by design, and says so);
    `probe.py zlib/adler32 adler32 --revert` then printed `nothing to
    restore: adler32's banked snapshot IS the current working tree` and
    left the edit in the tree. The undo undid nothing.
  * the BASELINE half does NOT reproduce, and that is a design reversal
    reported rather than a second fix: `baseline_bank_decision` keeps the
    session baseline once it exists and REFUSES to create a first one over
    an edited tree (run-48 item 5), so no restore overwrote it.

What was live is that the restore path re-scores through the ordinary
banking block, so a function-scoped restore's post-restore tree — which can
still carry edits to OTHER functions the restore cannot reach — was written
into the rolling snapshot.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from probe import restore_suppresses_bank  # noqa: E402


class RestoreSuppressesBankTests(unittest.TestCase):
    def test_revert_is_a_restore(self):
        self.assertTrue(restore_suppresses_bank(["probe.py", "u", "f",
                                                 "--revert"]))

    def test_revert_best_is_a_restore(self):
        self.assertTrue(restore_suppresses_bank(["probe.py", "u", "f",
                                                 "--revert-best"]))

    def test_a_named_restore_is_a_restore(self):
        self.assertTrue(restore_suppresses_bank(["probe.py", "u", "f"],
                                                restore_tag="cleanbase"))

    def test_an_ordinary_probe_still_banks(self):
        """The half that must not change: the edit/probe loop's whole undo
        story is that BASELINE, IMPROVED and NEUTRAL bank a revert point."""
        self.assertFalse(restore_suppresses_bank(["probe.py", "u", "f"]))
        self.assertFalse(restore_suppresses_bank(["probe.py", "u", "f",
                                                  "--ops"]))

    def test_revert_baseline_is_not_matched_by_the_revert_membership_test(
            self):
        """It returns before the banking path, and the membership test is
        on whole arguments, not a prefix — so it must read False here
        rather than accidentally True."""
        self.assertFalse(restore_suppresses_bank(["probe.py", "u", "f",
                                                  "--revert-baseline"]))

    def test_discard_is_not_matched_either(self):
        self.assertFalse(restore_suppresses_bank(["probe.py", "u", "f",
                                                  "--discard"]))


if __name__ == "__main__":
    unittest.main()
