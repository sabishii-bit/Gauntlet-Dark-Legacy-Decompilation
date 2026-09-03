#!/usr/bin/env python3
"""T18 run-48 item 6: savedregs refuses a proximity guess as UNRESOLVED.

THE DEFECT. `--per-web` pairs live ranges by ROLE and then takes the NEAREST
candidate. A role is a short string -- `li 0` is every zero-initialisation in
the function -- so where several candidates in DIFFERENT registers matched,
the tool returned the nearest one AS A REGISTER MAPPING, with no signal that
it had chosen. A confident wrong attribution of that shape carried a whole
hypothesis.

Reproduced live on game/anim/action::DoPlayerAction: the target's
`@0x88 li r19,0` has exactly two role-matching candidates, r24 and r18, and
BOTH sit 24 aligned rows away. The old code printed `PERMUTED r19->r24`.

THE DISCRIMINANT WAS CHOSEN FROM THREE MEASURED CANDIDATES, over every
live-range row in the tree (17,688 rows, 2,998 function pairs) at bb44ef4ab:

    A  tie-at-the-minimum only          3 rows /   3 fns (0.02%)
    B  distance-0-decisive  (SHIPPED)  98 rows /  52 fns (0.55%)
    C  any window disagreement        389 rows / 115 fns (2.20%)

C is REFUTED by the corpus, not merely noisy: it turns all four of the rows
claim.law.T12_pairing-callee-saved-definitions-by-ordinal-within-a-register-
fails-exactly-when-a-lifetime-moves.20260903.v1 names in its expiry check
(`--per-web` must keep naming r21->r19 x2 and r19->r12 x2) into refusals,
and `fnasm game/movie/movieplayer fn_800D8BCC 0x1c0:0x260 --diff` shows all
four are CORRECT. Each has a UNIQUE candidate at distance 0. A is too narrow:
an exact numeric tie is an accident, and 95 pure-proximity mappings survive
it. B refuses exactly the rows where nothing sits at the aligned position and
the window's candidates disagree.

Under B the shipped verdict census moves PERMUTED 1365->1327, ESCAPED
228->195, INTRUDER 158->134, and 17,590 of 17,688 rows keep their verdict.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import savedregs as sr  # noqa: E402


def cand(position, register):
    """A candidate tuple in _definition_index's shape."""
    return (position, register, (position * 4, f"li {register},0"))


class Ambiguity(unittest.TestCase):
    def test_one_candidate_is_never_ambiguous(self):
        self.assertFalse(sr.is_ambiguous([cand(10, "r19")], 10))
        self.assertFalse(sr.is_ambiguous([cand(30, "r19")], 10))

    def test_candidates_in_one_register_are_never_ambiguous(self):
        # Whichever is chosen, the MAPPING is the same, so nothing is being
        # guessed that the row reports.
        self.assertFalse(sr.is_ambiguous(
            [cand(12, "r19"), cand(20, "r19")], 10))

    def test_a_unique_candidate_at_the_aligned_position_decides(self):
        # The fn_800D8BCC shape the T12 law's expiry check depends on:
        # r19 at distance 0, alternatives 4 and 5 rows out.
        self.assertFalse(sr.is_ambiguous(
            [cand(10, "r19"), cand(14, "r4"), cand(15, "r6")], 10))

    def test_no_candidate_at_the_aligned_position_and_a_disagreement(self):
        # The DoPlayerAction shape: r24 and r18, both 24 rows away.
        self.assertTrue(sr.is_ambiguous(
            [cand(34, "r24"), cand(-14, "r18")], 10))

    def test_two_candidates_at_distance_zero_in_different_registers(self):
        # The aligned position cannot decide between them either.
        self.assertTrue(sr.is_ambiguous(
            [cand(10, "r19"), cand(10, "r24")], 10))

    def test_an_empty_candidate_set_is_not_ambiguous(self):
        self.assertFalse(sr.is_ambiguous([], 10))


class Contenders(unittest.TestCase):
    def test_only_the_closest_survive(self):
        near = [cand(12, "r19"), cand(12, "r24"), cand(20, "r18")]
        self.assertEqual({c[1] for c in sr.contenders(near, 10)},
                         {"r19", "r24"})

    def test_empty_in_empty_out(self):
        self.assertEqual(sr.contenders([], 10), [])


class Window(unittest.TestCase):
    def test_the_window_bounds_the_candidate_set(self):
        near = sr.in_window([cand(10, "r19"), cand(10 + sr.LIFETIME_WINDOW,
                                                   "r24"),
                             cand(10 + sr.LIFETIME_WINDOW + 1, "r18")], 10)
        self.assertEqual([c[1] for c in near], ["r19", "r24"])


class Note(unittest.TestCase):
    def test_the_note_names_every_candidate_with_its_distance(self):
        anchor = cand(10, "r19")
        note = sr.unresolved_note(anchor, [cand(34, "r24"), cand(-14, "r18")])
        self.assertIn("r24 (24 aligned rows away)", note)
        self.assertIn("r18 (24 aligned rows away)", note)
        self.assertIn("no candidate sits at the aligned position", note)
        self.assertIn("proximity GUESS", note)
        self.assertIn("--diff", note)

    def test_a_split_at_the_aligned_position_says_which_case_it_is(self):
        anchor = cand(10, "r19")
        note = sr.unresolved_note(anchor, [cand(10, "r24"), cand(10, "r18")])
        self.assertIn("share the aligned position", note)


class Summary(unittest.TestCase):
    def test_unresolved_is_counted_in_its_own_bucket(self):
        pairs = [("r19[1]", None, None, "UNRESOLVED", ""),
                 ("r21[0]", None, None, "PERMUTED r21->r19", ""),
                 ("r22[0]", None, None, "in place", "")]
        counts, _maps = sr.lifetime_summary(pairs)
        self.assertEqual(counts["unresolved"], 1)
        self.assertEqual(counts["permuted"], 1)
        # And NOT swept into unpaired, which is where an unknown head lands.
        self.assertEqual(counts["unpaired"], 0)


class LiveReproduction(unittest.TestCase):
    """Both halves, against the built objects."""

    def _pairs(self, unit, fn):
        import fnasm
        t_rows, _a, err_t = fnasm.parse_fn(unit, fn, ours=False)
        o_rows, _b, err_o = fnasm.parse_fn(unit, fn, ours=True)
        if err_t or err_o or not t_rows or not o_rows:
            self.skipTest(f"{unit}::{fn} objects not built")
        return sr.lifetime_pairs(t_rows, o_rows)

    def test_doplayeraction_li_zero_row_is_refused(self):
        verdicts = [v for _l, _t, _o, v, _n
                    in self._pairs("game/anim/action", "DoPlayerAction")]
        self.assertIn("UNRESOLVED", verdicts)

    def test_the_t12_laws_expiry_rows_survive(self):
        # claim.law.T12_...20260903.v1: `--per-web` must keep naming
        # r21->r19 x2 and r19->r12 x2. Discriminant C broke exactly this.
        verdicts = [v for _l, _t, _o, v, _n
                    in self._pairs("game/movie/movieplayer", "fn_800D8BCC")]
        self.assertEqual(
            sum(1 for v in verdicts if v.startswith("PERMUTED r21->r19")), 2)
        self.assertEqual(
            sum(1 for v in verdicts if v.startswith("ESCAPED r19->r12")), 2)
        self.assertEqual(
            sum(1 for v in verdicts if v.startswith("UNRESOLVED")), 0)


if __name__ == "__main__":
    unittest.main()
