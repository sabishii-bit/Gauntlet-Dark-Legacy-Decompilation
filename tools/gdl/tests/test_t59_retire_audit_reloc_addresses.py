#!/usr/bin/env python3
"""T3 run-59 item 11: relocations compared by BINDING, not by spelling.

THE OBSERVATION (ER lane). `retire_audit.positional_relocations` compared
relocation SYMBOL NAMES, and the two sides of a retirement never spell a
pool datum the same way. `object_image` normalizes our anonymous MWCC pool
label to its allocation while dtk names the same datum by its address:

    +0x050 R_PPC_EMB_SDA21  ours ['@', '.sdata2', 572, 4, 1, 0]
                            target 'lbl_80346A4C' (+0)
    +0x01a R_PPC_ADDR16_HA  ours ['@', '.data', 612, 32, 1, 0]
                            target 'jumptable_8011C25C' (+0)

game/enemy/enemy::gendir_8004FBC8 passes every other check and failed this
one on a NAMING limitation: ER hand-verified all 14 bindings -- same
offset, same type, same addend, identical resolved addresses. Every enemy
and critter retirement hits it.

THE CURE resolves both spellings to the address they denote: an anonymous
entry to its section's claimed base in config/GUNE5D/splits.txt plus its
offset, a named entry to its config/GUNE5D/symbols.txt address. It is a
NAMING resolution, not a value-equality relaxation -- the section BYTES are
still compared verbatim by `nontext_sections`, so a pool holding different
data still fails there; and an unresolvable name keeps its name, so a datum
whose ownership splits.txt does not claim cannot compare equal to anything.

LIVE CALIBRATION at 704ff6166 (build/t3_scratch/t3_item11_before_control.py
and t3_audit_run.py):

    game/enemy/enemy::gendir_8004FBC8   14/14 relocations
        by NAME    FAIL      by ADDRESS  PASS, 0 unresolved either side
        offsets, types and addends identical either way
    game/sys/memcard::memCardErrorPrompt  --before-ref e7f9d740b~1   PASS
    game/mb/mb_camera::MBCameraUpdate     --before-ref 30582a770~1   PASS
    game/game/player::ExpToLevel          --before-ref 37f9daac5~1   PASS

-- the three shipped retirements still pass end to end (every check, with
--skip-link), so the resolution did not buy gendir's PASS by weakening
them.

The negative side is below: a relocation bound to the WRONG datum, to the
wrong addend, or to a name neither side can resolve still FAILS.
"""

import os
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(TOOLS))

from tools.gdl import retire_audit as ra                      # noqa: E402
from tools.gdl.raw_object import resolve_object               # noqa: E402

SDA21 = ra.SDA21
RUNS = [
    ("game/enemy/enemy", ".sdata2", 0x80346810, 0x80346AAC),
    ("game/enemy/enemy", ".data", 0x8011BFF8, 0x8011C2A0),
    ("game/two/runs", ".rodata", 0x80300000, 0x80300100),
    ("game/two/runs", ".rodata", 0x80400000, 0x80400100),
]
SYMBOLS = {
    "lbl_80346A4C": {"addr": 0x80346A4C},
    "jumptable_8011C25C": {"addr": 0x8011C25C},
    "gEnemyTable": {"addr": 0x80250E00},
    "gendir_8004FBC8": {"addr": 0x8004FBC8},
    "dtor_800DB21C": {"addr": 0x800DB21C},
    "dtor_800DBB94": {"addr": 0x800DBB94},
}


class SectionBases(unittest.TestCase):
    def test_a_claimed_run_gives_its_section_a_base(self):
        bases = ra.section_bases("game/enemy/enemy", RUNS)
        self.assertEqual(bases[".sdata2"], 0x80346810)
        self.assertEqual(bases[".data"], 0x8011BFF8)

    def test_a_section_claimed_by_two_runs_is_omitted(self):
        """An offset does not determine an address with two runs."""
        self.assertEqual(ra.section_bases("game/two/runs", RUNS), {})

    def test_a_unit_with_no_runs_has_no_bases(self):
        self.assertEqual(ra.section_bases("game/no/such", RUNS), {})


class RelocationAddresses(unittest.TestCase):
    def test_a_named_symbol_resolves_to_its_symbols_txt_address(self):
        table = ra.relocation_addresses(SYMBOLS)
        self.assertEqual(table["lbl_80346A4C"], 0x80346A4C)

    def test_a_unique_dtk_local_gains_its_stripped_alias(self):
        """Our object emits `gendir`; dtk spells it `gendir_8004FBC8`."""
        table = ra.relocation_addresses(SYMBOLS)
        self.assertEqual(table["gendir"], 0x8004FBC8)

    def test_a_colliding_base_gains_NO_alias(self):
        table = ra.relocation_addresses(SYMBOLS)
        self.assertNotIn("dtor", table)

    def test_a_placeholder_name_is_never_stripped(self):
        """4,282 `lbl_*` names would otherwise mint the single key `lbl`."""
        table = ra.relocation_addresses(SYMBOLS)
        self.assertNotIn("lbl", table)
        self.assertNotIn("jumptable", table)


class ResolveOneSymbol(unittest.TestCase):
    def setUp(self):
        self.bases = ra.section_bases("game/enemy/enemy", RUNS)
        self.addresses = ra.relocation_addresses(SYMBOLS)

    def resolve(self, name):
        return ra.resolve_relocation_symbol(name, self.bases, self.addresses)

    def test_an_anonymous_entry_resolves_to_base_plus_offset(self):
        self.assertEqual(self.resolve(["@", ".sdata2", 572, 4, 1, 0]),
                         0x80346A4C)
        self.assertEqual(self.resolve(("@", ".data", 612, 32, 1, 0)),
                         0x8011C25C)

    def test_an_anonymous_entry_in_an_unclaimed_section_is_None(self):
        """Fail-closed: an unclaimed datum equals nothing by address."""
        self.assertIsNone(self.resolve(["@", ".rodata", 0, 4, 1, 0]))

    def test_a_named_symbol_resolves_and_an_unknown_one_does_not(self):
        self.assertEqual(self.resolve("gEnemyTable"), 0x80250E00)
        self.assertIsNone(self.resolve("no_such_symbol"))

    def test_a_malformed_entry_is_None_not_an_exception(self):
        self.assertIsNone(self.resolve(["@"]))
        self.assertIsNone(self.resolve(["notat", ".sdata2", 0]))


class TableComparison(unittest.TestCase):
    """The whole point: equal bindings pass, wrong ones still fail."""

    def setUp(self):
        self.bases = ra.section_bases("game/enemy/enemy", RUNS)
        self.addresses = ra.relocation_addresses(SYMBOLS)

    def resolve(self, rows):
        return ra.resolve_relocations(rows, self.bases, self.addresses)

    def test_one_datum_spelled_two_ways_compares_equal(self):
        ours = [(0x50, SDA21, ["@", ".sdata2", 572, 4, 1, 0], 0)]
        target = [(0x50, SDA21, "lbl_80346A4C", 0)]
        self.assertNotEqual(ours, target)          # by NAME: the old FAIL
        self.assertEqual(self.resolve(ours), self.resolve(target))

    def test_a_MIS_BOUND_relocation_still_fails(self):
        """The negative side: the neighbouring pool entry, not this one."""
        ours = [(0x50, SDA21, ["@", ".sdata2", 576, 4, 1, 0], 0)]
        target = [(0x50, SDA21, "lbl_80346A4C", 0)]
        self.assertNotEqual(self.resolve(ours), self.resolve(target))

    def test_a_changed_ADDEND_still_fails(self):
        ours = [(0x50, SDA21, ["@", ".sdata2", 572, 4, 1, 0], 4)]
        target = [(0x50, SDA21, "lbl_80346A4C", 0)]
        self.assertNotEqual(self.resolve(ours), self.resolve(target))

    def test_a_changed_OFFSET_or_TYPE_still_fails(self):
        base = [(0x50, SDA21, "gEnemyTable", 0)]
        self.assertNotEqual(self.resolve(base),
                            self.resolve([(0x54, SDA21, "gEnemyTable", 0)]))
        self.assertNotEqual(self.resolve(base),
                            self.resolve([(0x50, 4, "gEnemyTable", 0)]))

    def test_two_unresolvable_names_are_compared_verbatim(self):
        a = [(0x50, SDA21, "unknown_one", 0)]
        b = [(0x50, SDA21, "unknown_two", 0)]
        self.assertNotEqual(self.resolve(a), self.resolve(b))
        self.assertEqual(self.resolve(a), self.resolve(list(a)))

    def test_a_resolvable_name_never_equals_an_unresolvable_one(self):
        ours = [(0x50, SDA21, ["@", ".rodata", 0, 4, 1, 0], 0)]
        target = [(0x50, SDA21, "lbl_80346A4C", 0)]
        self.assertNotEqual(self.resolve(ours), self.resolve(target))


class LiveGendir(unittest.TestCase):
    """The function the item names, against the built objects."""

    UNIT, FN = "game/enemy/enemy", "gendir_8004FBC8"

    @classmethod
    def setUpClass(cls):
        target = ROOT / "build/GUNE5D/obj/game/enemy/enemy.o"
        if not target.exists():
            raise unittest.SkipTest("enemy objects are not built")
        # Old .postprocess/body objects can survive retirement and carry the
        # previous .data offsets. Resolve the active Ninja compiler edge so
        # the object and the current ownership map describe the same build.
        raw = resolve_object(cls.UNIT, root=ROOT, view="compiler").path
        if not raw.exists():
            raise unittest.SkipTest("enemy objects are not built")
        os.chdir(ROOT)
        cls.ours = ra.normalize_relocations(
            ra.object_image(raw)["functions"][cls.FN]["relocations"], "ours")
        cls.target = ra.normalize_relocations(
            ra.object_image(target)["functions"][cls.FN]["relocations"],
            "target")

    def test_the_two_tables_disagree_on_NAMES(self):
        """The observation itself, so the fix cannot become untestable."""
        self.assertEqual(len(self.ours), len(self.target))
        self.assertNotEqual(self.ours, self.target)

    def test_the_two_tables_agree_on_ADDRESSES(self):
        bases = ra.section_bases(self.UNIT)
        addresses = ra.relocation_addresses()
        ours = ra.resolve_relocations(self.ours, bases, addresses)
        target = ra.resolve_relocations(self.target, bases, addresses)
        self.assertEqual(ours, target)
        self.assertTrue(ours, "an empty table is not a certificate")
        unresolved = [n for _o, _k, n, _a in ours + target
                      if not (isinstance(n, list) and n[0] == "address")]
        self.assertEqual(unresolved, [])

    def test_offsets_types_and_addends_were_already_identical(self):
        self.assertEqual([(o, k, a) for o, k, _n, a in self.ours],
                         [(o, k, a) for o, k, _n, a in self.target])


if __name__ == "__main__":
    unittest.main()
