#!/usr/bin/env python3
"""T17 run-47 item 8: --fuzzy's exit-4, and the number printed ABOVE truncation.

Two halves, and only one of them was still open.

VERIFIED PRESENT (T16's work landed): `probe.py <unit> <fn> --fuzzy` returns
FUZZY_NO_NUMBER_EXIT = 4 when the readout produced no number. Before that,
"the report build failed" and "here is your fuzzy" were the same exit code to
any script, and the CONFLICT arbitration this readout exists to serve is
exactly the caller that must be able to tell them apart. The constant is
distinct from every other exit this tool returns (0 ok, 1 error, 2 refused,
3 CONFLICT-UNARBITRATED, 5 partial restore).

STILL OPEN, and fixed here: the number was printed LAST. Measured at
ecf51590f on game/world/camera::camera_mode_dest --fuzzy, the FUZZY line was
line 9 of 10 -- below the READOUT line, below a multi-line standing verdict,
and below the first-bank note. That is the buried-number half of AGENTS.md
trap 6a: `| Select-Object -First N` reports -1 for a run that exited 0 AND
truncates the output, so the one number this mode exists to produce was the
first casualty. After the fix the same command prints it as line 1 of 10.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class ExitCodes(unittest.TestCase):
    def test_the_no_number_exit_landed_and_is_four(self):
        self.assertEqual(probe.FUZZY_NO_NUMBER_EXIT, 4)

    def test_every_probe_exit_code_is_distinct(self):
        codes = {0, 1, 2, 3, probe.FUZZY_NO_NUMBER_EXIT,
                 probe.PARTIAL_RESTORE_EXIT}
        self.assertEqual(len(codes), 6)

    def test_the_no_number_branch_returns_it(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        self.assertIn("return FUZZY_NO_NUMBER_EXIT", text)


class HeadlineIsSeparateFromItsNotes(unittest.TestCase):
    """`measure_fuzzy` prints nothing; the caller decides the order."""

    def setUp(self):
        self.real = probe.report_fuzzy

    def tearDown(self):
        probe.report_fuzzy = self.real

    def test_the_headline_carries_the_number(self):
        probe.report_fuzzy = lambda *a: 97.5645
        state = {}
        headline, notes = probe.measure_fuzzy("u", "f", "f", state)
        self.assertEqual(headline, "FUZZY (fresh report): 97.5645%")
        self.assertEqual(notes, [])
        self.assertEqual(state["last_fuzzy"], 97.5645)

    def test_the_previous_value_rides_on_the_headline_not_a_note(self):
        probe.report_fuzzy = lambda *a: 98.0
        headline, _ = probe.measure_fuzzy("u", "f", "f",
                                          {"last_fuzzy": 97.5})
        self.assertIn("(prev 97.5000)", headline)

    def test_the_caching_lines_are_NOTES_so_they_sort_below(self):
        probe.report_fuzzy = lambda *a: 98.0
        state = {"best_bytes": "abc"}
        headline, notes = probe.measure_fuzzy("u", "f", "f", state,
                                              digest="abc")
        self.assertTrue(headline.startswith("FUZZY"))
        self.assertEqual(len(notes), 1)
        self.assertIn("BEST-STATE fuzzy anchor", notes[0])
        self.assertEqual(state["best_fuzzy"], 98.0)

    def test_no_number_gives_no_headline_and_an_explaining_note(self):
        probe.report_fuzzy = lambda *a: None
        headline, notes = probe.measure_fuzzy("u", "f", "f", {})
        self.assertIsNone(headline)
        self.assertIn("no number", notes[0])

    def test_a_raising_reader_is_a_no_number_not_a_crash(self):
        def boom(*a):
            raise RuntimeError("report build died")
        probe.report_fuzzy = boom
        headline, notes = probe.measure_fuzzy("u", "f", "f", {})
        self.assertIsNone(headline)
        self.assertIn("readout failed", notes[0])

    def test_nothing_is_cached_when_there_is_no_number(self):
        probe.report_fuzzy = lambda *a: None
        state = {}
        probe.measure_fuzzy("u", "f", "f", state, digest="abc")
        self.assertEqual(state, {})


class PrintOrder(unittest.TestCase):
    def test_the_headline_is_printed_before_the_READOUT_line(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        block = text[text.index('    if "--fuzzy" in sys.argv:'):]
        block = block[:block.index("return FUZZY_NO_NUMBER_EXIT")]
        self.assertLess(block.index("print(headline)"),
                        block.index('print(f"READOUT   real {real}'))

    def test_measure_fuzzy_prints_nothing_itself(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        start = text.index("def measure_fuzzy(")
        body = text[start:text.index("REPLAN_AT = 3")]
        self.assertNotIn("print(", body)


if __name__ == "__main__":
    unittest.main()
