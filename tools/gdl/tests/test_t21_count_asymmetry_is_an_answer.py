#!/usr/bin/env python3
"""T21 run-51 item 4: a count-asymmetric function is an ANSWER, and its two
counts must be labelled the way `fndiff` labels them.

REPRODUCED at 491f82e35 on game/game/gamemain::fn_80051C78, both halves in
one pair of commands:

    $ python tools/gdl/composed_census/wf_word_diff.py \
          game/game/gamemain fn_80051C78
    fn_80051C78: count-asymmetric (103 vs 105 insns) — outside every
    postprocessor class by construction
    EC=1                       <- PowerShell renders this as a
                                  NativeCommandError block, not a census row

    $ python tools/gdl/fndiff.py game/game/gamemain fn_80051C78 --count
    DIFF fn_80051C78  insns 105/103  lines 72  real 66
    EC2=0

Two opposite conventions for one question: wf_word_diff printed OURS first
and unlabelled, fndiff prints TARGET first. NC read a run-50 census row
backwards off exactly this pair, and 10 of 13 rows of that census came back
as error blocks because of the exit code.

TWO SEPARATE DEFECTS, TWO SEPARATE CURES:

  LABEL   the message now reads `target 105, ours 103 insns (ours -2)`,
          fndiff's order and fndiff's words.
  EXIT    count asymmetry exits 0. Run 42 grouped it with a missing object
          under "the measurement did not happen", and the two are opposites:
          a missing object is a failure to measure, count asymmetry is the
          measured verdict "no word residual exists here, by construction" —
          one of the answers this tool exists to give. A MISSING OBJECT
          still exits non-zero, asserted below.

TWO-SIDED CALIBRATION of the exit-code change, over all 3,032 comparable
function pairs in 257 units (T21_scratch/t21_parity_population.py, the same
census item 3 used, taken through cn_analyze.our_object/target_object — the
objects this tool reads):

  POSITIVES  117 functions (3.9%) are count-asymmetric today; every one of
             them exits 1 before this change and 0 after, and every one is a
             row a sweep wants to keep
  NEGATIVES  2,915 functions (96.1%) are at count parity and never reach
             this path at all; the missing-object refusal is unchanged and
             is asserted here so the two cannot be merged again

SIBLING SCREEN (AGENTS discipline 18): probe's own fallback line asserted
the SAME conflation with an "or" — "not measurable — the two streams are
count-asymmetric, or the raw body could not be read" — which is the sentence
claim.law.MP_probe-raw-drops-the-raw-word-count-... objected to, because it
named the one conclusion that removes a function from every postprocessor
class. `raw_word_residual` now reports WHICH failure it hit when the caller
asks, and the line prints the measured cause or admits it has none.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import probe          # noqa: E402
import wf_word_diff   # noqa: E402


class CountAsymmetricReport(unittest.TestCase):
    def test_the_counts_are_labelled_in_fndiff_s_order(self):
        gap = wf_word_diff.CountAsymmetric(
            "game/game/gamemain", "fn_80051C78", "raw postprocess body",
            103, 105)
        text = gap.report()
        self.assertIn("target 105, ours 103 insns", text)
        self.assertIn("(ours -2)", text)
        # the old, unlabelled, ours-first spelling must be gone
        self.assertNotIn("(103 vs 105", text)

    def test_the_sign_is_the_ours_side(self):
        gap = wf_word_diff.CountAsymmetric("u", "f", "k", 106, 105)
        self.assertIn("(ours +1)", gap.report())

    def test_it_names_itself_for_a_sweep_to_partition_on(self):
        gap = wf_word_diff.CountAsymmetric("u", "f", "k", 103, 105)
        self.assertIn("COUNT-ASYMMETRIC", gap.report())

    def test_it_says_this_is_the_answer_not_a_failure(self):
        gap = wf_word_diff.CountAsymmetric("u", "f", "k", 103, 105)
        self.assertIn("that IS the answer", gap.report())

    def test_it_stays_catchable_as_systemexit(self):
        # Every existing caller catches SystemExit; the subclass keeps them
        # working while letting main() tell it from a missing object.
        self.assertTrue(issubclass(wf_word_diff.CountAsymmetric, SystemExit))


class ProbeFallbackNamesItsCause(unittest.TestCase):
    def test_no_measured_cause_does_not_claim_count_asymmetry(self):
        line = probe.raw_words_line(None, None, None, None, False)
        self.assertIn("NOT evidence of count asymmetry", line)

    def test_a_measured_cause_is_quoted_verbatim(self):
        gap = wf_word_diff.CountAsymmetric("u", "f", "k", 103, 105)
        line = probe.raw_words_line(None, None, None, None, False,
                                    reason=gap.report())
        self.assertIn("target 105, ours 103", line)

    def test_the_notes_list_is_optional(self):
        # Callers that pass no list behave exactly as before.
        self.assertIsNone(probe.raw_word_residual(
            "game/does/not/exist", "nothing"))


if __name__ == "__main__":
    unittest.main()
