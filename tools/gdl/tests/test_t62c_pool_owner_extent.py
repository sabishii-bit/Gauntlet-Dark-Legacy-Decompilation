"""pool_owner: the first-use order is tested INSIDE one extent, base first.

TWO DEFECTS, both measured by lane J on options.c at fe3e4e736 with

    python tools/gdl/pool_owner.py game/ui/options --limit 0 \\
        --range .rodata:0x80113A78-0x80113ADA \\
        --range .sdata2:0x80347598-0x80347607

which printed

    FIRST-USE ORDER over 20 of 20 own referenced datums ...:
    DISAGREES at index 0 (first use lbl_80347598, address order lbl_80113A78)
      lbl_80113A78 sits earlier in the pool than any read this tool can see
      ...: an earlier reference is implied -- inlined, computed, or not
      reconstructed yet.

  1. index 0 compares a `.sdata2` datum against a `.rodata` one. They are
     separate pools; no single first-use order governs their union, and the
     20 datums came from two different `--range` hypotheses.
  2. the "an earlier reference is implied" hint sent the lane looking for an
     inlined caller, when `.rodata` 0x80113A78 was simply not the base --
     the section binds at 0x80113A0C.

TWO SIDES. The synthetic half drives both the grouping and the base check,
including the cases that must NOT fire (one extent, agreeing base, no
derivation available). The live half re-runs lane J's exact invocation and
enemy's single-run report, and skips without built objects.
"""
import unittest
from pathlib import Path

from tools.gdl import pool_owner as po

ROOT = Path(__file__).resolve().parents[3]


def datum(name, address, section, offset, owned=True, generated=True,
          disposition="POOL"):
    return {"name": name, "address": address, "section": section,
            "owner": "u" if owned else "other", "owned_by_this_unit": owned,
            "first_reference_text_offset": offset,
            "compiler_generated_name": generated, "disposition": disposition,
            "reference_sites": [{"function": "f", "offset": 0,
                                 "text_offset": offset, "relocation_type": 6}]}


class PoolExtents(unittest.TestCase):
    RUNS = [("u", ".sdata2", 0x80346810, 0x80346AAC),
            ("u", ".text", 0x80044000, 0x80045000),
            ("u", ".bss", 0x80250E00, 0x80251000),
            ("other", ".sdata2", 0x80347000, 0x80347100)]

    def test_only_this_units_pool_sections_are_extents(self):
        extents = po.pool_extents("u", self.RUNS)
        self.assertEqual([(row[1], row[2]) for row in extents],
                         [(".sdata2", 0x80346810)])

    def test_a_candidate_range_adds_an_extent_and_is_labelled_as_one(self):
        extents = po.pool_extents("u", self.RUNS,
                                  [(".rodata", 0x80113A78, 0x80113ADA)])
        self.assertEqual(len(extents), 2)
        labels = {row[1]: row[0] for row in extents}
        self.assertEqual(labels[".sdata2"], "splits.txt")
        self.assertIn("--range candidate", labels[".rodata"])

    def test_a_unit_owning_no_pool_run_has_no_extent(self):
        self.assertEqual(po.pool_extents("game/ui/options", self.RUNS), [])


class GroupingByExtent(unittest.TestCase):
    """Lane J's defect 1, reduced to its arithmetic."""

    ROWS = [datum("lbl_80113A78", "0x80113A78", ".rodata", 0x474),
            datum("lbl_80113A9C", "0x80113A9C", ".rodata", 0x900),
            datum("lbl_80347598", "0x80347598", ".sdata2", 0x100),
            datum("lbl_803475A0", "0x803475A0", ".sdata2", 0x200)]
    EXTENTS = [("--range candidate", ".rodata", 0x80113A78, 0x80113ADA),
               ("--range candidate", ".sdata2", 0x80347598, 0x80347608)]

    def test_two_sections_merged_would_disagree_at_index_zero(self):
        """The old behaviour, kept reachable so the defect stays visible."""
        merged = po.predict_first_use(self.ROWS, "u")
        self.assertFalse(merged["orders_agree"])
        self.assertEqual(merged["first_disagreement_index"], 0)
        self.assertIn("no extent supplied",
                      merged["extents"][0]["label"])

    def test_each_extent_is_ordered_on_its_own_and_both_agree(self):
        result = po.predict_first_use(self.ROWS, "u", self.EXTENTS)
        self.assertTrue(result["orders_agree"])
        self.assertEqual(len(result["extents"]), 2)
        for extent in result["extents"]:
            self.assertEqual(extent["modelled_generated_literals"], 2)
            self.assertTrue(extent["orders_agree"])

    def test_a_datum_outside_every_extent_is_named_not_ordered(self):
        rows = self.ROWS + [datum("lbl_80347700", "0x80347700", ".sdata2", 1)]
        result = po.predict_first_use(rows, "u", self.EXTENTS)
        self.assertEqual(result["modelled_outside_every_extent"],
                         ["lbl_80347700"])
        self.assertTrue(result["orders_agree"])

    def test_a_real_disagreement_inside_one_extent_still_reports(self):
        rows = [datum("lbl_80347598", "0x80347598", ".sdata2", 0x900),
                datum("lbl_803475A0", "0x803475A0", ".sdata2", 0x100)]
        result = po.predict_first_use(rows, "u", self.EXTENTS)
        self.assertFalse(result["orders_agree"])
        self.assertEqual(result["first_disagreement"]["actual"],
                         "lbl_80347598")
        self.assertTrue(result["first_disagreement"]
                        ["implied_missing_early_reference"])

    def test_an_empty_extent_is_reported_as_nothing_to_order(self):
        result = po.predict_first_use(
            [datum("lbl_80347598", "0x80347598", ".sdata2", 1)], "u",
            self.EXTENTS)
        rodata = next(row for row in result["extents"]
                      if row["section"] == ".rodata")
        self.assertEqual(rodata["modelled_generated_literals"], 0)
        self.assertIsNone(rodata["first_disagreement_index"])


class BaseDisagreement(unittest.TestCase):
    """Lane J's defect 2: a wrong extent, not a missing early reference."""

    ROWS = [datum("lbl_80113A78", "0x80113A78", ".rodata", 0x900),
            datum("lbl_80113A9C", "0x80113A9C", ".rodata", 0x100)]
    EXTENTS = [("--range candidate", ".rodata", 0x80113A78, 0x80113ADA)]

    def test_a_wrong_base_replaces_the_earlier_reference_hint(self):
        result = po.predict_first_use(self.ROWS, "u", self.EXTENTS,
                                      {".rodata": 0x80113A0C})
        row = result["first_disagreement"]
        self.assertFalse(row["implied_missing_early_reference"])
        self.assertEqual(row["base_disagreement"]["derived_base"],
                         "0x80113A0C")
        self.assertEqual(row["base_disagreement"]["extent_start"],
                         "0x80113A78")

    def test_a_wrong_base_is_reported_even_when_the_order_agrees(self):
        agreeing = [datum("lbl_80113A78", "0x80113A78", ".rodata", 0x100),
                    datum("lbl_80113A9C", "0x80113A9C", ".rodata", 0x900)]
        result = po.predict_first_use(agreeing, "u", self.EXTENTS,
                                      {".rodata": 0x80113A0C})
        self.assertTrue(result["orders_agree"])
        self.assertIsNotNone(result["extents"][0]["base_disagreement"])

    def test_an_agreeing_base_keeps_the_earlier_reference_hint(self):
        result = po.predict_first_use(self.ROWS, "u", self.EXTENTS,
                                      {".rodata": 0x80113A78})
        row = result["first_disagreement"]
        self.assertTrue(row["implied_missing_early_reference"])
        self.assertIsNone(row["base_disagreement"])
        self.assertIsNone(result["extents"][0]["base_disagreement"])

    def test_no_derivation_at_all_is_not_read_as_agreement(self):
        result = po.predict_first_use(self.ROWS, "u", self.EXTENTS, {})
        self.assertIsNone(result["extents"][0]["base_disagreement"])
        self.assertTrue(result["first_disagreement"]
                        ["implied_missing_early_reference"])

    def test_a_base_for_another_section_does_not_judge_this_extent(self):
        result = po.predict_first_use(self.ROWS, "u", self.EXTENTS,
                                      {".sdata2": 0x80347510})
        self.assertIsNone(result["extents"][0]["base_disagreement"])


class DerivedBasesNeverThrow(unittest.TestCase):
    def test_missing_objects_return_nothing_measured(self):
        self.assertEqual(
            po.derived_section_bases("no/such/unit", [".rodata"]), {})

    def test_an_unreadable_tree_returns_nothing_measured(self):
        self.assertEqual(
            po.derived_section_bases("game/ui/options", [".rodata"],
                                     root=ROOT / "no-such-root"), {})


@unittest.skipUnless(
    (ROOT / "build/GUNE5D/obj/game/ui/options.o").exists()
    and (ROOT / "build/GUNE5D/src/game/ui/options.o").exists(),
    "target/source objects are not built in this worktree")
class LiveLaneJInvocation(unittest.TestCase):
    RANGES = [".rodata:0x80113A78-0x80113ADA", ".sdata2:0x80347598-0x80347607"]

    @classmethod
    def setUpClass(cls):
        cls.result = po.analyze("game/ui/options", candidate_ranges=cls.RANGES)
        cls.text = po.format_report(cls.result)

    def test_the_two_ranges_become_two_extents(self):
        sections = [row["section"]
                    for row in self.result["first_use_prediction"]["extents"]]
        self.assertEqual(sorted(sections), [".rodata", ".sdata2"])

    def test_no_datum_is_ordered_against_one_from_the_other_section(self):
        for extent in self.result["first_use_prediction"]["extents"]:
            names = extent["actual_address_order"]
            rows = {row["name"]: row for row in self.result["datums"]}
            self.assertTrue(all(rows[name]["section"] == extent["section"]
                                for name in names), extent["section"])

    def test_the_rodata_range_is_flagged_as_the_wrong_base(self):
        rodata = next(row for row in
                      self.result["first_use_prediction"]["extents"]
                      if row["section"] == ".rodata")
        self.assertEqual(rodata["base_disagreement"]["derived_base"],
                         "0x80113A0C")
        self.assertIn("BASE DISAGREEMENT", self.text)

    def test_the_report_no_longer_claims_an_implied_early_reference_there(self):
        rodata = next(row for row in
                      self.result["first_use_prediction"]["extents"]
                      if row["section"] == ".rodata")
        self.assertIsNone(rodata["first_disagreement"])

    def test_the_derived_bases_are_printed_for_both_sections(self):
        self.assertEqual(self.result["derived_section_bases"],
                         {".rodata": "0x80113A0C", ".sdata2": "0x80347598"})


@unittest.skipUnless(
    (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").exists(),
    "target objects are not split in this checkout")
class LiveSingleExtentIsUnchanged(unittest.TestCase):
    """enemy owns exactly one pool run, so its recorded result must stand."""

    def test_enemy_still_disagrees_at_index_22_in_its_own_run(self):
        prediction = po.analyze("game/enemy/enemy")["first_use_prediction"]
        self.assertEqual(len(prediction["extents"]), 1)
        extent = prediction["extents"][0]
        self.assertEqual((extent["section"], extent["start"]),
                         (".sdata2", "0x80346810"))
        self.assertEqual(extent["first_disagreement_index"], 22)
        self.assertEqual(extent["first_disagreement"]["actual"],
                         "lbl_803468B8")
        self.assertTrue(extent["first_disagreement"]
                        ["implied_missing_early_reference"])

    def test_the_derivation_confirms_enemys_own_claim_start(self):
        result = po.analyze("game/enemy/enemy")
        self.assertEqual(result["derived_section_bases"].get(".sdata2"),
                         "0x80346810")
        self.assertIsNone(
            result["first_use_prediction"]["extents"][0]["base_disagreement"])


if __name__ == "__main__":
    unittest.main()
