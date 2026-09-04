import unittest

from tools.gdl.atree_exports import SymbolFact, choose_zero_symbol, require_exports


def fact(name, *, bind="STB_LOCAL", section=".sdata2", size=4,
         data=b"\0\0\0\0"):
    return SymbolFact(name, bind, "STT_OBJECT", section, 0, size, data)


class AtreeExportsTests(unittest.TestCase):
    def test_discovers_zero_by_form_and_bytes_not_pool_name(self):
        self.assertEqual("@731", choose_zero_symbol([fact("@731")]))

    def test_defined_sAtreeZero_bypasses_pool_discovery(self):
        self.assertIsNone(choose_zero_symbol([
            fact("sAtreeZero", bind="STB_GLOBAL"),
            fact("@731"),
        ]))

    def test_ambiguous_zero_pool_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "found 2"):
            choose_zero_symbol([fact("@1"), fact("@2")])

    def test_all_cross_tu_objects_must_have_expected_binding(self):
        symbols = [fact(name, section=".bss") for name in (
            "atree_handles", "atree_scroll", "whichatree", "natreelists"
        )]
        require_exports(symbols, "STB_LOCAL")
        with self.assertRaisesRegex(ValueError, "expected binding STB_GLOBAL"):
            require_exports(symbols, "STB_GLOBAL")


if __name__ == "__main__":
    unittest.main()
