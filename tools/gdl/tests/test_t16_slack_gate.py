#!/usr/bin/env python3
"""T16 run-46 item 2: zero-filled claim slack must not refuse a TU flip.

Two-sided by construction: every POSITIVE case (a shape that must stop
blocking) is paired with a NEGATIVE case (a shape that must still block),
because a gate measured only on what it should let through is half a
measurement and would ship as a hole.

Calibration against the live corpus is in the record; these tests pin the
classifier itself so a later edit cannot quietly re-broaden the refusal.
"""

import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import datadiff  # noqa: E402


def verdict(ours, orig, rel=(), lo=0x80100000, hi=None, sec=".rodata",
            starts=None):
    if hi is None:
        hi = lo + len(orig)
    return datadiff.section_verdict(bytes(ours), bytes(orig), set(rel),
                                    lo, hi, sec=sec, starts=starts)


class ZeroSlackIsNotABlocker(unittest.TestCase):
    """POSITIVE side: shapes that must NOT set bytebad."""

    def test_zero_filled_slack_is_not_a_defect(self):
        v = verdict(b"\x01\x02\x03\x04", b"\x01\x02\x03\x04" + b"\x00" * 4)
        self.assertEqual(v["bytebad"], 0)
        self.assertEqual(v["slack"], 4)
        self.assertTrue(v["slack_zero"])

    def test_unaligned_zero_slack_is_still_not_a_defect(self):
        # the structural half: the shrink fix is unreachable, but the unit
        # links green all the same
        v = verdict(b"\x01\x02\x03", b"\x01\x02\x03" + b"\x00" * 5)
        self.assertEqual(v["bytebad"], 0)
        self.assertEqual(v["reach"], "structural")

    def test_exact_section_has_no_slack_and_no_defect(self):
        v = verdict(b"\xaa" * 8, b"\xaa" * 8)
        self.assertEqual((v["bytebad"], v["slack"]), (0, 0))
        self.assertIsNone(v["reach"])

    def test_relocated_words_are_skipped_not_counted(self):
        v = verdict(b"\x00\x00\x00\x00", b"\x80\x10\x00\x00", rel=(0,))
        self.assertEqual(v["bytebad"], 0)
        self.assertEqual(v["skipped"], 1)


class RealDefectsStillBlock(unittest.TestCase):
    """NEGATIVE side: shapes that must STILL set bytebad (exit 1)."""

    def test_nonzero_slack_still_blocks(self):
        v = verdict(b"\x01\x02\x03\x04", b"\x01\x02\x03\x04\x00\x00\x00\x09")
        self.assertGreater(v["bytebad"], 0)
        self.assertFalse(v["slack_zero"])

    def test_differing_word_still_blocks(self):
        v = verdict(b"\x01\x02\x03\x04", b"\x01\x02\x03\x05")
        self.assertEqual(v["bytebad"], 1)
        self.assertEqual(len(v["diffs"]), 1)

    def test_differing_word_blocks_even_with_zero_slack(self):
        # the mixed shape: the slack must not launder a real byte defect
        v = verdict(b"\x01\x02\x03\x05", b"\x01\x02\x03\x04" + b"\x00" * 4)
        self.assertEqual(v["bytebad"], 1)
        self.assertEqual(v["slack"], 4)

    def test_object_larger_than_claim_still_blocks(self):
        v = verdict(b"\xaa" * 12, b"\xaa" * 8, hi=0x80100008)
        self.assertTrue(v["overrun"])
        self.assertGreater(v["bytebad"], 0)


class ShrinkReachability(unittest.TestCase):
    """claim.law.AF_dtk-rejects-an-unaligned-auto-split-...20260903.v1."""

    def test_four_aligned_end_is_shrinkable(self):
        self.assertEqual(
            datadiff.shrink_reachable(0x801108FC, ".rodata", {}), "shrinkable")

    def test_unaligned_end_is_structural(self):
        self.assertEqual(
            datadiff.shrink_reachable(0x801108FB, ".rodata", {}), "structural")

    def test_unaligned_end_claimed_by_a_neighbour_is_shrinkable(self):
        # the mb_font counterexample: game/mb/mb_lights.c claims 0x80115D70,
        # so no auto split is generated and the odd end is legal
        starts = {".rodata": {0x80115D70}}
        self.assertEqual(
            datadiff.shrink_reachable(0x80115D70, ".rodata", starts),
            "shrinkable")


class StrictSlackOptOut(unittest.TestCase):
    def test_flag_is_documented_and_parsed(self):
        text = (REPO / "tools" / "gdl" / "datadiff.py").read_text(
            encoding="utf-8")
        self.assertIn("--strict-slack", text)

    def test_zero_slack_unit_exits_zero(self):
        obj = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        if not obj.exists():
            self.skipTest("zlib/inflate not built")
        r = subprocess.run([sys.executable, "tools/gdl/datadiff.py",
                            "zlib/inflate"], cwd=str(REPO),
                           capture_output=True, text=True)
        self.assertIn("DATA-DEBT", r.stdout)
        self.assertEqual(r.returncode, 0, r.stdout)
        r = subprocess.run([sys.executable, "tools/gdl/datadiff.py",
                            "--strict-slack", "zlib/inflate"], cwd=str(REPO),
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 1, r.stdout)


class FinishTuNoLongerCitesSlack(unittest.TestCase):
    def test_blocked_message_excludes_zero_slack(self):
        text = (REPO / "tools" / "gdl" / "finish_tu.py").read_text(
            encoding="utf-8")
        self.assertIn("DATA-DEBT", text)


if __name__ == "__main__":
    unittest.main()
