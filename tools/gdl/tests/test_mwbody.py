import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from mwbody import (
    find_definition,
    format_instruction,
    format_signature,
    local_target,
    wrap_portable_definition,
)


class MwBodyTests(unittest.TestCase):
    def test_static_signature_places_asm_after_storage_class(self):
        self.assertEqual(
            format_signature("static s32 helper(void)"),
            "static asm s32 helper(void)",
        )

    def test_local_branch_uses_function_relative_offset(self):
        self.assertEqual(
            local_target("bge a54 <AllocMem32+0x3c>", "AllocMem32"),
            0x3C,
        )

    def test_external_branch_is_not_local(self):
        self.assertIsNone(
            local_target("bl 0 <FatalErrorf>", "AllocMem32")
        )

    def test_sda_relocation_uses_metrowerks_syntax(self):
        row = {
            "offset": 0,
            "text": "lwz r5,0(0)",
            "reloc": ("EMB_SDA21", "mlmMemUsed"),
        }
        self.assertEqual(
            format_instruction(row, {}, "AllocMem32"),
            "    lwz r5,mlmMemUsed(r0)",
        )

    def test_floating_sda_relocation_uses_placeholder_base(self):
        row = {
            "offset": 0,
            "text": "lfd f4,0(0)",
            "reloc": ("EMB_SDA21", "lbl_80349190"),
        }
        self.assertEqual(
            format_instruction(row, {}, "helper"),
            "    lfd f4,lbl_80349190(r0)",
        )

    def test_find_definition_ignores_braces_in_strings_and_comments(self):
        source = 'void f(void)\n{\n/* } */\nputs("{");\n}\nvoid g(void);\n'
        start, end = find_definition(source, "void f(void)")
        self.assertEqual(
            source[start:end], 'void f(void)\n{\n/* } */\nputs("{");\n}',
        )

    def test_wrap_keeps_portable_definition(self):
        source = "void f(void)\n{\n    work();\n}\n"
        result = wrap_portable_definition(
            source, "void f(void)", "asm void f(void)\n{\n    blr\n}",
        )
        self.assertEqual(
            result,
            "#ifdef __MWERKS__\nasm void f(void)\n{\n    blr\n}\n#else\n"
            "void f(void)\n{\n    work();\n}\n#endif\n",
        )


if __name__ == "__main__":
    unittest.main()
