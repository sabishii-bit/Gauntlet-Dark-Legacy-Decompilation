#!/usr/bin/env python3
"""T21 run-51 item 7: savedregs' unseen-class findings belong in the VERDICT,
and savedregs' own table discharges Gate I's register-quoting demand.

(7a) THE OBSERVATION: the `VOLATILE ROLE PROMOTED IN OURS` line "named
PlayerControls' defect in one sentence, buried below 4 paragraphs". The claim
is about OUTPUT ORDER, so it was measured over every function the tool can
score rather than on the one that was reported
(T21_scratch/t21_savedregs_position.py, 3,005 functions at 6a017ec3c):

    VOLATILE ROLE PROMOTED IN OURS: 71 function(s); mean line 29.3,
                                    deepest line 60 of 66
    LIFETIME ESCAPED TO A VOLATILE: 96 function(s); mean line 27.5
    LATER-WEB MISMATCH            : 80 function(s); mean line 27.9
    ROLE MISMATCH on              : 172 function(s); mean line 15.5

TWO-SIDED CALIBRATION of the headline trigger
(T21_scratch/t21_savedregs_allclear.py, same 3,005 functions):

    print the ASSIGNMENT MATCHES all-clear : 1457
    carry a finding the table cannot see   : 150   <- POSITIVES, get a headline
    BOTH (the dangerous subset)            : 8     <- and their all-clear is
                                                      qualified in place
    2,855 functions carry no such finding and print no new line at all.

The eight include game/movie/movieplayer::fn_800D8BCC — the function whose
all-clear three records took as a premise (attempt.MV_fn800d8bcc-duplicated-
branch-locals-belong-to-the-common-block.20260903.v1 and its two
predecessors) while its residual was a volatile colour cascade. Verified
live after the change: its report now reads `ASSIGNMENT MATCHES ... AND IT IS
NOT AN ALL-CLEAR HERE` followed immediately by `THE SAVE SET IS WRONG IN A
WAY THIS TABLE'S ROWS DO NOT SHOW: 3 VOLATILE ROLE PROMOTED IN OURS; 2
LIFETIME ESCAPED TO A VOLATILE`, at lines 15-16 instead of 29-30.

`LIFETIME PERMUTATION` is deliberately NOT promoted: a permutation changes
the first definitions themselves, so the table's own rows already show it.

(7b) GATE I. savedregs answers the gate's own question — which local lands
in which callee-saved register, read out of BOTH streams with no build — and
its verdict row strips the destination from the two roles:

    r26: target holds `li 0`, ours holds `add r30,r28`

`r26` appears in no instruction there, so a record quoting the tool's answer
verbatim was refused for naming a register it had in fact read. Two
submissions were lost to that.

TWO-SIDED CALIBRATION over all 2,118 accepted records / 224 hypothesis
statements (T21_scratch/t21_gate_i_calib.py):

  NEGATIVES  0 records change their gap list; the proven catch (run 37/38's
             r20, named in prose with no instruction anywhere) still refuses,
             and a role row for r26 does NOT discharge an unrelated r9
  POSITIVES  0 IN THE ACCEPTED CORPUS, and that number is not evidence of no
             defect: a REFUSAL gate's false positives are the submissions it
             refused, which by construction never became accepted records.
             Only 4 accepted records carry a savedregs table or role row at
             all. The positive side is therefore demonstrated by
             CONSTRUCTION, on the exact shipped output shape, below.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import savedregs               # noqa: E402
from memory_graph import core  # noqa: E402


class UnseenFindings(unittest.TestCase):
    """The counts the headline is built from, pure over lifetime pairs."""

    @staticmethod
    def pair(label, verdict):
        return (label, None, None, verdict, 1)

    def test_no_findings_means_no_headline(self):
        self.assertEqual(savedregs.unseen_findings([]), [])

    def test_a_permutation_alone_is_not_promoted(self):
        # The table's own rows show it; promoting it would make the headline
        # fire on 3 of every 4 imperfect functions and mean nothing.
        pairs = [self.pair("r23[0]", "PERMUTED r23->r25")]
        self.assertEqual(savedregs.unseen_findings(pairs), [])

    def test_a_later_web_mismatch_is_promoted(self):
        pairs = [self.pair("r24[1]", "DIFFERENT ROLE")]
        self.assertEqual(savedregs.unseen_findings(pairs),
                         ["1 LATER-WEB MISMATCH"])

    def test_a_first_web_mismatch_is_not_a_later_one(self):
        pairs = [self.pair("r24[0]", "DIFFERENT ROLE")]
        self.assertEqual(savedregs.unseen_findings(pairs), [])


class VerdictPlacement(unittest.TestCase):
    """The headline sits in the verdict, and qualifies the all-clear."""

    UNIT, FN = "game/movie/movieplayer", "fn_800D8BCC"

    def _report(self):
        import fnasm
        trows, _n, err1 = fnasm.parse_fn(self.UNIT, self.FN, ours=False)
        orows, _m, err2 = fnasm.parse_fn(self.UNIT, self.FN, ours=True)
        if err1 or err2 or not trows:
            self.skipTest("needs built objects for movieplayer")
        return savedregs.format_table(self.UNIT, self.FN, trows, orows,
                                      pins=savedregs.webfrank_pins())

    def test_the_recorded_incident_function_is_no_longer_an_all_clear(self):
        text = self._report()
        self.assertIn("ASSIGNMENT MATCHES", text)
        self.assertIn("NOT AN ALL-CLEAR HERE", text)

    def test_the_headline_precedes_the_scope_paragraphs(self):
        lines = self._report().splitlines()
        headline = next(i for i, ln in enumerate(lines)
                        if "THE SAVE SET IS WRONG" in ln)
        scope = next(i for i, ln in enumerate(lines)
                     if "SCOPE OF THIS TABLE" in ln)
        detail = next(i for i, ln in enumerate(lines)
                      if "VOLATILE ROLE PROMOTED IN OURS:" in ln)
        self.assertLess(headline, scope)
        self.assertLess(headline, detail)

    def test_the_headline_carries_the_counts(self):
        text = self._report()
        self.assertIn("3 VOLATILE ROLE PROMOTED IN OURS", text)
        self.assertIn("2 LIFETIME ESCAPED TO A VOLATILE", text)


class GateIAcceptsSavedregs(unittest.TestCase):
    ROLE_ROW = "    r26: target holds `li 0`, ours holds `add r30,r28`"
    TABLE_ROW = "  r29      @0x130  li r29,0                  @0x30  li r29,0"

    def test_a_role_row_discharges_its_register(self):
        self.assertEqual(
            core.register_definition_gaps("r26 holds the wrong web",
                                          self.ROLE_ROW), [])

    def test_a_role_row_discharges_only_its_own_register(self):
        self.assertEqual(
            core.register_definition_gaps("r9 holds the wrong web",
                                          self.ROLE_ROW), ["r9"])

    def test_a_table_row_discharges_its_register(self):
        self.assertEqual(
            core.register_definition_gaps("r29 is the zero web",
                                          self.TABLE_ROW), [])

    def test_the_proven_catch_still_refuses(self):
        # Run 37/38: r20 named in prose, no instruction quoted anywhere.
        prose = ("the r20 web is the level-table base and the loop head"
                 " reloads it")
        self.assertEqual(core.register_definition_gaps(prose, prose),
                         ["r20"])

    def test_the_table_row_shape_requires_ADJACENCY(self):
        # `rNN @0xAA` is the table's column layout. A register and an offset
        # merely co-occurring in a sentence is prose, and must not discharge.
        loose = "r20 is reloaded somewhere around @0x684 in the loop head"
        self.assertEqual(core.register_definition_gaps("r20 is the base",
                                                       loose), ["r20"])
        row = "  r20      @0x684  (never defined here)"
        self.assertEqual(core.register_definition_gaps("r20 is the base",
                                                       row), [])


if __name__ == "__main__":
    unittest.main()
