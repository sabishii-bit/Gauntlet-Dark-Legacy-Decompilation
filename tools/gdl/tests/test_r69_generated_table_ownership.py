import copy
import struct
import unittest

from tools.gdl.composed_census.r69_generated_table_ownership import CASES, obligation


class GeneratedTableOwnershipTests(unittest.TestCase):
    def fixture(self):
        return dict(fidelity=True, sections={".data": dict(type=1, flags=3, size=8, alignment=8, bytes="00"*8)},
                    symbols=[dict(name="@7", section=".data", type=1, value=0, size=8, binding=0)],
                    functions={"dispatch": dict(size=32, relocations=[[0, 6, "@7", 0], [4, 4, "@7", 0]])},
                    relocations={".data": [[0, 1, "dispatch", 8], [4, 1, "dispatch", 12]]})

    def check(self, source=None, target=None, symbols=None):
        return obligation(source or self.fixture(), 0x1000, 0x1008, (("table", "dispatch", 0, 2),),
                          target or struct.pack(">II", 0x2008, 0x2010),
                          symbols or {"table": (".data", 0x1000, 8), "dispatch": (".text", 0x2000, 32)})

    def test_ownership_keeps_differing_branch_offset_as_debt(self):
        result = self.check()
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["equal_pointers"], 1)
        self.assertEqual(len(result["differing_pointers"]), 1)

    def test_player_roster_covers_all_three_generated_tables(self):
        base, end, tables = CASES["game/game/player"]
        self.assertEqual((base, end), (0x80120B4C, 0x80120BEC))
        self.assertEqual(sum(t[3] for t in tables), 40)
        self.assertEqual(tables[-1], ("jumptable_80120BA8", "do_got_it_8007FC80", 92, 17))

    def test_no_mutation(self):
        source = self.fixture()
        before = copy.deepcopy(source)
        self.check(source)
        self.assertEqual(source, before)

    def test_refuses_unfaithful_source(self):
        source = self.fixture()
        source["fidelity"] = False
        with self.assertRaises(ValueError):
            self.check(source)

    def test_refuses_data_bytes_hidden_under_relocations(self):
        source = self.fixture()
        source["sections"][".data"]["bytes"] = "0000000100000000"
        with self.assertRaises(ValueError):
            self.check(source)

    def test_refuses_missing_or_duplicate_relocation(self):
        for relocations in ([[0, 1, "dispatch", 8]], [[0, 1, "dispatch", 8], [0, 1, "dispatch", 12]]):
            source = self.fixture()
            source["relocations"][".data"] = relocations
            with self.assertRaises(ValueError):
                self.check(source)

    def test_refuses_wrong_owner_kind_or_unaligned_target(self):
        for relocation in ([0, 1, "other", 8], [0, 4, "dispatch", 8], [0, 1, "dispatch", 9], [0, 1, "dispatch", 32]):
            source = self.fixture()
            source["relocations"][".data"][0] = relocation
            with self.assertRaises(ValueError):
                self.check(source)

    def test_refuses_wrong_retail_destination(self):
        for address in (0x1998, 0x2020, 0x2009):
            with self.assertRaises(ValueError):
                self.check(target=struct.pack(">II", address, 0x2010))

    def test_refuses_wrong_named_target_extent(self):
        with self.assertRaises(ValueError):
            self.check(symbols={"table": (".data", 0x1004, 8), "dispatch": (".text", 0x2000, 32)})

    def test_refuses_table_with_no_dispatch_consumer(self):
        source = self.fixture()
        source["functions"]["dispatch"]["relocations"] = []
        with self.assertRaises(ValueError):
            self.check(source)

    def test_refuses_extra_data_definition(self):
        source = self.fixture()
        source["symbols"].append(dict(source["symbols"][0], name="extra"))
        with self.assertRaises(ValueError):
            self.check(source)

    def test_refuses_claiming_source_slack(self):
        source = self.fixture()
        source["sections"][".data"]["size"] = 12
        with self.assertRaises(ValueError):
            self.check(source)

    def test_exact_tables_are_labelled_exact_by_pointer_count(self):
        result = self.check(target=struct.pack(">II", 0x2008, 0x200C))
        self.assertEqual(result["equal_pointers"], 2)
        self.assertEqual(result["differing_pointers"], [])

    def test_explicit_extracted_local_spelling_does_not_modify_source(self):
        source = self.fixture()
        source["functions"]["do_sel_menu_8008E4F4"] = source["functions"].pop("dispatch")
        for relocation in source["relocations"][".data"]:
            relocation[2] = "do_sel_menu_8008E4F4"
        before = copy.deepcopy(source)
        result = obligation(source, 0x1000, 0x1008, (("table", "do_sel_menu_8008E4F4", 0, 2),),
                            struct.pack(">II", 0x2008, 0x200C),
                            {"table": (".data", 0x1000, 8), "do_sel_menu": (".text", 0x2000, 32)})
        self.assertEqual(result["equal_pointers"], 2)
        self.assertEqual(source, before)


if __name__ == "__main__":
    unittest.main()
