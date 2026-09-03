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


class ScopeBannerTests(unittest.TestCase):
    """Run-41 item 5. The old closing line — "Whatever residual remains is
    NOT a save-register assignment question" — was a claim about the whole
    function drawn from a comparison of the FIRST definition of each
    CALLEE-SAVED register, and three records took it as a premise while
    fn_800D8BCC's residual was a VOLATILE colour cascade this tool never
    reads. Corpus calibration: of 493 functions where the old all-clear
    fired, 426 survive the wider screen and 67 do not (40 with volatile-only
    differing rows, 26 whose counts make the question unscreenable, 12 with
    a later-web mismatch)."""

    MATCHING = [[0x0, "stwu r1,-64(r1)"], [0x4, "mr r31,r3"],
                [0x8, "addi r4,r5,1"], [0xc, "blr"]]

    def test_the_all_clear_no_longer_claims_the_whole_function(self):
        ours = [[0x0, "stwu r1,-64(r1)"], [0x4, "mr r31,r3"],
                [0x8, "addi r4,r5,2"], [0xc, "blr"]]
        text = savedregs.format_table("u", "f", self.MATCHING, ours)
        self.assertIn("at its FIRST definition", text)
        self.assertNotIn("NOT a save-register assignment question", text)

    def test_a_volatile_only_differing_row_is_counted_and_named(self):
        ours = [[0x0, "stwu r1,-64(r1)"], [0x4, "mr r31,r3"],
                [0x8, "addi r4,r5,2"], [0xc, "blr"]]
        text = savedregs.format_table("u", "f", self.MATCHING, ours)
        self.assertIn("ROWS THIS TABLE CANNOT SEE: 1 differing", text)
        self.assertIn("volatile-register residual", text)

    def test_unequal_counts_report_UNSCREENED_rather_than_zero(self):
        ours = self.MATCHING[:-1]
        text = savedregs.format_table("u", "f", self.MATCHING, ours)
        self.assertIn("not computable", text)
        self.assertIn("UNSCREENED", text)

    def test_a_later_web_holding_a_different_value_is_reported(self):
        target = [[0x0, "mr r31,r3"], [0x4, "lwz r31,8(r4)"], [0x8, "blr"]]
        ours = [[0x0, "mr r31,r3"], [0x4, "lwz r31,12(r4)"], [0x8, "blr"]]
        later, count_diffs = savedregs.web_mismatches(
            target, ours, savedregs.GPR_SAVED + savedregs.FPR_SAVED)
        self.assertEqual([(row[0], row[1]) for row in later], [("r31", 1)])
        self.assertEqual(count_diffs, [])
        text = savedregs.format_table("u", "f", target, ours)
        self.assertIn("LATER-WEB MISMATCH: 1 definition(s)", text)

    def test_a_differing_definition_COUNT_is_reported_not_silently_zipped(self):
        target = [[0x0, "mr r31,r3"], [0x4, "lwz r31,8(r4)"], [0x8, "blr"]]
        ours = [[0x0, "mr r31,r3"], [0x8, "blr"]]
        _later, count_diffs = savedregs.web_mismatches(
            target, ours, savedregs.GPR_SAVED + savedregs.FPR_SAVED)
        self.assertEqual(count_diffs, [("r31", 2, 1)])

    def test_per_web_lists_every_definition_of_a_register(self):
        target = [[0x0, "mr r31,r3"], [0x4, "lwz r31,8(r4)"], [0x8, "blr"]]
        ours = [[0x0, "mr r31,r3"], [0x4, "lwz r31,12(r4)"], [0x8, "blr"]]
        text = savedregs.format_table("u", "f", target, ours, per_web=True)
        self.assertIn("r31[0]", text)
        self.assertIn("r31[1]", text)
        self.assertIn("DIFFERENT ROLE", text)

    def test_per_web_is_off_by_default(self):
        target = [[0x0, "mr r31,r3"], [0x4, "lwz r31,8(r4)"], [0x8, "blr"]]
        self.assertNotIn("r31[0]",
                         savedregs.format_table("u", "f", target, target))


class MoveToSpecialRegisterTests(unittest.TestCase):
    """`mtctr r31` moves TO the count register; r31 is its SOURCE.

    Without the `mt` entry in NON_DEFINING a loop-count setup manufactures a
    phantom definition of a callee-saved register — the one place the tool
    reads a source operand as a destination. Corpus: 12 such rows across the
    267 nonmatching functions with both objects on disk.
    """

    def test_mtctr_does_not_define_its_source(self):
        self.assertFalse(savedregs.defines("mtctr", ["r31"], "r31"))
        self.assertIsNone(savedregs.definition_target("mtctr r31"))

    def test_mtcrf_does_not_define_its_source(self):
        self.assertIsNone(savedregs.definition_target("mtcrf 8,r30"))

    def test_mflr_still_defines_its_destination(self):
        self.assertEqual("r0", savedregs.definition_target("mflr r0"))

    def test_a_phantom_definition_cannot_reach_the_table(self):
        rows = [[0x0, "mr r31,r3"], [0x4, "mtctr r31"], [0x8, "blr"]]
        defs = savedregs.first_definitions(rows, savedregs.GPR_SAVED)
        self.assertEqual([("r31", 0x0, "mr r31,r3")], defs)


class WebRoleTests(unittest.TestCase):
    """A role must be register-free or a PERMUTED range cannot be seen."""

    def test_an_induction_step_has_the_same_role_in_any_register(self):
        self.assertEqual(savedregs.web_role("addi r21,r21,4"),
                         savedregs.web_role("addi r19,r19,4"))

    def test_a_genuinely_different_source_is_still_a_different_role(self):
        self.assertNotEqual(savedregs.web_role("addi r19,r29,0"),
                            savedregs.web_role("addi r19,r25,0"))

    def test_the_copy_fold_survives_masking(self):
        self.assertEqual(savedregs.web_role("mr r31,r25"),
                         savedregs.web_role("addi r19,r25,0"))


class RegisterClassTests(unittest.TestCase):
    def test_the_callee_saved_bank_starts_at_14(self):
        self.assertEqual("callee-saved", savedregs.register_class("r14"))
        self.assertEqual("volatile", savedregs.register_class("r12"))
        self.assertEqual("callee-saved", savedregs.register_class("f31"))
        self.assertEqual("volatile", savedregs.register_class("f13"))

    def test_the_abi_registers_are_not_a_locals_home(self):
        for register in ("r1", "r2", "r13"):
            self.assertEqual("abi", savedregs.register_class(register))

    def test_a_non_register_token_has_no_class(self):
        self.assertIsNone(savedregs.register_class("8(r1)"))


class LifetimePairingTests(unittest.TestCase):
    """Run-42 item 1. The first `--per-web` paired later definitions by
    ORDINAL WITHIN A REGISTER, which is correct only while no live range
    changes register — the exact case the view exists for. Measured on
    game/movie/movieplayer::fn_800D8BCC (215/215 insns): ordinal pairing
    aligned the target's `add r19,r12,r0` against our `li r19,0` and printed
    5 DIFFERENT ROLE rows; lifetime pairing reads the same function as
    2 PERMUTED (r21->r19), 2 ESCAPED (r19->r12) and 2 INTRUDER (r21).

    Corpus calibration (54 units, 267 nonmatching functions with both
    objects): the ordinal view flagged 3187 later rows across 176 functions;
    lifetime pairing reclassifies 1869 of them, finds an ESCAPE or INTRUSION
    — a class the ordinal view cannot express at all — in 146 functions, and
    names a moved range in 20 functions the ordinal view called clean.
    """

    # The shape of fn_800D8BCC's second loop, both streams, at the offsets
    # it really holds: a permuted counter and a pointer that escaped.
    TARGET = [[0x1c4, "li r21,0"],
              [0x1d8, "add r19,r12,r0"],
              [0x1e0, "lbz r10,0(r24)"],
              [0x23c, "addi r21,r21,4"],
              [0x248, "add r19,r19,r9"]]
    OURS = [[0x1c4, "li r19,0"],
            [0x1d8, "add r12,r0,r11"],
            [0x1e0, "lbz r21,0(r24)"],
            [0x23c, "addi r19,r19,4"],
            [0x248, "add r12,r12,r9"]]

    def verdicts(self, target, ours):
        return [row[3] for row in savedregs.lifetime_pairs(target, ours)]

    def test_a_permuted_range_is_named_not_called_a_role_mismatch(self):
        verdicts = self.verdicts(self.TARGET, self.OURS)
        self.assertIn("PERMUTED r21->r19", verdicts)
        self.assertEqual(2, sum(v == "PERMUTED r21->r19" for v in verdicts))

    def test_a_range_that_escaped_to_a_volatile_is_named(self):
        self.assertIn("ESCAPED r19->r12", self.verdicts(self.TARGET,
                                                        self.OURS))

    def test_a_volatile_role_promoted_in_ours_is_named(self):
        self.assertIn("INTRUDER r21 (target uses r10)",
                      self.verdicts(self.TARGET, self.OURS))

    def test_an_intruder_row_is_labelled_by_OUR_register(self):
        """The range under discussion is ours; labelling it by the target's
        volatile reads as a claim about a volatile register."""
        labels = [row[0] for row in savedregs.lifetime_pairs(self.TARGET,
                                                             self.OURS)
                  if row[3].startswith("INTRUDER")]
        self.assertTrue(all(label.startswith("ours r21") for label in labels),
                        labels)

    def test_a_reordered_prologue_is_in_place_not_a_permutation(self):
        """Measured: naive position pairing called 3 of fn_800D8BCC's 5
        'permutations' on rows the first-definition table correctly reads as
        an emission-order difference. Same register + same role wins."""
        target = [[0x0, "li r29,0"], [0x4, "nop"], [0x8, "mr r31,r25"]]
        ours = [[0x0, "mr r31,r25"], [0x4, "nop"], [0x8, "li r29,0"]]
        self.assertEqual(["in place", "in place"],
                         self.verdicts(target, ours))

    def test_the_same_register_holding_a_different_value_is_a_role_mismatch(
            self):
        target = [[0x0, "mr r31,r3"], [0x4, "lwz r31,8(r4)"]]
        ours = [[0x0, "mr r31,r3"], [0x4, "lwz r31,12(r4)"]]
        self.assertEqual(["in place", "DIFFERENT ROLE"],
                         self.verdicts(target, ours))

    def test_a_range_with_no_counterpart_is_unpaired_not_invented(self):
        target = [[0x0, "mr r31,r3"], [0x4, "addi r30,r29,0"]]
        ours = [[0x0, "mr r31,r3"]]
        self.assertEqual(["in place", "UNPAIRED"], self.verdicts(target,
                                                                 ours))

    def test_a_moved_range_reports_how_far_the_pairing_reached(self):
        """The window sweep makes ESCAPED/INTRUDER counts window-dependent,
        so a moved row must carry its reach rather than stand as a bare
        claim."""
        notes = [row[4] for row in savedregs.lifetime_pairs(self.TARGET,
                                                            self.OURS)
                 if row[3].startswith(("PERMUTED", "ESCAPED", "INTRUDER"))]
        self.assertTrue(all("aligned rows apart" in note for note in notes),
                        notes)

    def test_the_banner_names_the_permutation_instead_of_a_bare_count(self):
        text = savedregs.format_table("u", "f", self.TARGET, self.OURS)
        self.assertIn("LIFETIME PERMUTATION", text)
        self.assertIn("r21->r19", text)
        self.assertIn("LIFETIME ESCAPED TO A VOLATILE", text)
        self.assertIn("VOLATILE ROLE PROMOTED IN OURS", text)

    def test_a_clean_function_reports_no_lifetime_finding(self):
        text = savedregs.format_table("u", "f", self.TARGET, self.TARGET)
        for headline in ("LIFETIME PERMUTATION",
                         "LIFETIME ESCAPED TO A VOLATILE",
                         "VOLATILE ROLE PROMOTED IN OURS",
                         "UNPAIRED LIVE RANGE"):
            self.assertNotIn(headline, text)

    def test_unequal_instruction_counts_still_pair(self):
        """Offset pairing is only defined when the counts match; the
        alignment this uses degrades to one-sided rows instead of to a
        silently wrong pairing."""
        target = [[0x0, "mr r31,r3"], [0x4, "nop"], [0x8, "li r30,0"]]
        ours = [[0x0, "mr r31,r3"], [0x4, "li r30,0"]]
        self.assertEqual(["in place", "in place"],
                         self.verdicts(target, ours))


if __name__ == "__main__":
    unittest.main()
