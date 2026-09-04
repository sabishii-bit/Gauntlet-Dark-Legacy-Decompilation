"""Chained webfrank stages, and the permutation window's own four hashes.

Run-53 item 3, from claim.law.CX_a-webfrank-function-can-carry-chained-rule-
stages-and-replaying-one-alone-refuses-on-a-hash-that-is-not-drift.20260904.v1.

Reproduced at c7b741799 before the fix:
  * `wr_try_rule.py game/world/btricol LineLineDist <stage0>` ->
    `REFUSED (raw postprocess body): LineLineDist: output hash cb0b03c8... !=
    expected 06e7f0aa...` — the same message a DRIFTED pin produces, on a pin
    that reads real 0.
  * A hand-written `instruction_permutation` window with no per-window hashes
    -> bare `KeyError: 'before_sha256'` from webfrank.py:4058.

TWO-SIDED CALIBRATION, measured live at c7b741799 over
config/GUNE5D/webfrank.json:
  POSITIVE — 155 entries over 153 distinct (unit, function) pins; exactly 2
  functions carry more than one entry (btricol::LineLineDist, message::
  msgDraw) and BOTH chain (entry[i].after_sha256 == entry[i+1].before_sha256).
  63 entries carry instruction_permutation, over 75 windows.
  NEGATIVE — of those 75 shipped windows, ZERO are missing any of the four
  hashes, so the new window screen refuses nothing that ships; and the 151
  single-entry functions take the same one-stage path they always did.
"""
import json
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import wr_try_rule as wr  # noqa: E402

CONFIG = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")


def shipped_units():
    with open(CONFIG, encoding="utf-8") as handle:
        return json.load(handle)["units"]


class PermutationWindowScreenTests(unittest.TestCase):
    def test_the_object_spelling_is_one_window(self):
        stage = {"instruction_permutation": {"start": "0x0", "end": "0x8"}}
        self.assertEqual(len(wr.permutation_windows(stage)), 1)

    def test_the_list_spelling_is_read_too(self):
        """webfrank accepts either; a screen reading only the object
        spelling would miss every multi-window rule."""
        stage = {"instruction_permutation": [{"start": "0x0"},
                                             {"start": "0x8"}]}
        self.assertEqual(len(wr.permutation_windows(stage)), 2)

    def test_a_stage_with_no_permutation_has_no_windows(self):
        self.assertEqual(wr.permutation_windows({"copy_register_fields": True}),
                         [])

    def test_all_four_hashes_are_required(self):
        stage = {"instruction_permutation": {"start": "0x14", "end": "0x1c",
                                             "order": [1, 0]}}
        (_window, absent), = wr.missing_window_hashes(stage)
        self.assertEqual(sorted(absent),
                         sorted(wr.PERMUTATION_HASH_KEYS))

    def test_a_complete_window_is_not_flagged(self):
        stage = {"instruction_permutation": dict(
            {"start": "0x14", "end": "0x1c", "order": [1, 0]},
            **{key: "0" * 64 for key in wr.PERMUTATION_HASH_KEYS})}
        self.assertEqual(wr.missing_window_hashes(stage), [])

    def test_no_shipped_window_would_be_refused(self):
        """The negative half: the screen must refuse nothing that ships."""
        short = []
        for unit, rows in shipped_units().items():
            for row in rows:
                for window, absent in wr.missing_window_hashes(row):
                    short.append((unit, row.get("function"),
                                  window.get("start"), absent))
        self.assertEqual(short, [])


class ShippedChainTests(unittest.TestCase):
    def test_entries_come_back_in_file_order_and_are_not_deduplicated(self):
        """File order IS application order, and a function's entries are
        STAGES: a reader taking 'the' entry silently takes the last one."""
        stages = wr.shipped_entries(ROOT, "game/world/btricol", "LineLineDist")
        self.assertEqual(len(stages), 2)
        self.assertEqual(stages[0]["after_sha256"], stages[1]["before_sha256"])

    def test_a_single_entry_function_yields_one_stage(self):
        stages = wr.shipped_entries(ROOT, "game/mb/mb_font", "MBRenderText")
        self.assertEqual(len(stages), 1)

    def test_an_unpinned_function_yields_nothing(self):
        self.assertEqual(
            wr.shipped_entries(ROOT, "game/world/btricol", "NoSuchFunction"),
            [])

    def test_every_multi_entry_function_actually_chains(self):
        """If a pair is ever found that does NOT chain, the law is falsified
        and folding them in file order is the wrong thing to do."""
        broken = []
        for unit, rows in shipped_units().items():
            by_function = {}
            for row in rows:
                by_function.setdefault(row.get("function"), []).append(row)
            for function, staged in by_function.items():
                for index in range(len(staged) - 1):
                    if (staged[index].get("after_sha256")
                            != staged[index + 1].get("before_sha256")):
                        broken.append((unit, function, index))
        self.assertEqual(broken, [])


if __name__ == "__main__":
    unittest.main()
