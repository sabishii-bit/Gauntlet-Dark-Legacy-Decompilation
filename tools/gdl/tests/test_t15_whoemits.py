"""t15_whoemits: the park-gate screen (run-45 item 2).

A park that says "MWCC will not emit this shape" is refuted by our own build
emitting it, and refuted at full strength when it emits it inside a function
whose bytes already match retail. nm_branchpair_census.py is the precedent
for one hard-coded shape; these tests pin the generalised matcher, and in
particular the two ways a pattern language quietly lies: an unanchored
mnemonic regex (`b` matching `beq`, which would turn a branch-pair census
into a branch census) and a window that spans a gap instead of consecutive
instructions.

Calibration numbers live in the tool's docstring; they are measurements, not
assertions, and no test pins them (the population moves with the tree).
"""

import re
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import t15_whoemits as who  # noqa: E402

# (offset, mnemonic, operand text), as scan_object collects them.
STREAM = [
    (0x00, "cmpwi", "r3,0"),
    (0x04, "beq", "8 <fn+0x10>"),
    (0x08, "b", "20 <fn+0x28>"),
    (0x0c, "lwz", "r4,0(r3)"),
    (0x10, "stfsu", "f1,4(r5)"),
    (0x14, "b", "30 <fn+0x38>"),
]


class PatternTests(unittest.TestCase):
    def test_a_single_mnemonic_matches_every_occurrence(self):
        hits = who.match_sites(STREAM, who.compile_pattern(["b"]))
        self.assertEqual([offset for offset, _text in hits], [0x08, 0x14])

    def test_the_mnemonic_regex_is_anchored(self):
        """`b` must not match `beq`: without fullmatch anchoring this screen
        answers a different question than the one asked."""
        hits = who.match_sites(STREAM, who.compile_pattern(["b"]))
        self.assertNotIn(0x04, [offset for offset, _text in hits])
        wide = who.match_sites(STREAM, who.compile_pattern(["b.*"]))
        self.assertIn(0x04, [offset for offset, _text in wide])

    def test_a_two_instruction_pattern_must_be_consecutive(self):
        pair = who.compile_pattern(["b(eq|ne)", "b"])
        self.assertEqual([offset for offset, _t in
                          who.match_sites(STREAM, pair)], [0x04])
        gapped = who.compile_pattern(["cmpwi", "b"])
        self.assertEqual(who.match_sites(STREAM, gapped), [])

    def test_the_match_text_names_every_instruction_matched(self):
        pair = who.compile_pattern(["b(eq|ne)", "b"])
        self.assertEqual(who.match_sites(STREAM, pair)[0][1], "beq b")

    def test_an_operand_filter_applies_to_the_last_instruction(self):
        pattern = who.compile_pattern(["stfsu"])
        self.assertEqual(len(who.match_sites(STREAM, pattern,
                                             re.compile(r"f1,"))), 1)
        self.assertEqual(who.match_sites(STREAM, pattern,
                                         re.compile(r"f9,")), [])

    def test_a_pattern_longer_than_the_stream_matches_nothing(self):
        long_pattern = who.compile_pattern(["b"] * (len(STREAM) + 1))
        self.assertEqual(who.match_sites(STREAM, long_pattern), [])

    def test_the_raw_body_is_preferred_when_one_exists(self):
        """A webfrank rule rewrites register fields AFTER the compiler, so a
        postprocessed body is not evidence about what MWCC emits."""
        self.assertTrue(who.raw_object("x/y.o").endswith("y.o"))
        self.assertEqual(who.raw_object("x/y.o"), "x/y.o")


if __name__ == "__main__":
    unittest.main()
