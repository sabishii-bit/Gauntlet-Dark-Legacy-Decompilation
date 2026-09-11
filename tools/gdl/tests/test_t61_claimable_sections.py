"""Two-sided tests for tools/gdl/claimable_sections.py (run 61 item 1).

The valid side is the two claims the project has already landed and verified
by hand -- game/game/controls' four sections and game/world/btricol's pool
extension -- reproduced from the pre-claim splits state. The invalid side is
the one that matters more: a section whose bytes DIFFER at its candidate base
must never be reported as claimable, however much relocation support that
base has.
"""
from pathlib import Path
import re
import unittest

from tools.gdl import claimable_sections as cs
from tools.gdl import fndiff

REPO = Path(__file__).resolve().parents[3]
SPLITS = REPO / "config" / "GUNE5D" / "splits.txt"

# name -> (section, address, size), the shape fndiff.symbol_table() returns.
SYMBOLS = {
    "lbl_80111F70": (".rodata", 0x80111F70, 0xC),
    "lbl_80111F7C": (".rodata", 0x80111F7C, 0xB),
    "lbl_80111FA0": (".rodata", 0x80111FA0, 0x9),
    "lbl_80345D40": (".sdata2", 0x80345D40, 0x8),
    "lbl_80345DB0": (".sdata2", 0x80345DB0, 0x8),
}


class ScoreBase(unittest.TestCase):
    def test_identical_words_are_a_hundred_percent(self):
        blob = bytes(range(16))
        score = cs.score_base(blob, set(), blob)
        self.assertEqual((score["compared"], score["equal"]), (4, 4))
        self.assertEqual(score["percent"], 100.0)
        self.assertIsNone(score["first_difference"])

    def test_one_differing_word_is_named_and_is_not_a_hundred(self):
        ours = bytes(range(16))
        theirs = bytearray(ours)
        theirs[9] ^= 0xFF
        score = cs.score_base(ours, set(), bytes(theirs))
        self.assertLess(score["percent"], 100.0)
        self.assertEqual(score["first_difference"]["offset"], 8)

    def test_relocated_words_are_skipped_not_counted_as_equal(self):
        ours = bytes(range(16))
        theirs = bytes(16)
        score = cs.score_base(ours, {0, 4, 8, 12}, theirs)
        self.assertEqual((score["compared"], score["skipped"]), (0, 4))
        self.assertIsNone(score["percent"])
        self.assertIn("every word is relocated", score["note"])

    def test_a_range_outside_the_dol_scores_nothing_not_a_hundred(self):
        score = cs.score_base(bytes(16), set(), None)
        self.assertIsNone(score["percent"])
        self.assertEqual(score["compared"], 0)
        self.assertIn("not inside the DOL", score["note"])


class Boundaries(unittest.TestCase):
    def test_exact_inside_and_gap_are_distinguished(self):
        self.assertEqual(
            cs.boundary_of(".rodata", 0x80111F70, SYMBOLS)["kind"], "exact")
        inside = cs.boundary_of(".rodata", 0x80111FA4, SYMBOLS)
        self.assertEqual(inside["kind"], "inside")
        self.assertEqual(inside["front_gap"], 4)
        self.assertEqual(inside["symbol"], "lbl_80111FA0")
        self.assertEqual(
            cs.boundary_of(".rodata", 0x80111FFF, SYMBOLS)["kind"], "gap")

    def test_an_end_inside_a_symbol_rounds_up_to_its_boundary(self):
        hit = cs.straddled_end(".sdata2", 0x80345DB4, SYMBOLS)
        self.assertEqual(hit, ("lbl_80345DB0", 0x80345DB0, 0x80345DB8))
        self.assertIsNone(cs.straddled_end(".sdata2", 0x80345DB8, SYMBOLS))

    def test_an_unaligned_end_that_is_no_symbol_start_is_rounded_and_said_so(self):
        starts = {entry[1] for entry in SYMBOLS.values()
                  if entry[0] == ".rodata"}
        end, note = cs.resolve_end(".rodata", 0x80111F73, SYMBOLS, starts)
        self.assertEqual(end, 0x80111F7C)
        self.assertIn("dtk would reject", note)
        aligned, quiet = cs.resolve_end(".rodata", 0x80111F74, SYMBOLS, starts)
        self.assertEqual((aligned, quiet), (0x80111F74, None))
        onsym, quiet2 = cs.resolve_end(".rodata", 0x80111F7C, SYMBOLS, starts)
        self.assertEqual((onsym, quiet2), (0x80111F7C, None))

    def test_an_overlap_is_found_and_the_units_own_claim_is_excluded(self):
        intervals = {".data": [(0x100, 0x200, "a.c"), (0x300, 0x400, "b.c")]}
        self.assertEqual(
            cs.overlapping_claim(".data", 0x180, 0x280, intervals)["unit"],
            "a.c")
        self.assertIsNone(cs.overlapping_claim(".data", 0x180, 0x280,
                                               intervals, exclude="a.c"))
        self.assertIsNone(cs.overlapping_claim(".data", 0x200, 0x300,
                                               intervals))


class CandidateBases(unittest.TestCase):
    """The mispairing that makes a single-row candidate base a lie."""

    OURS_SYMBOLS = {"@1529": (".sdata2", 0x00, 4), "@1528": (".sdata2", 0x10, 4)}

    def test_agreeing_rows_produce_one_base_and_a_mispair_adds_a_wrong_one(self):
        ours = [("f", 0x10, "R_PPC_EMB_SDA21", "@1529", 0),
                ("f", 0x20, "R_PPC_EMB_SDA21", "@1528", 0)]
        target = [("f", 0x10, "R_PPC_EMB_SDA21", "lbl_803463F8", 0),
                  ("f", 0x20, "R_PPC_EMB_SDA21", "lbl_80346418", 0)]
        bases, stats = cs.candidate_bases(ours, target, self.OURS_SYMBOLS)
        self.assertEqual(stats["paired"], 2)
        self.assertEqual(sorted(bases[".sdata2"]), [0x803463F8, 0x80346408])
        self.assertEqual(bases[".sdata2"][0x803463F8]["symbols"], ["@1529"])

    def test_an_unpairable_row_is_counted_never_guessed(self):
        ours = [("f", 0x10, "R_PPC_EMB_SDA21", "@1529", 0)]
        bases, stats = cs.candidate_bases(ours, [], self.OURS_SYMBOLS)
        self.assertEqual((stats["paired"], stats["unpaired"]), (0, 1))
        self.assertEqual(bases, {})


class Election(unittest.TestCase):
    def scored(self, table):
        return {base: {"support": support, "dissent": len(table) - 1,
                       "dol": None if percent is None else
                       {"percent": percent, "compared": 30, "equal": 30,
                        "skipped": 0, "first_difference": None, "note": ""}}
                for base, (support, percent) in table.items()}

    def test_the_dol_beats_relocation_support(self):
        # The measured controls .sdata2 shape, with the support deliberately
        # inverted: byte equality must still decide.
        best = cs._elect(self.scored({0x803463E8: (9, 13.3),
                                      0x803463F8: (1, 100.0),
                                      0x80346408: (9, 16.7)}), bss=False)
        self.assertEqual(best, 0x803463F8)

    def test_two_byte_equal_candidates_do_not_elect_anyone(self):
        self.assertIsNone(cs._elect(self.scored({1: (3, 100.0),
                                                 2: (3, 100.0)}), bss=False))

    def test_bss_needs_a_consensus_beating_every_rival_combined(self):
        table = {0x802407B8: (6, None), 0x80240800: (2, None),
                 0x80240900: (2, None), 0x80240A00: (2, None)}
        self.assertIsNone(cs._elect(self.scored(table), bss=True))
        table[0x802407B8] = (7, None)
        self.assertEqual(cs._elect(self.scored(table), bss=True), 0x802407B8)
        self.assertIsNone(cs._elect(self.scored({1: (1, None)}), bss=True))


class SectionVerdict(unittest.TestCase):
    def score(self, percent, compared=30):
        return {"support": 6, "dissent": 0,
                "dol": {"percent": percent, "compared": compared, "equal": 1,
                        "skipped": 0, "note": "",
                        "first_difference": {"offset": 0, "ours": "aa",
                                             "dol": "bb"}}}

    def call(self, percent, **kw):
        options = {"boundary": {"kind": "exact"}, "straddle": None,
                   "collision": None, "bss": False}
        options.update(kw)
        return cs.section_result(".rodata", 0x40, {0x80110000: {}}, 0x80110000,
                                 self.score(percent), options["boundary"],
                                 options["straddle"], options["collision"],
                                 options["bss"])

    def test_only_full_byte_equality_is_claimable(self):
        self.assertEqual(self.call(100.0)["verdict"], "claimable")

    def test_differing_bytes_are_never_claimable(self):
        for percent in (99.9, 51.8, 37.3, 0.0):
            with self.subTest(percent=percent):
                self.assertEqual(self.call(percent)["verdict"],
                                 "blocked-bytes-differ")

    def test_nothing_compared_is_undecided_not_a_difference(self):
        row = cs.section_result(".rodata", 0x40, {1: {}}, 1,
                                self.score(None, compared=0),
                                {"kind": "exact"}, None, None, False)
        self.assertEqual(row["verdict"], "unresolved-not-comparable")

    def test_a_base_inside_a_symbol_is_short_at_front_not_claimable(self):
        row = self.call(100.0, boundary={"kind": "inside", "symbol": "lbl_x",
                                         "start": 0x8010FFF0, "end": 0x80110010,
                                         "front_gap": 0x10})
        self.assertEqual(row["verdict"], "blocked-short-at-front")
        self.assertEqual(row["front_gap"], 0x10)

    def test_an_overlapping_extent_is_a_conflict(self):
        row = self.call(100.0, collision={"unit": "other.c", "start": 0,
                                          "end": 0xFFFFFFFF})
        self.assertEqual(row["verdict"], "conflict-existing-claim")
        self.assertIn("other.c", row["reason"])

    def test_no_base_and_no_election_are_separate_refusals(self):
        empty = cs.section_result(".rodata", 0x40, {}, None, {}, None, None,
                                  None, False)
        self.assertEqual(empty["verdict"], "unresolved-no-base")
        undecided = cs.section_result(".rodata", 0x40, {1: {}, 2: {}}, None,
                                      {}, None, None, None, False)
        self.assertEqual(undecided["verdict"], "unresolved-disagreement")

    def test_an_extension_counts_only_the_bytes_it_gains(self):
        row = cs.section_result(".sdata2", 0x74, {0x80345D40: {}}, 0x80345D40,
                                self.score(100.0), {"kind": "exact"},
                                ("lbl_80345DB0", 0x80345DB0, 0x80345DB8),
                                None, False, prior=(0x80345D40, 0x80345D70),
                                tail_bytes=bytes(4))
        self.assertEqual(row["verdict"], "claimable")
        self.assertEqual(row["claim_end"], "0x80345DB8")
        self.assertEqual(row["claim_bytes"], 0x48)
        self.assertEqual(row["kind"], "extend")

    def rounded_tail(self, tail_bytes, *, use_alignment=False):
        # memcard's 145-byte prefix was advertised as a 904-byte claim:
        # the coarse target string-run symbol extends well past our data.
        base, size, end = 0x801131C0, 145, 0x80113548
        return cs.section_result(
            ".rodata", size, {base: {}}, base, self.score(100.0),
            {"kind": "exact"},
            None if use_alignment else ("string_run", base, end),
            None, False,
            align=(lambda value: (end, "test rounded end"))
            if use_alignment else None, tail_bytes=tail_bytes)

    def test_a_matching_prefix_cannot_claim_a_nonzero_unemitted_tail(self):
        for use_alignment in (False, True):
            with self.subTest(use_alignment=use_alignment):
                row = self.rounded_tail(bytes(3) + b"NEXT" + bytes(752),
                                        use_alignment=use_alignment)
                self.assertEqual(row["verdict"], "blocked-unemitted-tail")
                self.assertEqual(row["unemitted_tail"]["first_nonzero"],
                                 "0x80113254")
                self.assertEqual(cs.rank_units([
                    {"unit": "memcard", "sections": [row]}])[0][1], 0)

    def test_missing_short_or_long_tail_measurements_fail_closed(self):
        for tail in (None, b"", bytes(758), bytes(760)):
            with self.subTest(length=None if tail is None else len(tail)):
                row = self.rounded_tail(tail)
                self.assertEqual(row["verdict"], "unresolved-unread-tail")

    def test_verified_zero_alignment_tail_is_reported(self):
        row = cs.section_result(
            ".rodata", 9, {0x1000: {}}, 0x1000, self.score(100.0),
            {"kind": "exact"}, None, None, False,
            align=lambda end: (0x100C, "align to four"), tail_bytes=bytes(3))
        self.assertEqual(row["verdict"], "claimable")
        self.assertEqual(row["unemitted_tail"],
                         {"start": "0x00001009", "size": 3, "all_zero": True})

    def test_exact_extent_needs_no_tail_measurement(self):
        row = self.call(100.0)
        self.assertEqual(row["verdict"], "claimable")
        self.assertNotIn("unemitted_tail", row)

    def test_a_base_that_contradicts_an_existing_claim_refuses(self):
        row = cs.section_result(".sdata2", 0x74, {1: {}}, 0x80345D50,
                                self.score(100.0), {"kind": "exact"}, None,
                                None, False, prior=(0x80345D40, 0x80345D70))
        self.assertEqual(row["verdict"], "unresolved-base-vs-claim")


class Ranking(unittest.TestCase):
    def test_claimable_bytes_outrank_blocked_ones(self):
        results = [
            {"unit": "a", "sections": [{"verdict": "claimable",
                                        "claim_bytes": 10, "ours_size": 10}]},
            {"unit": "b", "sections": [{"verdict": "blocked-bytes-differ",
                                        "ours_size": 900}]},
            {"unit": "c", "sections": [{"verdict": "claimable-bss",
                                        "claim_bytes": 64, "ours_size": 64}]},
            {"unit": "d", "sections": []}]
        self.assertEqual([row[0] for row in cs.rank_units(results)],
                         ["c", "a", "b"])


def calibration_splits(tmp, drop, shrink=()):
    """The live splits.txt with named claims removed / shortened again."""
    text = SPLITS.read_text(encoding="utf-8")
    out, unit = [], None
    for line in text.splitlines():
        head = re.match(r"^(\S.+):$", line)
        if head:
            unit = head.group(1)
        row = re.match(r"^\t(\S+)\s+start:0x([0-9A-Fa-f]+)"
                       r"\s+end:0x([0-9A-Fa-f]+)", line)
        if row and (unit, row.group(1)) in drop:
            continue
        if row and (unit, row.group(1)) in dict(shrink):
            line = "\t%-11s start:0x%s end:%s" % (
                row.group(1), row.group(2), dict(shrink)[(unit, row.group(1))])
        out.append(line)
    path = Path(tmp) / "t4_calibration_splits.txt"
    path.write_text("\n".join(out) + "\n", encoding="utf-8")
    return path


@unittest.skipUnless(
    SPLITS.is_file() and Path(fndiff.OBJDUMP).exists()
    and (REPO / "build/GUNE5D/obj/game/game/controls.o").is_file()
    and (REPO / "build/GUNE5D/src/game/game/controls.o").is_file(),
    "needs the split target objects and a built tree")
class LiveCalibration(unittest.TestCase):
    """Reproduce two already-landed, hand-verified claims from before them."""

    @classmethod
    def setUpClass(cls):
        import tempfile
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)

    def run_unit(self, unit, drop=(), shrink=()):
        path = calibration_splits(self.tmp.name, set(drop), shrink)
        splits = cs.parse_splits(path)
        result = cs.census(unit, splits, cs.claimed_intervals(splits))
        return {row["section"]: row for row in result["sections"]}

    def test_controls_four_landed_claims_are_reproduced_exactly(self):
        rows = self.run_unit("game/game/controls", drop={
            ("game/game/controls.c", section)
            for section in (".data", ".bss", ".sdata", ".sdata2")})
        expected = {".data": ("0x8011A220", "0x8011AED4", "claimable"),
                    ".sdata": ("0x80343BE0", "0x80343BE8", "claimable"),
                    ".sdata2": ("0x803463F8", "0x80346470", "claimable"),
                    ".bss": ("0x802407B8", "0x80240FD0", "claimable-bss")}
        for section, (start, end, verdict) in expected.items():
            with self.subTest(section=section):
                row = rows[section]
                self.assertEqual(row["verdict"], verdict)
                self.assertEqual((row["claim_start"], row["claim_end"]),
                                 (start, end))

    def test_controls_rodata_is_measured_unclaimable_not_pasted(self):
        rows = self.run_unit("game/game/controls")
        row = rows[".rodata"]
        self.assertNotIn("claimable", row["verdict"])
        self.assertLess(max(entry["dol_percent"] or 0.0
                            for entry in row["evidence"].values()), 100.0)

    def test_btricol_pool_extension_is_reproduced_exactly(self):
        rows = self.run_unit("game/world/btricol", shrink=[
            (("game/world/btricol.c", ".sdata2"), "0x80345D70")])
        row = rows[".sdata2"]
        self.assertEqual(row["verdict"], "claimable")
        self.assertEqual((row["claim_start"], row["claim_end"]),
                         ("0x80345D40", "0x80345DB8"))
        self.assertEqual(row["kind"], "extend")


if __name__ == "__main__":
    unittest.main()
