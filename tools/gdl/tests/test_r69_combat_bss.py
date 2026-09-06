"""Do not turn an identity/extent certificate into an allocation-match claim."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r69_combat_bss_audit as audit


def fixture(exact=False):
    symbols, addresses = {}, {}
    order = list(audit.OBJECTS) if exact else [audit.OBJECTS[i] for i in (6, 0, 7, 1, 2, 5, 3)]
    offset = 0
    for name, _, size in order:
        symbols[name] = dict(section=".bss", offset=offset, size=size, binding=1, bytes=None)
        offset += size
    sections = {".bss": dict(type=8, size=offset, alignment=8, bytes=None, relocations=[])}
    if not exact:
        symbols["PhoenixTree"] = dict(section=".sbss", offset=0, size=4, binding=1, bytes=None)
        sections[".sbss"] = dict(type=8, size=4, alignment=8, bytes=None, relocations=[])
    for name, offset, size in audit.OBJECTS:
        addresses[name] = (".bss", audit.BASE + offset, size)
    return dict(symbols=symbols, sections=sections), addresses


class CombatBssTests(unittest.TestCase):
    def test_nonexact_layout_is_explicit_not_failure(self):
        result = audit.allocation_obligation(*fixture())
        self.assertTrue(result["named_extent_coverage_exact"])
        self.assertFalse(result["source_target_layout_equal"])
        self.assertEqual(result["source_sections"], {".bss": 596, ".sbss": 4})

    def test_exact_layout_control(self):
        self.assertTrue(audit.allocation_obligation(*fixture(True))["source_target_layout_equal"])

    def test_extent_binding_and_initialized_bytes_cannot_pass(self):
        source, addresses = fixture()
        for key, value in (("size", 16), ("binding", 0), ("bytes", "00")):
            changed = deepcopy(source)
            changed["symbols"]["pmissile_sfxidx"][key] = value
            with self.assertRaisesRegex(ValueError, "extent/linkage/type"):
                audit.allocation_obligation(changed, addresses)

    def test_target_named_address_is_checked(self):
        source, addresses = fixture()
        addresses["PhoenixTree"] = (".bss", audit.BASE + 200, 4)
        with self.assertRaisesRegex(ValueError, "target identity/extent"):
            audit.allocation_obligation(source, addresses)

    def test_anonymous_padding_and_overlap_rejected(self):
        source, addresses = fixture()
        changed = deepcopy(source)
        changed["symbols"]["FamiliarTree"]["offset"] += 4
        with self.assertRaisesRegex(ValueError, "overlap or unidentified"):
            audit.allocation_obligation(changed, addresses)
        source["sections"][".bss"]["size"] += 4
        with self.assertRaisesRegex(ValueError, "tail"):
            audit.allocation_obligation(source, addresses)

    def test_missing_or_foreign_storage_rejected(self):
        source, addresses = fixture()
        source["symbols"].pop("PhoenixTree")
        with self.assertRaisesRegex(ValueError, "name coverage"):
            audit.allocation_obligation(source, addresses)

    def test_bss_has_no_data_or_relocations(self):
        source, addresses = fixture()
        for key, value in (("type", 1), ("alignment", 4), ("relocations", [[0, 1, "x", 0]])):
            changed = deepcopy(source)
            changed["sections"][".bss"][key] = value
            with self.assertRaisesRegex(ValueError, "section kind"):
                audit.allocation_obligation(changed, addresses)

    def test_zero_variant_preserves_other_source_and_line_endings(self):
        source = b"prefix\r\n" + b"\r\n".join(
            (("void* " + name + ";") if name == "PhoenixTree" else
             ("s32 " + name + "[2][4];")).encode()
            for name, _, _ in audit.OBJECTS) + b"\r\nvoid foo(void) { }\r\n"
        result = audit.zero_form(source)
        self.assertTrue(result.endswith(b"\r\nvoid foo(void) { }\r\n"))
        self.assertEqual(result.count(b" = {0};"), 7)
        self.assertIn(b"void* PhoenixTree = 0;\r\n", result)
        with self.assertRaisesRegex(ValueError, "roster"):
            audit.zero_form(source + b"\nvoid* PhoenixTree;\n")

    def test_output_guard_before_compiler(self):
        with tempfile.TemporaryDirectory(prefix="r69_combat_test_") as directory:
            root = Path(directory)
            with patch.object(audit, "ROOT", root), patch.object(audit.aux.cv, "read_edges", side_effect=AssertionError("compiled")):
                for path in ("src/combat.c", "build/report.json", "build/r69_combat_bad.txt"):
                    with self.assertRaisesRegex(ValueError, "build/r69_combat"):
                        audit.audit(root / path)


if __name__ == "__main__":
    unittest.main()
