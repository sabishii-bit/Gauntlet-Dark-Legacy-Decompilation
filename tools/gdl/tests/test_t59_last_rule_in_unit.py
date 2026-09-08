#!/usr/bin/env python3
"""T3 run-59 item 1: retiring a unit's LAST rule is a retirement.

THE OBSERVATION (carried). When the last rule in a TU is retired,
configure.py stops emitting the unit's WebFrank edge; `retire_audit` then
found no pre-postprocessor body edge and refused:

    game/audio/dcs has no pre-postprocessor body edge; there is no rule to
    retire in this unit                                            EXIT 2

-- said of a unit whose rule had just been retired. Live positive:
game/audio/dcs::update_chinfo, retired at c4d71a603 ("Retire the final DCS
WebFrank rule through voiceDuck"), whose parent 4e108c4d3 still carries the
`game/audio/dcs` key.

FOUR THINGS WERE WRONG, and the third was invisible from the report:

1. The refusal could not tell a RETIRED last rule from a unit that was
   never pinned. The discriminant is the BEFORE-ref's config, which is now
   read in `inputs()` (once -- `build_images` used to re-run `git show`).
2. `postprocess_edge` matched ANY edge writing build/GUNE5D/src/<unit>.o,
   and for an unpinned unit that is the COMPILE edge (`mwcc_sjis` on
   game/audio/dcs), so it reported a postprocessor for a unit with none.
   It now filters on the rule.
3. `rule_delta` read the unit's rules with `.get(unit, [])`, which cannot
   tell a REMOVED key from a key left holding an empty list -- and
   configure.py decides whether to emit the edge from the key. The key must
   now be absent when the last rule goes.
4. `rule_replay` had no BEFORE edge to replay through, because it is gone
   from the graph. It is DERIVED from the surviving webfrank edges and only
   when they prove the edge is a pure function of the unit name; the
   certificate records the derivation. There is no AFTER replay at all:
   webfrank.py refuses a unit it has no configuration for (measured:
   `KeyError: no webfrank configuration for 'game/audio/dcs'`), and the
   claim to certify is precisely that the shipped object IS the compiler
   output.

LIVE CALIBRATION at 966900511:

  POSITIVE  game/audio/dcs update_chinfo --before-ref c4d71a603~1
            PASS on all ten checks (link SKIPPED). The derived edge replays
            the before rules -- "WEBFRANK update_chinfo: adjusted 32
            instruction atoms/fields" -- and the after image is the fresh
            compile, sha256 fbb16807... equal to the shipped object.
            `last_rule_in_unit_retired: true`, `unit_key_present_after:
            false`, donor_edges 52, all agreeing on --image.
  NEGATIVE  game/sound/sounds_evt AudioPlayerEatSFX --before-ref
            c5afaa1b8~1 (the unit carried NO rule there)
            REFUSED, exit 2, "this unit was never pinned, so there is no
            retirement to certify".
  NEGATIVE  the normal path is untouched: game/sys/memcard
            memCardErrorPrompt --before-ref e7f9d740b~1, game/mb/mb_camera
            MBCameraUpdate --before-ref 30582a770~1 and game/game/player
            ExpToLevel --before-ref 37f9daac5~1 all still PASS end to end.

The tests below cover the four mechanisms over synthetic graphs and configs;
the end-to-end runs above are too slow (three compiles each) to keep in the
suite.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(TOOLS))

from tools.gdl import retire_audit as ra                      # noqa: E402

IMAGE = "orig/GUNE5D/sys/main.dol"


def webfrank_edge(unit, image=IMAGE, **override):
    variables = {
        "webfrank_config": "config\\GUNE5D\\webfrank.json",
        "webfrank_unit": unit,
        "webfrank_target": f"build/GUNE5D/obj/{unit}.o",
        "webfrank_image": image,
    }
    variables.update(override)
    return {"rule": "webfrank",
            "outputs": [f"build/GUNE5D/src/{unit}.o"],
            "inputs": [f"build/GUNE5D/src/.postprocess/body/{unit}.o"],
            "variables": variables}


def compile_edge(unit):
    return {"rule": "mwcc_sjis",
            "outputs": [f"build/GUNE5D/src/{unit}.o"],
            "inputs": [f"src/{unit}.c"], "variables": {}}


class PostprocessEdgeFilter(unittest.TestCase):
    """A COMPILE edge writing src/<unit>.o is not a postprocessor."""

    def test_a_compile_edge_is_not_a_postprocessor_edge(self):
        data = {"edges": [compile_edge("game/audio/dcs")]}
        self.assertIsNone(ra.postprocess_edge(data, "game/audio/dcs"))

    def test_a_webfrank_edge_is_found(self):
        data = {"edges": [webfrank_edge("game/enemy/enemy")]}
        self.assertIsNotNone(ra.postprocess_edge(data, "game/enemy/enemy"))

    def test_the_live_graph_agrees(self):
        path = ROOT / "build/GUNE5D/build_edges.json"
        if not path.exists():
            self.skipTest("checkout is not configured")
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertIsNone(ra.postprocess_edge(data, "game/audio/dcs"))
        self.assertIsNone(ra.postprocess_edge(data, "game/enemy/enemy"))


class DerivedEdge(unittest.TestCase):
    DONORS = {"edges": [webfrank_edge("game/enemy/enemy"),
                        webfrank_edge("game/ui/attract"),
                        compile_edge("game/audio/dcs")]}

    def test_the_derived_edge_substitutes_only_the_unit(self):
        edge = ra.derive_postprocess_edge(self.DONORS, "game/audio/dcs")
        self.assertEqual(edge["rule"], "webfrank")
        self.assertEqual(edge["variables"]["webfrank_unit"],
                         "game/audio/dcs")
        self.assertEqual(edge["variables"]["webfrank_target"],
                         "build/GUNE5D/obj/game/audio/dcs.o")
        self.assertEqual(edge["variables"]["webfrank_image"], IMAGE)

    def test_the_derivation_is_recorded_not_silent(self):
        edge = ra.derive_postprocess_edge(self.DONORS, "game/audio/dcs")
        self.assertEqual(edge["derived"]["donor_edges"], 2)
        self.assertIn("configure.py emits", edge["derived"]["why"])
        self.assertIn("webfrank_globalize_atree",
                      edge["derived"]["not_modelled"])

    def test_no_surviving_edge_refuses(self):
        with self.assertRaises(ra.Refused):
            ra.derive_postprocess_edge(
                {"edges": [compile_edge("game/audio/dcs")]}, "game/audio/dcs")

    def test_a_donor_missing_a_variable_refuses(self):
        edge = webfrank_edge("game/ui/attract")
        del edge["variables"]["webfrank_image"]
        with self.assertRaises(ra.Refused):
            ra.derive_postprocess_edge({"edges": [edge]}, "game/audio/dcs")

    def test_a_target_that_is_not_a_function_of_the_unit_refuses(self):
        """Substitution is only justified if the shape is uniform."""
        edge = webfrank_edge(
            "game/ui/attract",
            webfrank_target="build/GUNE5D/obj/somewhere/else.o")
        with self.assertRaisesRegex(ra.Refused, "cannot be derived"):
            ra.derive_postprocess_edge({"edges": [edge]}, "game/audio/dcs")

    def test_donors_disagreeing_on_the_image_refuse(self):
        data = {"edges": [webfrank_edge("game/enemy/enemy"),
                          webfrank_edge("game/ui/attract",
                                        image="orig/OTHER/sys/main.dol")]}
        with self.assertRaisesRegex(ra.Refused, "disagree on --image"):
            ra.derive_postprocess_edge(data, "game/audio/dcs")

    def test_the_live_graph_derives(self):
        path = ROOT / "build/GUNE5D/build_edges.json"
        if not path.exists():
            self.skipTest("checkout is not configured")
        data = json.loads(path.read_text(encoding="utf-8"))
        with self.assertRaisesRegex(ra.Refused,'no surviving'):
            ra.derive_postprocess_edge(data, "game/audio/dcs")


class RuleDeltaKeyRemoval(unittest.TestCase):
    """The unit KEY, not an empty list."""

    UNIT, FN = "game/audio/dcs", "update_chinfo"

    def delta(self, before_units, after_units):
        audit = ra.Audit(self.UNIT, self.FN, "HEAD~1")
        audit.before_rules = {"version": 1, "units": before_units}
        with tempfile.TemporaryDirectory(prefix="t3_rd_") as folder:
            path = Path(folder) / "webfrank.json"
            path.write_text(json.dumps({"version": 1, "units": after_units}),
                            encoding="utf-8")
            audit.config = path
            audit.rule_delta()
        return audit.checks[-1]

    def rule(self, name):
        return {"function": name, "before_sha256": "a", "after_sha256": "b"}

    def test_the_last_rule_retired_with_the_key_removed_passes(self):
        check = self.delta({self.UNIT: [self.rule(self.FN)]}, {})
        self.assertEqual(check["status"], "PASS", check["reasons"])
        self.assertTrue(check["last_rule_in_unit"])
        self.assertFalse(check["unit_key_present_after"])

    def test_an_empty_rule_list_left_behind_FAILS(self):
        """`.get(unit, [])` read this as success."""
        check = self.delta({self.UNIT: [self.rule(self.FN)]}, {self.UNIT: []})
        self.assertEqual(check["status"], "FAIL")
        self.assertTrue(any("must be REMOVED" in reason
                            for reason in check["reasons"]), check["reasons"])
        self.assertTrue(check["unit_key_present_after"])

    def test_retiring_one_of_several_rules_keeps_the_key_and_passes(self):
        check = self.delta(
            {self.UNIT: [self.rule(self.FN), self.rule("other")]},
            {self.UNIT: [self.rule("other")]})
        self.assertEqual(check["status"], "PASS", check["reasons"])
        self.assertFalse(check["last_rule_in_unit"])
        self.assertTrue(check["unit_key_present_after"])

    def test_a_rule_that_is_still_pinned_still_FAILS(self):
        check = self.delta({self.UNIT: [self.rule(self.FN)]},
                           {self.UNIT: [self.rule(self.FN)]})
        self.assertEqual(check["status"], "FAIL")
        self.assertTrue(any("STILL rule-served" in reason
                            for reason in check["reasons"]))

    def test_a_function_that_carried_no_rule_still_FAILS(self):
        check = self.delta({self.UNIT: [self.rule("other")]},
                           {self.UNIT: [self.rule("other")]})
        self.assertEqual(check["status"], "FAIL")
        self.assertTrue(any("carried no rule" in reason
                            for reason in check["reasons"]))


if __name__ == "__main__":
    unittest.main()
