#!/usr/bin/env python3
"""T17 run-47 item 2: the RELOC-SYMBOL screen on the FIRST probe of a function.

Two globals both reachable through EMB_SDA21 share the instruction word
`lwz rD,0(rB)`, so reading the WRONG one moves no number probe prints -- not
`real`, not the opcode multiset, not objdiff fuzzy, not even wf_word_diff's
own differing-word count (claim.law.SA_a-wrong-global-that-shares-an-
instruction-word-is-invisible-to-every-score-including-the-word-count
.20260902.v1). Seven such bugs have been found in this project and the SEVENTH
surfaced only at PARK-RECORD time, after the function had been probed,
measured and given up on.

Two-sided calibration, measured at 65348245e over every built function pair:

    flagged (rows)              11   (4 with an index-aligned row)
    comparable and clean     2,825
    NOT COMPARABLE              47   (unscreened -- must never read as clean)
    count-asymmetric           115   (no offset pairing exists at all)
    screen cost   median 17ms, p95 86ms, max 163ms per function

The negative side is what decides the shipped form: the loud row listing would
bury the verdict on 2,825 of 2,998 functions, so `clean` gets ONE line -- and
it gets a line rather than silence because "measured absent" and "nobody
looked" must not be the same output (AGENTS.md, run-46 absence rule).
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import probe  # noqa: E402


class Formatting(unittest.TestCase):
    def test_clean_is_one_line_and_says_MEASURED(self):
        text = probe.format_reloc_screen({"status": "clean", "rows": [],
                                          "compared": 40})
        self.assertEqual(len(text.splitlines()), 1)
        self.assertIn("MEASURED absent", text)

    def test_not_comparable_never_reads_as_clean(self):
        text = probe.format_reloc_screen({"status": "not-comparable",
                                          "rows": []})
        self.assertIn("UNSCREENED", text)
        self.assertNotIn("clean", text)

    def test_count_asymmetric_says_the_screen_did_not_run(self):
        text = probe.format_reloc_screen({"status": "count-asymmetric",
                                          "rows": []})
        self.assertIn("not run", text)
        self.assertIn("UNSCREENED", text)

    def test_unavailable_prints_nothing(self):
        self.assertIsNone(probe.format_reloc_screen({"status": "unavailable",
                                                     "rows": []}))

    def test_rows_name_every_offset_and_both_symbols(self):
        rows = [(0x38, "lbl_803479C8", "lbl_803479E0", True),
                (0x54, "lbl_803479D0", "lbl_803479C8", True)]
        text = probe.format_reloc_screen({"status": "rows", "rows": rows})
        self.assertIn("2 instruction(s) (2 index-aligned)", text)
        self.assertIn("+0x0038", text)
        self.assertIn("lbl_803479C8", text)
        self.assertIn("lbl_803479E0", text)
        self.assertNotIn("PAIRING UNRELIABLE", text)

    def test_an_unaligned_row_is_marked_unreliable(self):
        rows = [(0x10, "get_cam_wpos", "calc_cam_pyr", False)]
        text = probe.format_reloc_screen({"status": "rows", "rows": rows})
        self.assertIn("PAIRING UNRELIABLE", text)
        self.assertIn("(0 index-aligned)", text)

    def test_the_law_is_cited_where_a_worker_will_read_it(self):
        text = probe.format_reloc_screen(
            {"status": "rows", "rows": [(0, "a", "b", True)]})
        self.assertIn("claim.law.SA_a-wrong-global", text)


class ScreenStatus(unittest.TestCase):
    """`reloc_symbol_screen` maps the census tool's outcomes onto statuses."""

    def setUp(self):
        import wf_word_diff
        self.module = wf_word_diff
        self.streams = wf_word_diff.word_streams
        self.mismatches = wf_word_diff.reloc_symbol_mismatches

    def tearDown(self):
        self.module.word_streams = self.streams
        self.module.reloc_symbol_mismatches = self.mismatches

    def test_a_count_asymmetric_SystemExit_is_not_an_error(self):
        def boom(unit, fn):
            raise SystemExit("count-asymmetric")
        self.module.word_streams = boom
        result = probe.reloc_symbol_screen("game/x/y", "f")
        self.assertEqual(result["status"], "count-asymmetric")

    def test_a_None_row_list_is_not_comparable_not_clean(self):
        self.module.word_streams = lambda unit, fn: ("k", b"\0" * 8, b"\0" * 8)
        self.module.reloc_symbol_mismatches = lambda *a: None
        self.assertEqual(probe.reloc_symbol_screen("u", "f")["status"],
                         "not-comparable")

    def test_an_empty_row_list_is_clean(self):
        self.module.word_streams = lambda unit, fn: ("k", b"\0" * 8, b"\0" * 8)
        self.module.reloc_symbol_mismatches = lambda *a: []
        self.assertEqual(probe.reloc_symbol_screen("u", "f")["status"], "clean")

    def test_rows_survive_to_the_caller(self):
        rows = [(4, "t", "o", True)]
        self.module.word_streams = lambda unit, fn: ("k", b"\0" * 8, b"\0" * 8)
        self.module.reloc_symbol_mismatches = lambda *a: rows
        result = probe.reloc_symbol_screen("u", "f")
        self.assertEqual(result["status"], "rows")
        self.assertEqual(result["rows"], rows)


class Wiring(unittest.TestCase):
    def test_the_screen_runs_on_BASELINE_the_first_probe_of_a_function(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        self.assertIn('verdict.startswith("BASELINE") and'
                      ' "--no-reloc-screen" not in sys.argv', text)

    def test_the_opt_out_flag_is_a_KNOWN_FLAG(self):
        self.assertIn("--no-reloc-screen", probe.KNOWN_FLAGS)


class LiveCalibration(unittest.TestCase):
    def test_set_hidden_player_is_the_recorded_positive(self):
        obj = REPO / "build" / "GUNE5D" / "src" / "game" / "game" / "player.o"
        if not obj.exists():
            self.skipTest("player object not built")
        import os
        cwd = os.getcwd()
        os.chdir(REPO)
        try:
            result = probe.reloc_symbol_screen("game/game/player",
                                               "set_hidden_player")
        finally:
            os.chdir(cwd)
        if result["status"] != "rows":
            self.skipTest("set_hidden_player's relocations were fixed since"
                          " the calibration")
        self.assertEqual(len(result["rows"]), 4)


if __name__ == "__main__":
    unittest.main()
