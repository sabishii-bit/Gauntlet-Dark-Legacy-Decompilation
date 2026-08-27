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


if __name__ == "__main__":
    unittest.main()
