"""Mutation controls for the recovered twelve-byte combat launch vector."""
from copy import deepcopy
import struct
import unittest

from tools.gdl.composed_census import r69_combat_vector_recovery as audit


def retail_fixture():
    code = bytearray(0x758)
    # Full independently captured retail [+0x48,+0xC8) witness. Outside this
    # interval the fixture is synthetic; this test is NOT a whole-CFG proof.
    code[0x48:0xC8] = bytes.fromhex(
        "3ca080123be4000038858df857b1073e3b0000003aa000003b5a023841820010"
        "3b6413203aa000014800004454050463418200103b6413503aa0000148000030"
        "57a502d74182001857a5018d408200103b6412f03aa000014800001480be0008"
        "1ca500307f642a143b7b0170c01e01145400056befe0073241820018386413b0")
    neighborhood = bytearray(0x48)
    struct.pack_into(">3f", neighborhood, 0x30, *audit.VALUES)
    return code, neighborhood


def source_fixture():
    data = struct.pack(">3f", *audit.VALUES)
    section = dict(type=1, alignment=8, size=12, flags=3, relocations=[], bytes=data.hex())
    body = bytearray(0xEC)
    body[0xE0:0xE8] = bytes.fromhex("3c60000038630000")
    result = dict(functions={audit.FUNCTION: dict(offset=0, size=len(body), body=body.hex(),
                  relocations=[[0xE2, 6, audit.NAME, 0], [0xE6, 4, audit.NAME, 0]], binding=1)},
                  sections={".data": section, ".text": {"bytes": "4e800020"}},
                  symbols={audit.NAME: dict(section=".data", offset=0, size=12, binding=1, bytes=data.hex())},
                  exception_records=[])
    before = deepcopy(result)
    before["sections"][".data"].update(size=0, bytes="")
    before["symbols"] = {}
    return before, result, data


def table_fixture():
    raw = dict(sections={".data": {"relocations": []}}, symbols={}, functions={})
    target = {"functions": {}}
    addresses, data = {}, {}
    for offset, size, address, function in audit.TABLES:
        name, target_name = "@" + str(offset), f"jumptable_{address:08X}"
        base = 0x80010000 if function == "screen_limitation" else 0x80020000
        addresses[function] = (".text", base, 4096)
        addresses[target_name] = (".data", address, size)
        raw["symbols"][name] = dict(section=".data", offset=offset, size=size, binding=0)
        raw["functions"].setdefault(function, dict(size=4096, relocations=[]))
        target["functions"].setdefault(function, dict(relocations=[]))
        raw["functions"][function]["relocations"] += [[0, 6, name, 0], [4, 4, name, 0]]
        target["functions"][function]["relocations"] += [[0, 6, target_name, 0], [4, 4, target_name, 0]]
        data[address] = b"".join(struct.pack(">I", base + i*4) for i in range(size//4))
        raw["sections"][".data"]["relocations"] += [[offset+i*4, 1, function, i*4] for i in range(size//4)]
    return raw, target, addresses, data


class CombatVectorTests(unittest.TestCase):
    def test_consumer_computes_real_address_and_exact_values(self):
        result = audit.target_obligation(*retail_fixture())
        self.assertEqual(result["address"], "0x8011a1a8")
        self.assertEqual(result["bytes"], "00000000bf000000bfa00000")
        witness = result["reviewed_target_witness"]
        self.assertEqual(witness["kind"], "fixed-manually-reviewed-target-witness")
        self.assertEqual(witness["function_offset_range"], ["0x48", "0xc8"])
        self.assertEqual(witness["size"], 128)

    def test_intervening_r4_overwrite_or_branch_change_cannot_pass(self):
        code, neighborhood = retail_fixture()
        for offset, word in ((0x58, 0x38800000),  # li r4,0 instead of li r24,0
                             (0x6C, 0x38840004),  # addi r4,r4,4
                             (0x64, 0x48000060),  # changed branch to +0xC4
                             (0x4C, 0x3BE50000)):  # changed setup dependency
            with self.subTest(offset=hex(offset), word=hex(word)):
                changed = bytearray(code)
                struct.pack_into(">I", changed, offset, word)
                for selected in (0x48, 0x50, 0xC4):
                    self.assertEqual(changed[selected:selected+4], code[selected:selected+4])
                with self.assertRaisesRegex(ValueError, "fixed reviewed target witness"):
                    audit.target_obligation(changed, neighborhood)

    def test_changed_datum_bit_rejected(self):
        code, neighborhood = retail_fixture()
        neighborhood[0x34] ^= 1
        with self.assertRaisesRegex(ValueError, "float bytes"):
            audit.target_obligation(code, neighborhood)

    def test_wrong_consumer_register_or_displacement_rejected(self):
        code, neighborhood = retail_fixture()
        for at, word, why in ((0xC4, 0x386513B0, "register"), (0xC4, 0x386413B4, "address")):
            changed = bytearray(code)
            struct.pack_into(">I", changed, at, word)
            with self.assertRaisesRegex(ValueError, why):
                audit.target_obligation(changed, neighborhood)

    def test_compiled_datum_is_full_extent_not_prefix(self):
        _, current, data = source_fixture()
        self.assertEqual(audit.source_obligation(current, data)["size"], 12)
        current["symbols"][audit.NAME]["size"] = 16
        with self.assertRaisesRegex(ValueError, "three-float definition"):
            audit.source_obligation(current, data)

    def test_relocated_or_wrong_bytes_rejected(self):
        _, current, data = source_fixture()
        wrong = deepcopy(current)
        wrong["symbols"][audit.NAME]["bytes"] = "00" * 12
        with self.assertRaisesRegex(ValueError, "differs from retail"):
            audit.source_obligation(wrong, data)
        current["sections"][".data"]["relocations"] = [[4, 1, "not_float", 0]]
        with self.assertRaisesRegex(ValueError, "relocation within"):
            audit.source_obligation(current, data)

    def test_exact_addition_retains_every_other_obligation(self):
        result = audit.delta_obligation(*source_fixture())
        self.assertEqual(result["only_appended_datum_size"], 12)
        self.assertTrue(result["eh_unchanged"])

    def test_no_hidden_code_eh_or_section_regression(self):
        before, current, data = source_fixture()
        changes = [lambda r: r["functions"][audit.FUNCTION].update(body="60000000"),
                   lambda r: r["exception_records"].append({"unexpected": True}),
                   lambda r: r["sections"][".text"].update(bytes="60000000")]
        for change in changes:
            bad = deepcopy(current)
            change(bad)
            with self.assertRaises(ValueError):
                audit.delta_obligation(before, bad, data)

    def test_exact_datum_does_not_absolve_wrong_consumer_binding(self):
        _, current, data = source_fixture()
        current["functions"][audit.FUNCTION]["relocations"][0][2] = "unrelated"
        with self.assertRaisesRegex(ValueError, "bind the recovered"):
            audit.source_obligation(current, data)

    def test_data_metadata_and_extra_tail_cannot_hide(self):
        before, current, data = source_fixture()
        for key, value in (("relocations", [[0, 1, "x", 0]]), ("alignment", 4), ("size", 16)):
            bad = deepcopy(current)
            bad["sections"][".data"][key] = value
            with self.assertRaises(ValueError):
                audit.delta_obligation(before, bad, data)

    def test_typed_source_form_is_narrow(self):
        source = b"extern u8 lbl_8011A1A8[];\nMulVecMat3((f32*)lbl_8011A1A8, aim, playerView->mat);"
        result = audit.typed_form(source)
        self.assertIn(b"f32 lbl_8011A1A8[3] = {0.0f, -0.5f, -1.25f};", result)
        self.assertIn(b"MulVecMat3(lbl_8011A1A8, aim, playerView->mat);", result)
        with self.assertRaisesRegex(ValueError, "declaration/consumer"):
            audit.typed_form(source + b"\nextern u8 lbl_8011A1A8[];")

    def test_fragmented_table_mapping_covers_all_pointers(self):
        raw, target, addresses, data = table_fixture()
        result = audit.jump_table_context(raw, target, addresses, lambda at, size: data[at])
        self.assertEqual(sum(t["exact_relative_destinations"] for t in result["tables"]), 63)
        self.assertTrue(result["raw_data_not_one_contiguous_retail_run"])

    def test_table_offset_difference_remains_explicit(self):
        raw, target, addresses, data = table_fixture()
        raw["sections"][".data"]["relocations"][0][3] += 4
        result = audit.jump_table_context(raw, target, addresses, lambda at, size: data[at])
        self.assertEqual(result["tables"][0]["entries"][0]["delta"], 4)
        self.assertEqual(result["tables"][0]["exact_relative_destinations"], 14)

    def test_missing_pointer_wrong_consumer_or_outside_code_fails(self):
        raw, target, addresses, data = table_fixture()
        missing = deepcopy(raw)
        missing["sections"][".data"]["relocations"].pop()
        with self.assertRaisesRegex(ValueError, "sixty-three"):
            audit.jump_table_context(missing, target, addresses, lambda at, size: data[at])
        target["functions"]["screen_limitation"]["relocations"][0][2] = "wrong"
        with self.assertRaisesRegex(ValueError, "expected consumer"):
            audit.jump_table_context(raw, target, addresses, lambda at, size: data[at])
        raw, target, addresses, data = table_fixture()
        data[0x80118D5C] = struct.pack(">I", 0x80011000) + data[0x80118D5C][4:]
        with self.assertRaisesRegex(ValueError, "jump destination"):
            audit.jump_table_context(raw, target, addresses, lambda at, size: data[at])


if __name__ == "__main__":
    unittest.main()
