#!/usr/bin/env python3
"""Saved-register findings must qualify the verdict, not hide in detail."""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import savedregs               # noqa: E402


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


if __name__ == "__main__":
    unittest.main()
