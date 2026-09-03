import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from fndiff import (classify_function, cluster_flags,
                    compiler_private_aliases_from_symbols, count_real,
                    immediate_deltas, immediate_row_reliability,
                    real_reconciliation,
                    relocated_instructions, reloc_naming_only, shiftable_gap,
                    unit_key)


class UnitKeyTests(unittest.TestCase):
    """Run-43 item 8: one spelling rule for both tool families.

    Core tools took `game/x/y`, `game/x/y.c` and `src/game/x/y.c`; 16 of the
    18 composed_census tools that take a unit built
    `build/GUNE5D/obj/{unit}.o` from raw argv, so the `.c` form produced
    `...y.c.o` and a MISSING OBJECT — which reads as "this function is not
    in the census" rather than as a spelling.
    """

    def test_every_spelling_reaches_one_key(self):
        for spelling in ("game/mb/mb_camera", "game/mb/mb_camera.c",
                         "src/game/mb/mb_camera.c", "src\\game\\mb\\mb_camera.c",
                         "  game/mb/mb_camera.c  ", "/game/mb/mb_camera"):
            self.assertEqual(unit_key(spelling), "game/mb/mb_camera", spelling)

    def test_a_cpp_unit_keeps_its_stem(self):
        self.assertEqual(unit_key("src/game/movie/movieplayer.cpp"),
                         "game/movie/movieplayer")

    def test_an_inner_dot_c_is_not_stripped(self):
        """Only a TRAILING extension goes. `.replace('.c', '')` — the form
        one census tool used — would eat any inner occurrence."""
        self.assertEqual(unit_key("game/x/a.c.helper"), "game/x/a.c.helper")

    def test_the_key_is_idempotent(self):
        once = unit_key("src/game/ui/select.c")
        self.assertEqual(unit_key(once), once)


class ImmediateDeltaTests(unittest.TestCase):
    """run-31 item 4a: --ops reduces each instruction to its mnemonic, so a
    pair that agrees on the opcode and disagrees on a LITERAL is `equal` to
    the matcher and never reaches the cluster list. 165 functions carry one
    image-wide; 38 sat under "multiset IDENTICAL -- pure reorder,
    schedule-class residual" (census 2026-09-01)."""

    def test_same_opcode_differing_immediate_is_reported(self):
        t = ["li      r5,0", "addi    r3,r3,32", "blr"]
        b = ["li      r5,0", "addi    r3,r3,36", "blr"]
        got = immediate_deltas(t, b)
        self.assertEqual([(d[0], d[1], d[2]) for d in got],
                         [(1, 1, "immediate")])

    def test_identical_streams_report_nothing(self):
        t = ["li      r5,0", "addi    r3,r3,32", "blr"]
        self.assertEqual(immediate_deltas(t, list(t)), [])

    def test_register_only_difference_is_not_an_immediate_delta(self):
        t = ["addi    r3,r4,32"]
        b = ["addi    r5,r6,32"]
        self.assertEqual(immediate_deltas(t, b), [])

    def test_a_branch_target_delta_is_classified_separately(self):
        t = ["cmpw    r3,r5", "bne <fn+0x10>", "blr"]
        b = ["cmpw    r3,r5", "bne <fn+0x0>", "blr"]
        got = immediate_deltas(t, b)
        self.assertEqual([d[2] for d in got], ["branch"])

    def test_a_RELOCATED_immediate_is_never_flagged(self):
        """The linker owns those bits: the target object carries the
        address, ours carries zero. The first cut fired on both halves of
        every lis/addi address pair (G3DReadControlPadStates)."""
        t = ["lis     r3,-32727", "    R_PPC_ADDR16_HA gPadManager",
             "addi    r31,r3,25692", "    R_PPC_ADDR16_LO gPadManager"]
        b = ["lis     r3,0", "    R_PPC_ADDR16_HA gPadManager",
             "addi    r31,r3,0", "    R_PPC_ADDR16_LO gPadManager"]
        self.assertEqual(immediate_deltas(t, b), [])

    def test_relocated_instructions_flags_the_carrying_instruction(self):
        lines = ["mflr    r0", "lis     r3,0", "    R_PPC_ADDR16_HA g",
                 "blr"]
        ins, rel = relocated_instructions(lines)
        self.assertEqual(ins, ["mflr    r0", "lis     r3,0", "blr"])
        self.assertEqual(rel, [False, True, False])

    def test_deltas_inside_an_unequal_run_are_not_double_reported(self):
        """Clusters already cover non-equal runs; only EQUAL runs count."""
        t = ["li      r3,1", "add     r3,r3,r4", "blr"]
        b = ["li      r3,2", "blr"]
        self.assertTrue(all(d[2] in ("immediate", "branch")
                            for d in immediate_deltas(t, b)))


class RelocNamingOnlyTests(unittest.TestCase):
    """run-31 item 4b: 60 functions link byte-identical while --clean scored
    them OPERAND_DIFF with N real diff lines, because one side names a
    symbol the other emits as an anonymous local pool entry."""

    # The live DVDCheckDisk pair: the target names the jumptable, our
    # object emits the same pool entry anonymously as @647.
    T = ["lis     r4,0", "    R_PPC_ADDR16_HA jumptable_8023AAF8",
         "slwi    r0,r0,2"]
    B = ["lis     r4,0", "    R_PPC_ADDR16_HA @647", "slwi    r0,r0,2"]

    def test_anonymous_local_versus_named_symbol_is_naming_only(self):
        self.assertTrue(reloc_naming_only(self.T, self.B))

    def test_it_is_symmetric(self):
        self.assertTrue(reloc_naming_only(self.B, self.T))

    def test_two_disagreeing_CONCRETE_symbols_are_a_REAL_reloc_defect(self):
        t = ["lis     r4,0", "    R_PPC_ADDR16_HA jumptable_8023AAF8"]
        b = ["lis     r4,0", "    R_PPC_ADDR16_HA jumptable_80111111"]
        self.assertFalse(reloc_naming_only(t, b))

    def test_two_anonymous_locals_that_differ_are_not_absorbed(self):
        t = ["lis     r4,0", "    R_PPC_ADDR16_HA @1+0x4"]
        b = ["lis     r4,0", "    R_PPC_ADDR16_HA @2"]
        self.assertFalse(reloc_naming_only(t, b))

    def test_a_differing_reloc_TYPE_is_never_naming_only(self):
        t = ["lis     r4,0", "    R_PPC_ADDR16_HA @647"]
        b = ["lis     r4,0", "    R_PPC_ADDR16_LO jumptable_8023AAF8"]
        self.assertFalse(reloc_naming_only(t, b))

    def test_differing_instruction_words_are_never_naming_only(self):
        t = ["lis     r4,0", "    R_PPC_ADDR16_HA jumptable_8023AAF8", "blr"]
        b = ["lis     r5,0", "    R_PPC_ADDR16_HA @647", "blr"]
        self.assertFalse(reloc_naming_only(t, b))

    def test_a_missing_relocation_is_never_naming_only(self):
        t = ["lis     r4,0", "    R_PPC_ADDR16_HA jumptable_8023AAF8"]
        b = ["lis     r4,0"]
        self.assertFalse(reloc_naming_only(t, b))

    def test_fully_identical_functions_are_not_reported_as_naming(self):
        self.assertFalse(reloc_naming_only(self.T, list(self.T)))


class ShiftableGapTests(unittest.TestCase):
    """--ops cluster offsets are only meaningful when the gap cannot slide.

    A dense repeating block makes an LCS gap's position arbitrary, and a
    lane worked a cluster located that way for a session.
    """

    def test_gap_inside_a_repeating_run_can_slide(self):
        seq = ["stfd", "lfd", "stfd", "lfd", "stfd", "lfd"]
        self.assertTrue(shiftable_gap(seq, 2, 4))

    def test_gap_with_distinct_neighbours_is_pinned(self):
        seq = ["li", "addi", "mr", "blr"]
        self.assertFalse(shiftable_gap(seq, 1, 2))

    def test_empty_gap_is_not_shiftable(self):
        self.assertFalse(shiftable_gap(["a", "b"], 1, 1))


class ClusterFlagTests(unittest.TestCase):
    def test_balanced_cluster_is_a_local_reorder(self):
        to = ["li", "addi", "mr", "blr"]
        bo = ["li", "mr", "addi", "blr"]
        self.assertIn("BALANCED",
                      cluster_flags("replace", to, bo, 1, 3, 1, 3))

    def test_unbalanced_cluster_is_not_flagged_balanced(self):
        to = ["li", "addi", "blr"]
        bo = ["li", "mr", "blr"]
        self.assertNotIn("BALANCED",
                         cluster_flags("replace", to, bo, 1, 2, 1, 2))

    def test_delete_inside_a_repeating_run_is_shiftable(self):
        to = ["lfd", "stfd", "lfd", "stfd", "lfd", "stfd"]
        bo = ["lfd", "stfd", "lfd", "stfd"]
        self.assertIn("SHIFTABLE",
                      cluster_flags("delete", to, bo, 2, 4, 2, 2))


class ClassifyFunctionTests(unittest.TestCase):
    def test_private_data_alias_uses_same_section_and_offset(self):
        symbols = """
00000000 l       .data  00000000 ...data.0
00000000 g     O .data  0000078c sndDbTable
"""
        self.assertEqual(
            compiler_private_aliases_from_symbols(symbols),
            {"...data.0": "sndDbTable"},
        )

    def test_ambiguous_private_data_alias_is_not_assumed(self):
        symbols = """
00000000 l       .data  00000000 ...data.0
00000000 g     O .data  00000004 first
00000000 g     O .data  00000004 second
"""
        self.assertEqual(compiler_private_aliases_from_symbols(symbols), {})

    def test_exact(self):
        lines = ["addi r3,r3,1", "blr"]
        self.assertEqual(classify_function(lines, lines), "EXACT")

    def test_relocation_name_only(self):
        target = ["lis r3,0", "    R_PPC_ADDR16_HA\tlbl", "blr"]
        base = ["lis r3,0", "    R_PPC_ADDR16_HA\t@17", "blr"]
        self.assertEqual(classify_function(target, base), "RELOCATION_ONLY")

    def test_different_local_relocation_addend_is_not_ignored(self):
        target = ["addi r3,r3,0", "    R_PPC_ADDR16_LO\tlbl+4", "blr"]
        base = ["addi r3,r3,0", "    R_PPC_ADDR16_LO\t@17+8", "blr"]
        self.assertEqual(classify_function(target, base), "OPERAND_DIFF")

    def test_different_call_target_is_not_relocation_only(self):
        target = ["bl <reloc>", "    R_PPC_REL24\tmemcpy", "blr"]
        base = ["bl <reloc>", "    R_PPC_REL24\tmemset", "blr"]
        self.assertEqual(classify_function(target, base), "OPERAND_DIFF")

    def test_register_only_keeps_immediates_and_offsets(self):
        target = ["lwz r7,16(r3)", "add r8,r7,r4", "stw r8,20(r3)"]
        base = ["lwz r9,16(r5)", "add r10,r9,r6", "stw r10,20(r5)"]
        self.assertEqual(classify_function(target, base), "REGISTER_ONLY")

    def test_operand_difference_is_not_register_only(self):
        target = ["cmpwi r3,32", "beq <tgt>"]
        base = ["cmpwi r4,16", "beq <tgt>"]
        self.assertEqual(classify_function(target, base), "OPERAND_DIFF")

    def test_branch_destination_difference_is_not_register_only(self):
        target = ["cmpwi r3,0", "beq <fn+0x20>"]
        base = ["cmpwi r4,0", "beq <fn+0x28>"]
        self.assertEqual(classify_function(target, base), "OPERAND_DIFF")

    def test_reordered_operations_are_only_a_candidate(self):
        target = ["lwz r3,0(r4)", "addi r5,r5,1", "stw r3,0(r6)"]
        base = ["addi r7,r7,1", "lwz r8,0(r9)", "stw r8,0(r10)"]
        self.assertEqual(classify_function(target, base), "SCHEDULE_CANDIDATE")

    def test_added_instruction_is_structural(self):
        target = ["li r3,0", "blr"]
        base = ["li r3,0", "stw r3,0(r4)", "blr"]
        self.assertEqual(classify_function(target, base), "STRUCTURAL")


class TwoRealsTests(unittest.TestCase):
    """Run-32 item 5: `real` names two different numbers.

    --count's `real` drops every reloc line from the raw diff rows;
    --clean's `real` counts rows over reloc-NORMALIZED text. Measured on
    game/pb/pb_objregs::sDrawGeom before implementing:
        --count  DIFF sDrawGeom  insns 1013/1020  lines 1183  real 1177
        --clean  == sDrawGeom: STRUCTURAL, 1189 real diff lines
    Twelve apart, and neither line said the other number existed. The old
    "(+N pool-name lines suppressed)" note could not bridge it: computed as
    raw - real and clamped at zero, it printed NOTHING in exactly this
    case, where the filtered count is the larger of the two.
    """

    ROWS = ["-li      r3,0", "+li      r3,1",
            "-\t\t\t4: R_PPC_EMB_SDA21\tlbl_80345B30",
            "+\t\t\t4: R_PPC_EMB_SDA21\t@524"]

    def test_count_real_drops_every_reloc_row(self):
        self.assertEqual(count_real(self.ROWS), 2)

    # The measured sDrawGeom shape: 1183 raw rows, 6 of them reloc lines,
    # so --count reports 1177 while --clean reports 1189.
    SDRAWGEOM = (["-li      r3,0"] * 1177
                 + ["-\t4: R_PPC_EMB_SDA21\tlbl_80345B30"] * 6)

    def test_the_sDrawGeom_shape_reconciles_all_three_numbers(self):
        self.assertEqual(len(self.SDRAWGEOM), 1183)
        self.assertEqual(count_real(self.SDRAWGEOM), 1177)
        note = real_reconciliation(1189, self.SDRAWGEOM, noise=0)
        self.assertIn("raw rows 1183", note)
        self.assertIn("1177 non-reloc", note)
        self.assertIn("--count", note)

    def test_a_larger_filtered_count_is_no_longer_silent(self):
        """The exact case the clamped noise figure hid completely."""
        self.assertNotEqual(real_reconciliation(1189, self.SDRAWGEOM, 0), "")

    def test_agreeing_numbers_keep_the_old_pool_name_note(self):
        rows = ["-a", "+b"] + ["-\t4: R_PPC_EMB_SDA21\t@1"] * 14
        self.assertEqual(real_reconciliation(2, rows, noise=14),
                         " (+14 pool-name lines suppressed)")

    def test_agreeing_numbers_with_no_noise_print_nothing(self):
        self.assertEqual(real_reconciliation(2, ["-a", "+b"], noise=0), "")

    def test_the_reconciliation_outranks_the_noise_note(self):
        """When both could apply, the ambiguity is what needs saying."""
        rows = ["-a", "+b", "-\t4: R_PPC_REL24\tfoo"]
        note = real_reconciliation(5, rows, noise=1)
        self.assertIn("non-reloc", note)
        self.assertNotIn("suppressed", note)


class ImmediateRowReliabilityTests(unittest.TestCase):
    """run-39 item 12. immediate_deltas pairs positionally INSIDE the
    matcher's equal runs — sound in the middle of a long run, a guess at its
    edges, where the matcher CHOSE the boundary. A row printed there reads
    exactly like a wrong-constant bug and need not be one; UD nearly
    recorded one.

    ANNOTATES, never suppresses: hiding the row would trade a false positive
    for a false negative on the one word class that decides postprocessor
    eligibility. Censused over 12 real TUs at 00df6545d: 254 of 1041
    IMMEDIATE rows marked (24.4%).
    """

    def rel(self, t, b):
        return immediate_row_reliability(t, b)

    def test_a_row_beside_an_unpaired_block_is_marked(self):
        t = ["li      r5,0", "add     r3,r3,r4", "lwz     r3,36(r3)", "blr"]
        b = ["li      r5,0", "lwz     r3,16(r3)", "blr"]
        marked = self.rel(t, b)
        self.assertTrue(marked, "the lwz pair straddles the replace")
        self.assertIn("unpaired block", " ".join(marked.values()))

    def test_a_row_deep_inside_a_long_equal_run_is_not_marked(self):
        """Measured on game/game/player::SetPlayerWindows: T[84]/O[83] sits 5
        rows into a long run and its 36-vs-16 delta is the same +20 .bss
        shift as every other row — a genuine pairing the guard must not
        cast doubt on."""
        pad = [f"or      r{n},r{n},r{n}" for n in range(10, 20)]
        t = ["add     r3,r3,r4"] + pad + ["lwz     r3,36(r3)"] + pad + ["blr"]
        b = ["lwzx    r3,r3,r4"] + pad + ["lwz     r3,16(r3)"] + pad + ["blr"]
        self.assertNotIn(len(pad) + 1, self.rel(t, b))

    def test_a_fully_aligned_pair_of_streams_marks_nothing(self):
        t = ["li      r5,0", "addi    r3,r3,32", "blr"]
        b = ["li      r5,0", "addi    r3,r3,36", "blr"]
        self.assertEqual({}, self.rel(t, b))

    def test_the_function_edges_are_not_unpaired_blocks(self):
        """A run bounded by the function's own start/end abuts nothing the
        matcher had to choose, so its first and last rows are trustworthy."""
        t = ["addi    r3,r3,32", "li      r5,0", "blr"]
        b = ["addi    r3,r3,36", "li      r5,0", "blr"]
        self.assertEqual({}, self.rel(t, b))

    def test_stream_drift_is_reported_as_context_on_a_marked_row(self):
        t = ["add     r3,r3,r4", "lwz     r3,36(r3)", "blr"]
        b = ["lwzx    r3,r3,r4", "nop", "lwz     r3,16(r3)", "blr"]
        marked = self.rel(t, b)
        self.assertTrue(any("drifted by" in why for why in marked.values()),
                        marked)

    def test_drift_alone_does_not_mark_a_well_aligned_row(self):
        """One deletion upstream shifts every later index, but a long equal
        run after it is still correctly aligned — drift is context, adjacency
        is the signal."""
        pad = [f"or      r{n},r{n},r{n}" for n in range(10, 20)]
        t = ["nop"] + pad + ["addi    r3,r3,32"] + pad + ["blr"]
        b = pad + ["addi    r3,r3,36"] + pad + ["blr"]
        self.assertNotIn(len(pad) + 1, self.rel(t, b))

    def test_relocated_instructions_are_indexed_the_same_way_as_deltas(self):
        """The keys must line up with immediate_deltas' t_index or the
        annotation lands on the wrong row."""
        t = ["add     r3,r3,r4", "lwz     r3,36(r3)", "blr"]
        b = ["lwzx    r3,r3,r4", "lwz     r3,16(r3)", "blr"]
        deltas = immediate_deltas(t, b)
        marked = self.rel(t, b)
        self.assertTrue(deltas)
        self.assertTrue(any(d[0] in marked for d in deltas), (deltas, marked))


if __name__ == "__main__":
    unittest.main()
