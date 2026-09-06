import unittest

from tools.gdl.atree_exports import SymbolFact, require_source_exports, require_exports


def fact(name, *, bind="STB_LOCAL", section=".sdata2", size=4,
         data=b"\0\0\0\0"):
    return SymbolFact(name, bind, "STT_OBJECT", section, 0, size, data)


class AtreeExportsTests(unittest.TestCase):
    def test_source_definitions_allow_modified_values_and_extra_literals(self):
        require_source_exports([
            fact("sAtreeZero", bind="STB_GLOBAL", data=b"\x3f\x80\0\0"),
            fact("natreelists", bind="STB_GLOBAL", section=".sdata", data=b"\0\0\0\x03"),
            fact("@1"), fact("@2"),
        ])

    def test_anonymous_zero_cannot_replace_source_definition(self):
        with self.assertRaisesRegex(ValueError, "source-defined sAtreeZero; found 0"):
            require_source_exports([fact("@1"), fact("natreelists", bind="STB_GLOBAL")])

    def test_duplicate_or_non_global_definition_refused(self):
        definitions = [fact("sAtreeZero", bind="STB_GLOBAL"), fact("natreelists", bind="STB_GLOBAL")]
        with self.assertRaisesRegex(ValueError, "found 2"):
            require_source_exports(definitions + [fact("sAtreeZero", bind="STB_GLOBAL")])
        with self.assertRaisesRegex(ValueError, "global object"):
            require_source_exports([fact("sAtreeZero"), definitions[1]])
        invalid = SymbolFact("sAtreeZero", "STB_GLOBAL", "STT_FUNC", ".text", 0, 4, b"\0" * 4)
        with self.assertRaisesRegex(ValueError, "global object"):
            require_source_exports([invalid, definitions[1]])

    def test_all_cross_tu_objects_must_have_expected_binding(self):
        symbols = [fact(name, section=".bss") for name in (
            "atree_handles", "atree_scroll", "whichatree"
        )]
        require_exports(symbols, "STB_LOCAL")
        with self.assertRaisesRegex(ValueError, "expected binding STB_GLOBAL"):
            require_exports(symbols, "STB_GLOBAL")


if __name__ == "__main__":
    unittest.main()
