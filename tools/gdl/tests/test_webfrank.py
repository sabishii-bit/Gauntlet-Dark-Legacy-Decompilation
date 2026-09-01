import unittest

from tools.gdl.webfrank import (
    _parse_int,
    _relocation_sha256,
    _sha256,
    check_permutation_dependences,
    copy_register_fields,
    decode_copy_form,
    equivalent_copy_form,
    instruction_operands,
    permutation_windows,
    permute_instruction_atoms,
    recolor_instruction,
    register_slot_mask,
    verify_consistent_recolor,
    verify_relocation_binding,
)


class RecolorInstructionTests(unittest.TestCase):
    def test_d_form_load_recolors_target_register(self):
        # lhz r7, 52(r4) -> lhz r8, 52(r4)
        self.assertEqual(recolor_instruction(0xA0E40034, {7: 8}), 0xA1040034)

    def test_x_form_recolors_overlapping_register_web(self):
        # subf r7, r8, r7 -> subf r8, r9, r8
        self.assertEqual(
            recolor_instruction(0x7CE83850, {7: 8, 8: 9}),
            0x7D094050,
        )

    def test_unsupported_instruction_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "unsupported instruction"):
            recolor_instruction(0x4E800020, {7: 8})  # blr

    def test_numeric_config_values_accept_hex(self):
        self.assertEqual(_parse_int("0x1f"), 31)

    def test_register_field_copy_changes_no_other_bits(self):
        current = bytes.fromhex("a0e40034 7ce83850")
        target = bytes.fromhex("a1040034 7d094050")
        output, changed = copy_register_fields(current, target)
        self.assertEqual(output, target)
        self.assertEqual(changed, 2)

    def test_register_field_copy_rejects_opcode_changes(self):
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(bytes.fromhex("60000000"), bytes.fromhex("4e800020"))


class InstructionPermutationTests(unittest.TestCase):
    def setUp(self):
        # li r3,1; stw r3,0(r1); li r4,2
        self.current = bytes.fromhex("38600001 90610000 38800002")
        self.order = [2, 0, 1]
        self.expected = bytes.fromhex("38800002 38600001 90610000")
        # A relocation at +2 in source atom 0 must remain at +2 in its
        # destination atom 1.
        self.relocations = [(2, 0x1234, -8)]
        self.expected_relocations = [(6, 0x1234, -8)]

    def permute(self, **overrides):
        arguments = {
            "before_sha256": _sha256(self.current),
            "after_sha256": _sha256(self.expected),
            "before_relocations_sha256": _relocation_sha256(self.relocations),
            "after_relocations_sha256": _relocation_sha256(
                self.expected_relocations
            ),
        }
        arguments.update(overrides)
        return permute_instruction_atoms(
            self.current, self.order, self.relocations, **arguments
        )

    def test_success_moves_instruction_and_relocation_atom(self):
        output, relocations, moved = self.permute()
        self.assertEqual(output, self.expected)
        self.assertEqual(relocations, self.expected_relocations)
        self.assertEqual(moved, 3)

    def test_sys_poll_reset_button_moves_lis_relocation_with_atom(self):
        # sysPollResetButton +0x88: lis r3,@ha; mtctr r0 -> mtctr r0; lis r3,@ha.
        # The two atoms are independent and the lis relocation stays at byte +2
        # within its atom as that atom moves from region slot 0 to slot 1.
        current = bytes.fromhex("3c600000 7c0903a6")
        expected = bytes.fromhex("7c0903a6 3c600000")
        relocations = [(2, 0x1806, 0)]
        expected_relocations = [(6, 0x1806, 0)]
        output, moved_relocations, moved = permute_instruction_atoms(
            current,
            [1, 0],
            relocations,
            before_sha256="decb90402973a79a24378eaba97967a34562786645a9ca1d12a717e8cc276c91",
            after_sha256="9c86562d75c12cda2e5bad4e2aed2736865af0f90fbee9c32fe53488eb91c1eb",
            before_relocations_sha256="ba25df53f1ea2f7a904fada0025dcaffbfc0ebc725843925b1723c355bbd92e8",
            after_relocations_sha256="9f7d58b16bc53a6fb78303b594bea2e12a1a54d7458b83e9668c2e97482f8be0",
        )
        self.assertEqual(output, expected)
        self.assertEqual(moved_relocations, expected_relocations)
        self.assertEqual(moved, 2)

    def test_stale_input_hash_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "input hash changed"):
            self.permute(before_sha256="0" * 64)

    def test_invalid_permutation_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "not a bijection"):
            permute_instruction_atoms(
                self.current,
                [0, 0, 2],
                self.relocations,
                before_sha256=_sha256(self.current),
                after_sha256=_sha256(self.current),
                before_relocations_sha256=_relocation_sha256(self.relocations),
                after_relocations_sha256=_relocation_sha256(self.relocations),
            )

    def test_moved_control_instruction_fails_closed(self):
        current = bytes.fromhex("4e800020 38600001")  # blr; li r3,1
        with self.assertRaisesRegex(ValueError, "control op"):
            permute_instruction_atoms(
                current,
                [1, 0],
                [],
                before_sha256=_sha256(current),
                after_sha256=_sha256(current[4:] + current[:4]),
                before_relocations_sha256=_relocation_sha256([]),
                after_relocations_sha256=_relocation_sha256([]),
            )

    def test_relocation_preservation_hash_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "relocation output hash changed"):
            self.permute(after_relocations_sha256="0" * 64)


class FormAwareMaskTests(unittest.TestCase):
    def test_d_form_displacement_bits_are_not_register_fields(self):
        # lwz r3, 0x40(r4) vs lwz r3, 0x80(r4): the difference sits in bits
        # the old blanket mask treated as a register slot.
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(
                bytes.fromhex("80640040"), bytes.fromhex("80640080")
            )

    def test_rlwinm_mask_bounds_are_not_register_fields(self):
        # rlwinm r0,r3,0,24,31 vs rlwinm r0,r3,0,16,31 (andi 0xFF vs 0xFFFF).
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(
                bytes.fromhex("5460063e"), bytes.fromhex("5460043e")
            )

    def test_cmpwi_cr_field_is_not_a_register_field(self):
        # cmpwi cr0,r3,4 vs cmpwi cr1,r3,4.
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(
                bytes.fromhex("2c030004"), bytes.fromhex("2c830004")
            )

    def test_xo_bits_are_not_register_fields(self):
        # subf r3,r4,r0 vs neg r3,r4 differ only at an XO bit inside the old
        # blanket mask.
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(
                bytes.fromhex("7c640050"), bytes.fromhex("7c6400d0")
            )

    def test_unmodelled_form_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "unsupported instruction"):
            register_slot_mask(0x44000002)  # sc

    def test_operand_model_matches_known_forms(self):
        # lwz: RT define, RA base use; fmuls: FRT define, FRA/FRC uses.
        self.assertEqual(
            instruction_operands(0x80640040),
            (("g", 21, "d", False), ("g", 16, "u", True)),
        )
        self.assertEqual(
            instruction_operands(0xEC0100B2),
            (("f", 21, "d", False), ("f", 16, "u", False), ("f", 6, "u", False)),
        )


class ConsistentRecolorTests(unittest.TestCase):
    def test_pure_recolor_passes(self):
        # addi r4,r3,1; add r5,r4,r3; mr r3,r5; blr  vs the same body with
        # the scratch web homed in r0.
        current = bytes.fromhex("38830001 7ca41a14 7ca32b78 4e800020")
        target = bytes.fromhex("38030001 7ca01a14 7ca32b78 4e800020")
        verify_consistent_recolor(current, target)

    def test_swapped_independent_loads_fail(self):
        # The PointLineDist2D shape: two loads from fixed ABI bases exchange
        # positions; every differing bit is a register field, yet it is not a
        # recolor.
        current = bytes.fromhex("80a30000 80c40000 7ce53214 4e800020")
        target = bytes.fromhex("80a40000 80c30000 7ce62a14 4e800020")
        with self.assertRaisesRegex(ValueError, "does not correspond"):
            verify_consistent_recolor(current, target)

    def test_commuted_multiply_operands_pass(self):
        # fmuls f0,f1,f2 vs fmuls f0,f2,f1; blr.
        current = bytes.fromhex("ec0100b2 4e800020")
        target = bytes.fromhex("ec020072 4e800020")
        verify_consistent_recolor(current, target)

    def test_recolor_across_conditional_branch_passes(self):
        # cmpwi r3,0; beq +8; addi r4,r3,1; blr  with r4 recolored to r0.
        current = bytes.fromhex("2c030000 41820008 38830001 4e800020")
        target = bytes.fromhex("2c030000 41820008 38030001 4e800020")
        verify_consistent_recolor(current, target)


class PermutationDependenceTests(unittest.TestCase):
    def test_read_after_write_pair_fails(self):
        # li r3,1; mr r4,r3 cannot swap.
        region = bytes.fromhex("38600001 7c641b78")
        with self.assertRaisesRegex(ValueError, "def-use chains"):
            check_permutation_dependences(region, [1, 0])

    def test_store_load_reorder_fails(self):
        # stw r3,0(r4); lwz r5,0(r6) may alias.
        region = bytes.fromhex("90640000 80a60000")
        with self.assertRaisesRegex(ValueError, "def-use chains"):
            check_permutation_dependences(region, [1, 0])

    def test_load_load_reorder_passes(self):
        region = bytes.fromhex("80650000 80860000")
        check_permutation_dependences(region, [1, 0])

    def test_disjoint_stack_stores_reorder_passes(self):
        # stw r3,8(r1); stw r4,0xC(r1): distinct r1 slots are distinct
        # locations.
        region = bytes.fromhex("90610008 9081000c")
        check_permutation_dependences(region, [1, 0])

    def test_moved_final_write_needs_exit_liveness_proof(self):
        # li r0,1; li r0,2: swapping changes which write survives the region.
        region = bytes.fromhex("38000001 38000002")
        with self.assertRaisesRegex(ValueError, "final write"):
            check_permutation_dependences(region, [1, 0])
        check_permutation_dependences(region, [1, 0], lambda resource: True)


class ShippedRuleMechanismTests(unittest.TestCase):
    """One case per rule in config/GUNE5D/webfrank.json authored 2026-08-31.

    Each uses the real instruction words taken from the raw compiler output and
    the extracted retail object, so a guard regression that would silently
    accept or reject a shipped rule fails here first.
    """

    def test_camera_init_for_gamemode_exchanges_independent_loads(self):
        # +0xf8: lwz r0,832(r7); lwz r3,844(r7) -> exchanged.
        # attempt.parked.camera_init_for_gamemode.20260829.v1
        region = bytes.fromhex("80070340 8067034c")
        check_permutation_dependences(region, [1, 0])

    def test_msg_post_entry_exchange_is_dependence_free(self):
        # +0x30: addi r25,r25,64; addi r28,r3,0(@gMsgBoxes) -> exchanged.
        # attempt.parked.msgpost.20260831.v2
        region = bytes.fromhex("3b390040 3b830000")
        check_permutation_dependences(region, [1, 0])

    def test_msg_draw_argument_rotation_sinks_the_addi(self):
        # +0x1c8: addi r3,r3,20; li r5,-1; li r7,24; li r8,0 -> addi last.
        # attempt.parked.msgdraw.20260831.v2 stage 1
        region = bytes.fromhex("38630014 38a0ffff 38e00018 39000000")
        check_permutation_dependences(region, [1, 2, 3, 0])

    def test_msg_draw_fpr_gpr_load_exchange(self):
        # +0x3b8: lfs f1,0(0)(SDA21); lwz r3,16(r31) -> exchanged.
        # attempt.parked.msgdraw.20260831.v2 stage 2
        region = bytes.fromhex("c0200000 807f0010")
        check_permutation_dependences(region, [1, 0])

    def test_msg_draw_line_height_web_is_a_consistent_recolor(self):
        # The r25 -> r29 lineHeight web: addi/srawi/subf, then blr.
        current = bytes.fromhex("3b230002 7f390e70 7c99f050 4e800020")
        target = bytes.fromhex("3ba30002 7fbd0e70 7c9df050 4e800020")
        verify_consistent_recolor(current, target)

    def test_line_line_dist_entry_load_transposition(self):
        # +0x14: lfs f28,0(r4)(dirB->x); lfs f11,8(r7)(dirA->z) -> exchanged.
        # attempt.linelinedist-entry-load-permutation.20260831.v1 stage 1
        region = bytes.fromhex("c3840000 c1670008")
        check_permutation_dependences(region, [1, 0])

    def test_line_line_dist_paired_load_and_multiply_transposition(self):
        # +0x128: the two dirB loads 4(r4)/8(r4) AND the two fmuls consuming
        # them exchange across an independent lfs 0(r4).  The masked byte scan
        # sees only the loads; this ordering is what the bisimulation forced.
        # attempt.linelinedist-entry-load-permutation.20260831.v1 stage 2
        region = bytes.fromhex(
            "c3640004 c3840008 eda606f2 c3a40000 ed870732"
        )
        check_permutation_dependences(region, [1, 0, 4, 3, 2])

    def test_critter_do_texmod_node_index_web_crossing(self):
        # +0x3e4: the texmod-index lookup, verbatim from both objects.  The
        # sign-extended index web (lha/extsh./mulli) and the list-base web
        # (addi r,r4,8) cross r6<->r7 across an untouched li/bc pair.
        # claim.law.webfrank-cfg-entry-pseudotarget-false-negative
        current = bytes.fromhex(
            "a8fe0046 38c40008 7ce00735 7c86182e"
            "38a00000 4180000c 1c070050 7ca6002e"
        )
        target = bytes.fromhex(
            "a8de0046 38e40008 7cc00735 7c87182e"
            "38a00000 4180000c 1c060050 7ca7002e"
        )
        verify_consistent_recolor(current, target)

    def test_critter_do_texmod_node_scale_fpr_crossing(self):
        # +0x35c: the UV scale computation, verbatim from both objects.  The
        # node-scale and animated-delta FPR webs cross f0<->f1 through
        # fmuls/fmadds while the intervening integer word is untouched.
        current = bytes.fromhex(
            "c02400bc c01e0008 7c60ea14 ec3e0072 ec4207f2 efc007f2 d02300ac"
        )
        target = bytes.fromhex(
            "c00400bc c03e0008 7c60ea14 ec1e0032 ec4207f2 efc107f2 d00300ac"
        )
        verify_consistent_recolor(current, target)

    def test_fn_80011bbc_match_index_counter_recolor(self):
        # The r22 -> r24 match-index counter web: li, addi, cmpw, then blr.
        # The three real sites are +0xa4/+0xd4/+0xe0; they are gathered here
        # because the intervening code branches over the excerpt boundary.
        # attempt.fn80011bbc-matchindex-coalesce
        current = bytes.fromhex("3ac00000 3ad60001 7c160000 4e800020")
        target = bytes.fromhex("3b000000 3b180001 7c180000 4e800020")
        verify_consistent_recolor(current, target)

    def test_start_throw_magic_fx_sinks_the_type_mask_clrlwi(self):
        # +0x64: clrlwi r28,r25,28 (type & 0xF) sinks three atoms past
        # rlwinm r0,r25,2,26,29; add r3,r30,r0; lwz r23,3504(r3).
        # It defines r28 and reads only r25: no intervening atom writes r25
        # or touches r28, and it accesses no memory, so the rotation is
        # dependence-free WITHOUT an exit_dead escape.
        # attempt.RQ_startthrowmagicfx-permute-recolor-closure.20260901.v1
        region = bytes.fromhex("573c073e 572016ba 7c7e0214 82e30db0")
        check_permutation_dependences(region, [1, 2, 3, 0])

    def test_start_throw_magic_fx_effect_base_web_recolor(self):
        # The r3<->r4 exchange over +0x1a0..+0x1bc, gathered past the
        # non-differing words between the sites: the lwz defines our r3 where
        # retail defines r4, then the add retires that correspondence in the
        # opposite direction and the three stores check against the new
        # binding.  Same rule record as above.
        current = bytes.fromhex(
            "80630d9c 7c9f0214 7c600734 b0040c5e 93640c6c 4e800020"
        )
        target = bytes.fromhex(
            "80830d9c 7c7f0214 7c800734 b0030c5e 93630c6c 4e800020"
        )
        verify_consistent_recolor(current, target)

    def test_home_copy_coalescing_class_is_not_recolor_eligible(self):
        # claim.law.callee-saved-home-copy-coalescing-is-source-unreachable
        # names three gauntworld functions whose residual is a target-only
        # `mr rHome,rScratch`.  That is an EXTRA instruction, so the target is
        # strictly larger and BOTH webfrank paths must refuse the shape: the
        # recolor path needs equal sizes, and the permutation path needs a
        # bijection over the atoms that already exist.
        current = bytes.fromhex("83e400e0 2c1f0019")
        target = bytes.fromhex("801400e0 2c000019 7c1f0378")
        with self.assertRaisesRegex(ValueError, "equal word-aligned sizes"):
            verify_consistent_recolor(current, target)
        with self.assertRaisesRegex(ValueError, "not a bijection"):
            permute_instruction_atoms(
                current, [0, 1, 2], [],
                before_sha256=_sha256(current),
                after_sha256=_sha256(target),
                before_relocations_sha256=_relocation_sha256([]),
                after_relocations_sha256=_relocation_sha256([]),
            )

    def test_line_line_dist_fpr_renaming_is_consistent(self):
        # After the entry transposition: lfs f11,8(r7); lfs f28,0(r4);
        # fmuls f0,f28,f11 recolours to f12/f9 with the product following.
        current = bytes.fromhex("c1670008 c3840000 ec1c02f2 4e800020")
        target = bytes.fromhex("c1870008 c1240000 ec090332 4e800020")
        verify_consistent_recolor(current, target)

    def test_exp_to_level_three_cycle_recolor(self):
        # li r6,2970 / addi r5,r7,-1 / addi r0,r6,1000 / mullw r5,r5,r0 under
        # the r4->r6->r5->r4 cycle.
        # attempt.exptolevel-register-cycle-rescreen.20260831.v2
        current = bytes.fromhex("38c00b9a 38a7ffff 380603e8 7ca501d6 4e800020")
        target = bytes.fromhex("38800b9a 38c7ffff 380403e8 7cc601d6 4e800020")
        verify_consistent_recolor(current, target)

    def test_critter_line_collide_scaled_base_temp_web(self):
        # +0x80: the anonymous scaled-base temp for the pool element address
        # is r4 in ours and r5 in retail, carrying through the element load
        # and the mr into the walk register.
        # attempt.webfrank-closure2.critterlinecollide.20260831.v1
        current = bytes.fromhex("7c9f1a14 80040008 3bc40000 4e800020")
        target = bytes.fromhex("7cbf1a14 80050008 3bc50000 4e800020")
        verify_consistent_recolor(current, target)

    def test_critter_line_collide_sda_count_reread_web(self):
        # +0xec: the loop-count web re-read from the SDA21 global is r5 in
        # ours and r4 in retail — the other half of the symmetric crossing.
        # The RA=0 SDA21 base must stay absent on both sides.
        current = bytes.fromhex("80a00000 2c050000 4e800020")
        target = bytes.fromhex("80800000 2c040000 4e800020")
        verify_consistent_recolor(current, target)

    def test_load_player_geo_recycled_register_recolor(self):
        # The pad/cls crossing, including the mulli that RECYCLES r27 after
        # cls dies -- the define-kill step is what makes this provable.
        # attempt.loadplayergeo-web-crossing-rescreen.20260831.v3
        current = bytes.fromhex(
            "835f0004 837f000c 5740103a 5779103a 1f78004c 7c7eda14 4e800020"
        )
        target = bytes.fromhex(
            "837f0004 835f000c 5760103a 5759103a 1f58004c 7c7ed214 4e800020"
        )
        verify_consistent_recolor(current, target)

    def test_do_heal_players_fpr_crossing_recolor(self):
        # fmul into f31 vs the volatile f0, the self-coalesced frsp, and the
        # f30/f31 swap of giveq against the loop-hoisted zero constant.
        # attempt.do-heal-players-fpr-crossing-rescreen.20260831.v2
        # The excerpt stops at the fcmpo on purpose: the fmul's define maps
        # f31 onto f0 and so retires f0's identity entry, which the real
        # function re-establishes before its later fadds but a shorter slice
        # would not -- the full-function proof runs in the build itself.
        current = bytes.fromhex("ffe00072 ffe0f818 c3c00000 fc1ff040 4e800020")
        target = bytes.fromhex("fc000072 ffc00018 c3e00000 fc1ef840 4e800020")
        verify_consistent_recolor(current, target)

    def test_cam_orient_to_temporary_destination_recolor(self):
        # +0x1c: add r4,r5,r4 / addi r31,r4,200 vs the target's
        # add r31,r5,r4 / addi r31,r31,200 -- the one-instruction temporary
        # is coloured into cam's saved home instead of the dying operand.
        # attempt.camorient-add-destination-recolor-park.20260831.v1
        current = bytes.fromhex("7c852214 3be400c8 4e800020")
        target = bytes.fromhex("7fe52214 3bff00c8 4e800020")
        verify_consistent_recolor(current, target)

    def test_init_game_cam_address_materialization_rotation(self):
        # +0x8c: addi r7,r4,@lo / addi r6,r3,255 / addi r8,r5,@lo rotates so
        # the gPlayers address leads.  Distinct bases and distinct
        # destinations, so nothing in the region depends on the order.
        # attempt.initgamecam-u64-buttons-dance.20260830.v1
        region = bytes.fromhex("38e40000 38c300ff 39050000")
        check_permutation_dependences(region, [2, 0, 1])

    def test_msg_post_desc_offset_web_is_a_consistent_recolor(self):
        # The r29 -> r26 descOffset web: mulli, add, lwzx, then blr.
        current = bytes.fromhex("1fbe001c 7f3bea14 7c04e82e 4e800020")
        target = bytes.fromhex("1f5e001c 7f3bd214 7c04d02e 4e800020")
        verify_consistent_recolor(current, target)

    def test_pb_diag_draw_texture_saved_pair_transposition(self):
        # +0x20: li r28,0 / li r30,0 / lwz r0,SDA / lwz r29,SDA.  The two
        # callee-saved list webs are established into each other's home
        # (r28 <-> r29); every later use in the function follows this binding.
        # attempt.pbdiagdrawtexture-saved-register-set-closure.20260831.v1
        current = bytes.fromhex("3b800000 3bc00000 80000000 83a00000 4e800020")
        target = bytes.fromhex("3ba00000 3bc00000 80000000 83800000 4e800020")
        verify_consistent_recolor(current, target)

    def test_pb_diag_draw_texture_dead_scratch_compare_row(self):
        # +0x558: lwz r0,SDA / cmplwi r0,0.  Ours homes the loaded flag in the
        # volatile r0 and compares it there; retail uses r3.  The value dies at
        # the compare, so no source spelling reaches the choice.  The other
        # thirteen fields need bindings established earlier in the function --
        # the whole-function proof runs in the build's WEBFRANK step.
        current = bytes.fromhex("80000000 28000000 4e800020")
        target = bytes.fromhex("80600000 28030000 4e800020")
        verify_consistent_recolor(current, target)

    def test_draw_glow_text_scratch_renaming_with_commuted_multiply(self):
        # +0x24: the two glow-global loads exchange homes (r6 <-> r8), the
        # quotient/product temporary moves r9 -> r3, and the mullw additionally
        # exchanges its two commuting factors (mullw r0,r9,r0 vs mullw r0,r0,r3)
        # -- accepted through the bisimulation's commutative-operand pairing,
        # which is what makes the record's "operand order" axis a consequence of
        # the recolor rather than a separate difference.
        # attempt.drawglowtext-scratch-alloc-direction.20260831.v2
        current = bytes.fromhex(
            "80c00000 80000000 54c7083c 81000000 7d203a14 7c084b96 7c0901d6"
            " 7fa04050 4e800020"
        )
        target = bytes.fromhex(
            "81000000 80000000 5507083c 80c00000 7c603a14 7c061b96 7c0019d6"
            " 7fa03050 4e800020"
        )
        verify_consistent_recolor(current, target)

    def test_sumner_do_speech_address_pair_emission_order(self):
        # +0x1f0: lis r4,gPlayers@ha / lfs f31,SDA / lis r3,lbl@ha /
        # addi r29,r4,@lo / addi r30,r3,@lo.  Retail emits the lbl pair first.
        # The intervening lfs is why the region is five atoms and not the
        # adjacent pair the masked byte scan reports.
        # attempt.sumnerdospeech-emission-order-coloring-coupling.20260831.v1
        region = bytes.fromhex("3c800000 c3e00000 3c600000 3ba40000 3bc30000")
        check_permutation_dependences(region, [2, 1, 0, 4, 3])

    def test_sumner_do_speech_record_web_recolor(self):
        # +0x144: mr. r26,r3 / beq / lwz r4,SDA / addi r3,r26,0 under the
        # r26 -> r24 renaming that remains once the emission order is fixed.
        current = bytes.fromhex("7c7a1b79 41820050 80800000 387a0000 4e800020")
        target = bytes.fromhex("7c781b79 41820050 80800000 38780000 4e800020")
        verify_consistent_recolor(current, target)


class RejectedResidualTests(unittest.TestCase):
    """Residuals deliberately NOT shipped: the guards must keep rejecting them.

    Both are legal only in the target's register colouring, while the harness
    audits a permutation on our own colouring, where the def-use chain really
    does break.  Recorded so a later pass does not retry them blindly.
    """

    def test_sys_reset_service_load_may_not_hoist_over_a_global_store(self):
        # stw r0,gSysFlags(SDA21); lwz r5,gPadCur(SDA21).  Both are linker-
        # resolved base-zero accesses, so they may alias as far as the model
        # can prove -- and this project has distinct symbol spellings for one
        # address (sFlags vs gControllerButtons+0x4), so symbol-based
        # disambiguation would be unsound.
        region = bytes.fromhex("90000000 80a00000")
        with self.assertRaisesRegex(ValueError, "def-use chains"):
            check_permutation_dependences(region, [1, 0])

    def test_mb_render_text_spill_may_not_sink_past_a_reused_register(self):
        # lwz r3,8(r31); stw r3,72(r1); slwi r3,r0,2.  Our colouring homes the
        # baseY spill and the t-chain both in r3, so sinking the store past the
        # slwi changes which value it stores.
        region = bytes.fromhex("807f0008 90610048 5403103a")
        with self.assertRaisesRegex(ValueError, "def-use chains"):
            check_permutation_dependences(region, [0, 2, 1])

    def test_btrilinecol_copy_provenance_is_not_a_renaming(self):
        # lfs f2,lbl_80345D50; fmr f26,f2; fmuls f1,f0,f26.  The target keeps
        # the constant in f26 and copies it to f25, while we load into f2 and
        # copy to f26, so the later use reads an equal VALUE from a different
        # register.  Value equality via copies is not a renaming, and proving
        # it would need fmr copy-propagation the bisimulation deliberately
        # does not model.
        current = bytes.fromhex("c0400000 ff401090 ec2006b2 4e800020")
        target = bytes.fromhex("c3400000 ff20d090 ec2006b2 4e800020")
        with self.assertRaisesRegex(ValueError, "does not correspond"):
            verify_consistent_recolor(current, target)

    def test_init_effects_displacement_fold_is_not_a_register_difference(self):
        # sfx.c InitEffects was queued as a clean four-cycle callee-saved
        # permutation, but the current raw compile disagrees with retail at
        # three stores: retail folds the field displacement onto the frame base
        # (stw r0,1068(r31)) where we materialize a dedicated pointer and store
        # at zero (stw r0,0(r23)).  The immediate is a real instruction bit, so
        # this is a source-reachable addressing difference, not an allocator
        # colour -- WebFrank must never copy it.
        current = bytes.fromhex("90170000 4e800020")
        target = bytes.fromhex("901f042c 4e800020")
        with self.assertRaisesRegex(ValueError, "non-register instruction bits"):
            copy_register_fields(current, target)

    def test_ads_put_buffer_region_with_a_call_is_not_permutable(self):
        # adstream.c AdsPutBuffer was queued as a two-web callee-saved
        # transposition.  Its streams diverge from the prologue onward (206 of
        # 220 differing words escape the register mask) and the candidate region
        # spans a bl, which the permutation path rejects outright.
        region = bytes.fromhex("801a0078 48000001 901a0020")
        with self.assertRaisesRegex(ValueError, "contains a control op"):
            permute_instruction_atoms(
                region, [0, 2, 1], [],
                before_sha256=_sha256(region),
                after_sha256="",
                before_relocations_sha256=_relocation_sha256([]),
                after_relocations_sha256="",
            )


class JumptableTargetTests(unittest.TestCase):
    def test_function_entry_pointer_is_not_a_jumptable_slot(self):
        # A data-section ADDR32 reloc resolving to the function's own START
        # is a function pointer; collecting it as offset 0x0 gave every
        # bctr a spurious back-edge to the prologue and failed the
        # dependence audit before the first real difference.
        import struct as _struct
        from tools.gdl.webfrank import Section, _jumptable_targets
        data = bytearray(4096)
        fn_start, fn_end, text_index = 0x100, 0x200, 1
        symtab_off, rela_off = 0x400, 0x600
        # one symbol: value=0 (section base), section index = text
        _struct.pack_into(">I", data, symtab_off + 16 + 4, 0)
        _struct.pack_into(">H", data, symtab_off + 16 + 14, text_index)
        # two ADDR32 relocs against symbol #1: addends fn_start (entry
        # pointer) and fn_start+8 (a genuine jumptable slot)
        _struct.pack_into(">IIi", data, rela_off, 0, (1 << 8) | 1, fn_start)
        _struct.pack_into(">IIi", data, rela_off + 12, 4, (1 << 8) | 1,
                          fn_start + 8)
        sections = [
            Section(0, ".symtab", 2, symtab_off, 32, 0, 0, 16),
            Section(2, ".rela.data", 4, rela_off, 24, 0, 5, 12),
        ]
        targets = _jumptable_targets(data, sections, text_index,
                                     fn_start, fn_end)
        self.assertEqual(targets, {8})


class InstructionCountDeltaIneligibilityTests(unittest.TestCase):
    """Both stages must fail closed when the instruction count differs.

    Locks in claim.law.webfrank-cannot-close-instruction-count-deltas:
    a residual that is one instruction LONG (an ours-only `mr`) or one
    instruction SHORT is not postprocessor work in either direction, no
    matter how register-flavoured it looks.
    """

    def test_register_field_copy_rejects_extra_instruction(self):
        # ours carries an extra `mr r30,r0` the target never emits
        current = bytes.fromhex("3fc08028 3bde2850 7c1e0378")
        target = bytes.fromhex("3fc08028 3bde2850")
        with self.assertRaisesRegex(ValueError, "equal aligned sizes"):
            copy_register_fields(current, target)

    def test_register_field_copy_rejects_missing_instruction(self):
        current = bytes.fromhex("3fc08028 3bde2850")
        target = bytes.fromhex("3fc08028 3bde2850 7c1e0378")
        with self.assertRaisesRegex(ValueError, "equal aligned sizes"):
            copy_register_fields(current, target)

    def test_permutation_order_must_be_a_bijection(self):
        # a permutation can never add or drop an atom
        region = bytes.fromhex("3fc08028 3bde2850")
        with self.assertRaisesRegex(ValueError, "not a bijection"):
            permute_instruction_atoms(
                region, [0, 1, 1], [],
                before_sha256=_sha256(region),
                after_sha256=_sha256(region),
                before_relocations_sha256=_relocation_sha256([]),
                after_relocations_sha256=_relocation_sha256([]),
            )


class StackDisplacementIneligibilityTests(unittest.TestCase):
    """A differing stack displacement is an immediate, never a register.

    move_logic15's residual is six words that differ only in their
    displacement field (a spill at 0x40(r1) where retail uses 0x14(r1)).
    The mask must refuse them so no rule can hide a frame-layout gap.
    """

    def test_differing_stack_displacement_is_rejected(self):
        # stfd f4, 0x40(r1)  vs  stfd f2, 0x14(r1)
        current = bytes.fromhex("d0810040")
        target = bytes.fromhex("d0410014")
        with self.assertRaisesRegex(ValueError, "non-register"):
            copy_register_fields(current, target)

    def test_same_displacement_recolors_cleanly(self):
        # the register half alone IS eligible once the slot agrees
        current = bytes.fromhex("d0810014")
        target = bytes.fromhex("d0410014")
        output, changed = copy_register_fields(current, target)
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)


def _words(*values: int) -> bytes:
    return b"".join(value.to_bytes(4, "big") for value in values)


# MWCC copy/constant encodings used by the copy-form tests.
LI_R31_0 = 0x3BE00000       # li r31,0
LI_R31_5 = 0x3BE00005       # li r31,5
LI_R29_0 = 0x3BA00000       # li r29,0
LI_R23_0 = 0x3AE00000       # li r23,0     (== addi r23,r0,0)
ADDI_R29_R31 = 0x3BBF0000   # addi r29,r31,0
ADDI_R29_R3 = 0x3BA30000    # addi r29,r3,0
ADDI_R23_R6 = 0x3AE60000    # addi r23,r6,0
MR_R23_R6 = 0x7CD73378      # mr r23,r6    (== or r23,r6,r6)
MR_R23_R0 = 0x7C170378      # mr r23,r0    (a genuine copy OF r0)
MR_R29_R3 = 0x7C7D1B78      # mr r29,r3
MR_R30_R31 = 0x7FFEFB78     # mr r30,r31
MR_DOT_R23_R6 = 0x7CD73379  # mr. r23,r6   (Rc set: also writes CR0)
ADD_R23_R6_R6 = 0x7EE63214  # add r23,r6,r6
BLR = 0x4E800020
BL = 0x48000001
NOP = 0x60000000
BNE_PLUS_8 = 0x40800008
LI_R6_0 = 0x38C00000        # li r6,0      (a VOLATILE destination)
BCTRL = 0x4E800421          # indirect call through CTR
BLRL = 0x4E800021           # indirect call through LR
BNE_MINUS_4 = 0x4082FFFC    # bne -4: control, but not a call
ADDI_R5_R6 = 0x38A60000     # addi r5,r6,0 — writes r5, never r31


class DecodeCopyFormTests(unittest.TestCase):
    """The rS != r0 asymmetry is the whole hazard, so pin it directly."""

    def test_mr_is_a_copy(self):
        self.assertEqual(decode_copy_form(MR_R23_R6), ("copy", 23, 6))

    def test_addi_with_nonzero_base_is_a_copy(self):
        self.assertEqual(decode_copy_form(ADDI_R23_R6), ("copy", 23, 6))

    def test_addi_with_zero_base_is_a_constant_load_not_a_copy(self):
        # addi r23,r0,0 reads the zero rA field as the literal zero.
        self.assertEqual(decode_copy_form(LI_R23_0), ("li", 23, 0))

    def test_mr_from_r0_is_a_real_copy_of_r0(self):
        # ...whereas `or` really does read GPR 0, which is why the two
        # encodings diverge exactly at rS == 0.
        self.assertEqual(decode_copy_form(MR_R23_R0), ("copy", 23, 0))

    def test_record_setting_mr_is_not_a_copy(self):
        self.assertIsNone(decode_copy_form(MR_DOT_R23_R6))

    def test_unrelated_opcode_is_not_a_copy(self):
        self.assertIsNone(decode_copy_form(ADD_R23_R6_R6))
        self.assertIsNone(decode_copy_form(BLR))


class EquivalentCopyFormTests(unittest.TestCase):
    def rewrite(self, current, target, edits, **overrides):
        arguments = {
            "relocated_offsets": set(),
            "target_relocated_offsets": set(),
            "jumptable_offsets": set(),
        }
        arguments.update(overrides)
        return equivalent_copy_form(current, target, edits, **arguments)

    # ---- unconditional form pair (no dataflow obligation) ----

    def test_mr_to_addi_copy_rewrites(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        output, changed = self.rewrite(
            current, target, [{"at": 0, "proof": "unconditional"}]
        )
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)

    def test_addi_copy_to_mr_rewrites_the_other_direction(self):
        current = _words(ADDI_R29_R3, BLR)
        target = _words(MR_R29_R3, BLR)
        output, changed = self.rewrite(
            current, target, [{"at": 0, "proof": "unconditional"}]
        )
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)

    def test_copy_from_r0_is_rejected(self):
        # `mr r23,r0` copies GPR 0; `addi r23,r0,0` loads literal 0.  These
        # are NOT equivalent and the rule must refuse the pair.
        current = _words(MR_R23_R0, BLR)
        target = _words(LI_R23_0, BLR)
        with self.assertRaisesRegex(ValueError, "not a register copy"):
            self.rewrite(current, target, [{"at": 0, "proof": "unconditional"}])

    def test_differing_destination_is_rejected_as_a_recolor(self):
        current = _words(LI_R29_0, BLR)
        target = _words(MR_R30_R31, BLR)
        with self.assertRaisesRegex(ValueError, "destination differs"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "dominating_def"}]
            )

    def test_differing_source_is_rejected_as_a_recolor(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(0x3AE30000, BLR)  # addi r23,r3,0
        with self.assertRaisesRegex(ValueError, "source differs"):
            self.rewrite(current, target, [{"at": 0, "proof": "unconditional"}])

    def test_non_copy_opcode_pair_is_rejected(self):
        current = _words(ADD_R23_R6_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "not a copy form"):
            self.rewrite(current, target, [{"at": 0, "proof": "unconditional"}])

    def test_record_setting_move_is_rejected(self):
        current = _words(MR_DOT_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "not a copy form"):
            self.rewrite(current, target, [{"at": 0, "proof": "unconditional"}])

    def test_relocated_word_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "relocated word"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "unconditional"}],
                relocated_offsets={0},
            )

    def test_target_side_relocation_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "relocated word"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "unconditional"}],
                target_relocated_offsets={0},
            )

    def test_sda21_relocation_at_word_plus_two_is_rejected(self):
        """claim.law.HV_emb-sda21-relocation-offset-differs-....20260901.v1.

        MWCC records an EMB_SDA21 entry at the instruction offset PLUS 2,
        so an exact word-offset membership test never fires for it and a
        genuinely relocated word passes the 'not relocated' precondition.
        The screen must test the whole word RANGE [off, off+4).
        """
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "relocated word"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "unconditional"}],
                relocated_offsets={2},
            )

    def test_target_side_sda21_relocation_at_word_plus_two_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "relocated word"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "unconditional"}],
                target_relocated_offsets={2},
            )

    def test_addr16_lo_at_word_plus_two_is_rejected(self):
        """The live instance: pbWinSetup +0x314 is `addi r3,r3,lbl@l`,
        encoded 0x38030000, carrying ADDR16_LO at 790 with the word at
        788.  decode_copy_form reads it as a register COPY r3<-r3, so
        without the range screen the rule would rewrite an address half
        on the false premise that it is an unrelocated copy."""
        current = _words(0x38030000, BLR)   # addi r3,r3,lbl_801284D8@l
        target = _words(0x7C631B78, BLR)    # mr r3,r3
        with self.assertRaisesRegex(ValueError, "relocated word"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "unconditional"}],
                relocated_offsets={2},
            )

    def test_wrong_proof_label_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "requires"):
            self.rewrite(
                current, target, [{"at": 0, "proof": "dominating_def"}]
            )

    def test_duplicate_edit_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        target = _words(ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.rewrite(
                current, target,
                [{"at": 0, "proof": "unconditional"},
                 {"at": 0, "proof": "unconditional"}],
            )

    def test_already_matching_word_is_rejected(self):
        current = _words(MR_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "already matches"):
            self.rewrite(
                current, current, [{"at": 0, "proof": "unconditional"}]
            )

    # ---- li -> copy, which carries the dataflow obligation ----

    def test_dominating_definition_proves_the_constant(self):
        # li r31,0 ; li r29,0   ->   li r31,0 ; addi r29,r31,0
        # This is the measured AudioUnloadPart shape.
        current = _words(LI_R31_0, LI_R29_0, BLR)
        target = _words(LI_R31_0, ADDI_R29_R31, BLR)
        output, changed = self.rewrite(
            current, target, [{"at": 4, "proof": "dominating_def"}]
        )
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)

    def test_constant_mismatch_is_rejected(self):
        # r31 holds 5, so copying it does not reproduce `li r29,0`.
        current = _words(LI_R31_5, LI_R29_0, BLR)
        target = _words(LI_R31_5, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "not the required constant"):
            self.rewrite(
                current, target, [{"at": 4, "proof": "dominating_def"}]
            )

    def test_branch_into_the_span_is_rejected(self):
        # li r31,0 ; bne +8 ; nop ; li r29,0   -- the site is a branch
        # target, so r31 is not provably 0 on every incoming edge.
        current = _words(LI_R31_0, BNE_PLUS_8, NOP, LI_R29_0, BLR)
        target = _words(LI_R31_0, BNE_PLUS_8, NOP, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "branch target"):
            self.rewrite(
                current, target, [{"at": 12, "proof": "dominating_def"}]
            )

    def test_call_inside_the_span_is_rejected(self):
        # A call may clobber the source register.
        current = _words(LI_R31_0, BL, LI_R29_0, BLR)
        target = _words(LI_R31_0, BL, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "control instruction"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "dominating_def"}]
            )

    def test_interposed_redefinition_is_rejected(self):
        # add r31,... between the definition and the site.
        redefine = 0x7FE63214  # add r31,r6,r6
        current = _words(LI_R31_0, redefine, LI_R29_0, BLR)
        target = _words(LI_R31_0, redefine, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "redefined"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "dominating_def"}]
            )

    def test_missing_definition_is_rejected(self):
        current = _words(NOP, LI_R29_0, BLR)
        target = _words(NOP, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "no dominating"):
            self.rewrite(
                current, target, [{"at": 4, "proof": "dominating_def"}]
            )

    def test_li_site_requires_the_dataflow_proof_label(self):
        current = _words(LI_R31_0, LI_R29_0, BLR)
        target = _words(LI_R31_0, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "requires"):
            self.rewrite(
                current, target, [{"at": 4, "proof": "unconditional"}]
            )

    def test_size_mismatch_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "equal aligned sizes"):
            self.rewrite(
                _words(MR_R23_R6), _words(ADDI_R23_R6, BLR),
                [{"at": 0, "proof": "unconditional"}],
            )


class DominatingDefAcrossCallsTests(unittest.TestCase):
    """The `dominating_def_across_calls` relaxation.

    The scan may step over a direct, named, non-millicode `bl` when the
    copied source register is callee-saved.  Every one of those four words
    is load-bearing, so each gets its own rejection test alongside the
    accept path.  Shape under test is the measured G3DReadControlPadStates
    one: `li rS,0` ... `bl <callee>` ... `li rD,0` against a target that
    copies rD from the still-live rS.
    """

    def rewrite(self, current, target, edits, **overrides):
        arguments = {
            "relocated_offsets": set(),
            "target_relocated_offsets": set(),
            "jumptable_offsets": set(),
            "call_targets": {4: "G3DGetPadStatusBuffer"},
        }
        arguments.update(overrides)
        return equivalent_copy_form(current, target, edits, **arguments)

    def spanning_call(self):
        # li r31,0 ; bl <callee> ; li r29,0   ->   ... ; addi r29,r31,0
        return (
            _words(LI_R31_0, BL, LI_R29_0, BLR),
            _words(LI_R31_0, BL, ADDI_R29_R31, BLR),
        )

    # ---- accept ----

    def test_callee_saved_source_crosses_a_named_direct_call(self):
        current, target = self.spanning_call()
        edits = [{"at": 8, "proof": "dominating_def_across_calls"}]
        output, changed = self.rewrite(current, target, edits)
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)
        # The proof must name the real dominating definition, not merely
        # decline to fail.
        self.assertEqual(edits[0]["_proved_at"], 0)

    # ---- reject: each of the four checked facts ----

    def test_volatile_source_may_not_cross_a_call(self):
        # Same shape, but the copied source is r6, which any callee may
        # clobber.  li r6,0 ; bl ; li r23,0  ->  ... ; addi r23,r6,0
        current = _words(LI_R6_0, BL, LI_R23_0, BLR)
        target = _words(LI_R6_0, BL, ADDI_R23_R6, BLR)
        with self.assertRaisesRegex(ValueError, "r6 is volatile"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
            )

    def test_indirect_call_may_not_be_crossed(self):
        # bctrl reaches an address this scan cannot resolve to a symbol.
        current = _words(LI_R31_0, BCTRL, LI_R29_0, BLR)
        target = _words(LI_R31_0, BCTRL, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "only a direct `bl`"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
            )

    def test_indirect_call_through_lr_may_not_be_crossed(self):
        current = _words(LI_R31_0, BLRL, LI_R29_0, BLR)
        target = _words(LI_R31_0, BLRL, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "only a direct `bl`"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
            )

    def test_unnamed_call_may_not_be_crossed(self):
        # No REL24 relocation means no callee name, so the callee cannot be
        # screened against the millicode family at all.
        current, target = self.spanning_call()
        with self.assertRaisesRegex(ValueError, "no REL24 relocation"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
                call_targets={},
            )

    def test_register_millicode_may_not_be_crossed(self):
        # THE TRAP: _restgpr_28 deliberately rewrites r28-r31, so it is the
        # one direct call that breaks the EABI preservation contract.
        current, target = self.spanning_call()
        with self.assertRaisesRegex(ValueError, "millicode"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
                call_targets={4: "_restgpr_28"},
            )

    def test_save_millicode_is_refused_too(self):
        current, target = self.spanning_call()
        with self.assertRaisesRegex(ValueError, "millicode"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
                call_targets={4: "_savefpr_27"},
            )

    # ---- reject: the relaxation is about CALLS, not control in general ----

    def test_conditional_branch_is_still_not_crossable(self):
        # bne -4 is control but not a call, so it stays refused even with
        # the relaxation requested.
        current = _words(LI_R31_0, BNE_MINUS_4, LI_R29_0, BLR)
        target = _words(LI_R31_0, BNE_MINUS_4, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "only a direct `bl`"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
            )

    def test_branch_into_the_span_is_still_rejected(self):
        # The entry-index check is untouched by the relaxation.
        current = _words(LI_R31_0, BNE_PLUS_8, NOP, LI_R29_0, BLR)
        target = _words(LI_R31_0, BNE_PLUS_8, NOP, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "branch target"):
            self.rewrite(
                current, target,
                [{"at": 12, "proof": "dominating_def_across_calls"}],
            )

    def test_redefinition_after_the_call_is_still_rejected(self):
        redefine = 0x7FE63214  # add r31,r6,r6
        current = _words(LI_R31_0, BL, redefine, LI_R29_0, BLR)
        target = _words(LI_R31_0, BL, redefine, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "redefined"):
            self.rewrite(
                current, target,
                [{"at": 12, "proof": "dominating_def_across_calls"}],
            )

    def test_constant_mismatch_across_a_call_is_still_rejected(self):
        current = _words(LI_R31_5, BL, LI_R29_0, BLR)
        target = _words(LI_R31_5, BL, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "not the required constant"):
            self.rewrite(
                current, target,
                [{"at": 8, "proof": "dominating_def_across_calls"}],
            )

    # ---- the label must be asked for by name ----

    def test_plain_dominating_def_still_refuses_the_same_site(self):
        # The whole point of a separate label: an existing rule cannot
        # drift onto the ABI-dependent proof just because call_targets
        # happens to be available.
        current, target = self.spanning_call()
        with self.assertRaisesRegex(ValueError, "control instruction"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "dominating_def"}]
            )

    def test_unknown_proof_label_is_rejected(self):
        current, target = self.spanning_call()
        with self.assertRaisesRegex(ValueError, "requires"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "across_calls"}]
            )


class RelocatedWordInsideProofSpanTests(unittest.TestCase):
    """A permutation can move a relocated word into a proof span.

    Composing stages therefore forces the span's relocation handling to be
    exact rather than blanket: an interposed relocated word that does not
    write the source is harmless, but ONLY when its relocation type cannot
    rewrite the register fields the write set was decoded from.
    """

    def rewrite(self, current, target, edits, **overrides):
        arguments = {
            "relocated_offsets": set(),
            "target_relocated_offsets": set(),
            "jumptable_offsets": set(),
        }
        arguments.update(overrides)
        return equivalent_copy_form(current, target, edits, **arguments)

    def spanning_relocated_word(self):
        # li r31,0 ; addi r5,r6,<reloc> ; li r29,0  ->  ... ; addi r29,r31,0
        # The interposed word writes r5, never r31.  Its relocation entry
        # sits at byte +2 of the instruction at +0x4, hence offset 6.
        return (
            _words(LI_R31_0, ADDI_R5_R6, LI_R29_0, BLR),
            _words(LI_R31_0, ADDI_R5_R6, ADDI_R29_R31, BLR),
        )

    def test_immediate_only_relocation_may_be_stepped_over(self):
        current, target = self.spanning_relocated_word()
        edits = [{"at": 8, "proof": "dominating_def"}]
        output, changed = self.rewrite(
            current, target, edits,
            relocated_offsets={6}, relocation_types={1: 4},  # ADDR16_LO
        )
        self.assertEqual(output, target)
        self.assertEqual(changed, 1)
        self.assertEqual(edits[0]["_proved_at"], 0)

    def test_sda21_relocation_may_not_be_stepped_over(self):
        # R_PPC_EMB_SDA21 rewrites the base REGISTER field, so nothing
        # decoded from the raw word can be trusted.
        current, target = self.spanning_relocated_word()
        with self.assertRaisesRegex(ValueError, "outside the immediate field"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "dominating_def"}],
                relocated_offsets={6}, relocation_types={1: 109},
            )

    def test_unknown_relocation_type_fails_closed(self):
        current, target = self.spanning_relocated_word()
        with self.assertRaisesRegex(ValueError, "outside the immediate field"):
            self.rewrite(
                current, target, [{"at": 8, "proof": "dominating_def"}],
                relocated_offsets={6}, relocation_types={},
            )

    def test_relocated_definition_is_always_refused(self):
        # An unresolved address half is not the literal the proof needs,
        # however benign its relocation type is.
        current = _words(LI_R31_0, LI_R29_0, BLR)
        target = _words(LI_R31_0, ADDI_R29_R31, BLR)
        with self.assertRaisesRegex(ValueError, "defined by a relocated word"):
            self.rewrite(
                current, target, [{"at": 4, "proof": "dominating_def"}],
                relocated_offsets={2}, relocation_types={0: 4},
            )


class GauntworldPreheaderPermutationTests(unittest.TestCase):
    """config/GUNE5D/webfrank.json :: game/world/gauntworld :: fn_8005FB48.

    Real words from the raw compiler output and the extracted retail
    object.  attempt.HV_gauntworld-preheader-constant-permute.20260901.v1
    """

    # +0x74..+0x8c in OUR colouring: lfd f28(sZeroDouble); li r28,0;
    # lfs f29(sItemZero); li r30,0; lfd f30(own-pool 2^52); lis r24,17200.
    OURS = _words(0xCB800000, 0x3B800000, 0xC3A00000,
                  0x3BC00000, 0xCBC00000, 0x3F004330)
    # The target orders the same five constants [2^52, li, sZeroDouble,
    # li, sItemZero] and colours them one FPR/GPR further along.
    TARGET = _words(0xCB800000, 0x3BC00000, 0xCBA00000,
                    0x3B200000, 0xC3C00000, 0x3F004330)
    ORDER = [4, 3, 0, 1, 2, 5]

    def _permuted(self) -> bytes:
        atoms = [self.OURS[i:i + 4] for i in range(0, len(self.OURS), 4)]
        return b"".join(atoms[source] for source in self.ORDER)

    def test_preheader_constant_permutation_is_dependence_free(self):
        # Three loads from distinct SDA globals into distinct FPRs and two
        # `li` to distinct GPRs, with no store in the region.  Checked in
        # OUR colouring standing alone, because apply_patch runs the
        # permutation before the recolor snapshot.
        check_permutation_dependences(self.OURS, self.ORDER)

    def test_window_stops_before_the_branch(self):
        # +0x8c is `b`, and permute_instruction_atoms refuses any region
        # holding a control op -- which is why the window ends at 0x8c.
        with self.assertRaisesRegex(ValueError, "control op"):
            region = self.OURS + _words(0x48000198)
            permute_instruction_atoms(
                region, self.ORDER + [6], [],
                before_sha256=_sha256(region),
                after_sha256=_sha256(region),
                before_relocations_sha256=_relocation_sha256([]),
                after_relocations_sha256=_relocation_sha256([]),
            )

    def test_post_permute_residual_is_a_consistent_recolor(self):
        permuted = self._permuted()
        verify_consistent_recolor(permuted, self.TARGET)
        recolored, changed = copy_register_fields(permuted, self.TARGET)
        self.assertEqual(recolored, self.TARGET)
        self.assertEqual(changed, 4)

    def test_payload_check_does_not_bind_a_relocation_to_its_atom(self):
        """The guard gap this rule's derivation had to cover by hand.

        permute_instruction_atoms verifies relocations through a sorted
        multiset over ``(offset % 4, info, addend)``.  That proves no
        relocation was created, destroyed or retyped -- it does NOT prove
        each stayed with its own instruction.  Two SDA loads differ only
        in a register field, so a word-only matcher can swap them and
        produce correct text with the symbols on the wrong instructions,
        and this check will not object.  Binding a relocated atom to its
        target slot is therefore an obligation of whoever DERIVES the
        order, not something the guard discharges.
        """
        region = _words(0xC3A00000, 0xC3C00000)   # two SDA lfs
        first, second = 4321, 8765                # distinct symbols
        before = [(2, first, 0), (6, second, 0)]
        after = [(2, second, 0), (6, first, 0)]   # symbols exchanged
        atoms = [region[i:i + 4] for i in range(0, len(region), 4)]
        permuted = b"".join(atoms[source] for source in [1, 0])
        permute_instruction_atoms(
            region, [1, 0], before,
            before_sha256=_sha256(region),
            after_sha256=_sha256(permuted),
            before_relocations_sha256=_relocation_sha256(before),
            after_relocations_sha256=_relocation_sha256(after),
        )


class PermutationWindowSchemaTests(unittest.TestCase):
    """Multi-window permutation schema.

    claim.law.HV_single-permutation-region-is-the-binding-schema-limit
    .20260901.v1: apply_patch read instruction_permutation as ONE dict, so
    a function whose displaced words fall in two separated windows could
    not be expressed at all, however sound each window was.  That single
    fact blocked the pb_window file flip.
    """

    def window(self, start, end):
        return {"start": start, "end": end, "order": [0]}

    # ---- back-compatibility: every shipped rule keeps its meaning ----

    def test_single_dict_is_accepted_unchanged(self):
        window = self.window(0x10, 0x20)
        windows, ranges = permutation_windows(window, 0x100)
        self.assertEqual(windows, [window])
        self.assertEqual(ranges, [(0x10, 0x20)])

    def test_hex_string_bounds_are_parsed(self):
        windows, ranges = permutation_windows(
            {"start": "0x188", "end": "0x194", "order": [0]}, 0x400
        )
        self.assertEqual(ranges, [(0x188, 0x194)])
        self.assertEqual(len(windows), 1)

    # ---- the list form ----

    def test_two_disjoint_windows_are_accepted_in_order(self):
        first = self.window(0x188, 0x194)
        second = self.window(0x2c0, 0x2d8)
        windows, ranges = permutation_windows([first, second], 0x400)
        self.assertEqual(windows, [first, second])
        self.assertEqual(ranges, [(0x188, 0x194), (0x2c0, 0x2d8)])

    def test_adjacent_windows_are_disjoint_and_accepted(self):
        # Touching at a boundary is disjoint: [0x10,0x20) and [0x20,0x30).
        _windows, ranges = permutation_windows(
            [self.window(0x10, 0x20), self.window(0x20, 0x30)], 0x100
        )
        self.assertEqual(ranges, [(0x10, 0x20), (0x20, 0x30)])

    def test_overlapping_windows_are_refused(self):
        # Overlap would make the per-window before-hashes ill-defined.
        with self.assertRaisesRegex(ValueError, "disjoint"):
            permutation_windows(
                [self.window(0x10, 0x28), self.window(0x20, 0x30)], 0x100
            )

    def test_descending_windows_are_refused(self):
        with self.assertRaisesRegex(ValueError, "ascending"):
            permutation_windows(
                [self.window(0x2c0, 0x2d8), self.window(0x188, 0x194)], 0x400
            )

    def test_duplicate_window_is_refused(self):
        with self.assertRaisesRegex(ValueError, "disjoint"):
            permutation_windows(
                [self.window(0x10, 0x20), self.window(0x10, 0x20)], 0x100
            )

    def test_empty_list_is_refused(self):
        with self.assertRaisesRegex(ValueError, "empty"):
            permutation_windows([], 0x100)

    # ---- per-window range validation still fails closed ----

    def test_unaligned_window_is_refused(self):
        with self.assertRaisesRegex(ValueError, "invalid"):
            permutation_windows([self.window(0x12, 0x20)], 0x100)

    def test_window_past_the_function_end_is_refused(self):
        with self.assertRaisesRegex(ValueError, "invalid"):
            permutation_windows([self.window(0xf0, 0x120)], 0x100)

    def test_empty_window_is_refused(self):
        with self.assertRaisesRegex(ValueError, "invalid"):
            permutation_windows([self.window(0x20, 0x20)], 0x100)

    def test_second_window_is_range_checked_too(self):
        with self.assertRaisesRegex(ValueError, "invalid"):
            permutation_windows(
                [self.window(0x10, 0x20), self.window(0x30, 0x120)], 0x100
            )


class RelocationBindingTests(unittest.TestCase):
    """Closes claim.law.HV_permute-payload-check-does-not-bind-a-
    relocation-to-its-atom.20260901.v1.

    The payload multiset proves CONSERVATION (nothing created, destroyed,
    retyped or re-symboled, and each entry kept its byte position inside
    its instruction).  It does not prove BINDING: that each relocation is
    still attached to the instruction that should carry it.  Two SDA loads
    differ only in a register field, so a word-only matcher can exchange
    them and produce byte-correct text with the symbols on the wrong
    instructions.  verify_relocation_binding closes that by asserting our
    post-permute relocations against the TARGET object's relocations, word
    by word.
    """

    SDA21 = 26   # R_PPC_EMB_SDA21
    ADDR16_LO = 4

    def test_matching_binding_passes(self):
        ours = {2: (self.SDA21, "sZeroDouble"), 6: (self.SDA21, "sItemZero")}
        target = {0: (self.SDA21, "sZeroDouble"), 4: (self.SDA21, "sItemZero")}
        verify_relocation_binding(ours, target, region_start=0, region_end=8)

    def test_exchanged_symbols_are_refused(self):
        """The precise defect the law describes: correct text, relocations
        pointing the two loads at each other's globals.  The payload check
        accepts this; the binding check must not."""
        ours = {2: (self.SDA21, "sItemZero"), 6: (self.SDA21, "sZeroDouble")}
        target = {0: (self.SDA21, "sZeroDouble"), 4: (self.SDA21, "sItemZero")}
        with self.assertRaisesRegex(ValueError, "symbol"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8
            )

    def test_sda21_word_plus_two_offset_is_normalised(self):
        """Ours records EMB_SDA21 at word+2, the target at word+0.  That is
        a recording convention, not a mismatch, and must not be read as
        one."""
        ours = {2: (self.SDA21, "gTheGlobal")}
        target = {0: (self.SDA21, "gTheGlobal")}
        verify_relocation_binding(ours, target, region_start=0, region_end=4)

    def test_relocation_type_change_is_refused(self):
        ours = {2: (self.SDA21, "gTheGlobal")}
        target = {0: (self.ADDR16_LO, "gTheGlobal")}
        with self.assertRaisesRegex(ValueError, "type"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=4
            )

    def test_word_relocated_only_on_our_side_needs_region_words(self):
        """dtk resolves addresses when it extracts the retail object, so a
        word we still relocate can appear in the target as a baked literal.
        That word has no counterpart to bind against, so it is exempt --
        but only once nothing in the window could have been exchanged with
        it, which cannot be judged without the words."""
        ours = {2: (self.SDA21, "gTheGlobal"), 6: (self.SDA21, "gOther")}
        target = {0: (self.SDA21, "gTheGlobal")}
        with self.assertRaisesRegex(ValueError, "unexchangeable"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8
            )

    def test_unbindable_word_is_accepted_when_unexchangeable(self):
        # +0x4 is `addi r31,r3,LO` (relocated here, baked in the target);
        # the only other relocated word is an `lfs`, a different form, so
        # no permutation could have exchanged them.
        ours = {2: (self.ADDR16_LO, "gPadManager"), 6: (self.SDA21, "gOther")}
        target = {4: (self.SDA21, "gOther")}
        verify_relocation_binding(
            ours, target, region_start=0, region_end=8,
            words=[0x3BE30000, 0xC3A00000],
        )

    def test_unbindable_word_is_refused_when_exchangeable(self):
        # Both relocated words are `lfs fD,0(0)`, identical outside their
        # register fields, and one has no target counterpart: a
        # permutation could have exchanged them and neither can be bound.
        ours = {2: (self.SDA21, "gTheGlobal"), 6: (self.SDA21, "gOther")}
        target = {4: (self.SDA21, "gOther")}
        with self.assertRaisesRegex(ValueError, "exchanged"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8,
                words=[0xC3A00000, 0xC3C00000],
            )

    def test_word_relocated_only_on_the_target_side_is_refused(self):
        ours = {2: (self.SDA21, "gTheGlobal")}
        target = {0: (self.SDA21, "gTheGlobal"), 4: (self.SDA21, "gOther")}
        with self.assertRaisesRegex(ValueError, "relocated"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8
            )

    # ---- compiler pool labels ----

    def test_consistent_pool_label_correspondence_is_accepted(self):
        """MWCC spells its own constant-pool labels `@NNNN`; the extracted
        target spells the same objects `lbl_XXXXXXXX`.  They cannot be
        matched by name, so the rule requires a CONSISTENT one-to-one
        correspondence across the whole window instead."""
        ours = {2: (self.SDA21, "@1234"), 6: (self.SDA21, "@1235")}
        target = {
            0: (self.SDA21, "lbl_801284D8"),
            4: (self.SDA21, "lbl_801284E0"),
        }
        mapping = verify_relocation_binding(
            ours, target, region_start=0, region_end=8
        )
        self.assertEqual(
            mapping, {"@1234": "lbl_801284D8", "@1235": "lbl_801284E0"}
        )

    def test_pool_label_correspondence_must_be_one_to_one(self):
        # Two distinct pool labels of ours cannot both mean one target
        # label: that is exactly the exchange defect wearing pool names.
        ours = {2: (self.SDA21, "@1234"), 6: (self.SDA21, "@1235")}
        target = {
            0: (self.SDA21, "lbl_801284D8"),
            4: (self.SDA21, "lbl_801284D8"),
        }
        with self.assertRaisesRegex(ValueError, "one-to-one|correspondence"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8
            )

    def test_pool_label_correspondence_must_be_consistent(self):
        ours = {
            2: (self.SDA21, "@1234"),
            6: (self.SDA21, "@1235"),
            10: (self.SDA21, "@1234"),
        }
        target = {
            0: (self.SDA21, "lbl_801284D8"),
            4: (self.SDA21, "lbl_801284E0"),
            8: (self.SDA21, "lbl_801284E8"),
        }
        with self.assertRaisesRegex(ValueError, "one-to-one|correspondence"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=12
            )

    def test_identical_pool_label_names_bind_directly(self):
        """Our object often carries the target's own `lbl_XXXXXXXX`
        placeholder spelling for an own-pool datum, so an exact name match
        must bind whatever the name looks like.  Live instance:
        game/game/combat +0x4, symbol lbl_80240E30 on both sides."""
        ours = {2: (self.ADDR16_LO, "lbl_80240E30")}
        target = {0: (self.ADDR16_LO, "lbl_80240E30")}
        verify_relocation_binding(ours, target, region_start=0, region_end=4)

    def test_exchanged_identical_pool_label_names_are_still_refused(self):
        ours = {
            2: (self.ADDR16_LO, "lbl_80240E30"),
            6: (self.ADDR16_LO, "lbl_80240E38"),
        }
        target = {
            0: (self.ADDR16_LO, "lbl_80240E38"),
            4: (self.ADDR16_LO, "lbl_80240E30"),
        }
        # Both sides spell these the same way, so the exchange is caught by
        # the exact-name test rather than by the pool correspondence.
        with self.assertRaisesRegex(ValueError, "wrong instruction"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=8
            )

    def test_named_symbol_may_not_be_matched_against_a_pool_label(self):
        ours = {2: (self.SDA21, "gRealGlobal")}
        target = {0: (self.SDA21, "lbl_801284D8")}
        with self.assertRaisesRegex(ValueError, "symbol"):
            verify_relocation_binding(
                ours, target, region_start=0, region_end=4
            )

    # ---- the guard reaches the permutation stage ----

    def test_permute_refuses_a_relocation_exchanging_order(self):
        """The same demonstration as
        GauntworldPreheaderPermutationTests.test_payload_check_does_not_
        bind_a_relocation_to_its_atom, but with the target relocations
        supplied.  The order swaps two SDA loads; the text is correct and
        the payload multiset is unchanged, and it must now be refused."""
        region = _words(0xC3A00000, 0xC3C00000)   # two SDA lfs
        atoms = [region[i:i + 4] for i in range(0, len(region), 4)]
        permuted = b"".join(atoms[source] for source in [1, 0])
        before = [(2, (100 << 8) | self.SDA21, 0),
                  (6, (200 << 8) | self.SDA21, 0)]
        after = [(2, (200 << 8) | self.SDA21, 0),
                 (6, (100 << 8) | self.SDA21, 0)]
        with self.assertRaisesRegex(ValueError, "symbol|binding"):
            permute_instruction_atoms(
                region, [1, 0], before,
                before_sha256=_sha256(region),
                after_sha256=_sha256(permuted),
                before_relocations_sha256=_relocation_sha256(before),
                after_relocations_sha256=_relocation_sha256(after),
                our_symbols={2: "sZeroDouble", 6: "sItemZero"},
                target_relocations={
                    0: (self.SDA21, "sZeroDouble"),
                    4: (self.SDA21, "sItemZero"),
                },
            )

    def test_permute_accepts_the_correctly_bound_order(self):
        # Same two loads, but the target really does want them exchanged,
        # so after the swap each relocation sits where the target has it.
        region = _words(0xC3A00000, 0xC3C00000)
        atoms = [region[i:i + 4] for i in range(0, len(region), 4)]
        permuted = b"".join(atoms[source] for source in [1, 0])
        before = [(2, (100 << 8) | self.SDA21, 0),
                  (6, (200 << 8) | self.SDA21, 0)]
        after = [(2, (200 << 8) | self.SDA21, 0),
                 (6, (100 << 8) | self.SDA21, 0)]
        output, moved_relocations, _moved = permute_instruction_atoms(
            region, [1, 0], before,
            before_sha256=_sha256(region),
            after_sha256=_sha256(permuted),
            before_relocations_sha256=_relocation_sha256(before),
            after_relocations_sha256=_relocation_sha256(after),
            our_symbols={2: "sZeroDouble", 6: "sItemZero"},
            target_relocations={
                0: (self.SDA21, "sItemZero"),
                4: (self.SDA21, "sZeroDouble"),
            },
        )
        self.assertEqual(output, permuted)
        self.assertEqual(moved_relocations, after)


if __name__ == "__main__":
    unittest.main()
