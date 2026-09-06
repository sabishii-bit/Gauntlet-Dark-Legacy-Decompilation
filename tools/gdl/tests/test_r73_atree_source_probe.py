"""Mutation negatives for the bounded atree source-ownership comparison."""
import copy
import unittest

from tools.gdl.composed_census.r73_atree_source_probe import canonical_relocations, source_export_obligation


class AtreeSourceProofTests(unittest.TestCase):
    def setUp(self):
        self.inv = {"symbols": {"@1": dict(binding=0, section=".rodata", offset=8,
                            size=4, bytes="61626300")},
                    "sections": {".rodata": {"relocations": []}}}

    def test_private_rename_keeps_exact_location(self):
        other = copy.deepcopy(self.inv)
        other["symbols"]["@2"] = other["symbols"].pop("@1")
        self.assertEqual(canonical_relocations(self.inv, [(4, 6, "@1", 0)]),
                         canonical_relocations(other, [(4, 6, "@2", 0)]))
        other["symbols"]["@2"]["offset"] += 4
        self.assertNotEqual(canonical_relocations(self.inv, [(4, 6, "@1", 0)]),
                            canonical_relocations(other, [(4, 6, "@2", 0)]))

    def test_unreviewed_identity_refused(self):
        for key, value in (("binding", 1), ("section", ".data"), ("bytes", None)):
            other = copy.deepcopy(self.inv)
            other["symbols"]["@1"][key] = value
            with self.assertRaises(ValueError):
                canonical_relocations(other, [(4, 6, "@1", 0)])
        with self.assertRaises(ValueError):
            canonical_relocations(self.inv, [(4, 6, "@1", 4)])

    def test_order_site_type_addend_and_named_identity_retained(self):
        base = canonical_relocations(self.inv, [(4, 6, "@1", 0), (8, 10, "callee", 0)])
        for rows in ([(5, 6, "@1", 0), (8, 10, "callee", 0)],
                     [(4, 7, "@1", 0), (8, 10, "callee", 0)],
                     [(4, 6, "@1", 1), (8, 10, "callee", 0)],
                     [(4, 6, "@1", 0), (8, 10, "other", 0)],
                     [(8, 10, "callee", 0), (4, 6, "@1", 0)],
                     [(4, 6, "@1", 0)]):
            self.assertNotEqual(base, canonical_relocations(self.inv, rows))

    def test_source_export_negative_same_bytes_wrong_offset(self):
        symbols = {"sAtreeZero":dict(binding=1, section=".sdata2", offset=0,size=4,bytes="00000000"),
                   "natreelists":dict(binding=1, section=".sbss", offset=40,size=4,bytes=None)}
        pool = "00000000000000003fe000000000000044554d4d5900"
        raw = dict(symbols=symbols, sections={".sdata2":dict(size=22,alignment=8,relocations=[],bytes=pool),
             ".sbss":dict(size=44,alignment=8,type=8,bytes=None,relocations=[])})
        target = copy.deepcopy(raw)
        target["sections"][".sdata2"].update(size=24,bytes=pool+"0000")
        self.assertEqual("PASS", source_export_obligation(raw,target)["status"])
        wrong = copy.deepcopy(raw)
        wrong["symbols"]["sAtreeZero"]["offset"] = 4
        with self.assertRaises(ValueError):
            source_export_obligation(wrong,target)
        wrong = copy.deepcopy(raw)
        wrong["symbols"]["natreelists"]["binding"] = 0
        with self.assertRaises(ValueError):
            source_export_obligation(wrong,target)


if __name__ == "__main__":
    unittest.main()
