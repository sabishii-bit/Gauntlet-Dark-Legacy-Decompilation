"""pool_owner tests first-use order against a CANDIDATE extent (item 3d).

THE OBSERVATION. The first-use order test only ever ran over datums the unit
ALREADY claims in splits.txt, so exactly the units deciding whether to write
a claim got `FIRST-USE ORDER: NOT APPLICABLE`. Measured at 1958d45b3 over
five sampled units, three printed it -- `game/enemy/critter`,
`game/world/tower`, `dolphin/si/SIBios` -- all of which DO reference pool
datums, just none inside a run they own.

THE CURE is `--range SECTION:0xSTART-0xEND`: the candidate extent
`claimable_sections.py` derives and scores against the DOL, tested as a
HYPOTHESIS. Live at 1958d45b3:

    pool_owner.py game/enemy/critter --range .sdata2:0x80346470-0x80346559
    -> CANDIDATE EXTENT (--range): .sdata2 0x80346470..0x80346559 233 bytes
       FIRST-USE ORDER over 35 of 35 own referenced datums:
       DISAGREES at index 4 (first use lbl_80346498, address lbl_80346490)

THE NEGATIVE SIDE, three refusals: a malformed range, a range on a section
that has no literal pool, and -- the one that matters -- a range overlapping
ANOTHER unit's claimed run, which would answer a question about this unit
using another TU's datums. Reproduced live:
`--range .sdata2:0x80346410-0x80346470` on critter overlaps
game/game/controls' claimed 0x803463F8-0x80346470 and is refused, exit 2.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))

from tools.gdl import pool_owner  # noqa: E402

RUNS = [("game/game/controls.c", ".sdata2", 0x803463F8, 0x80346470),
        ("game/enemy/critter.c", ".text", 0x80000000, 0x80001000)]


def row(name, address, section=".sdata2", owner="UNCLAIMED"):
    return {"name": name, "address": "0x%08X" % address, "section": section,
            "owner": owner, "owned_by_this_unit": owner == "me"}


class ParseRange(unittest.TestCase):
    def test_a_well_formed_range_parses_half_open(self):
        self.assertEqual(pool_owner.parse_range(".sdata2:0x8000-0x8010"),
                         (".sdata2", 0x8000, 0x8010))
        self.assertEqual(pool_owner.parse_range(" .rodata : 16 - 32 "),
                         (".rodata", 16, 32))

    def test_a_malformed_range_refuses(self):
        for text in ("", ".sdata2", "0x10-0x20", ".sdata2:0x20",
                     ".sdata2:zz-0x20", "sdata2:0x10-0x20"):
            with self.subTest(text=text):
                with self.assertRaises(pool_owner.RangeRefused):
                    pool_owner.parse_range(text)

    def test_a_non_pool_section_refuses_with_the_reason(self):
        for section in (".bss", ".data", ".text"):
            with self.subTest(section=section):
                with self.assertRaisesRegex(pool_owner.RangeRefused,
                                            "not a pool section"):
                    pool_owner.parse_range("%s:0x10-0x20" % section)

    def test_an_empty_or_backwards_range_refuses(self):
        for text in (".sdata2:0x20-0x20", ".sdata2:0x30-0x20"):
            with self.subTest(text=text):
                with self.assertRaisesRegex(pool_owner.RangeRefused,
                                            "empty or backwards"):
                    pool_owner.parse_range(text)


class Conflicts(unittest.TestCase):
    def test_an_overlap_with_another_units_run_is_refused_by_name(self):
        with self.assertRaisesRegex(pool_owner.RangeRefused,
                                    "game/game/controls"):
            pool_owner.check_range_conflict(
                RUNS, "game/enemy/critter.c",
                (".sdata2", 0x80346410, 0x80346470))

    def test_a_range_that_only_touches_the_end_is_not_an_overlap(self):
        pool_owner.check_range_conflict(RUNS, "game/enemy/critter.c",
                                        (".sdata2", 0x80346470, 0x80346500))

    def test_this_units_own_run_is_never_a_conflict(self):
        pool_owner.check_range_conflict(RUNS, "game/game/controls.c",
                                        (".sdata2", 0x803463F8, 0x80346470))

    def test_a_different_section_at_the_same_addresses_is_not_a_conflict(self):
        pool_owner.check_range_conflict(RUNS, "game/enemy/critter.c",
                                        (".rodata", 0x803463F8, 0x80346470))


class Adoption(unittest.TestCase):
    def test_only_datums_inside_the_extent_are_adopted(self):
        rows = [row("a", 0x80346468), row("b", 0x80346470),
                row("c", 0x80346500), row("d", 0x80346560)]
        adopted = pool_owner.apply_candidate_ranges(
            rows, [(".sdata2", 0x80346470, 0x80346559)])
        self.assertEqual(adopted, 2)
        self.assertEqual([r["owned_by_this_unit"] for r in rows],
                         [False, True, True, False])

    def test_an_adopted_row_keeps_its_measured_owner_and_says_it_is_a_guess(self):
        rows = [row("a", 0x80346480, owner="UNCLAIMED")]
        pool_owner.apply_candidate_ranges(
            rows, [(".sdata2", 0x80346470, 0x80346559)])
        self.assertEqual(rows[0]["owner"], "UNCLAIMED")
        self.assertIn("NOT claimed in splits.txt",
                      rows[0]["ownership_source"])

    def test_a_row_in_another_section_is_never_adopted(self):
        rows = [row("a", 0x80346480, section=".rodata")]
        self.assertEqual(pool_owner.apply_candidate_ranges(
            rows, [(".sdata2", 0x80346470, 0x80346559)]), 0)

    def test_a_genuinely_owned_row_is_labelled_splits_not_candidate(self):
        rows = [row("a", 0x80346480, owner="me")]
        pool_owner.apply_candidate_ranges(
            rows, [(".sdata2", 0x80346470, 0x80346559)])
        self.assertEqual(rows[0]["ownership_source"], "splits.txt")

    def test_no_range_at_all_adopts_nothing(self):
        rows = [row("a", 0x80346480)]
        self.assertEqual(pool_owner.apply_candidate_ranges(rows, []), 0)
        self.assertFalse(rows[0]["owned_by_this_unit"])


LIVE = ((ROOT / "build/GUNE5D/obj/game/enemy/critter.o").is_file()
        and (ROOT / "config/GUNE5D/splits.txt").is_file())


@unittest.skipUnless(LIVE, "needs the split target objects and symbols")
class LiveCritter(unittest.TestCase):
    """The reported unit: NOT APPLICABLE without a range, a verdict with one."""

    def run_tool(self, *flags):
        return subprocess.run(
            [sys.executable, "tools/gdl/pool_owner.py", "game/enemy/critter",
             *flags], cwd=str(ROOT), capture_output=True, text=True)

    def test_without_a_range_it_still_says_not_applicable_and_offers_one(self):
        done = self.run_tool()
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("FIRST-USE ORDER: NOT APPLICABLE", done.stdout)
        self.assertIn("--range <section>:0xSTART-0xEND", done.stdout)
        self.assertIn("claimable_sections.py", done.stdout)

    def test_a_candidate_extent_produces_a_labelled_order_verdict(self):
        done = self.run_tool("--range", ".sdata2:0x80346470-0x80346559")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("CANDIDATE EXTENT (--range)", done.stdout)
        self.assertIn("NOT claimed in splits.txt", done.stdout)
        self.assertIn("FIRST-USE ORDER over", done.stdout)
        self.assertNotIn("FIRST-USE ORDER: NOT APPLICABLE", done.stdout)

    def test_an_extent_owned_by_another_unit_is_refused(self):
        done = self.run_tool("--range", ".sdata2:0x80346410-0x80346470")
        self.assertEqual(done.returncode, 2)
        self.assertIn("overlaps game/game/controls", done.stderr)
        self.assertNotIn("FIRST-USE ORDER", done.stdout)

    def test_an_extent_holding_no_referenced_datum_says_nothing_to_test(self):
        done = self.run_tool("--range", ".sdata2:0x80347F00-0x80347F10")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("NOTHING TO TEST", done.stdout)


if __name__ == "__main__":
    unittest.main()
