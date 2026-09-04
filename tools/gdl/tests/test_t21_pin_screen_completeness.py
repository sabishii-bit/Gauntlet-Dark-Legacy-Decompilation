#!/usr/bin/env python3
"""T21 run-51 item 6: `brief`'s mandatory webfrank pin screen listed only
the pins that carry `mechanism` prose.

REPRODUCED at 6c2e07f78:

    $ python - (reading config/GUNE5D/webfrank.json)
    game/pb/dbgtext 2
       fn= dbgTextInit    keys= [after_sha256, audit, before_sha256,
                                 copy_register_fields, function]
         mechanism? False
       fn= fn_800C031C    keys= [..., mechanism, value_equality_recolor]
         mechanism? True

    $ python memory_graph/gdlmem.py brief game/pb/dbgtext
      "webfrank_pins": [ { "function": "fn_800C031C", ... } ]

MECHANISM: `webfrank_pin_mechanisms` skips any rule whose `mechanism` is not
a string. That is right for the SEARCH surfaces it was written for
(`laws --query` / `--residual` search that prose), and wrong for the pin
SCREEN, whose job is to enumerate every FROZEN function in a TU before the
first edit (AGENTS.md trap 4).

POPULATION, measured over config/GUNE5D/webfrank.json at 6c2e07f78 — far
larger than the reported single row:

    total pins 154, with mechanism 113, WITHOUT 41 (27%)
    27 of the 52 pinned TUs listed fewer pins than they hold
    ELEVEN listed ZERO, i.e. read exactly like an unpinned TU:
      game/anim/anim, game/anim/anim_play, game/audio/mempool,
      game/g3d/g3dpad, game/g3d/sndvoice, game/game/pmotion,
      game/mb/mb_tree, game/pb/pb_winglobals, game/sound/sounds,
      game/sound/sounds_evt, game/sys/ml_mem
    worst live case in run 51: game/enemy/enemy listed 3 of 10, under an
      active flip claim

TWO-SIDED CALIBRATION:
  POSITIVES  41 pins become visible to the screen (113 -> 154)
  NEGATIVES  the search surfaces are UNCHANGED at 113: a pin with no prose
             has nothing for a prose query to match, and returning it there
             would be a hit on an empty string

SIBLING SCREEN (AGENTS discipline 18), same function, opposite direction:
the TU test was `tu in unit or unit in tu`, and the second half has no path
boundary. Measured over the 52 pinned units: 4 cross-matches in 2 pairs —
`game/anim/anim` <-> `game/anim/anim_play` and `game/sound/sounds` <->
`game/sound/sounds_evt` — each putting a FOREIGN TU's frozen function into a
lane's screen. An exact unit spelling now wins over the fragment reading,
and the fragment reading keeps its substring test.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))

from memory_graph import core  # noqa: E402

CONFIG = REPO / "config" / "GUNE5D" / "webfrank.json"


@unittest.skipUnless(CONFIG.exists(), "needs config/GUNE5D/webfrank.json")
class ScreenListsEveryPin(unittest.TestCase):
    def test_the_screen_surface_returns_more_than_the_search_surface(self):
        search = core.webfrank_pin_mechanisms(REPO, None)
        screen = core.webfrank_pin_mechanisms(
            REPO, None, include_without_mechanism=True)
        self.assertGreater(len(screen), len(search))

    def test_the_screen_returns_every_rule_in_the_config(self):
        import json
        data = json.loads(CONFIG.read_text(encoding="utf-8-sig"))
        total = sum(len(rules) for rules in (data.get("units") or {}).values()
                    if isinstance(rules, list))
        screen = core.webfrank_pin_mechanisms(
            REPO, None, include_without_mechanism=True)
        self.assertEqual(len(screen), total)

    def test_the_search_surface_is_unchanged(self):
        # The negative half: a prose query has nothing to match on a pin with
        # no prose, and must not start returning one.
        search = core.webfrank_pin_mechanisms(REPO, None)
        self.assertTrue(all(row["mechanism"] for row in search))

    def test_dbgtext_lists_every_one_of_its_pins(self):
        # Derived from the config, never hardcoded: the assertion under test
        # is "the screen enumerates every FROZEN function in the TU", and a
        # frozen literal set tests the config's CONTENTS instead, so a lane
        # that legitimately ships or promotes a rule fails a test about a
        # surface it did not touch. (Run 53: shipping the provenanced
        # fn_800C03E0 rule failed the literal form.)
        import json
        data = json.loads(CONFIG.read_text(encoding="utf-8-sig"))
        expected = {rule["function"]
                    for rule in (data.get("units") or {})["game/pb/dbgtext"]}
        names = {row["function"]
                 for row in core._pin_provenance(REPO, "game/pb/dbgtext")}
        self.assertEqual(names, expected)

    def test_a_mechanismless_pin_says_its_derivation_is_missing(self):
        # Any mechanism-less pin, chosen from the config for the same reason:
        # promoting or annotating one particular pin must not fail this.
        rows = [row for row in core.webfrank_pin_mechanisms(
                    REPO, None, include_without_mechanism=True)
                if not row.get("mechanism")]
        if not rows:
            self.skipTest("no mechanism-less pin left in the config")
        self.assertIn("NO `mechanism` PROSE", rows[0]["note"])
        self.assertIn("FROZEN", rows[0]["note"])


class TuMatching(unittest.TestCase):
    def test_a_fragment_still_matches(self):
        self.assertTrue(core._tu_matches_pin_unit("dbgtext",
                                                  "game/pb/dbgtext"))

    def test_a_full_spelling_with_src_and_extension_matches(self):
        self.assertTrue(core._tu_matches_pin_unit("src/game/pb/dbgtext.cpp",
                                                  "game/pb/dbgtext"))
        self.assertTrue(core._tu_matches_pin_unit("game\\pb\\dbgtext.c",
                                                  "game/pb/dbgtext"))

    def test_a_longer_tu_does_not_swallow_a_shorter_unit(self):
        self.assertFalse(core._tu_matches_pin_unit("game/anim/anim_play",
                                                   "game/anim/anim"))
        self.assertFalse(core._tu_matches_pin_unit("game/sound/sounds_evt",
                                                   "game/sound/sounds"))

    def test_normalization(self):
        self.assertEqual(core._normalized_tu("src/game/pb/dbgtext.cpp"),
                         "game/pb/dbgtext")
        self.assertEqual(core._normalized_tu("/game/pb/dbgtext/"),
                         "game/pb/dbgtext")


@unittest.skipUnless(CONFIG.exists(), "needs config/GUNE5D/webfrank.json")
class ExactSpellingWins(unittest.TestCase):
    def test_anim_does_not_inherit_anim_plays_pin(self):
        units = {row["unit"]
                 for row in core._pin_provenance(REPO, "game/anim/anim")}
        self.assertEqual(units, {"game/anim/anim"})

    def test_sounds_does_not_inherit_sounds_evts_pin(self):
        units = {row["unit"]
                 for row in core._pin_provenance(REPO, "game/sound/sounds")}
        self.assertEqual(units, {"game/sound/sounds"})


if __name__ == "__main__":
    unittest.main()
