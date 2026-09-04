#!/usr/bin/env python3
"""T21 run-51 item 5: a discard that reverts NOTHING must not exit 0.

REPRODUCED at 7d8142f77 on game/movie/movieplayer, with one line added
inside `MoviePlayer::~MoviePlayer` (i.e. outside fn_800D967C):

    $ python tools/gdl/probe.py game/movie/movieplayer fn_800D967C \
          --discard --function --no-rebuild
    discarded (function-scoped): src\\game\\movie\\movieplayer.cpp — 0
    hunk(s) inside fn_800D967C reverted; 1 hunk(s) elsewhere in the TU left
    untouched
    REVERT IS PARTIAL — edits this revert could NOT reach remain in the
    working tree:
        src/game/movie/movieplayer.cpp (+1/-0 vs HEAD)
    EC=0
    --- diff still present? ---
    1       0       src/game/movie/movieplayer.cpp

The prose was accurate and the EXIT CODE said "done". `--revert` in the same
shape already exited PARTIAL_RESTORE_EXIT (5), so the two spellings of one
operation disagreed about whether declining is a success.

TWO-SIDED CALIBRATION over the EXHAUSTIVE state space of the `--discard`
path — the population here is the tool's own four states, not the function
corpus — measured live on game/movie/movieplayer
(T21_scratch/t21_discard_states.py):

    state                        exit  dirty-after
    A --function, edit OUTSIDE      6  YES   <- POSITIVE: was 0, reverted none
    B --function, edit INSIDE       0  no    <- NEGATIVE: unchanged
    C whole-file, tree differs      0  no    <- NEGATIVE: unchanged
    D whole-file, tree IS HEAD      6  no    <- POSITIVE: was 0, reverted none

State B is the DOCUMENTED CONTRACT of `--discard --function` — revert this
function, leave the siblings — so it stays 0 even though the tree is not
HEAD afterwards. Only "reverted nothing" changed.

NOT CHANGED, AND REPORTED INSTEAD (AGENTS discipline 18, the sibling screen):
`--revert` when the banked snapshot IS the working tree prints "nothing to
restore: ... IS the current working tree (NEUTRAL probes bank too)" and then
FALLS THROUGH to an ordinary re-score, so its exit code carries the VERDICT's
meaning (conflict_gate writes it) and is not free to carry a restore's. That
is a third instance of the same shape and it needs a decision about what a
verdict-bearing exit code means, not a one-line change.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class ExitCodes(unittest.TestCase):
    def test_the_declined_code_is_distinct_from_every_other(self):
        codes = {probe.NOTHING_RESTORED_EXIT, probe.PARTIAL_RESTORE_EXIT,
                 probe.FUZZY_NO_NUMBER_EXIT, 0, 1, 2}
        self.assertEqual(len(codes), 6)

    def test_declining_is_nonzero(self):
        self.assertNotEqual(probe.NOTHING_RESTORED_EXIT, 0)

    def test_it_is_not_the_partial_restore_code(self):
        # "reverted some, others survive" and "reverted none" are different
        # answers to the retry question.
        self.assertNotEqual(probe.NOTHING_RESTORED_EXIT,
                            probe.PARTIAL_RESTORE_EXIT)


class ScopedRevertNotes(unittest.TestCase):
    """The measurement the exit code is derived from."""

    SNAP = ("void a(void) {\n    int x;\n}\n\n"
            "void b(void) {\n    int y;\n}\n")

    def test_an_edit_outside_the_named_function_reverts_nothing(self):
        current = self.SNAP.replace("int y;", "int y;\n    int z;")
        new_text, notes = probe.scoped_revert(self.SNAP, current, "a")
        self.assertEqual(new_text, current)      # nothing reverted
        self.assertIn("0 hunk(s) inside a reverted", notes)

    def test_an_edit_inside_the_named_function_is_reverted(self):
        current = self.SNAP.replace("int x;", "int x;\n    int w;")
        new_text, notes = probe.scoped_revert(self.SNAP, current, "a")
        self.assertEqual(new_text, self.SNAP)
        self.assertIn("1 hunk(s) inside a reverted", notes)

    def test_the_declined_case_is_exactly_new_text_equals_current(self):
        # This equality IS the discriminant the discard path uses; asserting
        # it here keeps the two from drifting apart.
        current = self.SNAP.replace("int y;", "int y;\n    int z;")
        new_text, _notes = probe.scoped_revert(self.SNAP, current, "a")
        self.assertTrue(new_text == current)
        current2 = self.SNAP.replace("int x;", "int x;\n    int w;")
        new_text2, _n = probe.scoped_revert(self.SNAP, current2, "a")
        self.assertFalse(new_text2 == current2)


class HelpText(unittest.TestCase):
    def test_the_behaviour_is_documented_where_discard_is(self):
        doc = probe.__doc__
        self.assertIn("A DISCARD THAT REVERTS NOTHING EXITS 6", doc)
        self.assertIn("still exits 0", doc)


if __name__ == "__main__":
    unittest.main()
