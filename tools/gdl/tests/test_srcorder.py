"""Two-sided tests for tools/gdl/composed_census/srcorder.py.

The pure half is calibrated on hand-built module rosters whose right answer
is readable from the test itself: a descending order, an ascending one, both
one-sided edges, and the refusals (no module, two modules, a module that
shares no name). The live half is calibrated on two units with DIFFERENT
shapes, so a passing suite means the tool tells them apart:

    game/game/player   NonMatching, 47 module names absent, 2 GC-only names,
                       67 down / 22 up steps — the unit the screen exists for
    game/mb/mb_tree    Matching (its emission order is proven by the link),
                       25 of 25 GC functions paired, NO GC-only name

Both read only the EXTRACTED target object build/GUNE5D/obj/<unit>.o, which
the split produces and ninja does not rebuild from source, so neither live
case depends on a source-built object of a NonMatching unit. They skip when
that object or the Xbox roster is absent rather than failing a checkout that
has not been split.

Reproduced at 9c4412023 with
    python tools/gdl/composed_census/srcorder.py game/game/player --quiet
"""

import os
import sys
import tempfile
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(GDL))
sys.path.insert(0, str(GDL / "composed_census"))

import srcorder  # noqa: E402

PLAYER_OBJ = ROOT / "build" / "GUNE5D" / "obj" / "game" / "game" / "player.o"
MBTREE_OBJ = ROOT / "build" / "GUNE5D" / "obj" / "game" / "mb" / "mb_tree.o"
ROSTER = ROOT / "research" / "xbox_symbols" / "functions_by_module.txt"


def live_ready(objfile):
    return objfile.exists() and ROSTER.exists()


ROSTER_TEXT = """\
# a header line the reader must ignore
== .\\Release\\DEMO.OBJ (.\\Release\\DEMO.OBJ)
[0001:00000000]     10 G alpha
[0001:00000010]     20 L beta
[0001:00000030]     30 G gamma
[0001:00000060]     40 L delta
[0001:00000090]     50 G epsilon
[0008:00001270]      0 D kTable

== .\\Release\\OTHER.OBJ (.\\Release\\OTHER.OBJ)
[0001:00000000]      8 G unrelated_one
[0001:00000010]      8 G unrelated_two
"""


class RosterTests(unittest.TestCase):
    def roster(self, text=ROSTER_TEXT):
        folder = Path(tempfile.mkdtemp())
        path = folder / "functions_by_module.txt"
        path.write_text(text, encoding="utf-8")
        return path

    def test_rows_are_read_per_module_with_their_kind(self):
        modules = srcorder.parse_modules(self.roster())
        self.assertEqual(sorted(modules), [".\\Release\\DEMO.OBJ",
                                           ".\\Release\\OTHER.OBJ"])
        demo = modules[".\\Release\\DEMO.OBJ"]
        self.assertEqual(len(demo), 6)
        self.assertEqual(demo[0], (1, 0x0, 0x10, "G", "alpha"))
        self.assertEqual(demo[-1], (8, 0x1270, 0, "D", "kTable"))

    def test_a_missing_roster_refuses(self):
        with self.assertRaises(srcorder.Refused):
            srcorder.parse_modules(Path(tempfile.mkdtemp()) / "absent.txt")

    def test_module_index_keeps_code_only_and_reports_no_resort(self):
        modules = srcorder.parse_modules(self.roster())
        code, by_name, resorted = srcorder.module_index(
            modules[".\\Release\\DEMO.OBJ"])
        self.assertEqual([row[4] for row in code],
                         ["alpha", "beta", "gamma", "delta", "epsilon"])
        self.assertEqual(by_name["gamma"], 2)
        self.assertFalse(resorted)

    def test_module_index_flags_a_file_that_was_not_in_offset_order(self):
        shuffled = ROSTER_TEXT.replace(
            "[0001:00000030]     30 G gamma\n", "")
        shuffled = shuffled.replace(
            "[0001:00000090]     50 G epsilon\n",
            "[0001:00000090]     50 G epsilon\n"
            "[0001:00000030]     30 G gamma\n")
        modules = srcorder.parse_modules(self.roster(shuffled))
        code, by_name, resorted = srcorder.module_index(
            modules[".\\Release\\DEMO.OBJ"])
        self.assertTrue(resorted)
        self.assertEqual(by_name["gamma"], 2)


class ChoiceTests(unittest.TestCase):
    MODULES = {".\\Release\\DEMO.OBJ": [], ".\\Release\\demo.obj": [],
               ".\\Release\\SOLO.OBJ": []}

    def test_a_unique_basename_is_chosen(self):
        name, why = srcorder.choose_module(self.MODULES, "game/x/solo")
        self.assertEqual(name, ".\\Release\\SOLO.OBJ")
        self.assertIn("basename", why)

    def test_two_modules_with_one_basename_refuse_rather_than_guess(self):
        with self.assertRaises(srcorder.Refused) as caught:
            srcorder.choose_module(self.MODULES, "game/x/demo")
        self.assertIn("--module", str(caught.exception))

    def test_an_unknown_basename_refuses(self):
        with self.assertRaises(srcorder.Refused):
            srcorder.choose_module(self.MODULES, "game/x/nothing")

    def test_an_explicit_exact_name_wins_over_the_basename_collision(self):
        name, _why = srcorder.choose_module(self.MODULES, "game/x/demo",
                                            ".\\Release\\demo.obj")
        self.assertEqual(name, ".\\Release\\demo.obj")

    def test_an_explicit_name_that_matches_nothing_refuses(self):
        with self.assertRaises(srcorder.Refused):
            srcorder.choose_module(self.MODULES, "game/x/solo", "NOPE.OBJ")

    def test_an_explicit_substring_matching_several_refuses_with_a_sample(self):
        with self.assertRaises(srcorder.Refused) as caught:
            srcorder.choose_module(self.MODULES, "game/x/solo", "Release")
        self.assertIn("matches 3 modules", str(caught.exception))


class RunTests(unittest.TestCase):
    def rows(self, pairs):
        return [(index, name, xbox) for index, (name, xbox)
                in enumerate(pairs)]

    def test_a_descending_run_is_found_and_a_single_step_is_not_a_run(self):
        rows = self.rows([("a", 9), ("b", 7), ("c", 5), ("d", 6)])
        runs = srcorder.monotone_runs(rows)
        self.assertEqual(len(runs), 1)
        direction, run = runs[0]
        self.assertEqual(direction, -1)
        self.assertEqual([row[1] for row in run], ["a", "b", "c"])

    def test_unpaired_names_are_skipped_not_treated_as_breaks(self):
        rows = self.rows([("a", 9), ("gconly", None), ("b", 7), ("c", 5)])
        direction, run = srcorder.monotone_runs(rows)[0]
        self.assertEqual(direction, -1)
        self.assertEqual([row[1] for row in run], ["a", "b", "c"])

    def test_step_census_counts_every_direction(self):
        rows = self.rows([("a", 9), ("b", 7), ("c", 8), ("d", 8)])
        self.assertEqual(srcorder.step_census(rows),
                         {"pairs": 3, "down": 1, "up": 1, "flat": 1})


class BoundTests(unittest.TestCase):
    """xbox 0..4 = alpha beta gamma delta epsilon; the GC file emits the
    survivors in DESCENDING xbox order, so an absent name must sit between
    its higher neighbour's slot and its lower neighbour's slot."""

    CODE = [(1, 0x00, 0x10, "G", "alpha"), (1, 0x10, 0x20, "L", "beta"),
            (1, 0x30, 0x30, "G", "gamma"), (1, 0x60, 0x40, "L", "delta"),
            (1, 0x90, 0x50, "G", "epsilon")]
    BY_NAME = {"alpha": 0, "beta": 1, "gamma": 2, "delta": 3, "epsilon": 4}

    def test_descending_emission_bounds_an_absent_name_between_neighbours(self):
        gc = ["epsilon", "delta", "alpha"]          # gamma and beta absent
        found = {row["name"]: row
                 for row in srcorder.bound_absent(self.CODE, self.BY_NAME, gc)}
        self.assertEqual(sorted(found), ["beta", "gamma"])
        gamma = found["gamma"]
        self.assertEqual(gamma["orientation"], "descending")
        self.assertEqual(gamma["bound"]["after"], "delta")
        self.assertEqual(gamma["bound"]["before"], "alpha")
        self.assertEqual(gamma["width"], 1)
        self.assertEqual(gamma["inside"], [])
        self.assertEqual(gamma["size"], 0x30)
        self.assertEqual(gamma["binding"], "G")

    def test_ascending_emission_reverses_the_bound(self):
        gc = ["alpha", "delta", "epsilon"]
        found = {row["name"]: row
                 for row in srcorder.bound_absent(self.CODE, self.BY_NAME, gc)}
        gamma = found["gamma"]
        self.assertEqual(gamma["orientation"], "ascending")
        self.assertEqual(gamma["bound"]["after"], "alpha")
        self.assertEqual(gamma["bound"]["before"], "delta")
        self.assertEqual(gamma["width"], 1)

    def test_a_wide_bound_lists_the_gc_functions_it_still_contains(self):
        gc = ["epsilon", "gamma", "beta", "alpha"]     # delta absent
        found = {row["name"]: row
                 for row in srcorder.bound_absent(self.CODE, self.BY_NAME, gc)}
        delta = found["delta"]
        self.assertEqual(delta["width"], 1)
        gc = ["epsilon", "beta", "alpha"]              # delta and gamma absent
        found = {row["name"]: row
                 for row in srcorder.bound_absent(self.CODE, self.BY_NAME, gc)}
        self.assertEqual(found["delta"]["width"], 1)
        self.assertEqual(found["delta"]["inside"], [])

    def test_an_edge_name_is_one_sided_and_never_widened_into_a_claim(self):
        gc = ["delta", "gamma", "beta"]      # alpha (low edge) absent
        found = {row["name"]: row
                 for row in srcorder.bound_absent(self.CODE, self.BY_NAME, gc)}
        self.assertEqual(found["alpha"]["orientation"], "one-sided")
        self.assertIsNone(found["alpha"]["bound"])
        self.assertEqual(found["epsilon"]["orientation"], "one-sided")
        self.assertIsNone(found["epsilon"]["bound"])

    def test_gc_only_names_are_the_ones_the_module_does_not_carry(self):
        gc = ["epsilon", "invented", "alpha"]
        self.assertEqual(srcorder.gc_only(gc, self.BY_NAME), [(1, "invented")])


class LivePlayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not live_ready(PLAYER_OBJ):
            raise unittest.SkipTest("no split game/game/player target object")
        cls.record = srcorder.survey("game/game/player")

    def test_the_module_is_identified_from_the_unit_basename_alone(self):
        self.assertEqual(self.record["module"], ".\\Release\\PLAYER.OBJ")
        self.assertEqual(self.record["gc_functions"], 92)
        self.assertEqual(self.record["module_functions"], 137)
        self.assertEqual(self.record["paired"], 90)

    def test_the_emission_order_is_predominantly_descending(self):
        # Integration 64-7 gave our check_player_atts / PlayerUpdateAtts
        # their PDB identities (the names were swapped), which aligns two
        # more GC/Xbox pairs: 67/22 -> 69/20.
        self.assertEqual(self.record["steps"],
                         {"pairs": 89, "down": 69, "up": 20, "flat": 0})

    def test_the_sdata2_creator_pair_is_bounded_to_one_interior_slot(self):
        found = {row["name"]: row for row in self.record["absent"]}
        for name, xbox in (("power_bar_state", 101),
                           ("hide_power_meter", 102)):
            row = found[name]
            self.assertEqual(row["xbox_index"], xbox)
            self.assertEqual(row["orientation"], "descending")
            self.assertEqual(row["bound"]["after"], "draw_power_meter")
            self.assertEqual(row["bound"]["after_gc_index"], 12)
            self.assertEqual(row["bound"]["before"], "get_display_mode")
            self.assertEqual(row["bound"]["before_gc_index"], 14)
            self.assertEqual(row["width"], 2)
            self.assertEqual(row["inside"], ["setup_player_display"])

    def test_the_two_gc_only_names_are_reported(self):
        self.assertEqual([name for _index, name in self.record["gc_only"]],
                         ["create_player_blits", "do_got_it_8007FC80"])

    def test_a_pinned_bound_is_the_common_case_not_a_rarity(self):
        pinned = [row for row in self.record["absent"] if row["width"] == 1]
        self.assertEqual(len(self.record["absent"]), 47)
        self.assertEqual(len(pinned), 25)   # 24 before the 64-7 name swap

    def test_the_wrong_module_is_refused_rather_than_tabulated(self):
        with self.assertRaises(srcorder.Refused) as caught:
            srcorder.survey("game/game/player", module="ZUTIL.OBJ")
        self.assertIn("shares NO function name", str(caught.exception))


class LiveExactControlTests(unittest.TestCase):
    """game/mb/mb_tree is Matching: the link proves its emission order, so
    the descending-run structure it shows is not an artefact of a broken
    unit."""

    @classmethod
    def setUpClass(cls):
        if not live_ready(MBTREE_OBJ):
            raise unittest.SkipTest("no split game/mb/mb_tree target object")
        cls.record = srcorder.survey("game/mb/mb_tree")

    def test_every_gc_function_pairs_and_none_is_gc_only(self):
        self.assertEqual(self.record["module"], ".\\Release\\MB_TREE.OBJ")
        self.assertEqual(self.record["gc_functions"], 25)
        self.assertEqual(self.record["paired"], 25)
        self.assertEqual(self.record["gc_only"], [])

    def test_the_exact_unit_shows_the_same_descending_structure(self):
        self.assertEqual(self.record["steps"],
                         {"pairs": 24, "down": 19, "up": 5, "flat": 0})
        self.assertTrue(any(run["direction"] == "descending"
                            and run["length"] >= 5
                            for run in self.record["runs"]))

    def test_its_absent_names_are_bounded_the_same_way(self):
        found = {row["name"]: row for row in self.record["absent"]}
        row = found["MBNodeSetEmpty"]
        self.assertEqual(row["orientation"], "descending")
        self.assertEqual(row["bound"]["after"], "MBNodeSetParent")
        self.assertEqual(row["bound"]["before"], "MBNewNode")


class CommandLineTests(unittest.TestCase):
    def test_an_absent_unit_refuses_with_exit_2_and_no_traceback(self):
        self.assertEqual(srcorder.main(["game/game/no_such_unit"]),
                         srcorder.REFUSED)

    def test_a_live_unit_returns_zero(self):
        if not live_ready(PLAYER_OBJ):
            self.skipTest("no split game/game/player target object")
        self.assertEqual(srcorder.main(["game/game/player", "--quiet"]),
                         srcorder.OK)


if __name__ == "__main__":
    unittest.main()
