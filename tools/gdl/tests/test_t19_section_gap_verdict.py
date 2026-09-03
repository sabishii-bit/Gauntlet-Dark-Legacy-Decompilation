#!/usr/bin/env python3
"""T19 run-49 item 3: datadiff must give zero-filled slack ONE verdict.

THE CONTRADICTION (WF). The byte mode calls zero-filled claim slack advisory
DATA-DEBT and exits 0; `--sections` called the SAME bytes a FLIP BLOCKER and
exited 1. Reproduced at 96d689120 on dolphin/si/SIBios -- an
`Object(Matching, ...)` unit that is linked and green, so a "flip blocker"
on it is false by construction:

    $ python tools/gdl/datadiff.py --no-deadstrip dolphin/si/SIBios
    [dolphin/si/SIBios.c] .data: DATA-DEBT 0xCD bytes compared, 0x3 claim
      slack (zero-filled; structural - advisory, not a flip blocker)
    EXIT=0

    $ python tools/gdl/datadiff.py --sections dolphin/si/SIBios
    [dolphin/si/SIBios.c] .data: SIZE target 0xD0 vs ours 0xCD <- FLIP BLOCKER
    [dolphin/si/SIBios.c] .sbss: SIZE target 0x10 vs ours 0xC  <- FLIP BLOCKER
    EXIT=1

Three bytes, two labels, opposite exit codes.

TWO-SIDED CENSUS at 96d689120, `--sections --matching` -- units that have
ALREADY flipped and link green, where every flagged row is a false positive
by construction. 231 rows printed, 74 flagged:

    54  zero-filled tail over an identical head  -> debt-zero-slack
    19  .bss/.sbss claim slack, target larger    -> debt-bss-slack
     1  OURS LARGER than target                  -> stays a blocker
     0  nonzero tail (real bytes missing)        -> stays a blocker
     0  differing head                           -> stays a blocker

The 54 are the same rows the byte mode already counts (its summary reads 54
sections over 45 units), so this removes a label, it does not add leniency.
After the fix the same command flags ONE row -- dolphin/demo/DEMOInit.c
.sbss, ours 0x28 vs target 0x20, which is precisely the OURS-LARGER case the
mode exists for -- and reports 73 advisory sections over 58 units.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import datadiff  # noqa: E402


class Classify(unittest.TestCase):
    def test_SIBios_shape_a_zero_tail_over_an_identical_head_is_debt(self):
        head = bytes(range(0xCD))
        self.assertEqual(
            datadiff.size_gap_class(head + b"\x00\x00\x00", head),
            "debt-zero-slack")

    def test_an_exactly_equal_pair_is_debt_with_an_empty_tail(self):
        head = b"\x01\x02\x03\x04"
        self.assertEqual(datadiff.size_gap_class(head, head),
                         "debt-zero-slack")

    def test_a_NONZERO_tail_stays_a_blocker(self):
        """Real bytes are missing from our object."""
        head = b"\x01\x02\x03\x04"
        self.assertEqual(datadiff.size_gap_class(head + b"\x00\x07", head),
                         "blocker-nonzero-tail")

    def test_a_DIFFERING_head_stays_a_blocker(self):
        self.assertEqual(
            datadiff.size_gap_class(b"\x01\x02\x00\x00", b"\x01\x09"),
            "blocker-head-differs")

    def test_OURS_LARGER_stays_a_blocker_even_when_our_tail_is_zero(self):
        """The one case --sections exists for: the DOL-range byte check is
        structurally blind to our object emitting MORE than the target.
        Live at 96d689120 on dolphin/demo/DEMOInit.c .sbss (0x28 vs 0x20)."""
        self.assertEqual(
            datadiff.size_gap_class(b"\x01\x02", b"\x01\x02\x00\x00"),
            "blocker-ours-larger")

    def test_bss_target_larger_is_debt_and_bss_ours_larger_is_not(self):
        self.assertEqual(datadiff.size_gap_class(b"", b"", bss=True),
                         "debt-bss-slack")
        self.assertEqual(
            datadiff.size_gap_class(b"", b"\x00\x00\x00\x00", bss=True),
            "blocker-ours-larger")


class Labels(unittest.TestCase):
    def test_every_class_has_a_blurb(self):
        for cls in ("debt-zero-slack", "debt-bss-slack",
                    "blocker-ours-larger", "blocker-nonzero-tail",
                    "blocker-head-differs"):
            self.assertIn(cls, datadiff.GAP_BLURB)
            self.assertTrue(datadiff.GAP_BLURB[cls])

    def test_a_blurb_does_not_carry_its_own_verdict_word(self):
        """The row prints `<- {mark} — {blurb}`; a blurb that also said
        DATA-DEBT printed the word twice, and under --strict-slack it
        printed the verdict that was NOT taken."""
        for blurb in datadiff.GAP_BLURB.values():
            self.assertNotIn("DATA-DEBT", blurb)
            self.assertNotIn("FLIP BLOCKER", blurb)


class Wiring(unittest.TestCase):
    SRC = (REPO / "tools" / "gdl" / "datadiff.py").read_text(encoding="utf-8")

    def test_the_size_row_is_classified_not_unconditionally_blocked(self):
        self.assertNotIn('ours 0x{olen or 0:X}  <- FLIP BLOCKER"', self.SRC)
        self.assertIn("size_gap_class(tb, ob, bss=False)", self.SRC)

    def test_strict_slack_restores_the_refusal_for_both_modes(self):
        self.assertIn("blocks = gap.startswith(\"blocker\") or strict_slack",
                      self.SRC)
        self.assertIn("section_table(key, strict_slack=strict_slack",
                      self.SRC)

    def test_the_advisory_rows_are_summarized_not_silently_dropped(self):
        self.assertIn("SECTION-SIZE DEBT (advisory, exit code unaffected)",
                      self.SRC)

    def test_the_importable_core_line_advertises_the_new_pure_function(self):
        """tools/gdl/tests/test_importable_core.py enforces the contract; the
        line has to name the function for a sweep to be allowed to call it."""
        self.assertIn("IMPORTABLE CORE: section_verdict, shrink_reachable,"
                      " size_gap_class", self.SRC)

    def test_the_content_readers_are_module_level_and_importable(self):
        self.assertTrue(callable(datadiff.section_sizes))
        self.assertTrue(callable(datadiff.section_bytes))


if __name__ == "__main__":
    unittest.main()
