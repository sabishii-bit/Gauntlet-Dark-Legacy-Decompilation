"""composed_census/secbind.py: bind one section against the DOL (item 4).

WHAT IT ADDS to claimable_sections, which already ELECTS a base. This binds
at the address the CALLER names — a splits.txt boundary, a neighbour TU's
first symbol, a PDB hypothesis — and reports the raw EQUAL PREFIX, the
DOL-side gaps and the section-END verdict there.

TWO NUMBERS, ONE SECTION, AND BOTH ARE TRUE. Measured at 6da714b06 on
`game/game/player` `.rodata` when it was still 0x20E bytes and unclaimed:

    bound at 0x80113AE0   EQUAL PREFIX 0x0, 0 of 131 words equal
    bound at 0x80113E28   EQUAL PREFIX 0x44, 131/131 after 3 insertions

0x80113AE0 was the front of a block the source did not have (a 0x348 FRONT
DEFICIT); 0x80113E28 was where what we DID have started. A tool printing
only one of those would size the recovery wrongly, in opposite directions.

That section has since been RECOVERED and claimed (0x80113AE0..0x80114220,
0x73E bytes) and now binds byte-for-byte at 0x80113AE0, so the live tests
below moved with it: `.rodata` exercises the CLAIM path and a byte-perfect
binding, and `.sdata2` — still open — exercises the ELECTION path, the gap
inventory and a straddled section end.

TWO DEFECTS THIS SUITE PINS, both found by re-measuring after that merge:

  THE WINDOW. Reading only `len(section)` DOL bytes left `gap_inventory`
  no room to look ahead for an insertion, and the same section then read
  `88/131` resynced-equal here against claimable_sections' `131/131` — two
  numbers for one section, which is precisely what importing that tool's
  aligner was meant to prevent.

  THE PARTIAL TAIL. With `first_difference` gated on `len(ours)` rather
  than on the word-aligned limit, the recovered 0x73E-byte `.rodata`
  reported `first differing word at +0x73C` whose two sides both printed
  `5900` — a difference that did not exist, on a byte-perfect section.

TWO-SIDED. Positive: the live claim binding, the live election binding, and
the synthetic prefix cases. Negative: one WORD off the claim collapsing the
prefix to 0, a BSS section (no DOL bytes at all), an absent section, an
address outside the DOL, a non-hex address, and a prefix that must NOT
round up past a partially-agreeing word.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import claimable_sections as cs  # noqa: E402
import secbind  # noqa: E402

TOOL = "tools/gdl/composed_census/secbind.py"
UNIT = "game/game/player"
SECTION = ".rodata"
FRONT_BLOCK = 0x80113AE0
LIVE = (ROOT / "build/GUNE5D/src" / (UNIT + ".o")).is_file()


class EqualPrefix(unittest.TestCase):
    def test_an_identical_run_is_the_whole_length(self):
        data = bytes(range(64))
        self.assertEqual(secbind.equal_prefix(data, data), 64)

    def test_a_first_word_difference_is_a_zero_prefix(self):
        self.assertEqual(secbind.equal_prefix(b"AAAA" + b"BBBB",
                                              b"XAAA" + b"BBBB"), 0)

    def test_the_prefix_is_WORD_aligned_not_byte_aligned(self):
        """A run agreeing for 0x32C bytes and then differing inside the
        next word is 0x32C, not 0x32E: everything else in this comparison
        is word-granular and a byte answer cannot be lined up with it."""
        ours = b"\x11\x22\x33\x44" * 3 + b"\xAA\xBB\xCC\xDD"
        theirs = b"\x11\x22\x33\x44" * 3 + b"\xAA\xBB\x00\x00"
        self.assertEqual(secbind.equal_prefix(ours, theirs), 12)

    def test_a_short_target_stops_the_prefix_at_its_end(self):
        self.assertEqual(secbind.equal_prefix(b"AAAABBBB", b"AAAA"), 4)

    def test_a_trailing_partial_word_is_never_counted(self):
        self.assertEqual(secbind.equal_prefix(b"AAAAB", b"AAAAB"), 4)

    def test_empty_input_is_zero_not_an_error(self):
        self.assertEqual(secbind.equal_prefix(b"", b"AAAA"), 0)


class TextRendering(unittest.TestCase):
    def test_printables_survive_and_the_rest_are_dotted(self):
        self.assertEqual(secbind.text_of(b"NO FLOOR\x00\x00"), "NO FLOOR..")

    def test_high_bytes_are_dotted_not_decoded(self):
        self.assertEqual(secbind.text_of(b"\xff\x80A"), "..A")


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class LiveClaimedSection(unittest.TestCase):
    """`.rodata`, which the project has RECOVERED and claimed."""

    def test_the_derived_base_is_this_units_own_split_claim(self):
        """claimable_sections stops censusing a section once it is claimed,
        so an election-only derivation refused on every section the project
        had already recovered."""
        record = secbind.bind(UNIT, SECTION)
        self.assertIn("splits.txt claim", record["base_source"])
        self.assertEqual(record["base"], FRONT_BLOCK)

    def test_a_recovered_section_binds_byte_for_byte(self):
        record = secbind.bind(UNIT, SECTION, FRONT_BLOCK)
        self.assertEqual(record["equal_prefix"], record["aligned_size"])
        self.assertEqual(record["words_equal_raw"], record["words"])
        self.assertIsNone(record["first_difference"])

    def test_a_trailing_PARTIAL_word_is_compared_and_not_mis_reported(self):
        """The defect this pins: with `first_difference` gated on
        len(ours) rather than on the WORD-ALIGNED limit, a 0x73E-byte
        section reported `first differing word at +0x73C` whose two sides
        both printed `5900` — a difference that did not exist, on a
        section that was byte-perfect."""
        record = secbind.bind(UNIT, SECTION, FRONT_BLOCK)
        self.assertEqual(record["size"] - record["aligned_size"],
                         record["tail"]["bytes"])
        self.assertTrue(record["tail"]["bytes"])
        self.assertTrue(record["tail"]["equal"])
        self.assertIsNone(record["first_difference"])

    def test_ONE_WORD_off_the_claim_collapses_the_prefix_to_zero(self):
        """The negative control: the prefix measures a binding, not a
        coincidence."""
        record = secbind.bind(UNIT, SECTION, FRONT_BLOCK + 4)
        self.assertEqual(record["equal_prefix"], 0)
        self.assertEqual(record["words_equal_raw"], 0)
        self.assertIsNotNone(record["first_difference"])


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class LiveUnclaimedSection(unittest.TestCase):
    """`.sdata2`, which is still open, so the election path is exercised."""

    SECTION = ".sdata2"

    def test_the_base_comes_from_claimable_sections_election(self):
        record = secbind.bind(UNIT, self.SECTION)
        self.assertIn("claimable_sections", record["base_source"])
        self.assertGreater(record["equal_prefix"], 0)

    def test_the_inventory_agrees_with_claimable_sections_EXACTLY(self):
        """The regression for the DOL-window defect: reading only
        len(section) DOL bytes left `gap_inventory` no room to look ahead
        for an insertion, and the same section then read 88/131 resynced-
        equal here against that tool's 131/131."""
        record = secbind.bind(UNIT, self.SECTION)
        splits = cs.parse_splits(ROOT / "config" / "GUNE5D" / "splits.txt")
        row = cs.census(UNIT, splits, cs.claimed_intervals(splits),
                        defined=None)
        theirs = [found for found in row["sections"]
                  if found["section"] == self.SECTION][0]
        self.assertEqual(int(theirs["base"], 16), record["base"])
        self.assertEqual(theirs["inventory"]["resynced_equal"],
                         record["inventory"]["resynced_equal"])
        self.assertEqual(len(theirs["inventory"]["gaps"]),
                         len(record["inventory"]["gaps"]))
        self.assertEqual(theirs["inventory"]["gap_bytes"],
                         record["inventory"]["gap_bytes"])

    def test_every_gap_carries_an_address_a_size_and_decoded_content(self):
        record = secbind.bind(UNIT, self.SECTION)
        self.assertTrue(record["inventory"]["gaps"])
        for gap in record["inventory"]["gaps"]:
            self.assertGreaterEqual(gap["address"], record["base"])
            self.assertTrue(gap["size"] % 4 == 0 and gap["size"] > 0)
            self.assertTrue(gap["content"])

    def test_the_section_end_verdict_names_the_straddled_symbol(self):
        record = secbind.bind(UNIT, self.SECTION)
        end = record["end_verdict"]
        self.assertEqual(end["end"], record["base"] + record["size"])
        self.assertEqual(end["boundary"], "inside")
        self.assertIsNotNone(end["straddled"])
        self.assertLess(end["straddled"]["start"], end["end"])
        self.assertGreater(end["straddled"]["end"], end["end"])
        self.assertGreater(end["next_symbol_start"], end["end"])


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class LiveBssRefusal(unittest.TestCase):
    def test_a_BSS_section_says_there_are_no_DOL_bytes_to_bind(self):
        with self.assertRaises(SystemExit) as caught:
            secbind.bind(UNIT, ".bss")
        self.assertIn("no bytes in the DOL", str(caught.exception))


class Refusals(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, TOOL, *args], cwd=str(ROOT),
                              capture_output=True, text=True)

    def test_help_exits_zero_on_stdout(self):
        done = self.run_tool("--help")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("NOT A SECOND GAP FINDER", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_an_absent_section_REFUSES_and_lists_the_real_ones(self):
        """objdump -j on an absent section exits 1; that must arrive as a
        message, not as an ObjdumpFailed traceback."""
        done = self.run_tool(UNIT, ".nosuch")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("defines no .nosuch", done.stdout)
        self.assertIn(".rodata", done.stdout)
        self.assertNotIn("Traceback", done.stderr)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_an_address_outside_the_DOL_REFUSES(self):
        done = self.run_tool(UNIT, SECTION, "0x00000000")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("not inside any DOL section", done.stdout)
        self.assertNotIn("Traceback", done.stderr)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_a_non_hex_address_REFUSES(self):
        done = self.run_tool(UNIT, SECTION, "banana")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("not a hex address", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_a_missing_object_REFUSES(self):
        done = self.run_tool(UNIT, SECTION, "--object",
                             "build/p5_no_such_object.o")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("run ninja first", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_the_claim_run_exits_zero_and_prints_the_whole_report(self):
        done = self.run_tool(UNIT, SECTION, "0x80113AE0")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("EQUAL PREFIX", done.stdout)
        self.assertIn("PARTIAL word", done.stdout)
        self.assertIn("byte-identical at this address", done.stdout)
        self.assertIn("SECTION END", done.stdout)
        self.assertIn("not ownership of the target extent", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_one_word_off_the_claim_prints_a_zero_prefix(self):
        done = self.run_tool(UNIT, SECTION, "0x80113AE4")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("EQUAL PREFIX 0x0", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_a_BSS_section_REFUSES_through_the_CLI_too(self):
        done = self.run_tool(UNIT, ".bss")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("no bytes in the DOL", done.stdout)


if __name__ == "__main__":
    unittest.main()
