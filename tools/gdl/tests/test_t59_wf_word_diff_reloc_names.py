#!/usr/bin/env python3
"""T3 run-59 item 5: the relocation screen was blind to dtk-suffixed names.

THE OBSERVATION. `wf_word_diff` speaks ELF names everywhere — `unit_bodies`
returns them, `webfrank.json` spells them, the per-function CLI takes them —
while `fndiff.parse` keys its line table on the REDUCED name. `_reloc_map`
did a plain `.get(fn)`, so for a dtk-suffixed function it missed and the
headline read

    game/enemy/enemy::gendir_8004FBC8 ... RELOC-SYMBOL MISMATCH = not
    comparable

at 4f3f9c6f0. That is the wrong-symbol class (claim.law.SA_a-wrong-global-
that-shares-an-instruction-word-is-invisible-to-every-score) going
UNSCREENED, and in `--unit` mode it was worse than visible: the relocation
TYPES were simply absent, so a word a relocation patches was decoded as a
codegen difference with no notice at all.

THE CURE is at the root — `fndiff.resolve_function_name` from run-59 item
4 — so the per-function mode, the `--unit` mode and every other reader
join on one convention.

LIVE CALIBRATION (build/t3_scratch/t3_item5_calibration.py, before/after
4f3f9c6f0): of 2,870 paired functions image-wide, 22 had NO relocation
table under the old lookup; the resolver recovers all 22 and leaves 0
unpairable, and none of the 22 carries a real symbol mismatch — the fix
closes a BLIND SPOT, it does not fix a current miscall. Both modes agree
on both calibration units: game/enemy/enemy 84 rows / 0 decode
disagreements / 84 screened / 0 not comparable, and
game/movie/movieplayer 52 rows / 0 disagreements / 51 screened (the 52nd,
DTextInitColorRamp, is COUNT-ASYMMETRIC and has no words to screen).

The NEGATIVE side stays negative: a function genuinely absent, and a
count that genuinely disagrees, still return None so the caller prints
"not comparable" rather than a silent 0.
"""

import os
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import wf_word_diff as wd                                    # noqa: E402

# One instruction, one relocation line, as fndiff.parse emits them.
LINES = ["lis     r3,0",
         "    R_PPC_ADDR16_HA\tgEnemyTable",
         "blr"]


class ParsedLines(unittest.TestCase):
    """The join, over hand-built tables — no ELF, no build."""

    def test_a_suffixed_query_finds_the_reduced_key(self):
        self.assertEqual(
            wd.parsed_lines({"gendir": LINES}, "gendir_8004FBC8"), LINES)

    def test_a_reduced_query_finds_a_suffixed_key(self):
        self.assertEqual(
            wd.parsed_lines({"gendir_8004FBC8": LINES}, "gendir"), LINES)

    def test_an_exact_key_still_wins(self):
        other = ["blr"]
        table = {"gendir": other, "gendir_8004FBC8": LINES}
        self.assertEqual(wd.parsed_lines(table, "gendir_8004FBC8"), LINES)
        self.assertEqual(wd.parsed_lines(table, "gendir"), other)

    def test_an_absent_function_is_still_None(self):
        """A measurement that did not happen must not read as zero."""
        self.assertIsNone(wd.parsed_lines({"gendir": LINES}, "no_such_fn"))

    def test_an_ambiguous_base_is_still_None(self):
        table = {"dtor_800DB21C": LINES, "dtor_800DBB94": LINES}
        self.assertIsNone(wd.parsed_lines(table, "dtor"))

    def test_a_placeholder_name_is_not_reduced_onto_a_neighbour(self):
        table = {"fn_800516F8": LINES}
        self.assertEqual(wd.parsed_lines(table, "fn_800516F8"), LINES)
        self.assertIsNone(wd.parsed_lines(table, "fn_80099999"))


class RelocMapFromLines(unittest.TestCase):
    """The count check `parsed_lines` must not weaken."""

    def test_a_paired_table_maps_the_relocation_onto_its_instruction(self):
        self.assertEqual(wd._reloc_map_from_lines(LINES, 2),
                         {0: ("R_PPC_ADDR16_HA", "gEnemyTable")})

    def test_a_count_disagreement_is_None_not_an_empty_map(self):
        self.assertIsNone(wd._reloc_map_from_lines(LINES, 3))

    def test_absent_lines_are_None(self):
        self.assertIsNone(wd._reloc_map_from_lines(None, 2))


class LiveScreen(unittest.TestCase):
    """The function the observation names, and the two-unit agreement."""

    @classmethod
    def setUpClass(cls):
        if not (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").exists():
            raise unittest.SkipTest("checkout is not built")
        os.chdir(ROOT)

    def test_the_suffixed_pin_is_screened_not_not_comparable(self):
        ours = wd.unit_bodies(wd.our_object("game/enemy/enemy")[0])
        target = wd.unit_bodies(wd.target_object("game/enemy/enemy"))
        rows = wd.reloc_symbol_mismatches(
            "game/enemy/enemy", "gendir_8004FBC8",
            ours["gendir_8004FBC8"], target["gendir_8004FBC8"])
        self.assertIsNotNone(rows, "gendir_8004FBC8 is UNSCREENED again")
        self.assertEqual(rows, [])

    def test_both_modes_decode_the_same_words_on_both_units(self):
        """The two INDEPENDENT paths: `unit_rows`'s hoisted tables against
        `_reloc_map`'s per-function ones, over every measured row.

        The object paths are resolved ONCE per unit rather than per
        function: `cn_analyze.our_object` goes through
        `raw_object.resolve_object`, which re-hashes build.ninja and all
        seven generator inputs on every call — 0.106 s a call, measured
        with build/t3_scratch/t3_resolve_cost.py — and 272 of those turned
        this test into a 31 s one for nothing it screens.
        """
        for unit in ("game/enemy/enemy", "game/movie/movieplayer"):
            rows, _kind = wd.unit_rows(unit)
            ours_path = wd.our_object(unit)[0]
            target_path = wd.target_object(unit)
            ours_all = wd.unit_bodies(ours_path)
            target_all = wd.unit_bodies(target_path)
            measured = 0
            for row in rows:
                if row["verdict"] != "MEASURED":
                    continue
                measured += 1
                name = row["function"]
                ours, tgt = ours_all[name], target_all[name]
                count = len(ours) // 4
                diffs = [(o, wd.wf._u32(ours, o), wd.wf._u32(tgt, o))
                         for o in range(0, len(ours), 4)
                         if wd.wf._u32(ours, o) != wd.wf._u32(tgt, o)]
                types = {}
                for path in (target_path, ours_path):
                    table = wd._reloc_map(path, name, count)
                    self.assertIsNotNone(
                        table, f"{unit}::{name} has NO relocation table")
                    for index, (rtype, _sym) in table.items():
                        types.setdefault(index, set()).add(rtype)
                single = wd.decode_counts(
                    diffs,
                    {i: tuple(sorted(t)) for i, t in types.items()})
                self.assertEqual(single, row["decode"], f"{unit}::{name}")
            self.assertGreater(measured, 0, unit)

    def test_reloc_types_by_index_pairs_both_objects_for_a_suffixed_name(self):
        """The unit-taking wrapper, on the function the item names."""
        types = wd.reloc_types_by_index(
            "game/enemy/enemy", "gendir_8004FBC8", 78)
        self.assertTrue(types, "no relocation types for a suffixed name")


if __name__ == "__main__":
    unittest.main()
