"""T25 run-55 item 9: a law falsifier naming an already-worked function.

REPORTED (CR): "the run-53 CR record's law_screen states the decl-swap cure
is already satisfied; the AL law's falsifier calls the same function the one
untried row - both accepted the same day, three hours apart, and disagree".

REPRODUCED at 215bd2193: claim.law.AL_the-addr16lo-surplus-copy-roster-...
.20260904.v1's falsifier names ProcessCritterList as "the one untried row",
and attempt.CR_processcritterlist-the-declaration-order-lever-is-inert-here-
so-the-addr16lo-reachable-remainder-is-zero.20260904.v1 (outcome `negative`,
recorded 06:00:13Z the same day) discharges exactly that. The screen finds
that pair as its first CROSS-LANE row.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

from t25_falsifier_freshness import (lane_prefix,  # noqa: E402
                                     named_functions, rows)

SYMBOLS = {"ProcessCritterList", "move_logic00", "InitCamera"}


def law(rid, falsifier, stamp):
    return {"id": rid, "kind": "claim", "falsifier": falsifier,
            "recorded_at": stamp}


def attempt(rid, function, stamp, outcome="negative", text=""):
    return {"id": rid, "kind": "attempt", "function": f"function:{function}",
            "recorded_at": stamp, "outcome": outcome,
            "attributes": {"law_screen": text}}


class NamedFunctionTests(unittest.TestCase):
    def test_only_real_symbols_are_extracted(self):
        """Prose cannot manufacture a row: a token is a function only when
        the image has one by that name."""
        got = named_functions(
            "ProcessCritterList is the one untried row carrying the"
            " responsive polarity", SYMBOLS)
        self.assertEqual(got, ["ProcessCritterList"])

    def test_a_falsifier_naming_nothing_yields_nothing(self):
        self.assertEqual(named_functions("any spelling of the addend", SYMBOLS),
                         [])

    def test_empty_text_is_safe(self):
        self.assertEqual(named_functions(None, SYMBOLS), [])


class LanePrefixTests(unittest.TestCase):
    def test_a_law_id_yields_its_lane(self):
        self.assertEqual(
            lane_prefix("claim.law.AL_the-addr16lo-surplus.20260904.v1"),
            "AL")

    def test_an_attempt_id_yields_its_lane(self):
        self.assertEqual(
            lane_prefix("attempt.CR_processcritterlist-the-decl.20260904.v1"),
            "CR")

    def test_an_unprefixed_id_is_blank(self):
        self.assertEqual(lane_prefix("attempt.parked.move_logic00.v1"), "")


class RowTests(unittest.TestCase):
    def test_the_reported_pair_is_a_cited_row(self):
        law_id = "claim.law.AL_addr16lo.20260904.v1"
        rec = "attempt.CR_processcritterlist.20260904.v1"
        got = rows([law(law_id, "ProcessCritterList is the one untried row",
                        "2026-09-04T03:00:00Z")],
                   [attempt(rec, "ProcessCritterList", "2026-09-04T06:00:13Z",
                            text=f"discharges {law_id}")],
                   SYMBOLS)
        self.assertEqual(len(got), 1)
        self.assertEqual(got[0][:3], (law_id, "ProcessCritterList", "CITED"))
        self.assertNotEqual(lane_prefix(got[0][0]), lane_prefix(got[0][3]))

    def test_an_older_record_is_not_a_row(self):
        """A record that predates the law is what the law was written from."""
        got = rows([law("claim.law.AL_x.20260904.v1", "ProcessCritterList",
                        "2026-09-04T06:00:00Z")],
                   [attempt("attempt.CR_y.20260904.v1", "ProcessCritterList",
                            "2026-09-04T03:00:00Z")],
                   SYMBOLS)
        self.assertEqual(got, [])

    def test_a_newer_record_that_does_not_cite_is_tier_two(self):
        got = rows([law("claim.law.AL_x.20260904.v1", "ProcessCritterList",
                        "2026-09-04T03:00:00Z")],
                   [attempt("attempt.CR_y.20260904.v1", "ProcessCritterList",
                            "2026-09-04T06:00:00Z")],
                   SYMBOLS)
        self.assertEqual(got[0][2], "NEWER")

    def test_a_law_with_no_falsifier_is_skipped(self):
        got = rows([{"id": "claim.law.AL_x.v1",
                     "recorded_at": "2026-09-04T03:00:00Z"}],
                   [attempt("attempt.CR_y.v1", "ProcessCritterList",
                            "2026-09-04T06:00:00Z")],
                   SYMBOLS)
        self.assertEqual(got, [])

    def test_a_record_on_another_function_is_not_a_row(self):
        got = rows([law("claim.law.AL_x.v1", "ProcessCritterList",
                        "2026-09-04T03:00:00Z")],
                   [attempt("attempt.CR_y.v1", "move_logic00",
                            "2026-09-04T06:00:00Z")],
                   SYMBOLS)
        self.assertEqual(got, [])


if __name__ == "__main__":
    unittest.main()
