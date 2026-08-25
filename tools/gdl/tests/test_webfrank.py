import unittest

from tools.gdl.webfrank import (
    _parse_int,
    _relocation_sha256,
    _sha256,
    copy_register_fields,
    permute_instruction_atoms,
    recolor_instruction,
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


if __name__ == "__main__":
    unittest.main()
