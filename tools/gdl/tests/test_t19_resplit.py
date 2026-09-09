#!/usr/bin/env python3
"""T19 run-49 item 8: obj/** is a dtk-split REFERENCE, and --resplit restores it.

`build/GUNE5D/obj/**` holds 331 objects extracted FROM the retail DOL by
`dtk dol split`. They are the TARGET side of every comparison in the project
-- fndiff, fnasm, datadiff --sections, regnorm, savedregs, webfrank -- and
they sit under `build/`, gitignored, looking disposable.

MEASURED at cf375c09d in this worktree:

    build.ninja lines referencing build/GUNE5D/obj/    223
    build.ninja edges DECLARING one as an output         0
    files on disk                                      331

The split rule's only declared output is build/GUNE5D/config.json; obj/** is
a side effect, so ninja cannot know a file is missing. Deleting
build/GUNE5D/obj/dolphin/si/SIBios.o and running a plain `ninja` left it
missing AND stopped the build ("subcommand failed"), while
`datadiff.py --sections dolphin/si/SIBios` degraded to
`SKIP --sections: missing [...]` -- a comparison silently not made. That is
the shape of the incident CU hit.

RECOVERY, verified end to end at the same commit: deleting config.json makes
the split rule out of date, `ninja build/GUNE5D/config.json` re-extracts the
tree, and the full ninja then prints `build/GUNE5D/main.dol: OK`. The live
run of `provision_worktree.py --resplit` reported `build/GUNE5D/obj: 330
file(s) -> 331`.

These tests are static and cheap: they assert the PROPERTY that makes the
hazard real (no declared output) and that the flag is wired, without
deleting anything.
"""

import re
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import provision_worktree  # noqa: E402


class SplitReference(unittest.TestCase):
    NINJA = REPO / "build.ninja"

    def setUp(self):
        if not self.NINJA.is_file():
            self.skipTest("build.ninja not generated in this checkout")
        self.text = self.NINJA.read_text(encoding="utf-8", errors="replace")

    def test_no_ninja_edge_declares_an_obj_reference_as_its_output(self):
        """The property that makes a deleted reference unrecoverable by
        rebuilding. If this ever fails, the hazard is gone and the
        documentation above should be revisited rather than the test."""
        outputs = [line for line in self.text.splitlines()
                   if re.match(r"^build\s+\S*GUNE5D[\\/]obj[\\/]", line)]
        self.assertEqual(outputs, [])

    def test_the_references_really_are_depended_on(self):
        """An absence only matters if something reads them."""
        hits = len(re.findall(r"GUNE5D[\\/]obj[\\/]", self.text))
        self.assertGreater(hits, 100, hits)

    def test_the_split_rule_declares_config_json_and_that_is_all(self):
        self.assertIn("rule split", self.text)
        self.assertRegex(self.text, r"build \S*GUNE5D.config\.json: split")


class ResplitFlag(unittest.TestCase):
    SRC = (REPO / "tools" / "gdl"
           / "provision_worktree.py").read_text(encoding="utf-8")

    def test_the_flag_is_dispatched_before_the_argument_forms(self):
        """The PROPERTY, not the spelling: run-63 item 5 added
        `--pdb-from`, whose value must be lifted out of the argument vector
        before anything counts positionals, so the dispatch now reads
        `in argv` rather than `in sys.argv` and this assertion moved with
        it. What has to stay true is that `--resplit` is decided BEFORE the
        one- and two-argument forms."""
        self.assertIn('if "--resplit" in argv:', self.SRC)
        self.assertLess(self.SRC.index('if "--resplit" in argv:'),
                        self.SRC.index("if len(args) == 2:"))

    def test_it_removes_config_json_rather_than_invoking_dtk_by_hand(self):
        """Re-deriving the rule's arguments is how the recovery path
        diverges from the one ninja actually runs."""
        self.assertIn("config.unlink()", self.SRC)
        self.assertIn('["ninja", str(Path("build") / VERSION'
                      ' / "config.json")]', self.SRC)

    def test_it_prints_the_before_and_after_file_count(self):
        """A recovery that cannot be seen to have worked is not one."""
        self.assertIn('obj: {before} file(s) -> {after}', self.SRC)

    def test_it_fails_loudly_when_the_tree_is_still_empty(self):
        self.assertIn("is still empty after the split", self.SRC)

    def test_resplit_is_callable_and_refuses_extra_arguments(self):
        self.assertTrue(callable(provision_worktree.resplit))
        self.assertIn("no other arguments", self.SRC)

    def test_the_docstring_states_the_measured_hazard(self):
        self.assertIn("IS A DTK-SPLIT REFERENCE, NOT A BUILD ARTIFACT",
                      self.SRC)
        self.assertIn("223 lines", self.SRC)


class AgentsDocumentation(unittest.TestCase):
    TEXT = (REPO / "AGENTS.md").read_text(encoding="utf-8", errors="replace")
    # These are safety obligations, not historical heading/line-wrap locks.
    NORMALIZED = " ".join(TEXT.split())

    def test_the_obj_hazard_and_its_one_command_recovery_are_written_down(self):
        self.assertIn("DTK-extracted reference inputs, not disposable "
                      "compiler outputs", self.NORMALIZED)
        self.assertIn("Ninja may not recreate a deleted individual object",
                      self.NORMALIZED)
        self.assertIn("provision_worktree.py --resplit", self.NORMALIZED)
        self.assertIn("never edit target objects or generated target assembly "
                      "to obtain a match", self.NORMALIZED)

    def test_the_header_comment_convention_is_written_down(self):
        """NC met a confident, TU-wide, WRONG header claim carrying 'do not
        grind' -- a veto with no scope, no date and no way to be cleared."""
        self.assertIn('historical stop claim (such as "do not grind")',
                      self.NORMALIZED)
        self.assertIn("A claim missing these checks is a hint, not a "
                      "permanent veto", self.NORMALIZED)

    def test_a_stop_claim_has_scope_measurement_recheck_and_falsifier(self):
        block = self.NORMALIZED.split("Before treating a historical stop "
                                      "claim", 1)[1]
        block = block.split("Report observations separately", 1)[0]
        for requirement in ("exact scope", "current measured premise",
                            "measurement date", "reproduction command",
                            "a falsifier: the evidence that would disprove",
                            "when to recheck", "changed inputs invalidate"):
            with self.subTest(requirement=requirement):
                self.assertIn(requirement, block)

    def test_header_comments_remain_useful_evidence_not_automatic_vetoes(self):
        """Header comments remain free evidence; the convention is about
        what makes one a VETO."""
        self.assertIn("Source and header comments are useful free evidence, "
                      "not automatic vetoes", self.NORMALIZED)
        self.assertIn("test its premise before following its prescription",
                      self.NORMALIZED)


if __name__ == "__main__":
    unittest.main()
