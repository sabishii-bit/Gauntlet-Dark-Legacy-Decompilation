"""Named target symbols, dtk's symbol-boundary rule, and the size sequence.

Run-53 item 5. Three findings, each reproduced at c7b741799 before any edit.

1. `fndiff.symbol_table()` returns (SECTION, ADDRESS, SIZE) and the census
   read `entry[0]` — the section STRING — then discarded the row because it
   was not an int. EVERY relocation naming a real target symbol was dropped
   and only the `lbl_XXXXXXXX` spelling ever resolved: on game/enemy/enemy,
   274 data-section relocation pairs, 220 lbl_ and **54 named (20%) silently
   discarded**, among them `ours @692 -> target jumptable_8011C0EC`, the pair
   that decides the .data base.

2. The `.bss` PASTE line ended at the object's own size, and dtk refused it:
   `Split game/enemy/enemy.c .bss (0x80250E00..0x8025758C) ends within symbol
   'gEnemies' (0x80251C18..0x80257590)`. The correct end was in the refusal.

3. DESIGN REVERSAL on the item's own second half. It asked for the
   size-sequence procedure a lane ran by hand for enemy.c's `.data`, and
   after (1) that base falls out of the relocations mechanically, matching
   the landed claim (`.data start:0x8011C0EC end:0x8011C27C`) exactly. The
   procedure ships as ADVISORY and never as a paste line, because a
   paste-ready line built on thin evidence is wrong and confident: measured,
   the OLD tool printed `.sbss start:0x80344758` for game/boss/boss on the
   strength of ONE relocation row, while the shipped verified claim is
   `start:0x80344378` — off by 0x3E0.

TWO-SIDED over all 92 built game/ units: 47 (unit, section) verdicts improve
(mostly nothing -> a paste-ready claim); 2 go SINGLE -> MULTI
(game/boss/boss .sbss and game/world/gauntworld .data) and BOTH are
corrections — boss's old single base is the wrong one above. 16 pairs still
resolve nothing from relocations, which is the size-sequence fallback's
customer count.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import af_data_base_census as af  # noqa: E402
import fndiff  # noqa: E402


class StraddledSymbolTests(unittest.TestCase):
    def test_an_end_inside_a_symbol_is_reported_with_its_boundary(self):
        """The exact case dtk refused, read from the live symbol table."""
        hit = af.straddled_symbol(".bss", 0x8025758C)
        self.assertIsNotNone(hit, "gEnemies must cover 0x8025758C")
        name, start, end = hit
        self.assertEqual(name, "gEnemies")
        self.assertEqual(start, 0x80251C18)
        self.assertEqual(end, 0x80257590)

    def test_an_end_exactly_on_a_boundary_is_not_straddled(self):
        """A split ending where a symbol ends is legal; rounding it further
        would invent slack dtk never asked for."""
        self.assertIsNone(af.straddled_symbol(".bss", 0x80257590))

    def test_the_section_must_match(self):
        self.assertIsNone(af.straddled_symbol(".data", 0x8025758C))


class NamedTargetSymbolTests(unittest.TestCase):
    def test_the_symbol_table_shape_is_section_address_size(self):
        """The shape the old `entry[0]` read wrongly. If this ever returns a
        bare int again, the census's unpacking must be revisited."""
        entry = fndiff.symbol_table()["gEnemies"]
        self.assertEqual(entry[0], ".bss")
        self.assertEqual(entry[1], 0x80251C18)
        self.assertEqual(entry[2], 0x5978)

    def test_enemy_data_and_bss_resolve_to_the_landed_claims(self):
        """End to end against config/GUNE5D/splits.txt's own committed lines.

        Both were derived by hand once; both are mechanical now.
        """
        sections = af.census("game/enemy/enemy")["sections"]
        self.assertEqual(sections[".data"]["splits_line"].split(),
                         [".data", "start:0x8011C0EC", "end:0x8011C27C"])
        self.assertEqual(sections[".bss"]["splits_line"].split(),
                         [".bss", "start:0x80250E00", "end:0x80257590"])
        self.assertEqual(sections[".bss"]["end_rounded_up_to"], "gEnemies")
        self.assertEqual(sections[".bss"]["object_end"], "0x8025758C")


class SizeSequenceTests(unittest.TestCase):
    def test_a_consecutive_run_of_matching_sizes_yields_the_base(self):
        osyms = {"@1": (".data", 0x00, 0x24),
                 "@2": (".data", 0x24, 0x20),
                 "@3": (".data", 0x44, 0xA8)}
        hits = af.size_sequence_candidates(osyms, ".data", 0xEC)
        self.assertEqual([hit["base"] for hit in hits], ["0x80120CB0"])
        self.assertEqual(hits[0]["symbols"],
                         ["jumptable_80120CB0", "jumptable_80120CD4",
                          "jumptable_80120CF4"])

    def test_a_wrong_size_sequence_matches_nothing(self):
        osyms = {"@1": (".data", 0x00, 0x24),
                 "@2": (".data", 0x24, 0x21),
                 "@3": (".data", 0x45, 0xA8)}
        self.assertEqual(af.size_sequence_candidates(osyms, ".data", 0xEC), [])

    def test_an_empty_object_section_yields_nothing(self):
        self.assertEqual(af.size_sequence_candidates({}, ".data", 0xEC), [])

    def test_it_never_becomes_a_splits_line(self):
        """The narrowing this item's calibration forced: advisory only."""
        sections = af.census("game/game/pmotion")["sections"]
        data = sections[".data"]
        self.assertEqual(data["candidate_bases"], {})
        self.assertNotIn("splits_line", data)
        self.assertTrue(data["size_sequence_candidates"])


if __name__ == "__main__":
    unittest.main()
