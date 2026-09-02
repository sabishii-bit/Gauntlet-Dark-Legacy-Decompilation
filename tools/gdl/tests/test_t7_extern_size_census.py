"""extern array-bound census (run-37 item 11).

UB found ONE wrong bound by hand and it was score-decisive:
`extern char lbl_80347F4C[12]` at src/game/ui/select.c:101 where
config/GUNE5D/symbols.txt says size:0x8. Twelve bytes sits above MWCC's
8-byte small-data threshold and eight sits below it, so the wrong bound
changes the addressing form the compiler picks. One hand-found instance is
not a population; this tool is the census.

The load-bearing design fact: THE SIZES IN symbols.txt ARE NOT ALL DECLARED
SIZES. Many are inferred by dtk from the layout, so a flat mismatch list
would have reported 90 rows as if they were 90 source bugs — `gPlayers` is
recorded size:0x9F1 (2545 bytes), which is not divisible by 4 and therefore
cannot be a 4-element player array size at all, while the source's
[4][0x335C] is plainly the real shape. The buckets exist so that
population is not conflated with the real one.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

from t7_extern_size_census import (DIM_RE, ELEMENT_SIZES,  # noqa: E402
                                   EXTERN_RE, classify_row, declared_bytes)


class DeclarationParsingTests(unittest.TestCase):

    def parse(self, line):
        match = EXTERN_RE.match(line)
        if not match:
            return None
        element, symbol, dim_text = match.groups()
        return element, symbol, DIM_RE.findall(dim_text)

    def test_ubs_declaration_parses(self):
        self.assertEqual(self.parse("extern char lbl_80347F4C[12];"),
                         ("char", "lbl_80347F4C", ["12"]))

    def test_a_hex_bound_parses(self):
        self.assertEqual(self.parse("extern u8 lbl_802411B0[0x540];"),
                         ("u8", "lbl_802411B0", ["0x540"]))

    def test_a_multidimensional_bound_parses(self):
        self.assertEqual(self.parse("extern u8 gPlayers[4][0x335C];"),
                         ("u8", "gPlayers", ["4", "0x335C"]))

    def test_a_pointer_array_is_not_matched(self):
        """Element size there is a pointer's; reporting it as a size error
        would be reporting a type error."""
        self.assertIsNone(self.parse("extern Thing* things[8];"))

    def test_an_unbounded_extern_is_not_matched(self):
        self.assertIsNone(self.parse("extern char name[];"))

    def test_a_non_extern_definition_is_not_matched(self):
        self.assertIsNone(self.parse("static char buf[12];"))


class DeclaredBytesTests(unittest.TestCase):

    def test_ubs_row(self):
        self.assertEqual(declared_bytes("char", ["12"]), 12)

    def test_hex_and_multidimensional(self):
        self.assertEqual(declared_bytes("u8", ["4", "0x10"]), 64)

    def test_float_elements_scale(self):
        """gIdentityMatrix[12] vs the linker's 0x40 = 16 floats."""
        self.assertEqual(declared_bytes("f32", ["12"]), 48)
        self.assertEqual(ELEMENT_SIZES["f32"], 4)

    def test_an_unknown_element_type_is_unmeasured_not_guessed(self):
        """A struct whose layout this tool cannot see must not produce a
        confident wrong row."""
        self.assertIsNone(declared_bytes("WorldObj", ["4"]))


class BucketTests(unittest.TestCase):

    def test_under_declared(self):
        """The source sees fewer bytes than the linker records."""
        self.assertEqual(classify_row(48, 64, gap=64), "under_declared")

    def test_ubs_confirmed_row_is_actionable(self):
        """declared 12, linker 8, next symbol 8 bytes away — it overlaps,
        and this is the bucket the one CONFIRMED defect must land in."""
        self.assertEqual(classify_row(12, 8, gap=8),
                         "over_declared_overlaps")

    def test_over_declared_but_fitting_is_the_weak_bucket(self):
        self.assertEqual(classify_row(12, 8, gap=64),
                         "over_declared_within_gap")

    def test_a_missing_gap_falls_to_the_weak_bucket(self):
        """The last symbol in a section has no next symbol; absence of
        evidence must not promote a row into the actionable bucket."""
        self.assertEqual(classify_row(12, 8, gap=None),
                         "over_declared_within_gap")


if __name__ == "__main__":
    unittest.main()
