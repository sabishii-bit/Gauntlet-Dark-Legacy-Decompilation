import unittest

from tools.gdl.composed_census.r68_symbol_binding_probe import compare, forms


class SymbolBindingTests(unittest.TestCase):
    def test_reversible_identifier_mapping(self):
        old = b"extern int x; int f(void) { return x; }"
        active, pair = forms(old, {"x": "x_80123456"}, 2)
        self.assertEqual(active, "old")
        active, reverse = forms(pair["corrected"], {"x": "x_80123456"}, 2)
        self.assertEqual(active, "corrected")
        self.assertEqual(reverse["old"], old)

    def test_mixed_or_missing_identifiers_refused(self):
        for source in (b"int x; int x_80123456;", b"int other;"):
            with self.assertRaises(ValueError):
                forms(source, {"x": "x_80123456"}, 2)

    def test_only_explicit_relocation_change_allowed(self):
        old = ({}, {".text": "00"}, {".bss": (8, 3, 8, 4)}, [(".text", 0, 6, "x", 0)])
        corrected = (*old[:3], [(".text", 0, 6, "y", 0)])
        self.assertEqual(len(compare(old, corrected, {"x": "y"})["changed_relocations"]), 1)
        with self.assertRaises(ValueError):
            compare(old, (*old[:3], [(".text", 4, 6, "y", 0)]), {"x": "y"})
        with self.assertRaises(ValueError):
            compare(old, (*old[:3], [(".text", 0, 6, "y", 4)]), {"x": "y"})

    def test_bss_extent_change_refused(self):
        old = ({}, {}, {".bss": (8, 3, 8, 4)}, [(".text", 0, 6, "x", 0)])
        new = ({}, {}, {".bss": (8, 3, 12, 4)}, [(".text", 0, 6, "y", 0)])
        with self.assertRaises(ValueError):
            compare(old, new, {"x": "y"})


if __name__ == "__main__":
    unittest.main()
