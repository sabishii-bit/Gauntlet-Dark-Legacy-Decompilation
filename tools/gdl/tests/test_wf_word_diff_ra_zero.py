"""A zero rA slot is the LITERAL zero, not GPR 0 (run-53 item 4).

`register_slot_mask` marks the rA field of a d-form instruction as a
five-bit register slot — correctly, it is one. What differs is what a ZERO in
it MEANS: `addi rD,0,K` is `li rD,K` (a constant load) and `lwz rD,d(0)` is an
absolute address, so no renaming produces the difference. A pair differing
only there therefore scored REGFIELD-ONLY and read as reachable by a
copy_register_fields rule, which is the class this project's DECODE line calls
"the only class a shipped rule reaches".

TWO-SIDED, measured at c7b741799 over the 127 scannable non-matching
functions (117 of 244 are count-asymmetric or have no object):

  POSITIVE  4,244 REGFIELD-ONLY rows; 64 in 39 functions carry the asymmetry
            (56 addi, 8 lwz). The decisive figure is the VERDICT flip: of the
            7 functions whose DECODE line declared "all differing words are
            register fields", 3 were wrong — critter::CritterLoadDone and
            critter::CritterLoadStartNext at 2 of 2 words each, and
            dbgtext::fn_800C03E0 at 1 of 202.
  NEGATIVE  4,180 rows keep REGFIELD-ONLY, and the other 4 of those 7
            functions keep the candidacy verdict. Excluding the 11 UPDATE
            opcodes (rA==0 is invalid there, not a literal) removed ZERO live
            rows — a correctness guard, not a filter doing work.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import wf_word_diff as wd  # noqa: E402


def addi(rd, ra, simm):
    return (14 << 26) | (rd << 21) | (ra << 16) | (simm & 0xFFFF)


def lwz(rd, ra, disp):
    return (32 << 26) | (rd << 21) | (ra << 16) | (disp & 0xFFFF)


def lwzu(rd, ra, disp):
    return (33 << 26) | (rd << 21) | (ra << 16) | (disp & 0xFFFF)


class RaZeroClassTests(unittest.TestCase):
    def test_mr_versus_li_is_not_a_register_field_difference(self):
        """`addi r4,r3,0` is `mr r4,r3`; `addi r4,0,0` is `li r4,0`."""
        self.assertEqual(wd.decode_word_class(addi(4, 3, 0), addi(4, 0, 0)),
                         "RA-ZERO")

    def test_the_direction_does_not_matter(self):
        self.assertEqual(wd.decode_word_class(addi(4, 0, 0), addi(4, 3, 0)),
                         "RA-ZERO")

    def test_a_differing_lwz_base_register_is_caught_too(self):
        """`lwz r3,0(0)` is an absolute address, not a load through GPR 0."""
        self.assertEqual(wd.decode_word_class(lwz(3, 0, 0), lwz(0, 3, 0)),
                         "RA-ZERO")

    def test_two_real_registers_stay_a_register_field_difference(self):
        """The NEGATIVE side: a renaming CAN map r3 onto r5."""
        self.assertEqual(wd.decode_word_class(addi(4, 3, 0), addi(4, 5, 0)),
                         "REGFIELD-ONLY")
        self.assertEqual(wd.decode_word_class(lwz(3, 4, 8), lwz(3, 5, 8)),
                         "REGFIELD-ONLY")

    def test_a_destination_only_difference_stays_regfield(self):
        self.assertEqual(wd.decode_word_class(addi(4, 3, 0), addi(7, 3, 0)),
                         "REGFIELD-ONLY")

    def test_both_sides_zero_is_not_an_asymmetry(self):
        """Two `li` forms differing in rD are a genuine recolor."""
        self.assertEqual(wd.decode_word_class(addi(4, 0, 0), addi(7, 0, 0)),
                         "REGFIELD-ONLY")

    def test_an_update_form_is_excluded(self):
        """rA==0 is an INVALID encoding for lwzu, not a literal zero, so the
        asymmetry cannot arise from legal codegen and must not be claimed."""
        self.assertFalse(wd.ra_zero_asymmetry(lwzu(3, 0, 4), lwzu(3, 5, 4)))

    def test_a_differing_immediate_still_wins(self):
        """An IMMEDIATE difference is decided before the rA question."""
        self.assertEqual(wd.decode_word_class(addi(4, 3, 1), addi(4, 0, 2)),
                         "IMMEDIATE")

    def test_different_opcodes_are_not_compared(self):
        self.assertFalse(wd.ra_zero_asymmetry(addi(4, 0, 0), lwz(4, 3, 0)))

    def test_the_class_is_published_and_not_reachable(self):
        self.assertIn("RA-ZERO", wd.DECODE_CLASSES)
        self.assertNotIn("RA-ZERO", wd.REACHABLE_DECODE_CLASSES)
        self.assertNotIn("RA-ZERO", wd.LINKER_OWNED_DECODE_CLASSES)


class DecodeSummaryTests(unittest.TestCase):
    def test_the_summary_explains_the_class_when_it_fires(self):
        counts = {name: 0 for name in wd.DECODE_CLASSES}
        counts["RA-ZERO"] = 2
        line = wd.decode_summary(counts, 2)
        self.assertIn("RA-ZERO 2", line)
        self.assertIn("LITERAL zero", line)
        self.assertIn("equivalent_copy_form", line)

    def test_a_clean_regfield_summary_is_unchanged(self):
        counts = {name: 0 for name in wd.DECODE_CLASSES}
        counts["REGFIELD-ONLY"] = 25
        line = wd.decode_summary(counts, 25)
        self.assertIn("all 25 differing word(s) are register fields", line)
        self.assertNotIn("LITERAL zero", line)


if __name__ == "__main__":
    unittest.main()
