#!/usr/bin/env python3
"""T16 run-46 item 4: re-deriving a webfrank pin whose BODY hash moved.

The recovery path before this was pasting the hash out of webfrank's own
abort message ("input hash X != expected Y"), done by hand twice in run 45.
That paste re-blesses whatever the compiler now emits without ever re-running
the rule, so a rule that has stopped closing its function looks exactly like
one that still does.

Two-sided: the pastes that must happen are tested next to the refusals that
must happen. Live end-to-end measurement at 05b3e534a on
game/anim/atree::fn_8001267C, against a SCRATCH copy of webfrank.json:

  before_sha256 zeroed  -> BODY-MOVED, BYTE-EQUAL after re-hashing (43 words
                           changed), --apply pasted 1 hash, file length delta
                           0 bytes (no reformat), re-run reads UNCHANGED
  after_sha256 zeroed   -> TARGET-MOVED, exit 2, config left untouched
  untouched rule        -> UNCHANGED, exit 0, nothing written
"""

import json
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import t16_rederive_body as rb  # noqa: E402


def rule(before="AA", after="BB", windows=None):
    out = {"function": "fn", "before_sha256": before, "after_sha256": after}
    if windows is not None:
        out["instruction_permutation"] = windows
    return out


class Classification(unittest.TestCase):
    def test_matching_hashes_are_unchanged(self):
        verdict, moved = rb.classify_move(
            rule(), {"before_sha256": "AA", "after_sha256": "BB"})
        self.assertEqual((verdict, moved), ("UNCHANGED", []))

    def test_a_moved_body_hash_is_body_moved(self):
        verdict, moved = rb.classify_move(
            rule(), {"before_sha256": "CC", "after_sha256": "BB"})
        self.assertEqual(verdict, "BODY-MOVED")
        self.assertEqual(moved, ["before_sha256"])

    def test_a_moved_target_hash_outranks_everything(self):
        verdict, _ = rb.classify_move(
            rule(), {"before_sha256": "CC", "after_sha256": "DD"})
        self.assertEqual(verdict, "TARGET-MOVED")

    def test_a_window_only_move_is_its_own_verdict(self):
        windows = [{"order": [0, 1], "before_sha256": "W0",
                    "after_sha256": "W1",
                    "before_relocations_sha256": "R0",
                    "after_relocations_sha256": "R1"}]
        verdict, moved = rb.classify_move(
            rule(windows=windows),
            {"before_sha256": "AA", "after_sha256": "BB",
             ("window", 0, "before_sha256"): "W9"})
        self.assertEqual(verdict, "WINDOW-MOVED")
        self.assertEqual(moved, [("window", 0, "before_sha256")])


class CandidateRule(unittest.TestCase):
    def test_the_original_rule_is_never_mutated(self):
        original = rule()
        new = rb.candidate_rule(original, {"before_sha256": "CC"})
        self.assertEqual(original["before_sha256"], "AA")
        self.assertEqual(new["before_sha256"], "CC")

    def test_window_slots_are_written_by_index(self):
        windows = [{"order": [0], "before_sha256": "W0"},
                   {"order": [0], "before_sha256": "X0"}]
        new = rb.candidate_rule(
            rule(windows=windows), {("window", 1, "before_sha256"): "X9"})
        self.assertEqual(
            new["instruction_permutation"][0]["before_sha256"], "W0")
        self.assertEqual(
            new["instruction_permutation"][1]["before_sha256"], "X9")

    def test_a_single_window_dict_is_handled(self):
        window = {"order": [0], "before_sha256": "W0"}
        new = rb.candidate_rule(
            rule(windows=window), {("window", 0, "before_sha256"): "W9"})
        self.assertEqual(
            new["instruction_permutation"]["before_sha256"], "W9")


class PastePairs(unittest.TestCase):
    def test_a_windows_own_before_hash_is_swapped_last(self):
        windows = [{"order": [0], "before_sha256": "W0",
                    "after_relocations_sha256": "R1"}]
        pairs = rb.paste_pairs(
            rule(windows=windows),
            {("window", 0, "before_sha256"): "W9",
             ("window", 0, "after_relocations_sha256"): "R9"},
            [("window", 0, "before_sha256"),
             ("window", 0, "after_relocations_sha256")])
        self.assertEqual([p[1] for p in pairs],
                         ["after_relocations_sha256", "before_sha256"])
        # the sibling swap is anchored on the window's still-present hash
        self.assertEqual(pairs[0][0], "W0")
        self.assertIsNone(pairs[1][0])


class LiveRule(unittest.TestCase):
    """The shipped config is READ ONLY here; nothing writes."""

    UNIT, FN = "game/anim/atree", "fn_8001267C"

    def setUp(self):
        cfg = REPO / "config" / "GUNE5D" / "webfrank.json"
        self.rule = next(
            (r for r in json.loads(cfg.read_text(encoding="utf-8"))
             .get("units", {}).get(self.UNIT, [])
             if r.get("function") == self.FN), None)
        if self.rule is None:
            self.skipTest("atree::fn_8001267C rule not in webfrank.json")
        body = (REPO / "build" / "GUNE5D" / "src" / "game" / "anim"
                / ".postprocess" / "body" / "atree.o")
        target = REPO / "build" / "GUNE5D" / "obj" / "game" / "anim" / "atree.o"
        if not (body.exists() and target.exists()):
            self.skipTest("atree objects not built")
        self.our = bytearray(body.read_bytes())
        self.target = bytearray(target.read_bytes())

    def test_a_current_pin_derives_to_its_own_hashes(self):
        derived = rb.derive_slots(self.rule, self.our, self.target, self.FN)
        verdict, moved = rb.classify_move(self.rule, derived)
        self.assertEqual((verdict, moved), ("UNCHANGED", []),
                         f"derived={derived}")

    def test_every_slot_the_rule_carries_is_derived(self):
        derived = rb.derive_slots(self.rule, self.our, self.target, self.FN)
        self.assertIn("before_sha256", derived)
        self.assertIn("after_sha256", derived)
        if "instruction_permutation" in self.rule:
            self.assertIn(("window", 0, "before_relocations_sha256"), derived)


class DocumentedContract(unittest.TestCase):
    def test_the_tool_advertises_its_importable_core(self):
        text = (REPO / "tools" / "gdl" / "composed_census"
                / "t16_rederive_body.py").read_text(encoding="utf-8")
        self.assertIn("IMPORTABLE CORE:", text)
        for name in ("derive_slots", "classify_move", "candidate_rule"):
            self.assertTrue(callable(getattr(rb, name)), name)

    def test_it_never_writes_without_apply(self):
        text = (REPO / "tools" / "gdl" / "composed_census"
                / "t16_rederive_body.py").read_text(encoding="utf-8")
        self.assertIn("if not arguments.apply:", text)


if __name__ == "__main__":
    unittest.main()
