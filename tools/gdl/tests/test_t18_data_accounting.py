#!/usr/bin/env python3
"""T18 run-48 item 3: the DATA verdict prices ALL-OR-NOTHING, not per byte.

THE DEFECT (the second DATA blindness). The gate priced a data-section change
by NETTING per-section BYTE agreement, while the project's Data measure -- the
PROGRESS `Data:` line, read from build/GUNE5D/report.json -- counts a data
section as matched only when it matches ENTIRELY. A section one byte wrong
contributes its FULL size to the gate and ZERO to the image, so a keep that
breaks a section reads NET +0 / GATE OK while the image loses the section.

Reproduced at cdfff02e2 on game/ui/btext, whose extabindex is one byte off:

    GATE per-section byte accounting
      .rodata      0/0     .sdata 0/0      .sdata2 0/0
      extab        248/248 FULL
      extabindex   371/372 PARTIAL
      GATE TOTAL (per-byte)          619/620
      IMAGE-EQUIVALENT (all-or-none) 248/620
    REPORT.JSON main/game/ui/btext: matched_data=248 total_data=620

TWO-SIDED CALIBRATION at cdfff02e2 over all 168 unit pairs the gate can price
(T18_scratch/t18_calib_item3.py):

    POSITIVES  91 units / 134 sections where the accountings DISAGREE;
               30,920 bytes of matched Data overstated in total
    NEGATIVES  77 units / 326 sections identical either way (inert)

VALIDATION against report.json's matched_data, over the 129 units whose
section set the gate fully covers (39 excluded: objdump -s cannot dump a
bss-family section, and the report does not count .init as data):

    110 reproduce report.json exactly
     18 UNDER-count it (objdiff credits data per SYMBOL and resolves
        relocations)
      1 OVER-counts it -- Runtime.PPCEABI.H/NMWException, by 16 bytes

So the new number is a LOWER BOUND on the image's, which is the direction a
gate needs; the per-byte form was an UPPER bound and did the opposite.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import defake_gate as dg  # noqa: E402


def banked(image, positional, size, sha="x"):
    """A run-48 baseline row: both accountings, image priced."""
    return {"sha": sha, "size": size, "matched": positional,
            "matched_image": image, "target_size": size}


def legacy(positional, size, sha="x"):
    """A pre-run-48 baseline row: per-byte only."""
    return {"sha": sha, "size": size, "matched": positional,
            "target_size": size}


class ImageAccounting(unittest.TestCase):
    """`image_matched_data_bytes` -- pure over two {section: bytes} maps."""

    def test_a_full_match_counts_the_whole_section(self):
        blob = bytes(range(16))
        self.assertEqual(
            dg.image_matched_data_bytes({"extab": blob}, {"extab": blob}),
            {"extab": (16, 16)})

    def test_one_wrong_byte_costs_the_whole_section(self):
        # The btext extabindex shape: 371 of 372 bytes agree, and the image
        # counts none of them.
        ours = b"\x01" * 371 + b"\x02"
        target = b"\x01" * 372
        self.assertEqual(
            dg.image_matched_data_bytes({"extabindex": ours},
                                        {"extabindex": target}),
            {"extabindex": (0, 372)})
        # ... while the positional form beside it reads 371.
        self.assertEqual(
            dg.matched_data_bytes({"extabindex": ours},
                                  {"extabindex": target}),
            {"extabindex": (371, 372)})

    def test_a_length_difference_is_never_a_match(self):
        # A prefix is not a match: the target's remaining bytes are missing.
        self.assertEqual(
            dg.image_matched_data_bytes({"extab": b"\x01\x02"},
                                        {"extab": b"\x01\x02\x03\x04"}),
            {"extab": (0, 4)})

    def test_a_section_we_do_not_emit_loses_all_of_it(self):
        self.assertEqual(
            dg.image_matched_data_bytes({}, {"extab": b"\x01" * 8}),
            {"extab": (0, 8)})

    def test_a_section_only_we_emit_is_zero_of_zero(self):
        self.assertEqual(
            dg.image_matched_data_bytes({".rodata": b"\x01" * 8}, {}),
            {".rodata": (0, 0)})

    def test_an_empty_target_section_cannot_be_matched(self):
        # `b"" == b""` is True, so a naive equality test would credit an
        # empty section with 0 bytes and call it a match; the size is what
        # is counted, and it is zero either way.
        self.assertEqual(
            dg.image_matched_data_bytes({"extab": b""}, {"extab": b""}),
            {"extab": (0, 0)})


class Pricing(unittest.TestCase):
    """`data_section_verdicts` prices on the image number."""

    def test_the_gate_no_longer_prints_net_zero_on_a_section_loss(self):
        # THE REPRODUCTION, in the shape that reaches the verdict: a keep
        # that destroys a 72-byte matched section while repairing bytes
        # elsewhere. Per-byte: 72 lost, 72 gained, NET +0. Image: the
        # repaired section still does not match, so it is a clean -72.
        base = {"data": {"extab": banked(72, 72, 72),
                         ".rodata": banked(0, 40, 112)}}
        cur = {"data": {"extab": banked(0, 0, 72, "y"),
                        ".rodata": banked(0, 112, 112, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("NET -72 B", detail)
        self.assertIn("DESTROYS MATCHED DATA", detail)
        self.assertIn("ALL-OR-NOTHING", detail)

    def test_the_per_byte_numbers_are_kept_as_a_diagnostic_column(self):
        base = {"data": {"extab": banked(72, 72, 72)}}
        cur = {"data": {"extab": banked(0, 71, 72, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("extab 72->0 of 72 (-72 B)", detail)
        self.assertIn("[per-byte 72->71, diagnostic only]", detail)

    def test_a_full_repair_is_still_priced_as_a_gain(self):
        base = {"data": {".rodata": banked(0, 100, 112)}}
        cur = {"data": {".rodata": banked(112, 112, 112, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("NET +112 B", detail)
        self.assertNotIn("DESTROYS", detail)

    def test_nothing_moved_is_still_silent(self):
        same = {"data": {"extab": banked(32, 32, 32),
                         ".rodata": banked(0, 9, 16)}}
        self.assertEqual(dg.data_section_verdicts(same, dict(same)), [])

    def test_a_positional_only_move_is_still_detected(self):
        # Detection stays strictly more sensitive than pricing: the image
        # number is unchanged (the section matches neither way) and the
        # per-byte number moved, so the row must still appear.
        base = {"data": {"extab": banked(0, 30, 32, "a")}}
        cur = {"data": {"extab": banked(0, 31, 32, "a")}}
        self.assertEqual(len(dg.data_section_verdicts(base, cur)), 1)


class LegacyBaselines(unittest.TestCase):
    """A pre-run-48 baseline is LABELLED, never silently mixed."""

    def test_a_legacy_baseline_is_flagged_as_positional(self):
        base = {"data": {"extab": legacy(72, 72)}}
        cur = {"data": {"extab": banked(0, 0, 72, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("ACCOUNTING IS POSITIONAL", detail)
        self.assertIn("re-take the baseline", detail)
        self.assertNotIn("accounting is ALL-OR-NOTHING", detail)

    def test_a_current_run48_pair_is_labelled_all_or_nothing(self):
        base = {"data": {"extab": banked(72, 72, 72)}}
        cur = {"data": {"extab": banked(0, 0, 72, "y")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("ALL-OR-NOTHING", detail)
        self.assertNotIn("ACCOUNTING IS POSITIONAL", detail)

    def test_a_bare_digest_baseline_is_still_unpriced_not_guessed(self):
        base = {"data": {"extab": "aaa"}}
        cur = {"data": {"extab": banked(0, 0, 32, "bbb")}}
        detail = dg.data_section_verdicts(base, cur)[0][2]
        self.assertIn("unpriced", detail)
        self.assertNotIn("NET", detail)

    def test_section_row_reports_the_basis(self):
        self.assertEqual(dg._section_row(banked(1, 2, 3))[3], "image")
        self.assertEqual(dg._section_row(legacy(2, 3))[3], "positional")
        self.assertIsNone(dg._section_row("digest")[3])
        self.assertIsNone(dg._section_row(None)[3])


class LiveShape(unittest.TestCase):
    """The measured reproduction, against real objects when they are built."""

    def test_btext_prices_248_of_620_not_619_of_620(self):
        ours = REPO / "build/GUNE5D/src/game/ui/btext.o"
        tgt = REPO / "build/GUNE5D/obj/game/ui/btext.o"
        if not (ours.exists() and tgt.exists()):
            self.skipTest("game/ui/btext objects not built")
        rows = dg.data_section_digests(ours, tgt)
        image = sum(r.get("matched_image") or 0 for r in rows.values())
        positional = sum(r.get("matched") or 0 for r in rows.values())
        total = sum(r.get("target_size") or 0 for r in rows.values())
        self.assertEqual((image, total), (248, 620))
        self.assertEqual((positional, total), (619, 620))


if __name__ == "__main__":
    unittest.main()
