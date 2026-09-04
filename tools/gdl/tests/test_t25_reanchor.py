"""T25 run-55 item 7: supply a baseline's missing fuzzy anchor in place.

REPORTED (SP, run 54): "defake_gate demands fuzzy arbitration it makes
impossible - the baseline file stores no fuzzy" and "the unpriced
section(s) advice is unactionable mid-pass: re-taking discards the anchor".

REPRODUCED at 5ff787149 on game/sys/memcard, through the ordinary loop:
  1. `defake_gate.py baseline game/sys/memcard --at-head` -> `fuzzy anchor
     from the current report` (the healthy case).
  2. `defake_gate.py check game/sys/memcard --rebuild` — which rebuilds the
     OBJECT and not the report, as its own docstring says.
  3. `defake_gate.py baseline game/sys/memcard --at-head` -> `no fuzzy
     anchor — re-take with --arbiter fuzzy to enable the fuzzy arbiter`.
Then any disputed row prints "this baseline carries no fuzzy anchor ...
re-take the baseline", and re-taking mid-pass discards the comparison point
the pass is being measured against.

`reanchor` writes ONLY the missing numbers. The soundness condition is that
the source has not moved since the baseline — otherwise the fuzzy read
describes the edit, and the arbiter would compare it against itself.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from defake_gate import reanchorable, _fuzzy_note  # noqa: E402


class ReanchorableTests(unittest.TestCase):
    def test_an_unmoved_tree_may_be_reanchored(self):
        ok, reason = reanchorable({"source_sha1": "abc123"}, "abc123")
        self.assertTrue(ok)
        self.assertEqual(reason, "")

    def test_a_moved_source_is_refused(self):
        """The whole soundness argument: a fuzzy read after an edit belongs
        to the edit, so anchoring it would compare the edit to itself."""
        ok, reason = reanchorable({"source_sha1": "abc123"}, "def456")
        self.assertFalse(ok)
        self.assertIn("MOVED", reason)
        self.assertIn("abc123 -> def456", reason)
        self.assertIn("probe.py", reason)

    def test_a_baseline_with_no_source_sha1_is_refused(self):
        ok, reason = reanchorable({}, "abc123")
        self.assertFalse(ok)
        self.assertIn("source_sha1", reason)

    def test_an_unreadable_source_is_refused(self):
        ok, reason = reanchorable({"source_sha1": "abc123"}, None)
        self.assertFalse(ok)
        self.assertIn("could not be read", reason)

    def test_a_missing_meta_is_refused_rather_than_crashing(self):
        ok, _reason = reanchorable(None, "abc123")
        self.assertFalse(ok)


class FuzzyAnchorMessageTests(unittest.TestCase):
    def test_the_no_anchor_message_is_what_reanchor_answers(self):
        delta, note = _fuzzy_note(None, 97.5)
        self.assertIsNone(delta)
        self.assertIn("no fuzzy anchor", note)

    def test_an_anchored_baseline_produces_a_delta(self):
        delta, note = _fuzzy_note(95.0, 97.5)
        self.assertAlmostEqual(delta, 2.5)
        self.assertIn("95.0000 -> 97.5000", note)


if __name__ == "__main__":
    unittest.main()
