import unittest

from tools.gdl.webfrank import (
    _parse_int,
    _relocation_sha256,
    _sha256,
    check_permutation_dependences,
    copy_register_fields,
    instruction_operands,
    permute_instruction_atoms,
    recolor_instruction,
    register_slot_mask,
    verify_consistent_recolor,
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


if __name__ == "__main__":
    unittest.main()
