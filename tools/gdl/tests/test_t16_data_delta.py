#!/usr/bin/env python3
"""T16 run-46 item 3: defake_gate prices a DATA change in BYTES.

The run-34 form banked a per-section digest, so `check` could only say "a
non-text section moved, go arbitrate by hand" — and a keep that destroyed
144 bytes of matched Data walked past every .text arbiter. A digest cannot
be subtracted; the baseline now banks matched-byte counts too.

Two-sided: every test that a loss is priced is paired with one that an
unchanged or improved section is not called a loss, and with the
back-compatibility case (an old string-digest baseline must still detect the
change and must say it is UNPRICED rather than inventing a number).

Live calibration at 6aff9e8cf, all 168 units with both objects built:
77 at 100% matched data bytes (the metric stays silent), 91 carrying a
shortfall, of which 46 lose exception-table bytes — 1866 B in total, the
population claim.law.WS_frame-widening-silently-breaks-the-tus-extab-match
describes.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import defake_gate as dg  # noqa: E402

DUMP = """
Contents of section .text:
 0000 60000000 60000000                    `...`...
Contents of section .rodata:
 0000 01020304 05060708                    ........
Contents of section extab:
 0000 aabbccdd                             abcd
Contents of section .comment:
 0000 4d574343                             MWCC
"""


def priced(matched, size, sha="x"):
    return {"sha": sha, "size": size, "matched": matched, "target_size": size}


class ByteParsing(unittest.TestCase):
    def test_bytes_are_parsed_and_text_excluded(self):
        secs = dg.parse_section_bytes(DUMP)
        self.assertNotIn(".text", secs)
        self.assertEqual(secs[".rodata"], bytes(range(1, 9)))
        self.assertEqual(secs["extab"], bytes.fromhex("aabbccdd"))

    def test_toolchain_metadata_is_not_matched_data(self):
        self.assertFalse(dg.is_scored_data(".comment"))
        self.assertFalse(dg.is_scored_data(".note.split"))
        self.assertTrue(dg.is_scored_data(".rodata"))
        self.assertTrue(dg.is_scored_data("extab"))

    def test_matched_counts_use_the_target_size_as_denominator(self):
        ours = {"extab": b"\x01\x02"}
        target = {"extab": b"\x01\x09\x09\x09"}
        self.assertEqual(dg.matched_data_bytes(ours, target),
                         {"extab": (1, 4)})

    def test_a_section_we_do_not_emit_loses_all_of_it(self):
        self.assertEqual(dg.matched_data_bytes({}, {"extab": b"\x01" * 8}),
                         {"extab": (0, 8)})


class LossesArePriced(unittest.TestCase):
    """POSITIVE side."""

    def test_a_destroyed_extab_reports_its_byte_delta(self):
        base = {"data": {"extab": priced(32, 32)}}
        cur = {"data": {"extab": priced(0, 32, sha="y")}}
        rows = dg.data_section_verdicts(base, cur)
        self.assertEqual(len(rows), 1)
        detail = rows[0][2]
        self.assertIn("extab 32->0 of 32 (-32 B)", detail)
        self.assertIn("NET -32 B", detail)
        self.assertIn("DESTROYS MATCHED DATA", detail)

    def test_a_gain_is_priced_too_and_not_called_a_loss(self):
        base = {"data": {".rodata": priced(100, 112)}}
        cur = {"data": {".rodata": priced(112, 112, sha="y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("NET +12 B", detail)
        self.assertNotIn("DESTROYS", detail)

    def test_sections_net_against_each_other(self):
        base = {"data": {"extab": priced(32, 32), ".rodata": priced(0, 8)}}
        cur = {"data": {"extab": priced(0, 32, "y"),
                        ".rodata": priced(8, 8, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("NET -24 B", detail)


class SilenceWhenNothingMoved(unittest.TestCase):
    """NEGATIVE side: the half that decides whether this can be trusted."""

    def test_identical_sections_produce_no_row(self):
        same = {"data": {"extab": priced(32, 32), ".rodata": priced(9, 16)}}
        self.assertEqual(dg.data_section_verdicts(same, dict(same)), [])

    def test_a_missing_baseline_produces_no_row(self):
        cur = {"data": {"extab": priced(0, 32)}}
        self.assertEqual(dg.data_section_verdicts(None, cur), [])
        self.assertEqual(dg.data_section_verdicts({"data": None}, cur), [])

    def test_a_same_digest_same_count_section_is_not_flagged(self):
        base = {"data": {"extab": priced(32, 32), ".sdata2": priced(6, 8)}}
        cur = {"data": {"extab": priced(32, 32), ".sdata2": priced(6, 8)}}
        self.assertEqual(dg.data_section_verdicts(base, cur), [])


class BackCompatibility(unittest.TestCase):
    def test_an_old_digest_baseline_still_detects_the_change(self):
        base = {"data": {"extab": "aaa"}}
        cur = {"data": {"extab": "bbb"}}
        rows = dg.data_section_verdicts(base, cur)
        self.assertEqual(len(rows), 1)
        self.assertIn("extab", rows[0][2])

    def test_an_old_baseline_says_unpriced_instead_of_guessing(self):
        base = {"data": {"extab": "aaa"}}
        cur = {"data": {"extab": priced(0, 32, "bbb")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("unpriced", detail)
        self.assertNotIn("NET", detail)

    def test_a_count_change_alone_is_detected_even_if_the_digest_holds(self):
        # can happen when our object shrinks to a prefix of itself
        base = {"data": {"extab": priced(32, 32, "same")}}
        cur = {"data": {"extab": priced(16, 32, "same")}}
        self.assertEqual(len(dg.data_section_verdicts(base, cur)), 1)


class LiveShape(unittest.TestCase):
    def test_a_real_object_prices_against_its_target(self):
        ours = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        tgt = REPO / "build" / "GUNE5D" / "obj" / "zlib" / "inflate.o"
        if not (ours.exists() and tgt.exists()):
            self.skipTest("zlib/inflate objects not built")
        secs = dg.data_section_digests(ours, tgt)
        self.assertIn("extab", secs)
        self.assertNotIn(".comment", secs)
        self.assertIsNotNone(secs["extab"].get("matched"))

    def test_without_a_target_the_rows_are_unpriced_not_zero(self):
        ours = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        if not ours.exists():
            self.skipTest("zlib/inflate object not built")
        secs = dg.data_section_digests(ours, Path("no/such/object.o"))
        self.assertTrue(secs)
        self.assertNotIn("matched", secs["extab"])


if __name__ == "__main__":
    unittest.main()
