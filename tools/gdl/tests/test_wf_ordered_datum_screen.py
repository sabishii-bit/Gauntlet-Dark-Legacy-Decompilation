"""Pin the ORDERED-datum screen's register-slot discriminator.

Every case below is a REFUSAL test in the sense AGENTS.md means: each fails
if its specific guard is removed, and each is a shape measured in the live
corpus rather than an invented one.  The two load-bearing ones are the pair
that decides the whole screen:

  * gamemain::fn_80057024 -- target `lfs f1,255.0f` then `lfs f4,0.0f`
    against ours `lfs f4,0.0f` then `lfs f1,255.0f`.  Each datum reaches
    the SAME register; only the emission order moved.  Drop the crossed-
    register REORDER test and this reads DEFECT, which is the false
    positive that would have shipped 44 functions as bugs.
  * enemy::move_logic00 -- both streams load f30, the target with pi and
    ours with 2pi.  Drop the same-slot DATUM-SWAP test and this reads
    BENIGN, which is fndiff's own documented blind spot re-opened.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import wf_ordered_datum_screen as screen                    # noqa: E402


class OrderedDatumScreen(unittest.TestCase):

    def test_identical_order_is_benign(self):
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa", "B:bb"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(result["ordered_mismatches"], 0)

    def test_crossed_registers_are_a_reorder_not_a_defect(self):
        # gamemain::fn_80057024 +0x... : f1 holds 255.0f and f4 holds 0.0f
        # in BOTH streams; only the emission order moved.
        result = screen.classify(
            ["B:437f0000", "B:00000000"], ["B:00000000", "B:437f0000"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f4,0(0)", "lfs     f1,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(result["mismatch_kinds"].get("REORDER"), 2)
        self.assertNotIn("DATUM-SWAP", result["mismatch_kinds"])

    def test_same_register_slot_is_a_datum_swap(self):
        # enemy::move_logic00: both streams load f30, target pi, ours 2pi.
        result = screen.classify(
            ["B:401921fb54524550", "B:400921fb54524550"],
            ["B:400921fb54524550", "B:401921fb54524550"],
            ["lfd     f30,0(0)", "lfd     f29,0(0)"],
            ["lfd     f30,0(0)", "lfd     f29,0(0)"])
        self.assertEqual(result["verdict"], "DEFECT")
        self.assertEqual(result["mismatch_kinds"].get("DATUM-SWAP"), 2)

    def test_store_pair_with_swapped_globals_is_a_datum_swap(self):
        # controls::ReadControls: `stw r3,A` / `stw r5,B` against
        # `stw r3,B` / `stw r5,A` -- the same value register writes the
        # other global.
        result = screen.classify(
            ["A:0x803445E4", "A:0x80344604"],
            ["A:0x80344604", "A:0x803445E4"],
            ["stw     r3,0(0)", "stw     r5,0(0)"],
            ["stw     r3,0(0)", "stw     r5,0(0)"])
        self.assertEqual(result["verdict"], "DEFECT")

    def test_conversion_magic_is_excluded(self):
        result = screen.classify(
            ["B:4330000080000000", "B:42700000"],
            ["B:42700000", "B:4330000080000000"],
            ["lfd     f2,0(0)", "lfs     f3,0(0)"],
            ["lfs     f3,0(0)", "lfd     f2,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(
            result["mismatch_kinds"].get("CONVERSION-MAGIC"), 2)

    def test_a_value_delta_is_never_reported_benign(self):
        # The multisets differ, so a wrong CONSTANT exists somewhere; this
        # screen answers the ORDER question only and must not clear it.
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa", "B:cc"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertIn("MULTISETS differ", result["why"])

    def test_unequal_relocation_counts_are_undecidable(self):
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"], ["lfs     f1,0(0)"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertIn("relocation counts differ", result["why"])

    def test_split_form_mirror_is_undecidable_not_a_defect(self):
        # An @ha/@lo materialisation interleave mirrors under two DIFFERENT
        # mnemonics; it is unpaired evidence, never a datum verdict.
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:bb", "B:aa"],
            ["lis     r3,0", "addi    r4,r5,0"],
            ["addi    r9,r5,0", "lis     r7,0"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertEqual(result["mismatch_kinds"].get("SPLIT-FORM"), 2)

    def test_register_field_reads_the_first_operand(self):
        self.assertEqual(screen.register_field("stw     r3,0(0)"), "r3")
        self.assertEqual(screen.register_field("addi    r27,r12,0"), "r27")
        self.assertEqual(screen.register_field("lfd     f30,0(0)"), "f30")
        self.assertEqual(screen.register_field(""), "")

    def test_ordered_symbols_pairs_each_relocation_with_its_instruction(self):
        lines = ["lis     r3,0", "    R_PPC_ADDR16_HA  lbl_1",
                 "nop", "lfs     f1,0(0)", "    R_PPC_EMB_SDA21  lbl_2"]
        symbols, instructions = screen.ordered_symbols(
            lines, with_offsets=True)
        self.assertEqual(symbols, ["lbl_1", "lbl_2"])
        self.assertEqual(instructions, ["lis     r3,0", "lfs     f1,0(0)"])


if __name__ == "__main__":
    unittest.main()
