#!/usr/bin/env python3
"""T21 run-51 item 8: gate G taxed a record for quoting a tool's own verdict.

THE OBSERVATION: `outside every postprocessor class` is a phrase this
project's TOOLS print. `wf_word_diff` answers a count-asymmetric function
with "— outside every postprocessor class by construction", and `probe --raw`
printed the same sentence, so a record pasting the measurement it was handed
tripped a gate about VERIFIER ENUMERATION. PR and NC each lost a submission.
AGENTS.md documents the identical false-positive class for the run-44 slot
gate: "that same gate's first EVIDENCE predicate looked only for the literal
tool name and scored eight records as violations that quote the tool's OUTPUT
verbatim".

REPRODUCED at 03ed3d1cc, over the whole accepted corpus
(T21_scratch/t21_gateg_phrase.py): the phrase occurs 54 times in 2,118
records, and 8 accepted records would trip gate G today — five of them on
this phrase, two of those named, in their own ids,
`postprocessor-ineligible-count-asymmetric`.

THE EXEMPTION IS NOT "IT CAME FROM A TOOL". It is that the claim has a
DIFFERENT AND COMPLETE DISCHARGE: count asymmetry puts a function outside
every postprocessor class as a THEOREM (every shipped class preserves the
instruction count), so no verifier subset was run, none could have been, and
`verifiers_run` has nothing true to say. Proposal gate B already forces such
a record to quote its counts as N/N — that is the evidence this looks for.

TWO-SIDED CALIBRATION over all 2,118 accepted records
(T21_scratch/t21_gateg_calib.py):

  BEFORE  8 records trip gate G
  AFTER   6 records trip gate G
  POSITIVES  2 exempted: WF_fn8005f0f4-postprocessor-ineligible-count-
             asymmetric and WF_resolveworlddata-postprocessor-ineligible-
             count-asymmetric
  NEGATIVES  6 still taxed, INCLUDING the gate's proven catch — both
             HV_drawmemcardmessage records, which concluded "the
             postprocessor path is closed" from a partial verifier screen

THE CALIBRATION KILLED THE FIRST DESIGN. It accepted any `T\\d+/O\\d+` count
pair as asymmetry evidence, and
attempt.HV_startshieldfx-and-drawblitflatquad-refuted-from-the-permute-class
.20260901.v1 quotes `insns T141/O141` — count PARITY — beside "NOT a webfrank
candidate at all" for a 6-token multiset difference. That is a genuine gate-G
target and the draft exempted it, taking the positives from 2 to 3 in the
wrong direction. The pair is now compared NUMERICALLY.

REFUTED, AND REPORTED RATHER THAN FIXED: the item names the slot gate as
carrying the same defect. It does not. `_SLOTDIFF_EVIDENCE_RE` already
accepts the tool's OUTPUT as well as its name — "SLOT MAP IDENTICAL",
"frame: target N ours M", "target-only slots", "(N uses)" — which is the
run-44 fix the item's own AGENTS.md citation describes.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))

from memory_graph import core  # noqa: E402

TOOL_LINE = ("`python tools/gdl/composed_census/wf_word_diff.py"
             " game/game/player do_players` prints 'do_players:"
             " count-asymmetric (1172 vs 1174 insns) — outside every"
             " postprocessor class by construction'")
VERIFIER_CLOSURE = ("verify_consistent_recolor refused on the whole body, so"
                    " the postprocessor path is closed and no permutation"
                    " repair will reopen it")


class CountAsymmetryEvidence(unittest.TestCase):
    def test_the_word_alone_is_evidence(self):
        self.assertTrue(core._count_asymmetry_evidence(
            "the function is count-asymmetric here"))

    def test_a_differing_count_pair_is_evidence(self):
        self.assertTrue(core._count_asymmetry_evidence("insns T104/O102"))
        self.assertTrue(core._count_asymmetry_evidence("(1172 vs 1174 insns)"))

    def test_an_EQUAL_count_pair_is_NOT_evidence(self):
        # The draft accepted this and exempted a genuine gate-G target.
        self.assertFalse(core._count_asymmetry_evidence("insns T141/O141"))
        self.assertFalse(core._count_asymmetry_evidence("(141 vs 141 insns)"))

    def test_unrelated_prose_is_not_evidence(self):
        self.assertFalse(core._count_asymmetry_evidence(
            "the recolor is 40 words wide"))


class ClosureClaim(unittest.TestCase):
    def test_a_quoted_tool_verdict_is_exempt(self):
        self.assertIsNone(core.postprocessor_closure_claim(TOOL_LINE))

    def test_a_verifier_closure_is_still_taxed(self):
        match = core.postprocessor_closure_claim(VERIFIER_CLOSURE)
        self.assertIsNotNone(match)
        self.assertIn("closed", match.group(0))

    def test_a_parity_function_called_not_a_candidate_is_still_taxed(self):
        text = ("StartShieldFX, real 64, insns T141/O141 but multiset 6t —"
                " NOT a webfrank candidate at all")
        self.assertIsNotNone(core.postprocessor_closure_claim(text))

    def test_the_scan_continues_past_an_exempt_match(self):
        # A record may quote the tool's count-asymmetry verdict for one
        # function AND claim a verifier closure elsewhere; only the second is
        # gate G's business, and stopping at the first match would exempt it.
        mixed = TOOL_LINE + (" filler prose. " * 40) + VERIFIER_CLOSURE
        self.assertIsNotNone(core.postprocessor_closure_claim(mixed))

    def test_nothing_at_all_returns_none(self):
        self.assertIsNone(core.postprocessor_closure_claim(
            "the residual is a two-FPR recolor at count parity"))
        self.assertIsNone(core.postprocessor_closure_claim(""))
        self.assertIsNone(core.postprocessor_closure_claim(None))


class SlotGateAlreadyAcceptsToolOutput(unittest.TestCase):
    """The refutation, asserted so it cannot quietly stop being true."""

    def test_slot_map_identical_discharges_without_the_tool_name(self):
        self.assertIsNone(core.slot_claim_without_slotdiff(
            "the frame gap is 8 bytes in the local area",
            "SLOT MAP IDENTICAL across the whole body"))

    def test_the_frame_line_discharges(self):
        self.assertIsNone(core.slot_claim_without_slotdiff(
            "the frame gap is 8 bytes in the local area",
            "frame: target 96  ours 88"))


if __name__ == "__main__":
    unittest.main()
