"""defake_gate refuses unknown flags instead of swallowing them (run 52).

A SIBLING-CALL-SITE finding, not a new one: run 46 closed exactly this hole
in probe.py ("probe swallowed unknown flags 45 runs") and left the tool
probe is alternated with in the same edit loop wide open.  Both build their
positional list as `[a for a in argv if not a.startswith("--")]` and test
every flag by membership, so an unrecognised flag is discarded in silence.

Reproduced at da7eee6c7 before the fix:

    $ python tools/gdl/defake_gate.py check game/enemy/critter \
          --arbitrate-fuzzy --nonsense
    [baseline was taken at commit dc40326c7, ...]
      PROGRESS SPLIT since this baseline: ...
    GATE OK
    exit=0

Two flags gone, `GATE OK`, exit 0 — the gate answered a different question
from the one asked and said nothing.  The same command through probe.py has
exited 2 with `unknown flag --nonsense` since run 46.

NEGATIVE SIDE, measured before the refusal shipped: every defake_gate
invocation spelled anywhere under tools/gdl — sources, tests and the tool's
own usage block — uses one of --rebuild, --update-improved, --arbiter,
--bank-arbitrated= or --at-head.  All five are in KNOWN_FLAGS, so the
refusal costs zero working call sites.  The class below pins that.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from defake_gate import KNOWN_FLAGS, unknown_flags  # noqa: E402


class UnknownFlagTests(unittest.TestCase):
    def test_a_bogus_flag_is_reported_with_suggestions(self):
        out = unknown_flags(["check", "game/enemy/critter",
                             "--arbitrate-fuzzy"])
        self.assertEqual(len(out), 1)
        flag, close = out[0]
        self.assertEqual(flag, "--arbitrate-fuzzy")
        self.assertIn("--arbiter", close)
        self.assertIn("--arbitrate", close)

    def test_the_run_51_reproduction_is_caught(self):
        out = unknown_flags(["check", "game/enemy/critter",
                             "--arbitrate-fuzzy", "--nonsense"])
        self.assertEqual([flag for flag, _c in out],
                         ["--arbitrate-fuzzy", "--nonsense"])

    def test_a_flag_with_no_near_miss_still_refuses(self):
        out = unknown_flags(["--nonsense"])
        self.assertEqual(out[0][0], "--nonsense")
        self.assertEqual(out[0][1], [])

    def test_value_carrying_flags_are_matched_on_their_name(self):
        self.assertEqual(
            unknown_flags(["--bank-arbitrated=get_vmu_directory",
                           "--rename=a=b", "--arbiter=fuzzy"]), [])

    def test_a_bare_double_dash_is_not_a_flag(self):
        self.assertEqual(unknown_flags(["--", "check", "game/x/y"]), [])

    def test_the_arbiter_value_is_positional_not_a_flag(self):
        self.assertEqual(
            unknown_flags(["check", "game/x/y", "--arbiter", "fuzzy"]), [])


class NegativeSideTests(unittest.TestCase):
    """The five spellings the project actually uses must all pass."""

    IN_USE = ["--rebuild", "--update-improved", "--arbiter",
              "--bank-arbitrated=fn", "--at-head",
              # documented aliases and escapes, same census
              "--build", "--arbitrate", "--rebase-best", "--ignore-claim",
              "--rename=old=new"]

    def test_no_documented_spelling_is_refused(self):
        self.assertEqual(unknown_flags(self.IN_USE), [])

    def test_every_known_flag_is_read_somewhere_in_the_cli(self):
        # A flag in the set that nothing reads would accept a spelling
        # that still does nothing — the same failure one layer in. The
        # window is main() and everything defined after it (measure_unit,
        # run_roster, run_single: the whole CLI half of the file), which
        # is where every flag this set names is consumed.
        source = (Path(__file__).resolve().parent.parent
                  / "defake_gate.py").read_text(encoding="utf-8")
        body = source.split("\ndef main():", 1)[1]
        for flag in sorted(KNOWN_FLAGS):
            self.assertIn(flag, body, f"{flag} is accepted but unread")


if __name__ == "__main__":
    unittest.main()
