#!/usr/bin/env python3
"""T21 run-51 item 3: `probe --arbitrate` read fuzzy without reading the
instruction COUNT, and so applied the fuzzy-decides rule outside its scope.

THE OBSERVATION (BG, run 50): on an edit that moved the instruction count
TOWARD the target and emptied the ours-only opcode multiset, the arbiter
printed

    ARBITER: fuzzy FELL — REVERT to the banked state even though real
             IMPROVED: this is exactly the shape where probe and defake_gate
             both pass a fuzzy loss

THE GOVERNING LAW SAYS THE OPPOSITE FOR THAT STATE, verbatim from
claim.law.insn-count-parity-outranks-local-opcode-fidelity.20260831.v1:

    "read `fndiff --count`'s insn parity FIRST. If the change restored
     parity, prefer real and expect fuzzy to lag. If the change left parity
     unchanged or broke it, prefer fuzzy and go read the --ops multiset for
     the structural divergence real is hiding."

and it was derived on this shape: damage_enemy 570/571 -> 571/571, real
151 -> 66, fuzzy 99.4046 -> 99.2207 — kept, because an off-by-one count
displaces every downstream branch while fuzzy's offset-tolerant measure
barely notices. claim.law.fuzzy-can-underweight-a-real-improvement
.20260830.v1 records two more (140/139 -> 140/140 and 355/354 -> 355/355,
fuzzy down both times, both the better result).

The fuzzy-decides rule is SOUND where it was scoped — equal counts, where
the streams differ in what the instructions ARE. `run_arbitrate` already had
both counts in hand from `score_function` and threw them away.

TWO-SIDED CALIBRATION of the trigger (the two states' ours-vs-target gap
differs), over all 3,032 comparable function pairs in 257 units
(T21_scratch/t21_parity_population.py):

  POSITIVES  117 functions (3.9%) sit OFF count parity today, so a
             parity-restoring edit is possible on them and today's arbiter
             can invert the law; 46 of the 117 are a gap of exactly +1
  NEGATIVES  2,915 functions (96.1%) are AT parity: an edit there leaves
             both counts equal, the branch cannot fire, and the arbiter text
             is byte-identical to today's — asserted below rather than
             assumed
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class InsnGap(unittest.TestCase):
    def test_the_score_function_spelling_parses(self):
        self.assertEqual(probe.insn_gap("T682/O680"), (682, 680, -2))
        self.assertEqual(probe.insn_gap("T571/O571"), (571, 571, 0))

    def test_exact_is_parity_with_unknown_absolute_counts(self):
        self.assertEqual(probe.insn_gap("exact"), (None, None, 0))

    def test_anything_else_is_unknown_not_zero(self):
        self.assertIsNone(probe.insn_gap(None))
        self.assertIsNone(probe.insn_gap(""))
        self.assertIsNone(probe.insn_gap("670"))


class CountParityNote(unittest.TestCase):
    def test_restoring_parity_overrides_a_fuzzy_fall(self):
        verdict, note = probe.count_parity_note("T571/O570", "T571/O571")
        self.assertIn("COUNT PARITY RESTORED", verdict)
        self.assertIn("KEEP", verdict)
        self.assertIn("ours 570 -> 571 against target 571", note)
        self.assertIn("insn-count-parity-outranks-local-opcode-fidelity",
                      note)

    def test_breaking_parity_is_categorical(self):
        verdict, note = probe.count_parity_note("T571/O571", "T571/O572")
        self.assertIn("COUNT PARITY LOST", verdict)
        self.assertIn("REVERT", verdict)
        self.assertIn("categorical", note)

    def test_a_gap_that_only_narrows_is_out_of_scope_not_a_keep(self):
        verdict, note = probe.count_parity_note("T400/O410", "T400/O403")
        self.assertIn("OUT OF SCOPE", verdict)
        self.assertIn("TOWARD", verdict)
        self.assertIn("does NOT arbitrate", note)
        self.assertNotIn("KEEP the current state", verdict)

    def test_a_widening_gap_says_away_from(self):
        verdict, _note = probe.count_parity_note("T400/O403", "T400/O410")
        self.assertIn("AWAY FROM", verdict)

    def test_equal_counts_do_not_fire(self):
        self.assertEqual(probe.count_parity_note("T571/O571", "T571/O571"),
                         (None, None))
        self.assertEqual(probe.count_parity_note("exact", "exact"),
                         (None, None))
        # unchanged NON-zero gap: still the fuzzy question, still not ours
        self.assertEqual(probe.count_parity_note("T400/O402", "T400/O402"),
                         (None, None))

    def test_an_unread_count_never_becomes_a_verdict(self):
        self.assertEqual(probe.count_parity_note(None, "T571/O571"),
                         (None, None))
        self.assertEqual(probe.count_parity_note("T571/O570", None),
                         (None, None))


class ArbitrateTable(unittest.TestCase):
    """The negative half: with no count change, nothing moves."""

    def test_the_old_text_is_unchanged_without_counts(self):
        without = probe.arbitrate_table("snap", 40, 99.4046, 16, 99.2207)
        self.assertIn("fuzzy FELL — REVERT", without)
        self.assertNotIn("COUNT PARITY", without)

    def test_equal_counts_keep_the_fuzzy_verdict(self):
        # 2,915 of 3,032 pairs are here.
        text = probe.arbitrate_table("snap", 40, 99.4046, 16, 99.2207,
                                     base_insns="T571/O571",
                                     cur_insns="T571/O571")
        self.assertIn("fuzzy FELL — REVERT", text)
        self.assertNotIn("COUNT PARITY", text)
        self.assertIn("insns T571/O571", text)

    def test_the_reported_defect_now_reads_the_count_first(self):
        # BG's state: real improved, fuzzy fell, count restored to parity.
        text = probe.arbitrate_table("snap", 151, 99.4046, 66, 99.2207,
                                     base_insns="T571/O570",
                                     cur_insns="T571/O571")
        self.assertIn("ARBITER: COUNT PARITY RESTORED", text)
        self.assertNotIn("ARBITER: fuzzy FELL", text)
        # the fuzzy number is still reported, just demoted
        self.assertIn("-0.1839", text)
        self.assertIn("not arbitrating", text)

    def test_counts_appear_in_the_two_state_rows(self):
        text = probe.arbitrate_table("snap", 40, 99.0, 16, 99.0,
                                     base_insns="T571/O570",
                                     cur_insns="T571/O571")
        self.assertIn("insns T571/O570", text)
        self.assertIn("insns T571/O571", text)

    def test_a_count_change_outranks_an_unmeasured_fuzzy(self):
        # INCONCLUSIVE was the old answer here; the count is a measurement.
        text = probe.arbitrate_table("snap", 151, None, 66, None,
                                     base_insns="T571/O570",
                                     cur_insns="T571/O571")
        self.assertIn("COUNT PARITY RESTORED", text)
        self.assertNotIn("INCONCLUSIVE", text)


if __name__ == "__main__":
    unittest.main()
