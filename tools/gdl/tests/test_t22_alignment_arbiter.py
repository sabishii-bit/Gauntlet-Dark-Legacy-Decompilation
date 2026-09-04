"""The structure arbiter is UNDEFINED across an alignment change (run 52).

`regnorm.analyze` can only emit a STRUCTURAL row for a PAIRED pair — an
instruction the aligner leaves unpaired lands in UNPAIRED-T/UNPAIRED-O and
is never counted — so `genuine` is a count over the paired subsequence and
comparing it across two states with different unpaired counts compares two
different domains.

Two run-51 sightings, one root, both on IMPROVING edits that the gate
printed as `genuine structural rows ... ROSE — structure agrees with real`:

  ProcessCritterList (attempt.CR_processcritterlist-pointer-materialisation-
  position-is-one-object-across-three-construct-classes.20260904.v1)
      regnorm HEAD  `57 paired, ... 5 STRUCTURAL (0 genuine), 9 unpaired`
      regnorm KEEP  `60 paired, ... 7 STRUCTURAL (2 genuine), 3 unpaired`
      real 25 -> 29, fresh fuzzy 84.0816% -> 94.5918%

  dcsHandleRequest (claim.law.NC_source-order-picks-which-of-two-equal-
  constants-is-rematerialised-and-which-is-copied.20260904.v1)
      "regnorm scored the misaligned BEFORE state as 0 structural rows
      because that region contained UNPAIRED instructions"
      real 144 -> 146, genuine 0 -> 1, fresh fuzzy 98.6618% -> 99.0024%

The first two classes below are those two cases as data. The third is the
falsifier for the ANALYZER claim the design rests on, driven through
regnorm itself so it fails if that ever stops being true.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import regnorm  # noqa: E402
from defake_gate import arbitrate_regressions  # noqa: E402


def no_ops(_unit, _name):
    return "  opcode multiset: DIFFERS  target-only: +1 b  ours-only: -1 beq"


def arb(verdicts, unit, baseline, genuine_now, unpaired_now=None,
        fuzzy_now=None):
    return arbitrate_regressions(
        list(verdicts), unit, baseline,
        genuine_fn=lambda _u, _n: genuine_now,
        unpaired_fn=(None if unpaired_now is None
                     else (lambda _u, _n: unpaired_now)),
        ops_fn=no_ops,
        fuzzy_fn=(None if fuzzy_now is None
                  else (lambda _u, _n: fuzzy_now)),
        arbiter=(None if fuzzy_now is None else "fuzzy"))


class ProcessCritterListTests(unittest.TestCase):
    """Costume 1: unpaired 9 -> 3 printed as `ROSE — structure agrees`."""

    VERDICTS = [("ProcessCritterList", "REGRESSION", "real 25 -> 29")]
    BASE = {"ProcessCritterList": {"genuine": 0, "unpaired": 9,
                                   "fuzzy": 84.0816}}

    def test_the_old_shape_asserted_agreement_on_an_improving_edit(self):
        # Without the unpaired counts the arbiter still says what it said
        # in run 51 — this is the defect, pinned so it cannot come back
        # silently as the DEFAULT for a re-taken baseline.
        out = arb(self.VERDICTS, "game/enemy/critter",
                  {"ProcessCritterList": {"genuine": 0}},
                  {"ProcessCritterList": 2})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("ROSE — structure agrees with real", out[0][2])
        self.assertIn("NO unpaired count in this baseline", out[0][2])

    def test_alignment_change_makes_the_comparison_UNDEFINED(self):
        out = arb(self.VERDICTS, "game/enemy/critter", self.BASE,
                  {"ProcessCritterList": 2}, {"ProcessCritterList": 3})
        self.assertIn("structure arbiter is UNDEFINED", out[0][2])
        self.assertIn("unpaired 9 -> 3 MOVED", out[0][2])
        # The direction word is exactly what must NOT be printed.
        self.assertNotIn("ROSE — structure agrees", out[0][2])

    def test_undefined_plus_a_rising_fuzzy_is_a_CONFLICT(self):
        out = arb(self.VERDICTS, "game/enemy/critter", self.BASE,
                  {"ProcessCritterList": 2}, {"ProcessCritterList": 3},
                  {"ProcessCritterList": 94.5918})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("fuzzy 84.0816 -> 94.5918", out[0][2])
        self.assertIn("+10.5102", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_undefined_without_a_fuzzy_number_stays_a_REGRESSION(self):
        out = arb(self.VERDICTS, "game/enemy/critter", self.BASE,
                  {"ProcessCritterList": 2}, {"ProcessCritterList": 3})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("arbitrate on fuzzy", out[0][2])

    def test_undefined_with_a_FALLING_fuzzy_stays_a_REGRESSION(self):
        out = arb(self.VERDICTS, "game/enemy/critter", self.BASE,
                  {"ProcessCritterList": 2}, {"ProcessCritterList": 3},
                  {"ProcessCritterList": 70.0})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("structure arbiter is UNDEFINED", out[0][2])
        self.assertIn("-14.0816", out[0][2])


class DcsHandleRequestTests(unittest.TestCase):
    """Costume 2: the 0-row erasure, same root, different numbers."""

    VERDICTS = [("dcsHandleRequest", "REGRESSION", "real 144 -> 146")]

    def test_zero_row_erasure_is_UNDEFINED_not_a_corroboration(self):
        out = arb(self.VERDICTS, "game/audio/dcsdrv",
                  {"dcsHandleRequest": {"genuine": 0, "unpaired": 2,
                                        "fuzzy": 98.6618}},
                  {"dcsHandleRequest": 1}, {"dcsHandleRequest": 0},
                  {"dcsHandleRequest": 99.0024})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("unpaired 2 -> 0 MOVED", out[0][2])
        self.assertIn("genuine 0 -> 1", out[0][2])


class AlignmentHeldTests(unittest.TestCase):
    """The NEGATIVE side: an equal unpaired count must not silence it."""

    VERDICTS = [("f", "REGRESSION", "real 48 -> 65")]

    def test_equal_unpaired_keeps_the_FELL_conflict(self):
        out = arb(self.VERDICTS, "game/sys/memcard",
                  {"f": {"genuine": 5, "unpaired": 4}}, {"f": 1}, {"f": 4})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("genuine structural rows 5 -> 1 FELL", out[0][2])

    def test_equal_unpaired_keeps_the_ROSE_regression_and_says_so(self):
        out = arb(self.VERDICTS, "game/sys/memcard",
                  {"f": {"genuine": 2, "unpaired": 4}}, {"f": 9}, {"f": 4})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("ROSE — structure agrees with real", out[0][2])
        self.assertIn("alignment held at 4 unpaired", out[0][2])

    def test_zero_unpaired_both_sides_is_the_92_percent_case(self):
        # 2792 of 3032 census rows sit here; the trigger must be silent.
        out = arb(self.VERDICTS, "game/sys/memcard",
                  {"f": {"genuine": 2, "unpaired": 0}}, {"f": 9}, {"f": 0})
        self.assertNotIn("UNDEFINED", out[0][2])

    def test_a_rising_fuzzy_beats_a_rising_row_count(self):
        # The one-directional half: a rise used to end the conversation.
        out = arb(self.VERDICTS, "game/sys/memcard",
                  {"f": {"genuine": 2, "unpaired": 4, "fuzzy": 90.04}},
                  {"f": 9}, {"f": 4}, {"f": 92.72})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("ROSE while fuzzy 90.0400 -> 92.7200", out[0][2])

    def test_byte_exact_rows_are_still_never_arbitrated(self):
        out = arb([("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
                  {"f": {"genuine": 0, "unpaired": 9}}, {"f": 2}, {"f": 3},
                  {"f": 99.9})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertNotIn("UNDEFINED", out[0][2])


class UnpairedInstructionsCannotBeStructuralTests(unittest.TestCase):
    """The falsifier for the claim the whole design rests on.

    Driven through regnorm.analyze itself, so it fails the day the
    analyzer stops erasing rows this way rather than quietly agreeing.
    `aligned_pairs` pairs only `min(len(t_block), len(o_block))` rows of
    an unbalanced block and drops the tail into t_only/b_only, and
    `Result.genuine` filters `self.structural`, which only paired rows
    ever enter — so an unpaired instruction is uncounted by construction.
    """

    TARGET = ["lis     r3,0", "li      r6,0", "blr"]
    BEFORE = ["addi    r4,r4,0", "blr"]
    AFTER = ["addi    r4,r4,0", "li      r6,1", "blr"]

    def rows(self, ours):
        result = regnorm.analyze(self.TARGET, ours, None)
        return (len(result.genuine), result.unpaired,
                result.t_insns, result.o_insns)

    def test_the_differing_instruction_is_erased_while_it_is_unpaired(self):
        genuine, unpaired, t, o = self.rows(self.BEFORE)
        self.assertEqual((genuine, unpaired), (1, 0 + 1))
        self.assertNotEqual(t, o)  # the unbalanced block that erases it

    def test_genuine_RISES_on_a_strictly_NEARER_stream(self):
        # ProcessCritterList in miniature. The AFTER stream gains count
        # parity with the target — it is nearer by every other measure —
        # and the genuine row count still goes UP, purely because the
        # erased instruction started being paired.
        g_before, u_before, t, o_before = self.rows(self.BEFORE)
        g_after, u_after, _t, o_after = self.rows(self.AFTER)
        self.assertGreater(g_after, g_before)
        self.assertLess(u_after, u_before)
        self.assertNotEqual(o_before, t)
        self.assertEqual(o_after, t)  # count parity GAINED

    def test_the_arbiter_calls_exactly_this_pair_UNDEFINED(self):
        g_before, u_before, _t, _o = self.rows(self.BEFORE)
        g_after, u_after, _t2, _o2 = self.rows(self.AFTER)
        out = arb([("f", "REGRESSION", "real 4 -> 6")], "game/sys/memcard",
                  {"f": {"genuine": g_before, "unpaired": u_before}},
                  {"f": g_after}, {"f": u_after})
        self.assertIn("structure arbiter is UNDEFINED", out[0][2])
        self.assertIn(f"unpaired {u_before} -> {u_after} MOVED", out[0][2])


if __name__ == "__main__":
    unittest.main()
