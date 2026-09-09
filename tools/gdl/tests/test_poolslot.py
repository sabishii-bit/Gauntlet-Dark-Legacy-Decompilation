"""Two-sided tests for tools/gdl/composed_census/poolslot.py.

The pure half builds the three shapes the classifier must tell apart on
hand-made datum runs whose right answer is readable from the test: a
bracketed gap whose referrers all sit OUTSIDE the bracket (the
any_player_walking class), one whose referrer sits INSIDE it (the
StartCompass class), and one with no referrer at all — plus the two ways a
bracket legitimately fails (a non-monotone surviving order, and no
attributed datum above the gap).

The live half is calibrated on two units with opposite shapes:

    game/game/player .sdata2   NonMatching, 3 DOL-side gaps, exactly one of
                               them bracketed — the case the screen exists for
    game/mb/mb_tree  .sdata2   Matching and claimed: NO gap at all, which is
                               the negative control for the whole pipeline

The player case reads build/GUNE5D/src/game/game/player.o, a source-built
object of a NonMatching unit; it skips when that object is absent and the
unit is already in the CI refresh `rm -f` list in .github/workflows/build.yml.

Reproduced at a386f2d48 with
    python tools/gdl/composed_census/poolslot.py game/game/player .sdata2
"""

import sys
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(GDL))
sys.path.insert(0, str(GDL / "composed_census"))

import poolslot  # noqa: E402

PLAYER_SRC = ROOT / "build" / "GUNE5D" / "src" / "game" / "game" / "player.o"
PLAYER_OBJ = ROOT / "build" / "GUNE5D" / "obj" / "game" / "game" / "player.o"
MBTREE_SRC = ROOT / "build" / "GUNE5D" / "src" / "game" / "mb" / "mb_tree.o"
ROSTER = ROOT / "research" / "xbox_symbols" / "functions_by_module.txt"


class RelocTests(unittest.TestCase):
    LINES = ["lis r3,0",
             "    R_PPC_ADDR16_HA\tlbl_80347820",
             "lfd f1,0(r3)",
             "    R_PPC_EMB_SDA21\tlbl_80347838+0x4",
             "blr"]

    def test_symbols_and_addends_are_both_read(self):
        self.assertEqual(poolslot.reloc_targets(self.LINES),
                         [("lbl_80347820", 0), ("lbl_80347838", 4)])

    def test_a_body_with_no_relocation_yields_nothing(self):
        self.assertEqual(poolslot.reloc_targets(["blr", "nop"]), [])


class BracketTests(unittest.TestCase):
    #     addr        name             size
    DATUMS = [(0x100, "below_two", 4), (0x104, "below_one", 4),
              (0x108, "in_gap", 4), (0x10C, "above_one", 4),
              (0x110, "above_two", 4)]
    GAP = {"address": 0x108, "size": 4, "our_offset": 0x8, "content": ""}

    def test_the_two_neighbouring_creators_bracket_the_gap(self):
        first = {0x100: (2, "early"), 0x104: (5, "below_fn"),
                 0x10C: (9, "above_fn"), 0x110: (11, "later")}
        bracket = poolslot.bracket_gap(self.GAP, self.DATUMS, first)
        self.assertEqual(bracket["below"]["function"], "below_fn")
        self.assertEqual(bracket["after"]["function"], "above_fn")
        self.assertTrue(bracket["monotone"])
        self.assertEqual(bracket["interval"], (5, 9))

    def test_a_non_monotone_surviving_order_refuses_the_bracket(self):
        first = {0x104: (9, "below_fn"), 0x10C: (5, "above_fn")}
        bracket = poolslot.bracket_gap(self.GAP, self.DATUMS, first)
        self.assertFalse(bracket["monotone"])
        self.assertIsNone(bracket["interval"])

    def test_no_attributed_datum_above_the_gap_refuses_the_bracket(self):
        first = {0x104: (5, "below_fn")}
        bracket = poolslot.bracket_gap(self.GAP, self.DATUMS, first)
        self.assertIsNone(bracket["after"])
        self.assertIsNone(bracket["interval"])
        self.assertIsNone(bracket["monotone"])

    def test_no_attributed_datum_below_the_gap_refuses_the_bracket(self):
        first = {0x10C: (9, "above_fn")}
        bracket = poolslot.bracket_gap(self.GAP, self.DATUMS, first)
        self.assertIsNone(bracket["below"])
        self.assertIsNone(bracket["interval"])


class ClassifyTests(unittest.TestCase):
    DATUMS = BracketTests.DATUMS
    GAP = BracketTests.GAP
    BRACKET = {"interval": (5, 9)}

    def test_a_referrer_outside_the_bracket_is_the_dead_stripped_class(self):
        found = poolslot.classify_gap(
            self.GAP, self.DATUMS, {"in_gap": [(20, "much_later")]},
            self.BRACKET)
        self.assertEqual(found["datums"], ["in_gap"])
        self.assertEqual(found["referrers_in_bracket"], [])
        self.assertIn("any_player_walking", found["verdict"])

    def test_a_referrer_inside_the_bracket_is_the_inlined_class(self):
        found = poolslot.classify_gap(
            self.GAP, self.DATUMS,
            {"in_gap": [(7, "neighbour"), (20, "much_later")]}, self.BRACKET)
        self.assertEqual(found["referrers_in_bracket"], [(7, "neighbour")])
        self.assertIn("StartCompass", found["verdict"])

    def test_no_referrer_at_all_is_its_own_verdict(self):
        found = poolslot.classify_gap(self.GAP, self.DATUMS, {}, self.BRACKET)
        self.assertEqual(found["referrers"], [])
        self.assertIn("NO REFERRER", found["verdict"])

    def test_an_unbracketed_gap_is_never_given_a_device_class(self):
        found = poolslot.classify_gap(
            self.GAP, self.DATUMS, {"in_gap": [(7, "neighbour")]},
            {"interval": None})
        self.assertEqual(found["referrers_in_bracket"], [])
        self.assertIn("UNBRACKETED", found["verdict"])


class CandidateTests(unittest.TestCase):
    ABSENT = [
        {"name": "tight_inside", "xbox_index": 1, "size": 0x10, "binding": "G",
         "bound": {"after_gc_index": 5, "before_gc_index": 7}},
        {"name": "far_below", "xbox_index": 2, "size": 0x10, "binding": "L",
         "bound": {"after_gc_index": 0, "before_gc_index": 5}},
        {"name": "far_above", "xbox_index": 3, "size": 0x10, "binding": "L",
         "bound": {"after_gc_index": 9, "before_gc_index": 12}},
        {"name": "very_wide", "xbox_index": 4, "size": 0x10, "binding": "G",
         "bound": {"after_gc_index": 0, "before_gc_index": 40}},
        {"name": "one_sided", "xbox_index": 5, "size": 0x10, "binding": "G",
         "bound": None},
    ]

    def test_only_intersecting_bounds_are_admitted_tightest_first(self):
        admitted, unscreened = poolslot.candidates_for(self.ABSENT, (5, 9))
        self.assertEqual([row["name"] for row in admitted],
                         ["tight_inside", "very_wide"])
        self.assertEqual(admitted[0]["intersection"], [5, 7])
        self.assertEqual(admitted[1]["intersection"], [5, 9])
        self.assertEqual(unscreened, ["one_sided"])

    def test_a_bound_that_merely_touches_the_bracket_is_not_an_intersection(self):
        admitted, _unscreened = poolslot.candidates_for(self.ABSENT, (0, 5))
        self.assertNotIn("tight_inside", [row["name"] for row in admitted])
        self.assertIn("far_below", [row["name"] for row in admitted])

    def test_without_a_bracket_nothing_is_admitted_and_all_are_unscreened(self):
        admitted, unscreened = poolslot.candidates_for(self.ABSENT, None)
        self.assertEqual(admitted, [])
        self.assertEqual(len(unscreened), len(self.ABSENT))


class LivePlayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (PLAYER_SRC.exists() and PLAYER_OBJ.exists()
                and ROSTER.exists()):
            raise unittest.SkipTest("game/game/player objects not built")
        cls.record = poolslot.survey("game/game/player", ".sdata2")

    def gap(self, address):
        for gap in self.record["gaps"]:
            if gap["address"] == address:
                return gap
        self.fail("no gap at 0x%08X; found %s"
                  % (address, [hex(row["address"])
                               for row in self.record["gaps"]]))

    def test_the_run_and_its_attribution_are_reported(self):
        """The run end follows our object's recovered .sdata2 extent: it was
        0x80347C6C before P10's literals, 0x80347D24 after them, 0x80347D34
        after P11's damage_player values (integration 64-2) and, since
        integration 64-6 closed the last three DOL-side gaps (SetPlayerLevel
        and player_scale_att outlined in the setup_player_display..AddExp
        bracket, start_magic's 0.707 as a compound-assigned literal, 95.0f
        through a local float, the "Yes"/"No" pointers as separate objects
        after both strings), the target run's true end 0x80347AFC. The
        bracket, device and candidate rules that the old live gaps pinned
        are pinned by BracketTests, ClassifyTests and CandidateTests."""
        self.assertEqual(self.record["run"], [0x80347608, 0x80347AFC])
        self.assertEqual(self.record["module"], ".\Release\PLAYER.OBJ")
        self.assertEqual(self.record["gaps"], [])

    def test_the_float_datum_is_not_reported_as_the_string_its_bytes_spell(self):
        """0x80347828 is f64 0.001 whose big-endian bytes begin "?PbM";
        the DOL bytes decide, so the check does not depend on our object."""
        note = poolslot.datum_note(0x80347828, 8)
        self.assertTrue(note.startswith("f64 0.001"), note)
        self.assertIn('"?PbM"', note)

    def test_a_bss_section_is_refused_not_bound(self):
        with self.assertRaises(poolslot.Refused) as caught:
            poolslot.survey("game/game/player", ".bss")
        self.assertIn("occupies no bytes in the DOL", str(caught.exception))

    def test_a_section_the_object_does_not_define_is_refused(self):
        with self.assertRaises(poolslot.Refused):
            poolslot.survey("game/game/player", ".nosuchsection")


class LiveExactControlTests(unittest.TestCase):
    """game/mb/mb_tree's .sdata2 is Matching AND claimed in splits.txt: our
    bytes cover the whole run, so the pipeline must report NO gap rather
    than manufacture a slot."""

    @classmethod
    def setUpClass(cls):
        if not (MBTREE_SRC.exists() and ROSTER.exists()):
            raise unittest.SkipTest("game/mb/mb_tree object not built")
        cls.record = poolslot.survey("game/mb/mb_tree", ".sdata2")

    def test_a_covered_run_reports_no_gap(self):
        self.assertEqual(self.record["gaps"], [])
        self.assertEqual(self.record["run"], [0x80348CA0, 0x80348CC8])

    def test_every_datum_in_the_covered_run_is_attributed(self):
        self.assertEqual(self.record["datums"], 6)
        self.assertEqual(self.record["attributed"], 6)


class CommandLineTests(unittest.TestCase):
    def test_a_bad_address_refuses_with_exit_2(self):
        self.assertEqual(
            poolslot.main(["game/game/player", ".sdata2", "zzz"]),
            poolslot.REFUSED)

    def test_an_absent_unit_refuses_with_exit_2(self):
        self.assertEqual(
            poolslot.main(["game/game/no_such_unit", ".sdata2"]),
            poolslot.REFUSED)

    def test_a_live_run_returns_zero(self):
        if not (PLAYER_SRC.exists() and PLAYER_OBJ.exists()
                and ROSTER.exists()):
            self.skipTest("game/game/player objects not built")
        self.assertEqual(poolslot.main(["game/game/player", ".sdata2"]),
                         poolslot.OK)


if __name__ == "__main__":
    unittest.main()
