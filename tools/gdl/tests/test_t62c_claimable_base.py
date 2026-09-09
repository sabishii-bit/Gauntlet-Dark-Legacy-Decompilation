"""claimable_sections: the base selection three lanes measured as wrong.

WHAT WAS WRONG (run 62, reproduced at fe3e4e736 before this change):

    game/game/player   .rodata  vote 0x80113E54   true 0x80113E28
    game/ui/options    .rodata  vote 0x80113A40   true 0x80113A0C
    game/ui/options    .sdata2  vote 0x80347598   a base that misaligns f64
    game/game/controls .rodata  vote 0x80111FB8   true 0x80111F70

The vote ranks candidates by relocation support and then by raw DOL
agreement, and both are the wrong measure for a section our source has not
finished emitting. Player `.rodata` scored 53.8% at a base 0x2C above the
truth and 14.4% at the truth, because at the RIGHT base every datum after
the first omission is compared against the WRONG target datum.

FOUR FIXES, each with its own refusal:

  * `object_symbols` could not see MWCC's section-base local at all -- its
    `objdump -t` row has a `l      ` flag field with no `O`, and the old
    whitespace-split pattern needed one more token. That symbol is the
    pool-base rule's entire anchor, so the rule could not fire anywhere.
  * `pool_base_sites` derives the base arithmetically instead of voting.
  * `proven_alignment` + `claim_containing` remove candidates that CANNOT
    be a base before anything is ranked.
  * `gap_inventory` resynchronises after each DOL-side insertion, so a
    short section's real shape (front? middle? both?) is measured rather
    than assumed, and two bases' numbers are comparable.

TWO SIDES. Synthetic rows drive every rule and every refusal. The live half
pins the four cases above as regression fixtures against the built objects
and skips when they are absent.
"""
import struct
import unittest
from pathlib import Path

from tools.gdl import claimable_sections as cs

REPO = Path(__file__).resolve().parents[3]
BUILD = REPO / "build" / "GUNE5D"

SYMBOLS = {
    "lbl_80111F70": (".rodata", 0x80111F70, 0xC),
    "lbl_80111F7C": (".rodata", 0x80111F7C, 0xB),
    "lbl_80113AE0": (".rodata", 0x80113AE0, 0xE),
    "optionsStringPool": (".rodata", 0x80113830, 0xD),
}


def addi(rd, ra, immediate):
    return (14 << 26) | (rd << 21) | (ra << 16) | (immediate & 0xFFFF)


class SymbolTableParsing(unittest.TestCase):
    """The defect that silenced the pool-base rule everywhere."""

    DUMP = "\r\n".join([
        "",
        "obj:     file format elf32-powerpc",
        "",
        "SYMBOL TABLE:",
        "00000000 l    df *ABS*\t00000000 player.c",
        "00000000 l    d  .rodata\t00000000 .rodata",
        "00000000 l       .rodata\t00000000 ...rodata.0",
        "00000000 l     O .rodata\t0000000a @125",
        "0000000c l     O .rodata\t0000000e @128",
        "00000000         *UND*\t00000000 lbl_8011FC48",
        "000009a8 g     F .text\t0000064c write_health_and_items",
    ])

    def parse(self):
        original = cs._dump
        cs._dump = lambda path, *flags: self.DUMP
        try:
            return cs.object_symbols("ignored")
        finally:
            cs._dump = original

    def test_the_section_base_local_is_visible(self):
        table = self.parse()
        self.assertEqual(table["...rodata.0"], (".rodata", 0, 0))

    def test_ordinary_and_sized_symbols_still_parse(self):
        table = self.parse()
        self.assertEqual(table["@125"], (".rodata", 0, 10))
        self.assertEqual(table["@128"], (".rodata", 0xC, 14))
        self.assertEqual(table["write_health_and_items"],
                         (".text", 0x9A8, 0x64C))

    def test_undefined_and_absolute_rows_are_not_defined_symbols(self):
        table = self.parse()
        self.assertNotIn("lbl_8011FC48", table)
        self.assertNotIn("player.c", table)

    def test_the_base_name_is_derived_from_the_section(self):
        self.assertEqual(cs.section_base_name(".rodata"), "...rodata.0")
        self.assertEqual(cs.section_base_name(".sdata2"), "...sdata2.0")

    def test_a_section_base_row_never_reaches_the_vote(self):
        ours = [("f", 0x10, "R_PPC_ADDR16_LO", "...rodata.0", 0),
                ("f", 0x20, "R_PPC_ADDR16_LO", "@125", 0)]
        target = [("f", 0x10, "R_PPC_ADDR16_LO", "lbl_80113AE0", 0),
                  ("f", 0x20, "R_PPC_ADDR16_LO", "lbl_80113AE0", 0)]
        bases, stats = cs.candidate_bases(
            ours, target, {"...rodata.0": (".rodata", 0, 0),
                           "@125": (".rodata", 0, 10)})
        self.assertEqual(stats["section_base_rows"], 1)
        self.assertEqual(sorted(bases[".rodata"]), [0x80113AE0])
        self.assertEqual(bases[".rodata"][0x80113AE0]["symbols"], ["@125"])


class PoolBaseRule(unittest.TestCase):
    """Both forms of the rule, and every way a site is refused."""

    def test_the_datum_form_derives_the_base_from_each_stored_pointer(self):
        # controls' SMTAB pointer table, reduced: eight rows agree on
        # 0x80111F70 and three later ones carry an extra 0x48.
        ours = {".data": [(0xC48, "R_PPC_ADDR32", "...rodata.0", 0),
                          (0xC4C, "R_PPC_ADDR32", "...rodata.0", 0xC),
                          (0xC80, "R_PPC_ADDR32", "...rodata.0", 0x128)]}
        target = {".data": [(0xC48, "R_PPC_ADDR32", "lbl_80111F70", 0),
                            (0xC4C, "R_PPC_ADDR32", "lbl_80111F7C", 0),
                            (0xC80, "R_PPC_ADDR32", "lbl_801120E0", 0)]}
        pool = cs.pool_base_sites(".rodata", ours, target, {}, {}, SYMBOLS)
        self.assertEqual(pool["bind_base"], 0x80111F70)
        self.assertEqual(pool["candidate_bases"],
                         [0x80111F70, 0x80111FB8])
        self.assertEqual(len(pool["sites"]), 3)
        self.assertIn("the lowest is the binding", pool["note"])

    def anchor_streams(self, our_immediates, target_immediates,
                       our_base_register=29, target_base_register=29):
        ours = {"f": {0x34: [addi(our_base_register, 4, 0), "...rodata.0"]}}
        theirs = {"f": {0x34: [addi(target_base_register, 4, 0),
                               "lbl_80113AE0"]}}
        for step, (mine, yours) in enumerate(zip(our_immediates,
                                                 target_immediates)):
            at = 0x40 + step * 4
            ours["f"][at] = [addi(7, our_base_register, mine), None]
            theirs["f"][at] = [addi(7, target_base_register, yours), None]
        return ours, theirs

    def test_the_anchor_form_separates_the_section_base_from_the_binding(self):
        # player write_health_and_items: ours 28/44/56, target 868/884/896.
        ours, theirs = self.anchor_streams([28, 44, 56], [868, 884, 896])
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertEqual(pool["section_base"], 0x80113AE0)
        self.assertEqual(pool["bind_base"], 0x80113AE0 + 840)
        self.assertEqual(pool["bind_base"], 0x80113E28)
        self.assertEqual(pool["candidate_bases"], [0x80113E28])

    def test_a_later_site_behind_an_insertion_does_not_win(self):
        ours, theirs = self.anchor_streams([28, 456], [868, 1784])
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertEqual(pool["candidate_bases"], [0x80113E28, 0x80114010])
        self.assertEqual(pool["bind_base"], 0x80113E28)

    def test_a_differing_base_register_is_skipped_with_its_reason(self):
        ours, theirs = self.anchor_streams([28], [868], 29, 28)
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertEqual(pool["sites"], [])
        self.assertIsNone(pool["bind_base"])
        self.assertEqual(pool["section_base"], 0x80113AE0)
        self.assertIn("base register differs", pool["skipped"][0])

    def test_an_unpaired_target_base_is_skipped_never_assumed(self):
        ours, theirs = self.anchor_streams([28], [868])
        theirs["f"][0x34][1] = None
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertIsNone(pool["bind_base"])
        self.assertIn("no paired target base relocation", pool["skipped"][0])

    def test_an_unresolvable_target_symbol_is_skipped(self):
        ours, theirs = self.anchor_streams([28], [868])
        theirs["f"][0x34][1] = "who_is_this"
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertIsNone(pool["bind_base"])
        self.assertIsNone(pool["section_base"])
        self.assertIn("unresolved", pool["skipped"][0])

    def test_a_relocated_consuming_word_is_not_an_immediate_pair(self):
        ours, theirs = self.anchor_streams([28], [868])
        ours["f"][0x40][1] = "@125"
        pool = cs.pool_base_sites(".rodata", {}, {}, ours, theirs, SYMBOLS)
        self.assertEqual(pool["sites"], [])

    def test_no_anchor_anywhere_says_so_instead_of_inventing_a_base(self):
        pool = cs.pool_base_sites(".sdata2", {}, {}, {}, {}, SYMBOLS)
        self.assertIsNone(pool["bind_base"])
        self.assertIsNone(pool["section_base"])
        self.assertIn("says nothing here", pool["note"])


class ProvenAlignment(unittest.TestCase):
    def test_padding_beyond_four_proves_an_eight_byte_requirement(self):
        # options .sdata2: "..." ends +0x63 and the f64 magic is at +0x68.
        symbols = {"@931": (".sdata2", 0x5C, 7), "@933": (".sdata2", 0x68, 8)}
        self.assertEqual(cs.proven_alignment(symbols, ".sdata2"), 8)

    def test_a_gap_a_smaller_alignment_explains_never_proves_eight(self):
        # +0x18 after an end of +0x17 is exactly what 4-alignment does, so
        # nothing above 2 is proven and 8 must not be claimed.
        symbols = {"a": (".sdata2", 0x10, 7), "b": (".sdata2", 0x18, 8)}
        self.assertLess(cs.proven_alignment(symbols, ".sdata2"), 4)

    def test_the_first_datum_alone_proves_nothing(self):
        # Whatever sits before the first symbol is not a symbol we can see.
        self.assertEqual(
            cs.proven_alignment({"a": (".sdata2", 0x10, 8)}, ".sdata2"), 1)

    def test_a_string_only_section_does_not_claim_eight(self):
        symbols = {"@444": (".rodata", 0x00, 15), "@519": (".rodata", 0x10, 9),
                   "@919": (".rodata", 0x1C, 13)}
        self.assertLess(cs.proven_alignment(symbols, ".rodata"), 8)

    def test_an_empty_or_foreign_section_is_one(self):
        self.assertEqual(cs.proven_alignment({}, ".rodata"), 1)
        self.assertEqual(
            cs.proven_alignment({"a": (".data", 0x8, 8)}, ".rodata"), 1)


class ClaimContainment(unittest.TestCase):
    INTERVALS = {".sdata2": [(0x80347448, 0x80347510, "game/world/newcam.c")]}

    def test_a_base_inside_another_units_run_is_named(self):
        hit = cs.claim_containing(".sdata2", 0x80347458, self.INTERVALS)
        self.assertEqual(hit["unit"], "game/world/newcam.c")

    def test_the_units_own_claim_and_the_boundary_are_not_a_collision(self):
        self.assertIsNone(cs.claim_containing(
            ".sdata2", 0x80347458, self.INTERVALS,
            exclude="game/world/newcam.c"))
        self.assertIsNone(cs.claim_containing(".sdata2", 0x80347510,
                                              self.INTERVALS))


class GapInventory(unittest.TestCase):
    def test_a_dol_side_insertion_is_found_and_described(self):
        ours = struct.pack(">6I", 1, 2, 3, 4, 5, 6)
        dol = struct.pack(">3I", 1, 2, 3) + b"NAME\x00\x00\x00\x00" \
            + struct.pack(">3I", 4, 5, 6)
        state = cs.gap_inventory(ours, set(), dol, 0x80110000)
        self.assertEqual(state["mismatched"], 0)
        self.assertEqual(state["resynced_equal"], 6)
        self.assertEqual(len(state["gaps"]), 1)
        gap = state["gaps"][0]
        self.assertEqual((gap["our_offset"], gap["address"], gap["size"]),
                         (12, 0x8011000C, 8))
        self.assertIn("NAME", gap["content"])

    def test_a_real_difference_is_not_absorbed_as_a_gap(self):
        ours = struct.pack(">6I", 1, 2, 3, 4, 5, 6)
        dol = struct.pack(">6I", 1, 2, 0xDEAD, 4, 5, 6)
        state = cs.gap_inventory(ours, set(), dol, 0x80110000)
        self.assertEqual(state["mismatched"], 1)
        self.assertEqual(state["gaps"], [])

    def test_a_leading_gap_is_a_base_correction_not_an_interior_gap(self):
        ours = struct.pack(">4I", 1, 2, 3, 4)
        dol = struct.pack(">5I", 0x99, 1, 2, 3, 4)
        state = cs.gap_inventory(ours, set(), dol, 0x80110000)
        self.assertEqual(state["leading_gap"], 4)
        self.assertEqual(state["implied_base"], 0x80110004)
        self.assertEqual(state["mismatched"], 0)

    def test_relocated_words_never_justify_a_resync(self):
        ours = bytes(24)
        dol = b"\xde\xad\xbe\xef" + bytes(24)
        state = cs.gap_inventory(ours, {0, 4, 8, 12, 16, 20}, dol, 0x80110000)
        self.assertEqual(state["relocated"], 6)
        self.assertEqual(state["gaps"], [])

    def test_a_short_dol_window_is_flagged_not_scored_as_agreement(self):
        state = cs.gap_inventory(bytes(64), set(), bytes(8), 0x80110000)
        self.assertTrue(state["truncated"])
        self.assertEqual(state["words"], 16)


class DescribeBytes(unittest.TestCase):
    def test_a_string_without_a_trailing_nul_is_still_read_as_text(self):
        blob = b"llo %2d lhi %2d rlo %2d"
        self.assertIn("llo %2d lhi", cs.describe_bytes(blob))

    def test_floats_are_decoded_and_anything_else_is_hex(self):
        self.assertIn("f32 1", cs.describe_bytes(struct.pack(">f", 1.0)))
        self.assertIn("f64 0.9", cs.describe_bytes(struct.pack(">d", 0.9)))
        self.assertTrue(cs.describe_bytes(b"\x01\x02\x03").startswith("0x"))
        self.assertEqual(cs.describe_bytes(b""), "")


class ElectBase(unittest.TestCase):
    def scored(self, table):
        """{base: (support, raw percent, resync mismatches, gaps, words)}."""
        out = {}
        for base, row in table.items():
            support, percent, mismatched, gaps, words = row
            out[base] = {
                "support": support, "dissent": len(table) - 1,
                "dol": {"percent": percent, "compared": 30, "equal": 1,
                        "skipped": 0, "note": "", "first_difference": None},
                "inventory": None if mismatched is None else {
                    "mismatched": mismatched, "gaps": [{}] * gaps,
                    "words": words, "leading_gap": 0}}
        return out

    def test_the_pool_base_beats_a_better_supported_vote(self):
        scored = self.scored({0x80113E28: (2, 14.4, 0, 3, 131),
                              0x80113E54: (3, 53.8, 17, 2, 131)})
        best, decision = cs.elect_base(
            scored, False, {"bind_base": 0x80113E28}, 4, None)
        self.assertEqual(best, 0x80113E28)
        self.assertEqual(decision["source"], "pool-base")
        self.assertEqual(decision["vote_base"], 0x80113E54)

    def test_a_misaligned_candidate_is_refused_with_the_reason(self):
        scored = self.scored({0x80347594: (5, 25.0, 0, 1, 28),
                              0x80347598: (4, 78.6, 6, 0, 28)})
        best, decision = cs.elect_base(scored, False, None, 8, None)
        self.assertEqual(best, 0x80347598)
        self.assertIn("0x80347594", decision["rejected"])
        self.assertIn("not 8-byte aligned", decision["rejected"]["0x80347594"])

    def test_a_base_inside_another_claim_is_refused_by_name(self):
        scored = self.scored({0x80347458: (9, 90.0, 0, 0, 28),
                              0x80347520: (1, 10.0, 5, 0, 28)})
        best, decision = cs.elect_base(
            scored, False, None, 4,
            lambda base: {"unit": "newcam.c", "start": 0x80347448,
                          "end": 0x80347510} if base < 0x80347510 else None)
        self.assertEqual(best, 0x80347520)
        self.assertIn("newcam.c", decision["rejected"]["0x80347458"])

    def test_the_only_cleanly_resyncing_candidate_beats_the_popular_one(self):
        # options .rodata: 74.4% at a base 10 words differ from, 28.2% at the
        # base every word resyncs against.
        scored = self.scored({0x80113A0C: (2, 28.2, 0, 1, 38),
                              0x80113A40: (4, 74.4, 10, 0, 38)})
        best, decision = cs.elect_base(scored, False, None, 4, None)
        self.assertEqual(best, 0x80113A0C)
        self.assertEqual(decision["source"], "clean-resync")

    def test_a_leading_gap_disqualifies_a_clean_candidate(self):
        scored = self.scored({0x80347590: (2, 3.6, 0, 2, 28),
                              0x80347598: (4, 78.6, 6, 0, 28)})
        scored[0x80347590]["inventory"]["leading_gap"] = 4
        best, decision = cs.elect_base(scored, False, None, 8, None)
        self.assertEqual(best, 0x80347598)
        self.assertEqual(decision["source"], "relocation-vote")
        self.assertEqual(decision["clean_resync"], [])

    def test_too_few_words_for_the_resync_rule_to_decide(self):
        scored = self.scored({0x100: (1, 10.0, 0, 1, 3),
                              0x200: (4, 90.0, 2, 0, 3)})
        best, decision = cs.elect_base(scored, False, None, 4, None)
        self.assertEqual(decision["source"], "relocation-vote")
        self.assertEqual(best, 0x200)

    def test_two_clean_candidates_do_not_elect_anyone_by_resync(self):
        scored = self.scored({0x100: (1, 10.0, 0, 1, 40),
                              0x200: (4, 90.0, 0, 1, 40)})
        _best, decision = cs.elect_base(scored, False, None, 4, None)
        self.assertEqual(decision["source"], "relocation-vote")
        self.assertEqual(decision["clean_resync"], ["0x00000100", "0x00000200"])

    def test_a_refused_pool_base_falls_back_and_says_so(self):
        scored = self.scored({0x80347594: (5, 25.0, 0, 1, 28),
                              0x80347598: (4, 78.6, 6, 0, 28)})
        best, decision = cs.elect_base(
            scored, False, {"bind_base": 0x80347594}, 8, None)
        self.assertEqual(best, 0x80347598)
        self.assertEqual(decision["source"], "relocation-vote")
        self.assertIn("not 8-byte aligned", decision["pool_base_rejected"])


class ShortSectionVerdicts(unittest.TestCase):
    def inventory(self, mismatched=0, gaps=0, equal=80, words=80):
        return {"mismatched": mismatched, "gaps": [{}] * gaps, "words": words,
                "resynced_equal": equal, "gap_bytes": 0x48 * gaps,
                "leading_gap": 0, "relocated": 0, "truncated": False}

    def call(self, **kw):
        options = {"boundary": {"kind": "exact"}, "inventory": None,
                   "front_deficit": None}
        options.update(kw)
        score = {"support": 6, "dissent": 0,
                 "dol": {"percent": 37.3, "compared": 83, "equal": 31,
                         "skipped": 0, "note": "",
                         "first_difference": {"offset": 0x7C, "ours": "aa",
                                              "dol": "bb"}}}
        return cs.section_result(
            ".rodata", 0x14A, {0x80111F70: {}}, 0x80111F70, score,
            options["boundary"], None, None, False,
            inventory=options["inventory"],
            front_deficit=options["front_deficit"])

    def test_a_hole_in_the_middle_is_not_reported_as_a_short_front(self):
        # controls .rodata: +0x000..+0x07C byte-identical, one 0x48 hole.
        row = self.call(inventory=self.inventory(gaps=1), front_deficit=0)
        self.assertEqual(row["verdict"], "blocked-short-in-middle")
        self.assertIn("0x0 byte(s) missing at the FRONT", row["reason"])

    def test_a_front_deficit_alone_is_short_at_front(self):
        row = self.call(inventory=self.inventory(gaps=0), front_deficit=0x348)
        self.assertEqual(row["verdict"], "blocked-short-at-front")

    def test_both_are_named_together(self):
        row = self.call(inventory=self.inventory(gaps=3), front_deficit=0x348)
        self.assertEqual(row["verdict"], "blocked-short-front-and-middle")
        self.assertIn("0x348", row["reason"])

    def test_words_that_still_differ_are_never_called_short(self):
        row = self.call(inventory=self.inventory(mismatched=30, gaps=0))
        self.assertEqual(row["verdict"], "blocked-bytes-differ")
        self.assertIn("still differ after allowing", row["reason"])

    def test_full_byte_equality_is_still_the_only_claimable(self):
        score = {"support": 6, "dissent": 0,
                 "dol": {"percent": 100.0, "compared": 83, "equal": 83,
                         "skipped": 0, "note": "", "first_difference": None}}
        row = cs.section_result(".rodata", 0x14A, {1: {}}, 0x80111F70, score,
                                {"kind": "exact"}, None, None, False,
                                inventory=self.inventory(gaps=0),
                                front_deficit=0)
        self.assertEqual(row["verdict"], "claimable")


def live_objects(unit):
    ours = BUILD / "src" / (unit + ".o")
    target = BUILD / "obj" / (unit + ".o")
    return (ours, target) if ours.is_file() and target.is_file() else None


@unittest.skipUnless(live_objects("game/game/controls"),
                     "built objects are not present in this worktree")
class LiveRegressionFixtures(unittest.TestCase):
    """The three lanes' cases, with the TRUE bases recorded.

    Each was reported at a different, wrong base before this change; the
    values here are the ones lanes H, I and J established independently from
    the DOL bytes and their own build experiments.
    """

    CASES = (
        # unit, section, true bind base, true target section base or None
        # player's .rodata was here with a 0x348 front deficit; the block
        # and its interior literals are recovered, so the deficit is 0 and
        # the case no longer describes a short section.
        ("game/game/controls", ".rodata", 0x80111F70, None),
        ("game/ui/options", ".rodata", 0x80113A0C, None),
    )

    @classmethod
    def setUpClass(cls):
        cls.symbols = None

    def measure(self, unit, section):
        from tools.gdl import fndiff
        ours, target = live_objects(unit)
        pool = cs.pool_base_sites(
            section, cs.data_relocation_rows(ours),
            cs.data_relocation_rows(target), cs.instruction_map(ours),
            cs.instruction_map(target), fndiff.symbol_table())
        payload = cs.section_bytes(ours, section)
        relocated = cs.relocated_offsets(ours).get(section, set())
        return pool, payload, relocated

    def test_each_case_resolves_to_its_recorded_base(self):
        from tools.gdl import fndiff
        splits = cs.parse_splits(REPO / "config" / "GUNE5D" / "splits.txt")
        intervals = cs.claimed_intervals(splits)
        for unit, section, bind, section_base in self.CASES:
            with self.subTest(unit=unit):
                ours, target = live_objects(unit)
                pool, payload, relocated = self.measure(unit, section)
                bases, _stats = cs.candidate_bases(
                    cs.relocation_rows(ours), cs.relocation_rows(target),
                    cs.object_symbols(ours))
                found = dict(bases.get(section, {}))
                if pool["bind_base"] is not None:
                    found.setdefault(pool["bind_base"], {"symbols": ["pool"],
                                                         "targets": []})
                scored = {}
                for base in found:
                    window = fndiff.dol_read(base, len(payload) + 0x400)
                    scored[base] = {
                        "support": len(found[base]["symbols"]),
                        "dissent": len(found) - 1,
                        "dol": cs.score_base(
                            payload, relocated,
                            fndiff.dol_read(base, len(payload))),
                        "inventory": None if window is None else
                        cs.gap_inventory(payload, relocated, window, base)}
                best, _decision = cs.elect_base(
                    scored, False,
                    pool, cs.proven_alignment(cs.object_symbols(ours), section),
                    lambda base: cs.claim_containing(section, base, intervals,
                                                     exclude=unit + ".c"))
                self.assertEqual(best, bind, "%s %s" % (unit, section))
                if section_base is not None:
                    self.assertEqual(pool["section_base"], section_base)

    def test_every_word_resyncs_at_the_recorded_base(self):
        from tools.gdl import fndiff
        for unit, section, bind, _section_base in self.CASES:
            with self.subTest(unit=unit):
                _pool, payload, relocated = self.measure(unit, section)
                window = fndiff.dol_read(bind, len(payload) + 0x400)
                state = cs.gap_inventory(payload, relocated, window, bind)
                self.assertEqual(state["mismatched"], 0)
                self.assertEqual(state["leading_gap"], 0)
                self.assertGreater(len(state["gaps"]), 0)

    def test_the_old_vote_bases_do_not_resync(self):
        """The negative half: each wrong base leaves real differences."""
        from tools.gdl import fndiff
        for unit, section, wrong in (
                                     ("game/game/controls", ".rodata",
                                      0x80111FB8),
                                     ("game/ui/options", ".rodata",
                                      0x80113A40)):
            with self.subTest(unit=unit):
                _pool, payload, relocated = self.measure(unit, section)
                window = fndiff.dol_read(wrong, len(payload) + 0x400)
                state = cs.gap_inventory(payload, relocated, window, wrong)
                self.assertGreater(state["mismatched"], 0)

    def test_options_sdata2_refuses_the_base_that_misaligns_its_doubles(self):
        ours, _target = live_objects("game/ui/options")
        symbols = cs.object_symbols(ours)
        self.assertEqual(cs.proven_alignment(symbols, ".sdata2"), 8)
        self.assertNotEqual(0x80347594 % 8, 0)

    def test_controls_rodata_is_short_in_the_middle_at_0x7c(self):
        from tools.gdl import fndiff
        _pool, payload, relocated = self.measure("game/game/controls",
                                                 ".rodata")
        window = fndiff.dol_read(0x80111F70, len(payload) + 0x400)
        state = cs.gap_inventory(payload, relocated, window, 0x80111F70)
        self.assertEqual(len(state["gaps"]), 1)
        gap = state["gaps"][0]
        self.assertEqual((gap["our_offset"], gap["size"]), (0x7C, 0x48))
        self.assertIn("circle", gap["content"])


if __name__ == "__main__":
    unittest.main()
