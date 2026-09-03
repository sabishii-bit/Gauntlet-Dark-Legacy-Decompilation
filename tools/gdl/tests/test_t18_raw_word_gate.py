#!/usr/bin/env python3
"""T18 run-48 item 1: the RAW word count and the raw-word bank gate.

THE DEFECT, reproduced at c3f3aea99 on game/ui/btext::DrawGlowText (a
webfrank-pinned function; 153 distinct pinned functions in 52 units carry the
same shape):

  1. `probe --raw` never printed the raw DIFFERING-WORD count -- the number
     AGENTS.md makes mandatory for any residual-signature claim and the one
     that decides postprocessor candidacy. Every promotion A/B on the pinned
     backlog was therefore hand-paired: `probe --raw` for `real`, then a
     second `wf_word_diff.py <unit> <fn>` call for the arbiter.

  2. The fuzzy gate that guards every banking verdict measures a DIFFERENT
     OBJECT than `--raw` scores. objdiff fuzzy is read from
     build/GUNE5D/report.json, which is generated over the POSTPROCESSED
     object, so on a pinned function it has exactly two states:

       clean tree   `BASELINE real 16 (insns T86/O86, multiset 0t)` printed
                    directly under `FUZZY (fresh report): 100.0000%`, while
                    the raw body differed in 8 words. That 100 was banked as
                    the fuzzy ANCHOR.
       edited tree  with `u8 unused[8]` -> `[16]` in DrawGlowText, the body
                    hash moves, the WEBFRANK stage aborts, the report build
                    fails, and the gate prints `[fuzzy gate: no number -- the
                    report build FAILED or this function is absent from
                    build/GUNE5D/report.json]` -- and banks anyway.

     A gate that reads 100 when it passes and nothing when it does not is not
     an arbiter.

TWO-SIDED CALIBRATION of the trigger (`raw AND pinned`), measured at
c3f3aea99 over config/GUNE5D/webfrank.json and build/GUNE5D/report.json by
T18_scratch/t18_calib_rawgate.py:

    POSITIVES  153 pinned functions in 52 units (155 rule rows; btricol::
               LineLineDist and message::msgDraw each carry two)  -> raw gate
    NEGATIVES  2,837 functions keep the fuzzy gate, which is sound for them
               1,442 of those live INSIDE the 52 pinned units -- the exact
                     population a `--raw`-only trigger would have misrouted
               1,395 sit in units with no pin at all
    TOTAL      2,990 functions in report.json

Verified live after the change on game/ui/btext::DrawGlowText:
  clean  `BASELINE real 16` + `RAW WORDS = 8 of 86 insns; CLASS: RECOLOR`,
         with NO report build spent and the raw-word gate line explaining why;
  edited `REGRESSED ... real 16 -> 40` + `RAW WORDS = 20 (+12 vs the last
         probe's 8)`.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class RawGateTrigger(unittest.TestCase):
    """`raw AND pinned` -- not `--raw` alone, and never without --raw."""

    def test_pinned_under_raw_takes_the_raw_gate(self):
        self.assertTrue(probe.raw_gate_applies(True, True))

    def test_unpinned_under_raw_keeps_the_fuzzy_gate(self):
        # The 1,442-function negative half: for these the postprocessed and
        # raw bodies are the same bytes, so fuzzy is a real measurement.
        self.assertFalse(probe.raw_gate_applies(True, False))

    def test_pinned_without_raw_keeps_the_fuzzy_gate(self):
        # Without --raw the verdict itself describes the postprocessed
        # object, so fuzzy and the verdict agree about which bytes they mean.
        self.assertFalse(probe.raw_gate_applies(False, True))

    def test_neither(self):
        self.assertFalse(probe.raw_gate_applies(False, False))


class RawWordsLine(unittest.TestCase):
    """The count, its delta, and the class call."""

    def test_first_probe_prints_the_count_with_no_delta(self):
        line = probe.raw_words_line(8, None, 86, 0, False)
        self.assertIn("RAW WORDS = 8", line)
        self.assertNotIn("vs the last probe", line)
        self.assertIn("RECOLOR", line)

    def test_delta_against_the_previous_probe(self):
        line = probe.raw_words_line(20, 8, 86, 0, False)
        self.assertIn("RAW WORDS = 20 (+12 vs the last probe's 8)", line)

    def test_a_falling_count_signs_its_delta(self):
        line = probe.raw_words_line(8, 20, 86, 0, False)
        self.assertIn("(-12 vs the last probe's 20)", line)

    def test_zero_delta_is_still_printed(self):
        # An unchanged count is the answer on a NEUTRAL probe; suppressing it
        # would make "measured, unmoved" and "not measured" the same output.
        self.assertIn("(+0 vs the last probe's 8)",
                      probe.raw_words_line(8, 8, 86, 0, False))

    def test_mnemonic_divergence_names_the_schedule_class(self):
        line = probe.raw_words_line(91, None, 290, 7, False)
        self.assertIn("SCHEDULE-REORDER", line)
        self.assertIn("7 mnemonic divergence(s)", line)
        self.assertNotIn("RECOLOR", line)

    def test_pinned_says_the_rule_already_discharges_the_count(self):
        # The wf_word_diff PIN SCREEN, carried into the loop: a pinned
        # function's raw residual is closed work, and a sweep that ranks by
        # differing words puts finished functions first without this.
        self.assertIn("PINNED", probe.raw_words_line(8, None, 86, 0, True))
        self.assertNotIn("PINNED", probe.raw_words_line(8, None, 86, 0,
                                                        False))

    def test_the_decode_census_qualifies_the_class_line(self):
        # Run-48 item 4, carried into the loop: zero mnemonic divergence
        # reads RECOLOR, and a BRANCH or IMMEDIATE word is not a
        # register-assignment question however aligned the streams are.
        decode = {"REGFIELD-ONLY": 0, "IMMEDIATE": 2, "BRANCH": 0,
                  "OPCODE": 0, "RELOCATED": 0}
        line = probe.raw_words_line(2, None, 266, 0, True, decode)
        self.assertIn("RECOLOR-SHAPED BUT NOT RECOLOURABLE", line)
        self.assertIn("DECODE: REGFIELD-ONLY 0, IMMEDIATE 2", line)

    def test_a_genuine_recolour_keeps_the_recolor_line(self):
        decode = {"REGFIELD-ONLY": 8, "IMMEDIATE": 0, "BRANCH": 0,
                  "OPCODE": 0, "RELOCATED": 0}
        line = probe.raw_words_line(8, None, 86, 0, False, decode)
        self.assertIn("CLASS: RECOLOR —", line)
        self.assertNotIn("NOT RECOLOURABLE", line)

    def test_without_a_decode_the_class_line_falls_back(self):
        line = probe.raw_words_line(8, None, 86, 0, False, None)
        self.assertIn("CLASS: RECOLOR —", line)
        self.assertNotIn("DECODE:", line)

    def test_unmeasurable_says_so_rather_than_printing_a_zero(self):
        line = probe.raw_words_line(None, None, None, None, False)
        self.assertIn("not measurable", line)
        self.assertIn("count-asymmetric", line)
        self.assertNotIn("RAW WORDS = 0", line)


PRIOR_BEST = {"best_real": 16, "best_multiset": 0, "best_insns": "T86/O86",
              "best_bytes": "deadbeef", "best_fuzzy": None}


class RawWordGate(unittest.TestCase):
    """The bank refusal, pure over the two counts."""

    def test_a_non_banking_verdict_is_untouched(self):
        verdict, state = probe.apply_raw_word_gate(
            "REGRESSED vs best 16: real 16 -> 40", {"best_real": 40},
            PRIOR_BEST, 20, 8)
        self.assertTrue(verdict.startswith("REGRESSED"))
        self.assertEqual(state, {"best_real": 40})

    def test_a_falling_count_banks(self):
        verdict, _state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 8, 20)
        self.assertTrue(verdict.startswith("IMPROVED"))

    def test_an_equal_count_banks(self):
        verdict, _state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 8, 8)
        self.assertTrue(verdict.startswith("IMPROVED"))

    def test_no_anchor_yet_banks_silently(self):
        verdict, _state = probe.apply_raw_word_gate(
            "BASELINE  real 16", {"best_real": 16}, PRIOR_BEST, 8, None)
        self.assertEqual(verdict, "BASELINE  real 16")

    def test_a_rising_count_refuses_and_restores_the_prior_anchor(self):
        # `real` and the multiset both improved; the raw residual a rule
        # would have to discharge GREW. That is the promotion question.
        verdict, state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16 (best was 40)", {"best_real": 16},
            PRIOR_BEST, 20, 8)
        self.assertTrue(verdict.startswith("RAW-WORDS-REGRESSED"))
        self.assertIn("8 -> 20 (+12)", verdict)
        self.assertIn("IMPROVED  real 40 -> 16 (best was 40)", verdict)
        self.assertEqual(state["best_real"], 16)
        # best_fuzzy was None in the prior anchor: restoring must DROP the
        # key, not write a null over it.
        self.assertNotIn("best_fuzzy", state)

    def test_the_refusal_does_not_bank(self):
        verdict, _state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 20, 8)
        self.assertFalse(probe.banks_best(verdict))

    def test_rebase_best_alone_is_still_refused(self):
        # The run-40 lesson, applied to this gate at birth: a flag that
        # declares an arbitration is not an arbitration.
        verdict, _state = probe.apply_raw_word_gate(
            "REBASED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 20, 8,
            rebase_best=True)
        self.assertTrue(verdict.startswith("RAW-WORDS-REGRESSED"))
        self.assertFalse(probe.banks_best(verdict))

    def test_an_acknowledged_loss_banks_with_the_loss_in_the_headline(self):
        verdict, _state = probe.apply_raw_word_gate(
            "REBASED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 20, 8,
            rebase_best=True, accept_loss=True)
        self.assertTrue(verdict.startswith("REBASED-RAW-WORD-LOSS"))
        self.assertIn("8 -> 20", verdict)
        self.assertTrue(probe.banks_best(verdict))

    def test_accept_loss_without_rebase_best_does_not_open_the_door(self):
        verdict, _state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, 20, 8,
            rebase_best=False, accept_loss=True)
        self.assertTrue(verdict.startswith("RAW-WORDS-REGRESSED"))

    def test_unmeasurable_with_an_anchor_banks_but_says_it_is_unarbitrated(
            self):
        verdict, _state = probe.apply_raw_word_gate(
            "IMPROVED  real 40 -> 16", {"best_real": 16}, PRIOR_BEST, None, 8)
        self.assertIn("RAW-WORD GATE UNMEASURED", verdict)
        self.assertTrue(probe.banks_best(verdict))

    def test_unmeasurable_with_no_anchor_says_nothing(self):
        # Nothing to gate against, and a line on every such probe is noise.
        verdict, _state = probe.apply_raw_word_gate(
            "BASELINE  real 16", {"best_real": 16}, PRIOR_BEST, None, None)
        self.assertEqual(verdict, "BASELINE  real 16")


if __name__ == "__main__":
    unittest.main()
