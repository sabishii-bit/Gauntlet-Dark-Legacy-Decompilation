#!/usr/bin/env python3
"""T16 run-46 item 8: probe stops silently swallowing flags it does not know.

REPRODUCTION at ea9341850, one command:

    python tools/gdl/probe.py zlib/inflate inflate --revert-bset --no-build
    -> exit 0, verdict BASELINE, "[BEST state banked ...]"

A mistyped `--revert-best` ran an ordinary probe and BANKED. probe parses by
membership (`"--revert" in sys.argv`), so anything it does not recognise was
dropped in silence, and the typo failed in the one direction that overwrites
the state the lane was trying to get back to.

Two-sided, and the negative half is the one that decides whether this can
refuse at all: EVERY flag the tool's own docstring documents must still be
accepted. Measured here over all 37, plus the `--flag=value` spelling.

The second half of the item -- "--fuzzy on CONFLICT exits 255 with no
number" -- did not reproduce as a probe defect. Measured on the same commit:
captured, `probe --fuzzy` exits 0 and prints "FUZZY (fresh report):
100.0000%"; run through `| Select-Object -First 2` the SAME command reports
exit -1 and the FUZZY line is cut off, because it is printed last. That is
AGENTS.md trap 6a, not probe. What WAS real: a readout that produced no
number also exited 0, so a script could not tell it from a good one. It now
exits FUZZY_NO_NUMBER_EXIT (4) and says which of the two situations it is in.
"""

import re
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402


class KnownFlagsAreAccepted(unittest.TestCase):
    """NEGATIVE side: no false refusals."""

    def test_every_known_flag_passes_the_screen(self):
        for flag in sorted(probe.KNOWN_FLAGS):
            self.assertEqual(probe.unknown_flags([flag]), [], flag)

    def test_every_flag_the_docstring_documents_is_known(self):
        doc = probe.__doc__ or ""
        documented = set(re.findall(r"(?<![\w-])(--[a-z][a-z0-9-]+)", doc))
        missing = sorted(documented - set(probe.KNOWN_FLAGS))
        self.assertEqual(missing, [], f"documented but not accepted: {missing}")

    def test_the_flag_equals_value_spelling_is_screened_by_name(self):
        self.assertEqual(probe.unknown_flags(["--restore=mybank"]), [])

    def test_positional_arguments_are_not_flags(self):
        self.assertEqual(
            probe.unknown_flags(["game/x/y", "fn", "mybank"]), [])

    def test_a_bare_double_dash_is_not_a_flag(self):
        self.assertEqual(probe.unknown_flags(["--"]), [])


class UnknownFlagsAreRefused(unittest.TestCase):
    """POSITIVE side."""

    def test_the_measured_typo_is_caught_and_suggested(self):
        found = probe.unknown_flags(["--revert-bset"])
        self.assertEqual(len(found), 1)
        self.assertIn("--revert-best", found[0][1])

    def test_a_flag_from_another_tool_is_caught(self):
        # defake_gate's word, typed into probe by a lane alternating tools
        found = probe.unknown_flags(["--update-improved"])
        self.assertEqual(len(found), 1)

    def test_several_unknowns_are_all_reported(self):
        self.assertEqual(len(probe.unknown_flags(["--nope", "--also-nope"])), 2)


class ScopeConflict(unittest.TestCase):
    def test_both_scope_words_together_are_refused(self):
        self.assertIsNotNone(probe.scope_conflict(["--revert-best",
                                                   "--function",
                                                   "--whole-file"]))

    def test_either_alone_is_fine(self):
        self.assertIsNone(probe.scope_conflict(["--revert-best", "--function"]))
        self.assertIsNone(probe.scope_conflict(["--discard", "--whole-file"]))

    def test_the_message_states_both_defaults(self):
        message = probe.scope_conflict(["--function", "--whole-file"])
        self.assertIn("FUNCTION-scoped", message)
        self.assertIn("WHOLE-FILE", message)


class FuzzyExitCode(unittest.TestCase):
    def test_no_number_has_its_own_exit_code(self):
        self.assertEqual(probe.FUZZY_NO_NUMBER_EXIT, 4)

    def test_the_trap_6a_confusion_is_named_where_it_bites(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        self.assertIn("trap 6a", text)
        self.assertIn("Select-Object -First", text)


class EndToEnd(unittest.TestCase):
    def _run(self, *flags):
        return subprocess.run(
            [sys.executable, "tools/gdl/probe.py", "zlib/inflate", "inflate",
             *flags], cwd=str(REPO), capture_output=True, text=True)

    def test_a_typo_refuses_instead_of_probing(self):
        if not (REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
                ).exists():
            self.skipTest("zlib/inflate not built")
        result = self._run("--revert-bset", "--no-build")
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIn("unknown flag --revert-bset", result.stdout)
        self.assertNotIn("BASELINE", result.stdout)

    def test_contradictory_scope_refuses(self):
        result = self._run("--revert-best", "--function", "--whole-file")
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIn("opposite restore scopes", result.stdout)


if __name__ == "__main__":
    unittest.main()
