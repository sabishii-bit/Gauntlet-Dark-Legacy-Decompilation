"""`--help` must never do work, and an unknown flag must never be swallowed.

Run-53 item 2. All four reported tools shared one cause: they read `sys.argv`
with no argparse and either indexed it positionally (`hv_try.py` -> IndexError
on `--help`) or filtered with `arg.startswith("--")`, which discards every
flag the tool does not implement. `build_rule.py --help` ran the whole
pb_window composition and WROTE a rules JSON — a help flag with a side effect
on disk.

Calibration is in `tools/gdl/cliscreen.py`'s docstring: 53 of 128 arg-reading
modules under tools/gdl are in the class (positive side), and ZERO live
invocations in the record corpus, AGENTS.md, README.md or the test suite pass
a flag these four tools do not implement (negative side) — which is why the
screen refuses rather than warns.
"""
import io
import os
import subprocess
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cliscreen  # noqa: E402

# tools/gdl/tests/<this file> -> repo root is FOUR levels up. Three was one
# short and produced `tools/tools/gdl/build_rule.py`, which the subprocess
# reported as a missing file rather than as a bad path.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))


class UnknownFlagTests(unittest.TestCase):
    def test_an_unimplemented_flag_is_named(self):
        self.assertEqual(cliscreen.unknown_flags(["--bogus"], ("--out",)),
                         ["--bogus"])

    def test_an_implemented_flag_passes(self):
        self.assertEqual(cliscreen.unknown_flags(["--out", "x"], ("--out",)),
                         [])

    def test_the_equals_spelling_is_screened_by_NAME(self):
        """`--out=PATH` is build_rule's own spelling; screening the whole
        token would report the tool's documented flag as unknown."""
        self.assertEqual(
            cliscreen.unknown_flags(["--out=build/x.json"], ("--out",)), [])
        self.assertEqual(
            cliscreen.unknown_flags(["--nope=1"], ("--out",)), ["--nope"])

    def test_positionals_and_short_dashes_are_not_flags(self):
        self.assertEqual(
            cliscreen.unknown_flags(["game/pb/pb_window", "fn", "-"], ()), [])

    def test_a_bare_double_dash_ends_flag_parsing(self):
        self.assertEqual(
            cliscreen.unknown_flags(["--", "--not-a-flag"], ()), [])

    def test_help_is_always_known(self):
        self.assertEqual(cliscreen.unknown_flags(["--help", "-h"], ()), [])


class ScreenExitTests(unittest.TestCase):
    def test_help_exits_zero_before_any_work(self):
        buffer = io.StringIO()
        with self.assertRaises(SystemExit) as raised, redirect_stdout(buffer):
            cliscreen.screen_argv(["--help"], ("--out",), usage="usage: x",
                                  doc="what x does")
        self.assertEqual(raised.exception.code, 0)
        self.assertIn("usage: x", buffer.getvalue())
        self.assertIn("what x does", buffer.getvalue())

    def test_an_unknown_flag_exits_two_and_lists_the_vocabulary(self):
        """Status 2, argparse's usage-error status — NOT the string form.

        `raise SystemExit("text")` prints the text but sets `.code` to the
        string and the process status to 1, which is the status a tool that
        merely failed returns. A caller cannot then tell "you typed a flag I
        do not have" from "the run went wrong".
        """
        buffer = io.StringIO()
        with self.assertRaises(SystemExit) as raised, \
                redirect_stderr(buffer):
            cliscreen.screen_argv(["--bogus"], ("--out",), usage="usage: x")
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("--bogus", buffer.getvalue())
        self.assertIn("--out", buffer.getvalue())

    def test_a_clean_argv_returns_and_does_not_raise(self):
        self.assertIsNone(
            cliscreen.screen_argv(["game/pb/pb_window", "--out=x"], ("--out",)))


class ReportedToolsSurviveHelpTests(unittest.TestCase):
    """The end-to-end reproduction: each of the four reported invocations.

    These are subprocess runs because the defect was in module-level and
    `main()`-entry behaviour, which importing the module cannot exercise.
    """

    TOOLS = (
        ("tools/gdl/rule_derive.py", "Re-derive permutation specs"),
        ("tools/gdl/build_rule.py", "Compose the pb_window rules"),
        ("tools/gdl/composed_census/hv_try.py", "usage: hv_try.py"),
        ("tools/gdl/composed_census/hv_formfirst.py", "usage: hv_formfirst.py"),
    )

    def test_help_exits_zero_and_prints_usage(self):
        for relative, expected in self.TOOLS:
            with self.subTest(tool=relative):
                done = subprocess.run(
                    [sys.executable, os.path.join(ROOT, *relative.split("/")),
                     "--help"],
                    cwd=ROOT, capture_output=True, text=True, timeout=300)
                self.assertEqual(done.returncode, 0, done.stderr[-800:])
                self.assertIn(expected, done.stdout)

    def test_help_writes_no_artifact(self):
        """`build_rule.py --help` used to WRITE its rules JSON."""
        artifact = os.path.join(ROOT, "build", "GUNE5D", "rules",
                                "t23_help_side_effect_probe.json")
        if os.path.exists(artifact):
            os.remove(artifact)
        done = subprocess.run(
            [sys.executable, os.path.join(ROOT, "tools", "gdl",
                                          "build_rule.py"),
             f"--out={artifact}", "--help"],
            cwd=ROOT, capture_output=True, text=True, timeout=300)
        self.assertEqual(done.returncode, 0, done.stderr[-800:])
        self.assertFalse(os.path.exists(artifact),
                         "--help must not write anything")


if __name__ == "__main__":
    unittest.main()
