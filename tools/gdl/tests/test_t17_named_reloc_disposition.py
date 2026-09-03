#!/usr/bin/env python3
"""T17 run-47 item 4: the positional reloc census stops shipping candidates as bugs.

Third costume of the run-42 finding ("a list of candidates advertised as a list
of bugs"). `es_named_reloc_census.py` pairs relocations BY POSITION inside
difflib's equal runs, then labelled every unexplained row `WRONG` -- a word
that reads as a verdict. Its own docstring said "candidate WRONG-CONSTANT BUG";
the printed column said WRONG.

The discriminant folded in is `fndiff.datum_multiset_screen`, which is
alignment-free BY CONSTRUCTION: it compares the multiset of datum VALUES the
two functions read, so it cannot be fooled by a pairing the census had to
guess. The two facts stay independent -- fndiff documents that a multiset is
blind to a TRANSPOSITION, which is exactly what the positional view is good at.

Two-sided calibration, over the census's own 28 functions at 998144326:

    before:  TRANSPOSED  8   WRONG 20
    after:   WRONG-DATUM 7   TRANSPOSED 8   TRANSPOSED? 2   CANDIDATE 11

  POSITIVE  7 WRONG-DATUM rows survive: the whole-function multiset says
            VALUE-DELTA, so the function reads a value retail's does not.
  POSITIVE  2 functions MOVED from WRONG to TRANSPOSED -- set_hidden_player
            and InitNameAudio are 4-label ROTATIONS, and the old test asked
            only whether every (a,b) had a matching (b,a), which is true of a
            2-cycle and of nothing longer.
  NEGATIVE  11 of the 20 previously-WRONG functions (55%) are demoted to
            CANDIDATE: every datum value they read, retail reads too.
  NEGATIVE  2 TRANSPOSED functions gained a question mark -- world_update and
            camera_mode_follow are swaps AND carry a VALUE-DELTA elsewhere,
            which the old label hid.
"""

import json
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import es_named_reloc_census as census  # noqa: E402

SWAP = [{"target": "lbl_A", "ours": "lbl_B"},
        {"target": "lbl_B", "ours": "lbl_A"}]
ROTATION = [{"target": "lbl_A", "ours": "lbl_D"},
            {"target": "lbl_B", "ours": "lbl_A"},
            {"target": "lbl_C", "ours": "lbl_B"},
            {"target": "lbl_D", "ours": "lbl_C"}]
UNRELATED = [{"target": "lbl_A", "ours": "lbl_Z"}]


class Disposition(unittest.TestCase):
    def setUp(self):
        self.real = census.datum_multiset_screen

    def tearDown(self):
        census.datum_multiset_screen = self.real

    def screen(self, verdict):
        census.datum_multiset_screen = (
            lambda unit, fn: None if verdict is None else {"verdict": verdict})

    def test_VALUE_DELTA_and_not_a_permutation_is_the_only_WRONG_DATUM(self):
        self.screen("VALUE-DELTA")
        self.assertEqual(census.disposition("u", "f", UNRELATED)[0],
                         "WRONG-DATUM")

    def test_VALUE_EQUAL_and_not_a_permutation_is_a_CANDIDATE(self):
        """The 11-function class: every value retail reads, we read."""
        self.screen("VALUE-EQUAL")
        self.assertEqual(census.disposition("u", "f", UNRELATED)[0],
                         "CANDIDATE")

    def test_a_swap_with_an_equal_multiset_is_TRANSPOSED(self):
        self.screen("VALUE-EQUAL")
        self.assertEqual(census.disposition("u", "f", SWAP)[0], "TRANSPOSED")

    def test_a_ROTATION_is_a_transposition_too(self):
        """The old 2-cycle test filed a 4-label rotation as WRONG."""
        self.screen("VALUE-EQUAL")
        verdict, _, permutation = census.disposition("u", "f", ROTATION)
        self.assertTrue(permutation)
        self.assertEqual(verdict, "TRANSPOSED")
        old_pairs = {(r["target"], r["ours"]) for r in ROTATION}
        self.assertFalse(all((b, a) in old_pairs for a, b in old_pairs))

    def test_a_permutation_with_a_VALUE_DELTA_keeps_its_question_mark(self):
        self.screen("VALUE-DELTA")
        self.assertEqual(census.disposition("u", "f", SWAP)[0], "TRANSPOSED?")

    def test_no_screen_is_UNSCREENED_never_clean_and_never_a_bug(self):
        self.screen(None)
        verdict, datum, _ = census.disposition("u", "f", UNRELATED)
        self.assertEqual(verdict, "UNSCREENED")
        self.assertEqual(datum, "NO-SCREEN")


class ShippedOutput(unittest.TestCase):
    """The word `WRONG` alone must not survive anywhere a reader scans."""

    def test_no_bare_WRONG_disposition_remains(self):
        """The module docstring quotes the historical label; the CODE must
        not be able to emit it."""
        text = (REPO / "tools" / "gdl" / "composed_census"
                / "es_named_reloc_census.py").read_text(encoding="utf-8")
        code = text.split('"""', 2)[2]
        self.assertNotIn('"WRONG"', code)
        self.assertIn("WRONG-DATUM", code)
        self.assertIn("CANDIDATE", code)

    def test_the_json_carries_the_evidence_not_only_the_verdict(self):
        path = REPO / "build" / "GUNE5D" / "es_named_census.json"
        if not path.exists():
            self.skipTest("census not run in this tree")
        findings = json.loads(path.read_text())
        if not findings:
            self.skipTest("census found nothing")
        for row in findings:
            self.assertIn("datum_multiset", row)
            self.assertIn("row_labels_are_a_permutation", row)
            self.assertNotEqual(row["disposition"], "WRONG")

    def test_the_live_corpus_still_splits_both_ways(self):
        """A classifier that calls everything one thing proves nothing."""
        path = REPO / "build" / "GUNE5D" / "es_named_census.json"
        if not path.exists():
            self.skipTest("census not run in this tree")
        verdicts = {row["disposition"]
                    for row in json.loads(path.read_text())}
        if not verdicts:
            self.skipTest("census found nothing")
        self.assertGreater(len(verdicts), 1, verdicts)


if __name__ == "__main__":
    unittest.main()
