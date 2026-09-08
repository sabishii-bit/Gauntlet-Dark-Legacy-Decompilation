"""The DOL decides between disagreeing bases in af_data_base_census.

Run 61 item 8. Reproduced at 20c0d7ea1: the census printed THREE bases for
game/game/controls `.sdata2` -- 0x803463E8 (one anonymous-pool row),
0x803463F8 (six rows) and 0x80346408 (one row) -- for a section whose
shipped claim is 0x803463F8 and whose bytes are 100% equal there. Row counts
cannot settle that; the same run printed SIX mutually disagreeing `.sbss`
bases for the same unit.

TWO-SIDED, remeasured on the native-only tree at 7ab03f3c1 over the 312
(unit, section) pairs that resolve any base (build/c62_af_rule_delta.py):
7 paste lines gained, 5 withdrawn because the single candidate is measurably
wrong, 300 unchanged.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import af_data_base_census as af  # noqa: E402
import fndiff  # noqa: E402

BUILT = (os.path.exists(os.path.join(
    ROOT, "build", "GUNE5D", "obj", "game", "game", "controls.o"))
    and os.path.exists(str(fndiff.OBJDUMP)))


@unittest.skipUnless(BUILT, "needs the split target objects and a built tree")
class ControlsDisagreement(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sections = af.census("game/game/controls")["sections"]

    def test_the_three_sdata2_bases_are_still_all_reported(self):
        row = self.sections[".sdata2"]
        self.assertEqual(sorted(row["candidate_bases"]),
                         ["0x803463E8", "0x803463F8", "0x80346408"])

    def test_only_the_byte_equal_base_becomes_a_paste_line(self):
        row = self.sections[".sdata2"]
        self.assertEqual(row["splits_line"].split(),
                         [".sdata2", "start:0x803463F8", "end:0x80346470"])
        self.assertIn("100%", row["base_decided_by"])

    def test_the_measured_scores_separate_the_three(self):
        scores = self.sections[".sdata2"]["dol_agreement"]
        self.assertEqual(scores["0x803463F8"]["percent"], 100.0)
        for wrong in ("0x803463E8", "0x80346408"):
            with self.subTest(base=wrong):
                self.assertLess(scores[wrong]["percent"], 20.0)

    def test_a_section_whose_best_base_is_not_equal_gets_no_paste_line(self):
        row = self.sections[".rodata"]
        self.assertGreater(len(row["candidate_bases"]), 1)
        self.assertNotIn("splits_line", row)
        self.assertTrue(row["dol_comparable"])
        self.assertLess(max(entry["percent"] for entry
                            in row["dol_agreement"].values()), 100.0)

    def test_multi_candidate_bss_stays_a_candidate_list(self):
        row = self.sections[".sbss"]
        self.assertGreater(len(row["candidate_bases"]), 1)
        self.assertNotIn("splits_line", row)
        self.assertFalse(row["dol_comparable"])
        self.assertTrue(all(entry is None
                            for entry in row["dol_agreement"].values()))

    def test_single_candidate_bss_is_pasted_but_labelled_unverified(self):
        row = self.sections[".bss"]
        self.assertEqual(row["splits_line"].split(),
                         [".bss", "start:0x802407B8", "end:0x80240FD0"])
        self.assertIn("consensus ONLY", row["base_decided_by"])
        self.assertIn("no DOL bytes", row["base_decided_by"])


@unittest.skipUnless(BUILT, "needs the split target objects and a built tree")
class WithdrawnAndKept(unittest.TestCase):
    def test_a_single_candidate_that_is_measurably_wrong_is_withdrawn(self):
        # game/audio/dcs .rodata: one candidate, 95.6% equal. A tool that
        # pastes it hands the next lane a claim that is 4% wrong.
        row = af.census("game/audio/dcs")["sections"][".rodata"]
        self.assertEqual(len(row["candidate_bases"]), 1)
        self.assertNotIn("splits_line", row)
        percent = next(iter(row["dol_agreement"].values()))["percent"]
        self.assertTrue(0.0 < percent < 100.0)

    def test_an_all_relocated_section_keeps_its_consensus_paste_line(self):
        # zlib/inflate .data relocates every word it has, so no comparison
        # is possible; treating "unmeasurable" as "failed" would withdraw a
        # correct landed claim.
        row = af.census("zlib/inflate")["sections"][".data"]
        self.assertFalse(row["dol_comparable"])
        self.assertIn("splits_line", row)
        self.assertIn("consensus ONLY", row["base_decided_by"])

    def test_a_byte_equal_single_candidate_still_pastes(self):
        row = af.census("game/world/btricol")["sections"][".sdata2"]
        self.assertEqual(row["splits_line"].split(),
                         [".sdata2", "start:0x80345D40", "end:0x80345DB8"])
        self.assertIn("100%", row["base_decided_by"])


if __name__ == "__main__":
    unittest.main()
