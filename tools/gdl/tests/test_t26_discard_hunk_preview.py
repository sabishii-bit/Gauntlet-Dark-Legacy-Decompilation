"""Run-56 item 8: a refusal that names hunks by line number alone.

REPRODUCED at e810cbeae with the reporter's command, on a real TU edited in
two places (`u8 unused[16]` -> `[24]` inside TextHeightMLines, and a new local
in the sibling FontHeight):

    $ python tools/gdl/probe.py game/ui/btext TextHeightMLines --discard
    REFUSED: --discard would restore ALL of game/ui/btext to HEAD.
      1 uncommitted hunk(s) lie OUTSIDE TextHeightMLines (L1129-L1130) - ...
      WHICH REMEDY depends on something probe CANNOT see: whether those hunks
      are ANOTHER function's work or the other half of the edit you made to
      TextHeightMLines. Read `git diff -- game/ui/btext` and decide ...

The message asks the reader a question it then makes them leave to answer.
Printing the hunk's own lines answers it in place.

A SECOND DEFECT, unreported, found while reproducing the first: the remedy
command it printed does not work. `git diff -- game/ui/btext` is a UNIT KEY,
not a path -- measured in that same tree, it printed ZERO lines and exited 0
while `git diff -- src/game/ui/btext.c` printed 21. So the reader was sent to
a command whose empty output says the opposite of the refusal above it.

TWO-SIDED: the preview is additive and conditional. With source lines it adds
the hunk bodies; with `source_lines=None` -- every existing caller, including
the five in the suite that pass five arguments -- the text is byte-identical
to before, which is what the last test here pins.
"""
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))

import probe                                                     # noqa: E402


class HunkPreview(unittest.TestCase):

    LINES = [f"line {n}" for n in range(1, 41)]

    def test_the_outside_hunk_bodies_are_printed_with_line_numbers(self):
        text = probe.discard_refusal(
            "alpha", "game/x/y", 1, 1, [("outside", 10, 12)],
            source_lines=self.LINES)
        self.assertIn("[outside L10-12]", text)
        self.assertIn("10| line 10", text)
        self.assertIn("11| line 11", text)
        self.assertNotIn("12| line 12", text)   # the span end is exclusive

    def test_a_straddling_hunk_body_is_printed_too(self):
        text = probe.discard_refusal(
            "alpha", "game/x/y", 0, 0, [("straddling", 5, 7)],
            source_lines=self.LINES)
        self.assertIn("[straddling L5-7]", text)
        self.assertIn("5| line 5", text)

    def test_long_hunks_and_many_hunks_are_both_capped(self):
        many = [("outside", n, n + 1) for n in range(1, 12)]
        text = probe.discard_refusal("alpha", "game/x/y", 1, len(many), many,
                                     source_lines=self.LINES)
        self.assertIn("more hunk(s) not shown", text)
        big = [("outside", 1, 40)]
        text = probe.discard_refusal("alpha", "game/x/y", 1, 1, big,
                                     source_lines=self.LINES)
        self.assertIn("more line(s)", text)

    def test_an_out_of_range_span_does_not_raise(self):
        """The spans index the WORKING file; a caller passing a stale or
        truncated line list must degrade, never crash a refusal path."""
        text = probe.discard_refusal(
            "alpha", "game/x/y", 1, 1, [("outside", 900, 950)],
            source_lines=self.LINES)
        self.assertIn("REFUSED", text)


class TheDiffCommandIsRunnable(unittest.TestCase):

    def test_a_unit_key_becomes_a_real_pathspec(self):
        self.assertEqual(probe._diff_pathspec("game/ui/btext"),
                         "src/game/ui/btext.c")
        self.assertEqual(probe._diff_pathspec("game/ui/btext.c"),
                         "src/game/ui/btext.c")
        self.assertEqual(probe._diff_pathspec("src/game/movie/movieplayer.cpp"),
                         "src/game/movie/movieplayer.cpp")

    def test_an_explicit_path_wins_over_the_guess(self):
        self.assertEqual(
            probe._diff_pathspec("game/ui/btext",
                                 Path("src/game/ui/btext.c")),
            "src/game/ui/btext.c")

    def test_the_refusal_prints_the_runnable_form(self):
        text = probe.discard_refusal("alpha", "game/ui/btext", 1, 1,
                                     [("outside", 10, 12)])
        self.assertIn("git diff -- src/game/ui/btext.c", text)
        self.assertNotIn("git diff -- game/ui/btext`", text)


class TheOldCallShapeIsUnchanged(unittest.TestCase):
    """The negative side: five-argument callers must see the old text."""

    def test_no_preview_without_source_lines(self):
        text = probe.discard_refusal("alpha", "game/x/y", 1, 1,
                                     [("outside", 10, 12)])
        self.assertIn("L10-L12", text)
        self.assertNotIn("[outside L10-12]", text)
        self.assertNotIn("| line", text)

    def test_the_existing_assertions_still_hold(self):
        text = probe.discard_refusal("alpha", "game/x/y", 1, 2,
                                     [("outside", 10, 12), ("outside", 40, 41),
                                      ("straddling", 60, 62)])
        self.assertIn("2 uncommitted hunk(s) lie OUTSIDE alpha"
                      " (L10-L12, L40-L41)", text)
        self.assertIn("1 hunk(s) STRADDLE alpha's boundary (L60-L62)", text)
        self.assertNotIn("straddling L", text)


if __name__ == "__main__":
    unittest.main()
