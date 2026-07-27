import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from fndiff import classify_function, compiler_private_aliases_from_symbols


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


if __name__ == "__main__":
    unittest.main()
