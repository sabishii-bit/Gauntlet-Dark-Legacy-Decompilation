"""Keep the executable/PDB-backed CRITTER/DBMODES boundary reconstruction.

These are ownership regressions, not proof that CRITTER's native source is
exact. The GC initializer clears only critter objects and calls HealthMeterInit;
the named PS2 CritterInit and Xbox CRITTER.OBJ corroborate its identity. The two
following retail blr hooks are independently identified by DBMODES.OBJ and
their ordered callers in main. Boundary negatives protect neighboring owners.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/gdl"))
import pool_owner


class CritterOwnership(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runs = pool_owner.load_splits(ROOT / "config/GUNE5D/splits.txt")

    def owner(self, section, address):
        return pool_owner.owner_of(self.runs, section, address)

    def test_initializer_and_final_return_belong_to_critter(self):
        for address in (0x8004229C, 0x80042390):
            self.assertEqual(self.owner(".text", address), "game/enemy/critter")

    def test_debug_hooks_do_not_belong_to_critter_or_audio(self):
        for address in (0x80042394, 0x80042398):
            self.assertEqual(self.owner(".text", address), "game/sys/dbmodes")
        self.assertEqual(self.owner(".text", 0x8004239C), "game/audio/soundmgr")

    def test_initializer_exception_records_move_with_the_function(self):
        self.assertEqual(self.owner("extab", 0x80005F28), "game/enemy/critter")
        self.assertEqual(self.owner("extabindex", 0x8000970C), "game/enemy/critter")
        self.assertEqual(self.owner("extab", 0x80005F30), "game/audio/soundmgr")
        self.assertEqual(self.owner("extabindex", 0x80009718), "game/audio/soundmgr")

    def test_critter_constant_pool_does_not_absorb_either_neighbor(self):
        self.assertEqual(self.owner(".sdata2", 0x80346468), "game/game/controls")
        for address in (0x80346470, 0x8034669F):
            self.assertEqual(self.owner(".sdata2", address), "game/enemy/critter")
        self.assertNotEqual(self.owner(".sdata2", 0x803466A0), "game/enemy/critter")

    def test_small_globals_stop_before_audio(self):
        for section, start, last, neighbor in (
            (".sdata", 0x80343BE8, 0x80343BEF, 0x80343BF0),
            (".sbss", 0x80344628, 0x8034466F, 0x80344670),
        ):
            with self.subTest(section=section):
                self.assertEqual(self.owner(section, start), "game/enemy/critter")
                self.assertEqual(self.owner(section, last), "game/enemy/critter")
                self.assertNotEqual(self.owner(section, neighbor), "game/enemy/critter")

    def test_type_table_has_no_invented_alignment_member(self):
        symbols = (ROOT / "config/GUNE5D/symbols.txt").read_text()
        match = re.search(r"^gCritterHeaders = \.bss:0x8024C004;.*size:0x([0-9A-Fa-f]+)",
                          symbols, re.MULTILINE)
        self.assertIsNotNone(match)
        self.assertEqual(int(match[1], 16), 9 * 6 * 4)
        self.assertEqual(self.owner(".bss", 0x8024C0DB), "game/enemy/critter")
        self.assertNotEqual(self.owner(".bss", 0x8024C0DC), "game/enemy/critter")
        self.assertNotEqual(self.owner(".bss", 0x8024C0E0), "game/enemy/critter")

    def test_original_hook_names_replace_audio_placeholders(self):
        audio = (ROOT / "src/game/audio/soundmgr.c").read_text()
        hooks = (ROOT / "src/game/sys/dbmodes.c").read_text()
        for name in ("DoFingerLoModes", "CheckFingerLoModes"):
            self.assertRegex(hooks, rf"void {name}\(void\)\s*\{{\s*\}}")
        self.assertNotRegex(audio, r"void\s+sndSys(?:Init|Stub0|Stub1)\s*\(")


if __name__ == "__main__":
    unittest.main()
