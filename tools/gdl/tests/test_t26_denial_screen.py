"""Run-56 item 4: a shipped census contradicting an accepted denial.

REPRODUCED at c8e3b3479 with the reporter's own commands:

    $ python tools/gdl/composed_census/al_addrlo_positive.py
    $ python tools/gdl/composed_census/al_dest_split.py
    --- V: class (b) VOLATILE dest -- DECL-ORDER LEVER LIVE  (4 function(s))
      game/enemy/critter  ProcessCritterList  B 0->0  V 0->1  T49/O50

against, in the graph and accepted:

    attempt.CR_processcritterlist-the-declaration-order-lever-is-inert-here-
    so-the-addr16lo-reachable-remainder-is-zero.20260904.v1
      denial.scope: "(a) DECLARATION ORDER of the function's four locals,
      measured across three distinct orders."

DESIGN REVERSAL, recorded because it is the whole lesson of the item: the
first implementation matched flat PHRASES, calibrated cleanly, and then
reported NO CONTRADICTION on its own seed case -- the census spells the axis
`DECL-ORDER` and the denial spells it `DECLARATION ORDER`. A screen that
compares words answers the wrong question; it has to compare AXES. The
vocabulary is now canonical-axis -> spellings, and this file asserts the two
spellings collapse.
"""
import json
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS / "composed_census"))

import t26_denial_screen as screen                               # noqa: E402


class AxisSpellingsCollapse(unittest.TestCase):

    def test_the_two_spellings_of_the_seed_case_name_one_axis(self):
        census_side = screen.axis_terms(
            "class (b) VOLATILE dest -- DECL-ORDER LEVER LIVE")
        record_side = screen.axis_terms(
            "(a) DECLARATION ORDER of the function's four locals")
        self.assertIn("declaration-order", census_side)
        self.assertIn("declaration-order", record_side)
        self.assertTrue(census_side & record_side)

    def test_an_unrelated_row_names_no_axis(self):
        self.assertEqual(
            screen.axis_terms("class (a) callee-saved dest -- "
                              "source-unreachable"), set())


class ExemptionClausesAreNotDenials(unittest.TestCase):
    """A scope's job includes saying what it does NOT deny."""

    SCOPE = ("The pragma axis on fn_X only. It does NOT deny statement order "
             "or the loop form.")

    def test_the_denying_half_stops_at_the_exemption(self):
        half = screen.denying_half(self.SCOPE)
        self.assertIn("pragma", half)
        self.assertNotIn("statement order", half)

    def test_the_screen_uses_the_denying_half(self):
        self.assertEqual(screen.axis_terms(screen.denying_half(self.SCOPE)),
                         {"pragma"})
        # ... and the naive whole-scope read would have fired on both.
        self.assertEqual(screen.axis_terms(self.SCOPE),
                         {"pragma", "statement-order", "loop-form"})

    def test_every_opener_spelling_cuts(self):
        for opener in ("It does not deny", "It denies nothing about",
                       "does not speak to"):
            text = f"declaration order on fn_X. {opener} the cast form."
            self.assertNotIn("cast", screen.denying_half(text).lower(),
                             opener)


class SupersededDenialsAreNotVetoes(unittest.TestCase):

    def test_a_superseded_record_is_dropped(self):
        records = [
            {"id": "attempt.old.v1", "kind": "attempt",
             "function": "function:Foo",
             "denial": {"scope": "declaration order on Foo"}},
            {"id": "attempt.new.v2", "kind": "attempt",
             "function": "function:Foo", "supersedes": "attempt.old.v1"},
        ]
        self.assertEqual(screen.live_denials(records), {})


class TheSeedCaseIsCaught(unittest.TestCase):
    """The measurement, end to end, against the live corpus."""

    @classmethod
    def setUpClass(cls):
        cls.denials = screen.live_denials()

    def test_the_reported_contradiction_is_reported(self):
        rows = [{"fn": "ProcessCritterList", "unit": "game/enemy/critter",
                 "text": "class (b) VOLATILE dest -- DECL-ORDER LEVER LIVE"}]
        found = screen.contradictions(rows, self.denials)
        self.assertEqual(len(found), 1, found)
        self.assertEqual(found[0]["axes"], ["declaration-order"])
        self.assertIn("processcritterlist", found[0]["record_id"].lower())
        self.assertTrue(found[0]["expiry_check"],
                        "a contradiction must hand back the way to clear it")

    def test_the_negative_side_of_the_same_census_stays_clean(self):
        """37 of al_dest_split's rows are the callee-saved class."""
        rows = [{"fn": "AdsPutBuffer", "unit": "game/audio/adstream",
                 "text": "class (a) callee-saved dest -- source-unreachable"}]
        self.assertEqual(screen.contradictions(rows, self.denials), [])


class CensusHarvestIsShapeAgnostic(unittest.TestCase):

    def test_it_finds_rows_in_a_nested_census(self):
        payload = {"counts": {"V": 1},
                   "rows": [{"unit": "u", "fn": "Foo", "cls": "V",
                             "disposition": "DECL-ORDER LEVER LIVE"}]}
        rows = screen._rows_from_census(payload)
        self.assertEqual([r["fn"] for r in rows], ["Foo"])
        self.assertIn("DECL-ORDER", rows[0]["text"])

    def test_the_shipped_census_still_parses(self):
        path = REPO / "build" / "GUNE5D" / "al_dest_split.json"
        if not path.exists():
            self.skipTest("al_dest_split census not generated in this tree")
        rows = screen._rows_from_census(
            json.loads(path.read_text(encoding="utf-8")))
        self.assertGreaterEqual(len(rows), 40)


if __name__ == "__main__":
    unittest.main()
