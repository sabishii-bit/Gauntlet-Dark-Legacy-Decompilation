"""savedregs.py — the callee-saved correspondence table.

Why the tool exists: `--ops` compares opcode MULTISETS, so a function whose
streams differ only in which callee-saved register holds which local reads
"opcode multiset: IDENTICAL". MV spent three lanes on
game/movie/movieplayer.cpp::fn_800D8BCC that way and the answer appeared as
soon as the two assignments were written side by side
(claim.law.MV_callee-saved-numbering-has-a-width-class-ahead-of-declaration-
order.20260902.v1).

Why the tests are shaped this way: the FIRST version of correspondence()
paired the two streams by first-definition ORDER, and running it on the very
function the law came from refuted it — fn_800D8BCC's assignment already
matches the target and only the emission order differs, so order-pairing
reported six permuted webs where there are none. Assignment and schedule are
different questions and the table now answers them separately.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import savedregs  # noqa: E402


class ParseInstructionTests(unittest.TestCase):
    def test_operands_are_split_and_trimmed(self):
        self.assertEqual(("addi", ["r31", "r25", "0"]),
                         savedregs.parse_instruction("addi r31,r25,0"))

    def test_a_relocation_suffix_is_not_an_operand(self):
        mnemonic, operands = savedregs.parse_instruction(
            "addi r31,r3,0  @potionicon_tab(ADDR16_LO)")
        self.assertEqual("addi", mnemonic)
        self.assertEqual(["r31", "r3", "0"], operands)

    def test_a_bare_mnemonic_has_no_operands(self):
        self.assertEqual(("nop", []), savedregs.parse_instruction("nop"))


class DefinesTests(unittest.TestCase):
    def test_the_first_operand_is_the_destination(self):
        self.assertTrue(savedregs.defines("addi", ["r31", "r25", "0"], "r31"))

    def test_a_source_operand_is_not_a_definition(self):
        self.assertFalse(savedregs.defines("addi", ["r31", "r25", "0"], "r25"))

    def test_a_store_writes_memory_not_its_first_operand(self):
        self.assertFalse(savedregs.defines("stw", ["r31", "8(r1)"], "r31"))
        self.assertFalse(savedregs.defines("stfs", ["f31", "0(r3)"], "f31"))

    def test_a_compare_writes_a_condition_register(self):
        self.assertFalse(savedregs.defines("cmpwi", ["r31", "0"], "r31"))

    def test_a_branch_defines_nothing(self):
        self.assertFalse(savedregs.defines("bl", ["r31"], "r31"))


class RoleTests(unittest.TestCase):
    """The role fold is what keeps a MATCHING web off the mismatch list."""

    def test_the_destination_is_not_part_of_the_role(self):
        self.assertEqual(savedregs.role("slwi r30,r23,2"),
                         savedregs.role("slwi r29,r23,2"))

    def test_the_two_spellings_of_a_copy_are_one_role(self):
        """Measured on fn_800D8BCC: target `addi r31,r25,0` vs ours
        `mr r31,r25` is the SAME web, and without this fold the table calls
        a matching assignment permuted."""
        self.assertEqual(savedregs.role("mr r31,r25"),
                         savedregs.role("addi r31,r25,0"))

    def test_different_values_are_different_roles(self):
        self.assertNotEqual(savedregs.role("li r29,0"),
                            savedregs.role("li r29,1"))
        self.assertNotEqual(savedregs.role("addi r30,r6,0"),
                            savedregs.role("li r30,-1"))

    def test_addi_with_a_nonzero_displacement_is_not_a_copy(self):
        self.assertNotEqual(savedregs.role("addi r18,r31,3136"),
                            savedregs.role("mr r18,r31"))


class FrameAndSaveSetTests(unittest.TestCase):
    ROWS = [[0x0, "stwu r1,-96(r1)"], [0x4, "mflr r0"],
            [0x8, "stmw r19,44(r1)"], [0x10, "mr r20,r3"]]

    def test_frame_size_is_read_from_the_prologue(self):
        self.assertEqual(96, savedregs.frame_size(self.ROWS))

    def test_a_function_with_no_frame_reports_none(self):
        self.assertIsNone(savedregs.frame_size([[0, "mr r3,r4"]]))

    def test_the_stmw_base_names_the_contiguous_save_run(self):
        self.assertEqual(19, savedregs.multiple_save_base(self.ROWS))

    def test_no_stmw_is_not_an_error(self):
        self.assertIsNone(savedregs.multiple_save_base([[0, "mr r3,r4"]]))


class FirstDefinitionTests(unittest.TestCase):
    ROWS = [[0x0, "stwu r1,-96(r1)"],
            [0x8, "stmw r19,44(r1)"],
            [0x10, "mr r20,r3"],
            [0x14, "addi r19,r6,0"],
            [0x20, "mr r20,r7"],          # a LATER write, not the first
            [0x30, "stw r20,8(r1)"],      # a store must not create a row
            [0xa8, "li r29,0"]]

    def test_only_the_first_definition_of_each_register_is_kept(self):
        rows = savedregs.first_definitions(self.ROWS, savedregs.GPR_SAVED)
        self.assertEqual([("r20", 0x10, "mr r20,r3"),
                          ("r19", 0x14, "addi r19,r6,0"),
                          ("r29", 0xa8, "li r29,0")], rows)

    def test_prologue_stmw_never_manufactures_a_web(self):
        rows = savedregs.first_definitions(self.ROWS, savedregs.GPR_SAVED)
        self.assertNotIn("r19", [r[0] for r in rows if r[1] == 0x8])

    def test_caller_saved_registers_are_not_callee_saved_webs(self):
        rows = savedregs.first_definitions([[0, "mr r3,r4"]],
                                           savedregs.GPR_SAVED)
        self.assertEqual([], rows)

    def test_rows_come_back_in_first_definition_order(self):
        rows = savedregs.first_definitions(self.ROWS, savedregs.GPR_SAVED)
        self.assertEqual([0x10, 0x14, 0xa8], [r[1] for r in rows])


class CorrespondenceTests(unittest.TestCase):
    """Pairing is by REGISTER NUMBER. The refuted first design paired by
    first-definition ORDER and reported six permuted webs on a function
    whose assignment matches exactly."""

    # fn_800D8BCC's real rows, in FIRST-DEFINITION order as
    # first_definitions() returns them. The assignment is identical; only
    # the emission order moves.
    TARGET = [("r29", 0xa8, "li r29,0"), ("r31", 0xb0, "addi r31,r25,0"),
              ("r30", 0xc8, "slwi r30,r23,2")]
    OURS = [("r31", 0xa8, "mr r31,r25"), ("r30", 0xd0, "slwi r30,r23,2"),
            ("r29", 0xd4, "li r29,0")]

    def test_the_matching_assignment_reports_no_mismatch(self):
        pairs = savedregs.correspondence(self.TARGET, self.OURS)
        self.assertEqual({}, savedregs.permutation(pairs))

    def test_order_only_differences_are_reported_as_schedule_not_assignment(
            self):
        self.assertNotEqual(savedregs.emission_order(self.TARGET),
                            savedregs.emission_order(self.OURS))
        self.assertEqual({}, savedregs.permutation(
            savedregs.correspondence(self.TARGET, self.OURS)))

    def test_a_genuinely_different_role_is_named_with_both_values(self):
        ours = [("r30", 0x48, "li r30,-1")]
        moved = savedregs.permutation(savedregs.correspondence(
            [("r30", 0x6c, "addi r30,r6,0")], ours))
        self.assertEqual({"r30": ("mr r6", "li -1")}, moved)

    def test_rows_are_ordered_r31_downward_with_fprs_separate(self):
        pairs = savedregs.correspondence(
            [("r29", 0, "li r29,0"), ("f31", 4, "fmr f31,f1"),
             ("r31", 8, "mr r31,r3")], [])
        self.assertEqual(["f31", "r31", "r29"], [p[0] for p in pairs])

    def test_a_web_in_one_stream_only_is_paired_against_none(self):
        pairs = savedregs.correspondence(
            [], [("r15", 0x28, "addi r15,r4,0")])
        self.assertEqual([("r15", None, ("r15", 0x28, "addi r15,r4,0"))],
                         pairs)
        # It is not a role mismatch: there is no role to compare it against.
        self.assertEqual({}, savedregs.permutation(pairs))


class FormatTableTests(unittest.TestCase):
    TARGET = [[0x0, "stwu r1,-96(r1)"], [0x10, "mr r20,r3"],
              [0xa8, "li r29,0"]]

    def test_a_matching_assignment_says_so_in_one_line(self):
        text = savedregs.format_table("u", "f", self.TARGET, self.TARGET)
        self.assertIn("ASSIGNMENT MATCHES", text)
        self.assertNotIn("DIFFERENT ROLE", text)

    def test_a_mismatch_names_the_register_and_both_roles(self):
        ours = [[0x0, "stwu r1,-96(r1)"], [0x10, "mr r20,r3"],
                [0xa8, "li r29,1"]]
        text = savedregs.format_table("u", "f", self.TARGET, ours)
        self.assertIn("DIFFERENT ROLE", text)
        self.assertIn("r29: target holds `li 0`, ours holds `li 1`", text)

    def test_the_width_class_law_is_quoted_only_when_it_applies(self):
        ours = [[0x0, "stwu r1,-96(r1)"], [0x10, "mr r20,r3"],
                [0xa8, "li r29,1"]]
        self.assertIn("NARROW/byte-typed",
                      savedregs.format_table("u", "f", self.TARGET, ours))
        self.assertNotIn(
            "NARROW/byte-typed",
            savedregs.format_table("u", "f", self.TARGET, self.TARGET))

    def test_a_function_with_no_saved_webs_says_so_instead_of_an_empty_table(
            self):
        rows = [[0x0, "mr r3,r4"], [0x4, "blr"]]
        text = savedregs.format_table("u", "f", rows, rows)
        self.assertIn("no callee-saved register is defined", text)
        self.assertIn("slotdiff", text)

    def test_a_save_set_size_difference_is_called_out(self):
        ours = self.TARGET + [[0xb0, "addi r15,r4,0"]]
        text = savedregs.format_table("u", "f", self.TARGET, ours)
        self.assertIn("SAVE-SET SIZE DIFFERS", text)

    def test_the_frame_and_stmw_base_are_reported(self):
        target = [[0x0, "stwu r1,-256(r1)"], [0x8, "stmw r16,200(r1)"],
                  [0x10, "mr r20,r3"]]
        ours = [[0x0, "stwu r1,-264(r1)"], [0x8, "stmw r15,196(r1)"],
                [0x10, "mr r20,r3"]]
        text = savedregs.format_table("u", "f", target, ours)
        self.assertIn("frame: target 256  ours 264", text)
        self.assertIn("stmw base: target r16 ours r15", text)


if __name__ == "__main__":
    unittest.main()
