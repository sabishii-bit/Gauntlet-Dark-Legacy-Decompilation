"""unabsorbed.py residual-CLASS tests.

Run-34 criticism (MV): two probes reported an IDENTICAL unabsorbed count of 4
while the residual moved from a class no postprocessor can reach to one a
permutation window can. The number was flat and the eligibility was not, so
the roster reported "no change" on a real change. Every row now carries a
class, and these tests pin the classifier's decision order.

The classifier is deliberately exercised over raw word bytes: it must not
need a built object, a toolchain, or the webfrank backend to be testable.
"""

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from unabsorbed import (ELIGIBLE_CLASSES, opcode_key, residual_class)


def words(*ws):
    return b"".join(struct.pack(">I", w) for w in ws)


# Real PPC encodings, so the opcode-key widths are exercised on the forms
# this project's objects actually contain.
LI_R3_1 = 0x38600001        # addi r3,r0,1        primary 14
LI_R3_2 = 0x38600002        # addi r3,r0,2        primary 14, other immediate
LWZ_R3_8_R4 = 0x80640008    # lwz  r3,8(r4)       primary 32
LWZ_R3_12_R4 = 0x8064000C   # lwz  r3,12(r4)      primary 32, other disp
OR_R3_R4_R4 = 0x7C831B78    # mr   r3,r4          primary 31, XO 444
ADD_R3_R4_R5 = 0x7C642A14   # add  r3,r4,r5       primary 31, XO 266
FMADDS_C4 = 0xEC22213A      # fmadds fr1,fr2,fr4,fr4   primary 59, A-form 29
FMADDS_C5 = 0xEC22217A      # fmadds fr1,fr2,fr5,fr4   primary 59, A-form 29
FMR_F1_F2 = 0xFC201090      # fmr  fr1,fr2        primary 63, X-form XO 72


class OpcodeKeyTests(unittest.TestCase):
    def test_register_and_immediate_fields_do_not_change_the_key(self):
        self.assertEqual(opcode_key(LI_R3_1), opcode_key(LI_R3_2))
        self.assertEqual(opcode_key(LWZ_R3_8_R4), opcode_key(LWZ_R3_12_R4))

    def test_extended_opcode_separates_same_primary_instructions(self):
        self.assertNotEqual(opcode_key(OR_R3_R4_R4), opcode_key(ADD_R3_R4_R5))

    def test_a_form_fp_uses_the_five_bit_xo_so_frc_is_not_an_opcode(self):
        """The soundness case for the A-form width rule.

        Read with the 10-bit XO, two fmadds differing only in FRC would key
        as DIFFERENT opcodes and a pure recolor would be reported as
        source-structural — the exact misclassification this column exists
        to prevent.
        """
        self.assertEqual(opcode_key(FMADDS_C4), opcode_key(FMADDS_C5))
        self.assertEqual(opcode_key(FMADDS_C4), (59, 29))

    def test_x_form_fp_keeps_the_ten_bit_xo(self):
        self.assertEqual(opcode_key(FMR_F1_F2), (63, 72))
        self.assertNotEqual(opcode_key(FMR_F1_F2), opcode_key(FMADDS_C4))


class ResidualClassTests(unittest.TestCase):
    def test_undefined_metric_is_count_asymmetric(self):
        self.assertEqual(residual_class(b"", b"", None), "count-asymmetric")

    def test_no_unabsorbed_words_is_allocator(self):
        ours = words(LI_R3_1, LWZ_R3_8_R4)
        self.assertEqual(residual_class(ours, ours, []), "allocator")

    def test_a_reorder_of_the_same_words_is_schedule(self):
        ours = words(LI_R3_1, LWZ_R3_8_R4)
        target = words(LWZ_R3_8_R4, LI_R3_1)
        self.assertEqual(residual_class(ours, target, [0, 4]), "schedule")

    def test_same_opcodes_different_displacement_is_operand(self):
        ours = words(LWZ_R3_8_R4)
        target = words(LWZ_R3_12_R4)
        self.assertEqual(residual_class(ours, target, [0]), "operand")

    def test_a_different_opcode_is_source_structural(self):
        ours = words(OR_R3_R4_R4)
        target = words(ADD_R3_R4_R5)
        self.assertEqual(residual_class(ours, target, [0]),
                         "source-structural")

    def test_opcode_evidence_outranks_the_permutation_test(self):
        """A structural row must never be reported as a schedule candidate.

        Both sides hold two words; the word multisets differ AND the opcode
        multisets differ, so the strongest evidence has to win.
        """
        ours = words(OR_R3_R4_R4, LI_R3_1)
        target = words(ADD_R3_R4_R5, LI_R3_1)
        self.assertEqual(residual_class(ours, target, [0, 4]),
                         "source-structural")

    def test_only_the_unabsorbed_offsets_are_examined(self):
        """Absorbed words differ by a register respell by construction.

        Including them would conflate the recolor stage with everything
        else: offset 0 differs structurally here, but it is ABSORBED, so the
        class is decided by offset 4 alone.
        """
        ours = words(OR_R3_R4_R4, LWZ_R3_8_R4)
        target = words(ADD_R3_R4_R5, LWZ_R3_12_R4)
        self.assertEqual(residual_class(ours, target, [4]), "operand")

    def test_the_same_count_can_carry_different_classes(self):
        """THE run-34 CRITICISM, as a test.

        Two residuals, both exactly 2 unabsorbed words: one is a permutation
        candidate, the other is source work. The count cannot tell them
        apart; the class must.
        """
        offsets = [0, 4]
        schedule = residual_class(
            words(LI_R3_1, LWZ_R3_8_R4), words(LWZ_R3_8_R4, LI_R3_1), offsets)
        structural = residual_class(
            words(LI_R3_1, OR_R3_R4_R4), words(LI_R3_1, ADD_R3_R4_R5),
            offsets)
        self.assertNotEqual(schedule, structural)
        self.assertIn(schedule, ELIGIBLE_CLASSES)
        self.assertNotIn(structural, ELIGIBLE_CLASSES)


class EligibilityTests(unittest.TestCase):
    def test_only_allocator_and_schedule_are_called_reachable(self):
        self.assertEqual(set(ELIGIBLE_CLASSES), {"allocator", "schedule"})
        for name in ("operand", "source-structural", "count-asymmetric"):
            self.assertNotIn(name, ELIGIBLE_CLASSES)


if __name__ == "__main__":
    unittest.main()
