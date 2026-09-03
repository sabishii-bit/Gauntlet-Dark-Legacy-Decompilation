"""rule_derive --emit: the draft webfrank entry (run 41 item 10).

Authoring a rule meant reading webfrank.py's `apply_patch` dispatch to learn
which verifier a given rule key actually runs and what it refuses to compose
with — NM spent roughly eight calls doing that by hand. The map now travels
with the tool that derives the windows.

CALIBRATED against the live config/GUNE5D/webfrank.json: 145 shipped rules
use 11 distinct rule keys, every one of them is in the map, and every map
entry is used by a shipped rule (T11_scratch/t11_rule_class_census.py). That
coverage is asserted below so a new class cannot be shipped in webfrank.json
while --emit stays silent about it.
"""

import json
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tools" / "gdl"))

from tools.gdl.rule_derive import (  # noqa: E402
    RULE_CLASSES, class_note, differing_runs, positional_args)

BOOKKEEPING = {"function", "before_sha256", "after_sha256", "audit",
               "mechanism"}


def words(*values):
    import struct
    return b"".join(struct.pack(">I", value) for value in values)


class ClassMapCoverageTests(unittest.TestCase):
    def setUp(self):
        path = REPO / "config" / "GUNE5D" / "webfrank.json"
        if not path.exists():
            self.skipTest("no webfrank.json in this checkout")
        self.units = json.loads(path.read_text(encoding="utf-8"))["units"]

    def keys_in_use(self):
        return {key for rules in self.units.values()
                for rule in rules for key in rule}

    def test_every_shipped_rule_key_is_in_the_map(self):
        unmapped = sorted(self.keys_in_use() - set(RULE_CLASSES)
                          - BOOKKEEPING)
        self.assertEqual(unmapped, [],
                         "a rule class ships that --emit cannot describe")

    def test_every_map_entry_is_a_class_that_actually_ships(self):
        """A map row nobody uses is a claim about the postprocessor that
        no live rule checks."""
        unused = sorted(set(RULE_CLASSES) - self.keys_in_use())
        self.assertEqual(unused, [])


class ClassNoteTests(unittest.TestCase):
    def test_every_note_names_a_verifier_and_what_it_proves(self):
        for kind in RULE_CLASSES:
            note = class_note(kind)
            self.assertIn("VERIFIER:", note, kind)
            self.assertIn("proves", note, kind)

    def test_the_composition_refusals_are_quoted(self):
        """The zero class refuses a register stage and the audit escape;
        both refusals are what an author needs before drafting."""
        note = class_note("equivalent_zero_form")
        self.assertIn("unproven_recolor_audit", note)
        self.assertIn("register stage", note)

    def test_the_audit_escape_says_it_proves_nothing(self):
        note = class_note("unproven_recolor_audit")
        self.assertIn("nothing", note)


class ArgumentParsingTests(unittest.TestCase):
    """`--class equivalent_copy_form` does not start with `--`, so a naive
    flag filter reads the class name as a function name."""

    def parse(self, *argv):
        saved = sys.argv
        try:
            sys.argv = ["rule_derive.py", *argv]
            return positional_args()
        finally:
            sys.argv = saved

    def test_the_class_value_is_not_read_as_a_function(self):
        self.assertEqual(
            self.parse("game/game/player", "fn", "--emit",
                       "--class", "equivalent_copy_form"),
            ["game/game/player", "fn"])

    def test_plain_positionals_survive(self):
        self.assertEqual(self.parse("game/x/y", "a", "b"),
                         ["game/x/y", "a", "b"])

    def test_a_trailing_class_flag_does_not_crash_the_filter(self):
        self.assertEqual(self.parse("game/x/y", "a", "--class"),
                         ["game/x/y", "a"])


class DifferingRunTests(unittest.TestCase):
    def test_adjacent_differing_words_form_one_run(self):
        ours = words(1, 2, 3, 4)
        target = words(1, 9, 9, 4)
        self.assertEqual(differing_runs(ours, target), [[4, 8]])

    def test_a_gap_splits_the_run(self):
        ours = words(1, 2, 3, 4, 5)
        target = words(9, 2, 3, 9, 5)
        self.assertEqual(differing_runs(ours, target), [[0], [12]])

    def test_identical_streams_have_no_runs(self):
        stream = words(1, 2, 3)
        self.assertEqual(differing_runs(stream, stream), [])


if __name__ == "__main__":
    unittest.main()
