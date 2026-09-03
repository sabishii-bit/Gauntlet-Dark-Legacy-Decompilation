#!/usr/bin/env python3
"""T18 run-48 item 4: wf_word_diff's per-word DECODE column.

THE DEFECT. The whole-function CLASS line reads MNEMONIC divergence alone --
0 means "RECOLOR, cure is a register-assignment question", anything else means
SCHEDULE-REORDER -- and neither says whether a differing word is inside a
class a shipped rule reaches. WR built `WR_scratch/wr_wordscreen.py` to
answer that per word and then named it as the EXPIRY CHECK of a denial:

    attempt.WR_limitcamval2-is-not-postprocessor-eligible-a-branch-
    displacement-decides-it.20260903.v1, denial.expiry_check:
      "`python WR_scratch/wr_wordscreen.py game/boss/bosscam LimitCamVal2`
       -- the denial expires the moment +0x13c stops reading a BRANCH-class
       difference"

That path is a lane scratch directory. It does not exist in this checkout
(`Test-Path WR_scratch` -> False), so the denial was unfalsifiable by anyone
but its author. The same record's premise_measurement carries the numbers the
prototype produced:

    88 insns, DIFFERING WORDS = 29, MNEMONIC DIVERGENCE = 5, RELOC-SYMBOL
    MISMATCH = 0, PINNED = no
    Per-word decode over all 29: REGFIELD-ONLY 23, OPCODE 5, BRANCH 1
    THE DECIDING WORD is +0x13c, `ours 4081000c / target 40810008`

Reproduced EXACTLY by the promoted implementation at cdfff02e2.

TWO-SIDED CALIBRATION over all 2,883 comparable function pairs in this tree
(115 count-asymmetric, not scored) -- T18_scratch/t18_calib_item4.py:

    POSITIVES  16 functions whose CLASS line said RECOLOR while carrying
               words no register assignment reaches
    NEGATIVES  99 stay RECOLOR (genuinely recolourable), 162 stay
               SCHEDULE-REORDER with the verdict unchanged, 2,606 have no
               residual and print nothing

The RELOCATED class is what keeps that honest: 18 differing words image-wide
differ only in bits a relocation patches, and without the relocation table
four functions (gutil::gstrcmp, gutil::gstrcpy, pbutils::stricmp,
OSTime::OSGetTime) read `BRANCH 1` on a `R_PPC_REL14` displacement dtk emits
with a zero payload. Positives fell 20 -> 16.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import wf_word_diff as w  # noqa: E402

# The record's deciding word: both `ble`, xor 0x4, the whole difference in
# the BD displacement.
BLE_OURS, BLE_TARGET = 0x4081000C, 0x40810008
# gutil::gstrcmp +0x44, measured live: the target carries R_PPC_REL14 there.
BNE_OURS, BNE_TARGET = 0x4082FFBC, 0x40820000
# `lwz r0,0x340(r7)` vs `lwz r3,0x34c(r7)` -- camera_init_for_gamemode's
# transposed pair. Same opcode, and the displacement differs.
LWZ_OURS, LWZ_TARGET = 0x80070340, 0x8067034C


class WordClasses(unittest.TestCase):
    def test_a_branch_displacement_is_BRANCH(self):
        self.assertEqual(w.decode_word_class(BLE_OURS, BLE_TARGET), "BRANCH")

    def test_a_differing_opcode_is_OPCODE(self):
        # +0x140 of LimitCamVal2: `fmr` against `b`.
        self.assertEqual(w.decode_word_class(0xFC602090, 0x48000008), "OPCODE")

    def test_a_pure_register_recolour_is_REGFIELD_ONLY(self):
        # +0x138: fmr-family, one register field apart.
        self.assertEqual(w.decode_word_class(0xFC002040, 0xFC022040),
                         "REGFIELD-ONLY")

    def test_a_differing_displacement_is_IMMEDIATE(self):
        self.assertEqual(w.decode_word_class(LWZ_OURS, LWZ_TARGET),
                         "IMMEDIATE")

    def test_a_relocated_field_is_the_linkers(self):
        # Without the relocation this reads BRANCH -- "no shipped rule
        # reaches it" -- on bits the linker patches.
        self.assertEqual(w.decode_word_class(BNE_OURS, BNE_TARGET), "BRANCH")
        self.assertEqual(
            w.decode_word_class(BNE_OURS, BNE_TARGET, ("R_PPC_REL14",)),
            "RELOCATED")

    def test_an_unknown_relocation_type_owns_the_whole_word(self):
        # An unattributable relocation must not be credited to codegen.
        self.assertEqual(
            w.decode_word_class(BLE_OURS, BLE_TARGET, ("R_PPC_SOMETHING",)),
            "RELOCATED")

    def test_a_relocation_does_not_absorb_a_difference_outside_its_field(self):
        # R_PPC_REL14 owns bits 16..29; a differing BO field (bits 6..10) is
        # ours, and the word stays a BRANCH.
        self.assertEqual(
            w.decode_word_class(0x4181000C, 0x4081000C, ("R_PPC_REL14",)),
            "BRANCH")

    def test_an_opcode_difference_outranks_a_relocation(self):
        self.assertEqual(
            w.decode_word_class(0xFC602090, 0x48000008, ("R_PPC_REL24",)),
            "OPCODE")

    def test_the_register_mask_is_intersected_across_both_forms(self):
        # `li r3,4` (addi r3,0,4) against `addi r3,r4,4`: the differing bit
        # is in rA, a register slot in BOTH forms, so it is a recolour.
        self.assertEqual(w.decode_word_class(0x38600004, 0x38640004),
                         "REGFIELD-ONLY")


class Summary(unittest.TestCase):
    def test_unreachable_words_are_named_and_counted(self):
        counts = {"REGFIELD-ONLY": 23, "IMMEDIATE": 0, "BRANCH": 1,
                  "OPCODE": 5, "RELOCATED": 0}
        line = w.decode_summary(counts, 29)
        self.assertIn("REGFIELD-ONLY 23", line)
        self.assertIn("BRANCH 1", line)
        self.assertIn("6 of 29 word(s) lie OUTSIDE", line)

    def test_a_clean_recolour_says_so(self):
        counts = {"REGFIELD-ONLY": 12, "IMMEDIATE": 0, "BRANCH": 0,
                  "OPCODE": 0, "RELOCATED": 0}
        line = w.decode_summary(counts, 12)
        self.assertIn("all 12 differing word(s) are register fields", line)
        self.assertNotIn("OUTSIDE", line)

    def test_relocated_words_are_neither_reachable_nor_a_blocker(self):
        counts = {"REGFIELD-ONLY": 0, "IMMEDIATE": 0, "BRANCH": 0,
                  "OPCODE": 0, "RELOCATED": 1}
        line = w.decode_summary(counts, 1)
        self.assertIn("the LINKER's", line)
        self.assertIn("fndiff --relocs", line)
        self.assertNotIn("OUTSIDE", line)

    def test_no_differing_words_prints_nothing(self):
        counts = {name: 0 for name in w.DECODE_CLASSES}
        self.assertEqual(w.decode_summary(counts, 0), "")


class Rows(unittest.TestCase):
    def test_rows_carry_the_class_and_respect_the_reloc_index(self):
        rows = [(0x44, BNE_OURS, BNE_TARGET)]
        self.assertEqual(w.decode_rows(rows)[0][3], "BRANCH")
        self.assertEqual(
            w.decode_rows(rows, {0x44 // 4: ("R_PPC_REL14",)})[0][3],
            "RELOCATED")

    def test_counts_cover_every_class_even_at_zero(self):
        counts = w.decode_counts([(0, BLE_OURS, BLE_TARGET)])
        self.assertEqual(set(counts), set(w.DECODE_CLASSES))
        self.assertEqual(counts["BRANCH"], 1)
        self.assertEqual(counts["REGFIELD-ONLY"], 0)


class LiveReproduction(unittest.TestCase):
    """The record's own numbers, against the built objects."""

    def test_limitcamval2_decodes_23_5_1(self):
        try:
            _kind, ours, tgt = w.word_streams("game/boss/bosscam",
                                              "LimitCamVal2")
        except SystemExit:
            self.skipTest("game/boss/bosscam objects not built")
        rows = [(o, w.wf._u32(ours, o), w.wf._u32(tgt, o))
                for o in range(0, len(ours), 4)
                if w.wf._u32(ours, o) != w.wf._u32(tgt, o)]
        self.assertEqual(len(rows), 29)
        self.assertEqual(w.mnemonic_divergence(ours, tgt), 5)
        counts = w.decode_counts(
            rows, w.reloc_types_by_index("game/boss/bosscam", "LimitCamVal2",
                                         len(tgt) // 4))
        self.assertEqual(counts["REGFIELD-ONLY"], 23)
        self.assertEqual(counts["OPCODE"], 5)
        self.assertEqual(counts["BRANCH"], 1)
        self.assertEqual(counts["IMMEDIATE"], 0)
        deciding = [row for row in w.decode_rows(rows) if row[0] == 0x13C]
        self.assertEqual(deciding[0][1:], (BLE_OURS, BLE_TARGET, "BRANCH"))


if __name__ == "__main__":
    unittest.main()
