"""Regression tests for the name-bound relocation-hash migration (item 11).

The defect: four derivers still passed bare (offset, info, addend) triples
to webfrank._relocation_sha256, which has REQUIRED a symbol-name map since
run 28. Windows with no relocation kept working, so the failure looked like
a property of the function; windows WITH one raised ValueError, and
hv_sweep's blanket `except Exception: continue` turned that into a false
"does not close" row.

Reproduced live before the fix: `python tools/gdl/build_rule.py` on its own
default unit died with "relocation hash needs the symbol name for the
relocation at +0x2".
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from reloc_symbols import moved_symbols, region_symbols  # noqa: E402
from webfrank import _relocation_sha256  # noqa: E402


class RegionSymbolsTests(unittest.TestCase):
    # function-relative RAW r_offset -> (type, name); the SDA21 entry at
    # +0x16 belongs to the instruction at +0x14.
    RELOCS = {
        0x04: (6, "gAlpha"),      # ADDR16_HA on the insn at +0x04
        0x0A: (4, "gAlpha"),      # ADDR16_LO on the insn at +0x08
        0x16: (109, "gBeta"),     # EMB_SDA21 on the insn at +0x14
        0x40: (10, "someCallee"),  # REL24, outside the window below
    }

    def test_window_relative_keys_and_names(self):
        got = region_symbols(self.RELOCS, 0x04, 0x18)
        self.assertEqual(got, {0x00: "gAlpha", 0x06: "gAlpha",
                               0x12: "gBeta"})

    def test_membership_is_tested_on_the_FLOORED_offset(self):
        """The SDA21 at +0x16 belongs to the instruction at +0x14, so a
        window ending at +0x18 must include it and one ending at +0x14
        must not."""
        self.assertIn(0x12, region_symbols(self.RELOCS, 0x04, 0x18))
        self.assertNotIn(0x16 - 0x04, region_symbols(self.RELOCS, 0x04, 0x14))

    def test_relocations_outside_the_window_are_excluded(self):
        self.assertNotIn("someCallee",
                         region_symbols(self.RELOCS, 0x04, 0x18).values())

    def test_a_window_with_no_relocation_is_empty_not_an_error(self):
        self.assertEqual(region_symbols(self.RELOCS, 0x20, 0x30), {})


class MovedSymbolsTests(unittest.TestCase):
    def test_a_swap_moves_both_names(self):
        window = {0x00: "gAlpha", 0x04: "gBeta"}
        self.assertEqual(moved_symbols(window, [1, 0]),
                         {0x04: "gAlpha", 0x00: "gBeta"})

    def test_the_sub_word_remainder_rides_along(self):
        """An SDA21 at slot 1 + 2 bytes must land at its new slot + 2."""
        self.assertEqual(moved_symbols({0x06: "gBeta"}, [1, 0]),
                         {0x02: "gBeta"})

    def test_identity_order_is_a_no_op(self):
        window = {0x00: "gAlpha", 0x06: "gBeta"}
        self.assertEqual(moved_symbols(window, [0, 1]), window)


class RelocationHashTests(unittest.TestCase):
    """The end-to-end contract the four call sites were violating."""

    RECORDS = [(0x00, (339 << 8) | 6, 0), (0x06, (12 << 8) | 109, 4)]

    def test_bare_triples_are_refused(self):
        with self.assertRaisesRegex(ValueError, "needs the symbol name"):
            _relocation_sha256(self.RECORDS)

    def test_a_name_map_from_region_symbols_is_accepted(self):
        relocs = {0x10: (6, "gAlpha"), 0x16: (109, "gBeta")}
        symbols = region_symbols(relocs, 0x10, 0x18)
        self.assertEqual(len(_relocation_sha256(self.RECORDS, symbols)), 64)

    def test_the_hash_is_bound_to_the_NAME_not_the_symbol_index(self):
        """The whole point of the migration: renumbering must not move it."""
        symbols = {0x00: "gAlpha", 0x06: "gBeta"}
        renumbered = [(0x00, (999 << 8) | 6, 0), (0x06, (7 << 8) | 109, 4)]
        self.assertEqual(_relocation_sha256(self.RECORDS, symbols),
                         _relocation_sha256(renumbered, symbols))

    def test_a_DIFFERENT_name_changes_the_hash(self):
        a = _relocation_sha256(self.RECORDS, {0x00: "gAlpha", 0x06: "gBeta"})
        b = _relocation_sha256(self.RECORDS, {0x00: "gAlpha", 0x06: "gGamma"})
        self.assertNotEqual(a, b)

    def test_a_missing_name_for_one_record_still_refuses(self):
        with self.assertRaisesRegex(ValueError, r"\+0x6"):
            _relocation_sha256(self.RECORDS, {0x00: "gAlpha"})


class CallSiteMigrationTests(unittest.TestCase):
    """Every deriver that hashes relocations must pass a name map.

    A source-level assertion, because the runtime path needs built objects:
    the defect is precisely a call site spelled with one argument.
    """

    DERIVERS = [
        "build_rule.py",
        "composed_census/build_rule_pw.py",
        "composed_census/cn_derive.py",
        "composed_census/cn_search.py",
        "composed_census/ch_derive.py",
        "composed_census/ha_close.py",
        "composed_census/wf_rederive_pin.py",
    ]

    def test_no_deriver_calls_the_hash_with_a_bare_triple_list(self):
        import re
        root = Path(__file__).resolve().parents[1]
        offenders = []
        for rel in self.DERIVERS:
            text = (root / rel).read_text(encoding="utf-8")
            for match in re.finditer(
                    r"_relocation_sha256\(\s*([^()]*?)\s*\)", text):
                args = match.group(1)
                if args and "," not in args:
                    offenders.append(f"{rel}: _relocation_sha256({args})")
        self.assertEqual(offenders, [], "; ".join(offenders))


if __name__ == "__main__":
    unittest.main()
