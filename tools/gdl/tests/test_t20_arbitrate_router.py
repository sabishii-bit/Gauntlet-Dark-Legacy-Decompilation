"""Run-50 item 4: the --arbitrate refusal becomes a router, plus --vs-head.

REPRODUCED VERBATIM at run-50 HEAD on game/anim/atree, following the
reported sequence (probe an edit, `--discard --whole-file`, re-apply the
same edit, `--arbitrate`):

    nothing to arbitrate: the working tree IS the banked rolling snapshot,
    so both halves would measure the same bytes. Edit first, or use --fuzzy
    for a single-state readout.                                    exit 1

THE MEASUREMENT THAT KILLED THE PROPOSED CURE.  The queue item asked for a
comparison against the BUILT OBJECT.  In that state the rolling snapshot
was byte-identical to the tree (48402 bytes both sides), so the two halves
compile to the SAME object: proceeding would have spent two builds to print
two identical rows.  What was actually missing was that the SESSION
BASELINE (48333 bytes) and HEAD both DIFFERED -- `--vs-baseline` would have
worked on the next line and the refusal never mentioned it.

TWO-SIDED, measured on that state:
  POSITIVE  3 candidate states, 2 usable; the router names both and the
            flag for each.  `--arbitrate --vs-head` then ran and printed
            `BANKED  (HEAD)  real 0  fuzzy 100.0000%`.
  NEGATIVE  when every candidate matches the tree there is genuinely
            nothing to compare, and the router still refuses (exit 1) and
            says so -- covered below, since that state cannot be produced
            on a tracked file without deleting the banks.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import probe                                                   # noqa: E402
from probe import state_router                                 # noqa: E402


TREE = b"edited source"
OTHER = b"a different source"


class ArbitrationRouter(unittest.TestCase):

    def test_it_names_every_usable_state_and_its_flag(self):
        states = [("(default)", "rolling snapshot", TREE, None),
                  ("--vs-baseline", "session baseline", OTHER, None),
                  ("--vs-head", "the TU's committed bytes at HEAD", OTHER,
                   None)]
        text = state_router(states, TREE, "rolling snapshot")
        self.assertIn("nothing to arbitrate against the rolling snapshot",
                      text)
        self.assertIn("re-run with --vs-baseline", text)
        self.assertIn("re-run with --vs-head", text)
        self.assertIn("cannot serve", text)
        self.assertNotIn("genuinely nothing to compare", text)

    def test_an_unavailable_state_says_why_not_just_that_it_is_absent(self):
        states = [("(default)", "rolling snapshot", TREE, None),
                  ("--vs-baseline", "session baseline", None,
                   "not banked yet (a BASELINE or IMPROVED probe banks one)"),
                  ("--vs-head", "HEAD", None, "unavailable (untracked file)")]
        text = state_router(states, TREE, "rolling snapshot")
        self.assertIn("UNAVAILABLE — not banked yet", text)
        self.assertIn("UNAVAILABLE — unavailable (untracked file)", text)
        # Nothing else can serve, so the reader must be told to stop.
        self.assertIn("genuinely nothing to compare", text)
        self.assertIn("--fuzzy", text)

    def test_all_states_equal_the_tree_is_still_a_refusal(self):
        states = [("(default)", "rolling snapshot", TREE, None),
                  ("--vs-baseline", "session baseline", TREE, None),
                  ("--vs-head", "HEAD", TREE, None)]
        text = state_router(states, TREE, "session baseline")
        self.assertIn("genuinely nothing to compare", text)
        self.assertNotIn("usable, re-run", text)

    def test_the_dead_end_wording_is_gone(self):
        # The old message named ONLY the state that could not serve. (The
        # phrase survives in `arbitration_states`' docstring, which QUOTES
        # it as the defect — so this pins the printed form.)
        source = Path(probe.__file__).read_text(encoding="utf-8")
        self.assertNotIn("nothing to arbitrate: the working tree IS", source)

    def test_vs_head_is_a_recognised_flag(self):
        self.assertIn("--vs-head", probe.KNOWN_FLAGS)


if __name__ == "__main__":
    unittest.main()
