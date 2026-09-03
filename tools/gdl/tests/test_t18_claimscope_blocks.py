#!/usr/bin/env python3
"""T18 run-48 item 7: webfrank.json is owned by BLOCK, not by file.

THE DEFECT. config/GUNE5D/webfrank.json is one file holding one block per
unit, and AGENTS.md first-five-minutes trap 15b makes re-deriving a pin the
chore of whoever edits the TU -- "the upstream freeze is a re-derivation
chore, not a wall" -- with `probe --rederive-pin` writing the file on the
source lane's behalf. File-level ownership therefore refused the one edit
the workflow REQUIRES of every source lane. Reproduced at 33a1bad50 with
GDL_LANE=claude-fleet-worker-DA, the lane whose run-48 order tells it to keep
a pinned edit in game/world/items:

    claimscope.py config/GUNE5D/webfrank.json
      "status": "foreign", owner claude-fleet-worker-WF        exit 3

    claimscope.py 'config/GUNE5D/webfrank.json#game/world/items'
      "status": "ok", owners [], own_claims []                 exit 0

The second was "ok" only because nothing covered the spelling -- an accident
with no owner named and nobody to coordinate with.

TWO-SIDED CALIBRATION at 33a1bad50 over all 52 blocks against run-48's six
active claims (T18_scratch/t18_calib_item7.py):

    POSITIVES   8 blocks resolve through the block's OWN unit -- audio,
                auxscreen (NC), gamemain, items (DA), mb_camera, dbgtext
                (FT), movieplayer (CU), psfx (WF). Seven change owner.
    NEGATIVES  44 blocks resolve file-level, unchanged.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import claimscope as cs  # noqa: E402

CONFIG = "config/GUNE5D/webfrank.json"

CLAIMS = [
    {"id": "claim.wf", "owner": "lane-WF", "declared": True, "path": "x",
     "owned_units": [CONFIG, "tools/gdl/webfrank.py", "game/sfx/psfx"]},
    {"id": "claim.da", "owner": "lane-DA", "declared": True, "path": "y",
     "owned_units": ["game/world/items", "game/game/gamemain"]},
    {"id": "claim.explicit", "owner": "lane-EX", "declared": True, "path": "z",
     "owned_units": [f"{CONFIG}#game/ui/btext"]},
]


class Splitting(unittest.TestCase):
    def test_a_bare_unit_has_no_block(self):
        self.assertEqual(cs.split_block("game/world/items"),
                         ("game/world/items", None))

    def test_a_block_scope_splits(self):
        self.assertEqual(cs.split_block(f"{CONFIG}#game/world/items"),
                         (CONFIG, "game/world/items"))

    def test_the_block_half_is_normalized(self):
        self.assertEqual(cs.split_block(f"{CONFIG}#src/game/world/items.c"),
                         (CONFIG, "game/world/items"))

    def test_an_empty_block_is_no_block(self):
        self.assertEqual(cs.split_block(f"{CONFIG}#"), (CONFIG, None))

    def test_scopes_are_most_specific_first(self):
        self.assertEqual(cs.resolution_scopes(f"{CONFIG}#game/world/items"),
                         [f"{CONFIG}#game/world/items", "game/world/items",
                          CONFIG])
        self.assertEqual(cs.resolution_scopes("game/world/items"),
                         ["game/world/items"])


class Resolution(unittest.TestCase):
    def _check(self, unit, lane):
        return cs.check_unit(unit, lane=lane, claims=CLAIMS, repo=REPO)

    def test_a_source_lane_owns_its_own_units_block(self):
        verdict = self._check(f"{CONFIG}#game/world/items", "lane-DA")
        self.assertEqual(verdict["status"], "ok")
        self.assertEqual(verdict["own_claims"], ["claim.da"])
        self.assertEqual(verdict["resolved_by"], "game/world/items")

    def test_another_lane_is_refused_on_that_same_block(self):
        verdict = self._check(f"{CONFIG}#game/world/items", "lane-WF")
        self.assertEqual(verdict["status"], "foreign")
        self.assertEqual([o["owner"] for o in verdict["owners"]], ["lane-DA"])
        self.assertEqual(verdict["resolved_by"], "game/world/items")

    def test_an_unclaimed_units_block_falls_back_to_the_file_owner(self):
        verdict = self._check(f"{CONFIG}#game/ui/message", "lane-DA")
        self.assertEqual(verdict["status"], "foreign")
        self.assertEqual([o["owner"] for o in verdict["owners"]], ["lane-WF"])
        self.assertEqual(verdict["resolved_by"], CONFIG)

    def test_an_explicit_block_entry_outranks_the_units_owner(self):
        # btext is unclaimed as a unit; the explicit block entry decides.
        verdict = self._check(f"{CONFIG}#game/ui/btext", "lane-WF")
        self.assertEqual(verdict["status"], "foreign")
        self.assertEqual([o["owner"] for o in verdict["owners"]], ["lane-EX"])
        self.assertEqual(verdict["resolved_by"], f"{CONFIG}#game/ui/btext")

    def test_the_bare_file_keeps_file_level_semantics(self):
        verdict = self._check(CONFIG, "lane-DA")
        self.assertEqual(verdict["status"], "foreign")
        self.assertEqual([o["owner"] for o in verdict["owners"]], ["lane-WF"])
        self.assertNotIn("block", verdict)

    def test_the_file_owner_still_owns_the_bare_file(self):
        self.assertEqual(self._check(CONFIG, "lane-WF")["status"], "ok")

    def test_only_the_first_matching_scope_decides(self):
        # The file's owner must NOT be able to overrule the block's owner by
        # appearing further down the resolution list.
        verdict = self._check(f"{CONFIG}#game/game/gamemain", "lane-DA")
        self.assertEqual(verdict["status"], "ok")
        self.assertEqual(verdict["owners"], [])

    def test_a_plain_unit_query_is_untouched_by_the_block_machinery(self):
        self.assertEqual(self._check("game/world/items", "lane-DA")["status"],
                         "ok")
        self.assertEqual(self._check("game/world/items", "lane-WF")["status"],
                         "foreign")


class LiveBlocks(unittest.TestCase):
    def test_every_pinned_unit_gets_a_block_row(self):
        units = cs.webfrank_units()
        if not units:
            self.skipTest("no webfrank.json in this checkout")
        blocks = cs.webfrank_block_owners()
        self.assertEqual(sorted(blocks), sorted(units))
        for unit, row in blocks.items():
            self.assertTrue(row["owners"], unit)
            self.assertIn(row["resolved_by"],
                          (unit, cs.WEBFRANK_CONFIG,
                           f"{cs.WEBFRANK_CONFIG}#{unit}", None))


if __name__ == "__main__":
    unittest.main()
