"""Two-sided tests for tools/gdl/composed_census/bssorder.py.

The pure half exercises the seating device and the two order comparisons on
hand-built layouts whose right answer is readable from the test: a seat that
validates, a seat that CONFLICTS (the hypothesis refuted, not the seat), and
the difference between a target order that follows declaration order and one
that follows first-reference order.

The live half is calibrated on units with three different shapes, so a
passing suite means the screen distinguishes them:

    game/game/player  .bss   NonMatching: one EXTERNAL seated mid-run, and
                             first-reference order does not explain it
    game/game/player  .sbss  our object defines NOTHING there — the
                             declaration comparison must say UNAVAILABLE
    game/mb/mb_poly   .bss   Matching: zero discordant pairs under BOTH laws
    game/sys/recorder .bss   Matching: zero under declaration order but FOUR
                             under first-reference order — the control that
                             proves a first-reference disagreement is not by
                             itself a defect

The two player cases read build/GUNE5D/src/game/game/player.o, a source-built
object of a NonMatching unit; they skip when it is absent, and player is
already in the CI refresh `rm -f` list in .github/workflows/build.yml.
game/mb/mb_poly and game/sys/recorder are Matching, so their source objects
are built by the ordinary `ninja all` and need no new CI refresh entry.

Reproduced at 1bf4aabbf with
    python tools/gdl/composed_census/bssorder.py game/game/player
    python tools/gdl/composed_census/bssorder.py game/game/player \\
        --section .sbss --run 0x80344AFC..0x80344B18
"""

import sys
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(GDL))
sys.path.insert(0, str(GDL / "composed_census"))

import bssorder  # noqa: E402

SRC = ROOT / "build" / "GUNE5D" / "src"
OBJ = ROOT / "build" / "GUNE5D" / "obj"
PLAYER_RUN = (0x80344AFC, 0x80344B18)


def built(unit):
    return (SRC / (unit + ".o")).exists() and (OBJ / (unit + ".o")).exists()


class SeatingTests(unittest.TestCase):
    STATICS = [(0x00, 0x10, "a", "static"), (0x10, 0x20, "b", "static"),
               (0x30, 0x10, "c", "static")]
    EXTERNALS = [(0x40, 0x8, "g", "external")]

    def test_an_external_is_seated_and_the_statics_resume_past_it(self):
        layout, conflicts = bssorder.seat_layout(
            self.STATICS, self.EXTERNALS, {"g": 0x30})
        self.assertEqual(conflicts, [])
        self.assertEqual([(offset, name) for offset, _size, name, _b
                          in layout],
                         [(0x00, "a"), (0x10, "b"), (0x30, "g"), (0x38, "c")])

    def test_an_external_with_no_known_target_offset_is_appended(self):
        layout, conflicts = bssorder.seat_layout(
            self.STATICS, self.EXTERNALS, {})
        self.assertEqual(conflicts, [])
        self.assertEqual([name for _o, _s, name, _b in layout],
                         ["a", "b", "c", "g"])

    def test_a_seat_the_cursor_has_already_passed_is_a_reported_conflict(self):
        layout, conflicts = bssorder.seat_layout(
            self.STATICS, self.EXTERNALS, {"g": 0x08})
        self.assertEqual(len(conflicts), 1)
        self.assertEqual(conflicts[0]["name"], "g")
        self.assertTrue(layout)

    def test_validation_scores_only_the_symbols_it_did_not_seed(self):
        layout, _conflicts = bssorder.seat_layout(
            self.STATICS, self.EXTERNALS, {"g": 0x30})
        found = bssorder.validate_seating(
            layout, {"g": 0x30, "a": 0x00, "c": 0x38}, seeded={"g"})
        self.assertEqual((found["agree"], found["disagree"]), (2, 0))
        self.assertIn(("g", 0x30, 0x30, "seeded"), found["rows"])

    def test_validation_reports_a_seating_that_misses_a_known_offset(self):
        layout, _conflicts = bssorder.seat_layout(
            self.STATICS, self.EXTERNALS, {"g": 0x30})
        found = bssorder.validate_seating(
            layout, {"g": 0x30, "c": 0x99}, seeded={"g"})
        self.assertEqual((found["agree"], found["disagree"]), (0, 1))
        self.assertIn(("c", 0x99, 0x38, "DIFFERS"), found["rows"])


class PartitionTests(unittest.TestCase):
    def test_statics_before_externals_is_the_law_shape(self):
        rows = [(0x0, 4, "a", "static"), (0x4, 4, "g", "external")]
        statics, externals, shape = bssorder.partition(rows)
        self.assertEqual(len(statics), 1)
        self.assertEqual(len(externals), 1)
        self.assertTrue(shape)

    def test_an_external_among_the_statics_breaks_the_law_shape(self):
        rows = [(0x0, 4, "g", "external"), (0x4, 4, "a", "static")]
        _statics, _externals, shape = bssorder.partition(rows)
        self.assertFalse(shape)

    def test_a_section_with_only_statics_cannot_break_the_shape(self):
        rows = [(0x0, 4, "a", "static"), (0x4, 4, "b", "static")]
        _statics, externals, shape = bssorder.partition(rows)
        self.assertEqual(externals, [])
        self.assertTrue(shape)


class VerdictTests(unittest.TestCase):
    def test_declaration_order_with_no_break_reports_no_suffix(self):
        found = bssorder.order_verdict(
            ["a", "b", "c"], ["a", "b", "c"],
            {"a": (5, 0), "b": (1, 0), "c": (9, 0)})
        self.assertEqual(found["declaration_breaks"], [])
        self.assertEqual(found["suffix"], [])
        self.assertEqual(found["firstref_breaks"], [("a", "b")])

    def test_a_declaration_break_starts_the_suffix_at_the_offender(self):
        found = bssorder.order_verdict(
            ["a", "g", "b", "c"], ["a", "b", "c", "g"],
            {"a": (1, 0), "g": (7, 0), "b": (2, 0), "c": (3, 0)})
        self.assertEqual(found["declaration_breaks"], [("g", "b")])
        self.assertEqual(found["suffix"], ["g", "b", "c"])
        self.assertEqual(found["suffix_first_reference"],
                         [(7, 0), (2, 0), (3, 0)])

    def test_a_name_missing_from_a_side_is_skipped_not_guessed(self):
        found = bssorder.order_verdict(["a", "unknown", "b"], ["a", "b"],
                                       {"a": (1, 0)})
        self.assertEqual(found["declaration_breaks"], [])
        self.assertEqual(found["firstref_breaks"], [])


class RunTests(unittest.TestCase):
    def test_a_range_is_parsed_in_hex_or_decimal(self):
        self.assertEqual(bssorder.parse_run("0x10..0x20"), (0x10, 0x20))
        self.assertEqual(bssorder.parse_run("16..32"), (16, 32))

    def test_a_malformed_range_raises_rather_than_defaulting(self):
        with self.assertRaises(ValueError):
            bssorder.parse_run("0x10-0x20")

    def test_a_non_bss_section_is_refused(self):
        with self.assertRaises(bssorder.Refused) as caught:
            bssorder.survey("game/game/player", ".rodata")
        self.assertIn("not a BSS section", str(caught.exception))


class LivePlayerBssTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not built("game/game/player"):
            raise unittest.SkipTest("game/game/player objects not built")
        cls.record = bssorder.survey("game/game/player", ".bss")

    def test_our_layout_obeys_the_law_and_puts_the_external_last(self):
        self.assertEqual(self.record["run"], [0x80274EA0, 0x80275AE0])
        self.assertEqual((self.record["statics"], self.record["externals"]),
                         (15, 1))
        self.assertTrue(self.record["law_shape"])
        self.assertEqual(self.record["ours"][-1][:3],
                         (0xC34, 0xC, "gDefaultPlayerPosition"))

    def test_the_target_seats_that_external_at_0x934(self):
        offsets = {name: offset for offset, _size, name, _b
                   in self.record["target_rows"]}
        self.assertEqual(offsets["gDefaultPlayerPosition"], 0x934)

    def test_the_seating_validates_against_every_unseeded_target_symbol(self):
        validation = self.record["validation"]
        self.assertEqual((validation["agree"], validation["disagree"]), (5, 0))
        seated = {name: offset for offset, _size, name, _b
                  in self.record["layout"]}
        self.assertEqual(seated["gDefaultPlayerPosition"], 0x934)
        self.assertEqual(seated["lbl_802757E0"], 0x940)
        self.assertEqual(seated["frame_blit"], 0xBE0)

    def test_the_provenance_tracker_rejects_lane_p7s_two_false_positives(self):
        """hud_pad_034's `52(r3)` is a pointer-field store and stays
        UNCONFIRMED; gDefaultPlayerPosition's first reference is kill_player,
        not do_players, whose r21 is one byte past the end of the run."""
        confirmed = self.record["confirmed"]
        self.assertNotIn("hud_pad_034", confirmed)
        self.assertEqual(self.record["unconfirmed"]["hud_pad_034"][1],
                         "PlayerUnsetParent")
        self.assertEqual(confirmed["gDefaultPlayerPosition"][0], 34)
        self.assertEqual(confirmed["gDefaultPlayerPosition"][2], "kill_player")

    def test_declaration_order_holds_and_the_one_break_is_the_seat(self):
        verdict = self.record["verdict"]
        self.assertTrue(verdict["declaration_known"])
        self.assertEqual(verdict["declaration_breaks"],
                         [("gDefaultPlayerPosition", "lbl_802757E0")])

    def test_first_reference_order_cannot_explain_the_seated_suffix(self):
        verdict = self.record["verdict"]
        self.assertEqual(verdict["suffix"][0], "gDefaultPlayerPosition")
        self.assertEqual(len(verdict["suffix"]), 8)
        self.assertEqual([row[0] for row in verdict["suffix_first_reference"]],
                         [34, 62, 5, 9, 5, 5, 5, 5])


class LivePlayerSbssTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not built("game/game/player"):
            raise unittest.SkipTest("game/game/player objects not built")
        cls.record = bssorder.survey("game/game/player", ".sbss", PLAYER_RUN)

    def test_an_unclaimed_bss_run_refuses_rather_than_guessing(self):
        with self.assertRaises(bssorder.Refused) as caught:
            bssorder.survey("game/game/player", ".sbss")
        self.assertIn("--run", str(caught.exception))

    def test_with_no_definitions_the_declaration_comparison_is_unavailable(self):
        self.assertEqual(self.record["ours"], [])
        self.assertFalse(self.record["verdict"]["declaration_known"])
        self.assertEqual(self.record["verdict"]["declaration_breaks"], [])

    def test_alpha_is_second_by_address_and_first_of_its_pair_by_reference(self):
        order = [name for _offset, _size, name, _b in self.record["objects"]]
        self.assertEqual(order[:2], ["key_blit_idx", "alpha"])
        confirmed = self.record["confirmed"]
        self.assertEqual(confirmed["alpha"][2], "write_health_and_items")
        self.assertEqual(confirmed["key_blit_idx"][2],
                         "write_health_and_items")
        self.assertLess(confirmed["alpha"][:2], confirmed["key_blit_idx"][:2])


class LiveExactControlTests(unittest.TestCase):
    def test_an_exact_unit_agrees_under_both_laws(self):
        if not built("game/mb/mb_poly"):
            self.skipTest("game/mb/mb_poly objects not built")
        record = bssorder.survey("game/mb/mb_poly", ".bss")
        self.assertEqual(record["verdict"]["declaration_breaks"], [])
        self.assertEqual(record["verdict"]["firstref_breaks"], [])
        self.assertEqual(record["verdict"]["suffix"], [])

    def test_an_exact_unit_may_still_break_first_reference_order(self):
        """The decisive negative control: game/sys/recorder links
        byte-identically, so its .bss order is right by construction, and it
        breaks first-reference order four times. A first-reference
        disagreement is therefore not a defect on its own."""
        if not built("game/sys/recorder"):
            self.skipTest("game/sys/recorder objects not built")
        record = bssorder.survey("game/sys/recorder", ".bss")
        self.assertEqual(record["verdict"]["declaration_breaks"], [])
        self.assertEqual(len(record["verdict"]["firstref_breaks"]), 4)
        self.assertEqual(record["verdict"]["suffix"], [])


class CommandLineTests(unittest.TestCase):
    def test_a_malformed_run_refuses_with_exit_2(self):
        self.assertEqual(
            bssorder.main(["game/game/player", "--run", "nonsense"]),
            bssorder.REFUSED)

    def test_an_absent_unit_refuses_with_exit_2(self):
        self.assertEqual(bssorder.main(["game/game/no_such_unit"]),
                         bssorder.REFUSED)

    def test_a_live_run_returns_zero(self):
        if not built("game/game/player"):
            self.skipTest("game/game/player objects not built")
        self.assertEqual(bssorder.main(["game/game/player"]), bssorder.OK)


if __name__ == "__main__":
    unittest.main()
