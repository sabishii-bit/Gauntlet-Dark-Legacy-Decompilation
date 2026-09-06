"""Two-sided scope and mutation controls for the bounded tower data proof."""
from copy import deepcopy
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r69_tower_ownership_audit as audit


def fixture():
    raw = bytearray(488)
    raw[:340] = bytes(i % 251 for i in range(340))
    raw[0xA2:0xA4] = b"\0\0"
    symbols, addresses = {}, {}
    for name, offset, size in audit.TABLES:
        symbols[name] = dict(section=".data", offset=offset, size=size,
                             binding=1, bytes=raw[offset:offset+size].hex())
        addresses[name] = (".data", audit.BASE + offset, 44 if size == 42 else size)
    symbols["@jump"] = dict(section=".data", offset=340, size=148,
                             binding=0, bytes=raw[340:].hex())
    addresses["TowerCheckMessages"] = (".text", 0x800A2894, 0xAE4)
    relocs = [(at, 1, "TowerCheckMessages", i * 4)
              for i, at in enumerate(range(340, 488, 4))]
    target = bytearray(raw)
    for at, _, _, addend in relocs:
        struct.pack_into(">I", target, at, 0x800A2894 + addend)
    source = dict(sections={".data": dict(type=1, alignment=8, size=488,
                                         bytes=raw.hex(), relocations=relocs)},
                  symbols=symbols, functions={"TowerCheckMessages": {"size": 0xAE4}})
    return source, bytes(target), addresses


class TowerDataTests(unittest.TestCase):
    def test_exact_complete_data_passes(self):
        result = audit.data_obligation(*fixture())
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["size"], 488)
        self.assertEqual(len(result["jump_relocations"]), 37)

    def test_retail_numeric_mutation_fails(self):
        source, target, addresses = fixture()
        changed = bytearray(target)
        changed[0x20] ^= 1
        with self.assertRaisesRegex(ValueError, "array bytes"):
            audit.data_obligation(source, changed, addresses)

    def test_alignment_mutation_fails_even_if_both_sides_agree(self):
        source, target, addresses = fixture()
        target = bytearray(target)
        raw = bytearray.fromhex(source["sections"][".data"]["bytes"])
        target[0xA2] = raw[0xA2] = 1
        source["sections"][".data"]["bytes"] = raw.hex()
        with self.assertRaisesRegex(ValueError, "alignment"):
            audit.data_obligation(source, target, addresses)

    def test_full_extent_required_not_exact_prefix(self):
        source, target, addresses = fixture()
        source["sections"][".data"]["size"] = 340
        with self.assertRaisesRegex(ValueError, "complete"):
            audit.data_obligation(source, target, addresses)

    def test_array_extent_and_named_home_required(self):
        source, target, addresses = fixture()
        for key, value in (("size", 44), ("offset", 0x7C)):
            wrong = deepcopy(source)
            wrong["symbols"]["lbl_80124CE8"][key] = value
            with self.assertRaisesRegex(ValueError, "array layout"):
                audit.data_obligation(wrong, target, addresses)
        addresses["crystal_order"] = (".data", audit.BASE + 0xA8, 56)
        with self.assertRaisesRegex(ValueError, "array layout"):
            audit.data_obligation(source, target, addresses)

    def test_missing_jump_site_fails(self):
        source, target, addresses = fixture()
        source["sections"][".data"]["relocations"].pop()
        with self.assertRaisesRegex(ValueError, "37 jump"):
            audit.data_obligation(source, target, addresses)

    def test_wrong_relocated_pointer_fails_despite_exact_arrays(self):
        source, target, addresses = fixture()
        source["sections"][".data"]["relocations"][0] = (340, 1, "TowerCheckMessages", 4)
        with self.assertRaisesRegex(ValueError, "jump pointer differs"):
            audit.data_obligation(source, target, addresses)

    def test_unsupported_kind_and_interior_bounds_fail(self):
        source, target, addresses = fixture()
        for row in ((340, 6, "TowerCheckMessages", 0), (340, 1, "Elsewhere", 0),
                    (340, 1, "TowerCheckMessages", 0xAE4), (340, 1, "TowerCheckMessages", 2)):
            wrong = deepcopy(source)
            wrong["sections"][".data"]["relocations"][0] = row
            with self.assertRaisesRegex(ValueError, "unsupported or out-of-bounds"):
                audit.data_obligation(wrong, target, addresses)

    def test_output_guard_runs_before_compiler(self):
        with tempfile.TemporaryDirectory(prefix="r69_tower_test_") as directory:
            root = Path(directory)
            with patch.object(audit, "ROOT", root), patch.object(audit.aux.cv, "read_edges", side_effect=AssertionError("compiled")):
                for path in ("src/tower.c", "build/report.json", "build/r69_tower_bad.txt"):
                    with self.assertRaisesRegex(ValueError, "build/r69_tower"):
                        audit.audit(root / path)


if __name__ == "__main__":
    unittest.main()
