"""regnorm annotator CANARY.

The failure this file exists to catch is UNDER-REPORTING. regnorm's
census is ranked on structural rows, so an annotator that silently stops
firing does not break anything visibly — it just queues finished
functions as work. Measured instance: the reloc artifact annotator
covered ...data.N/...rodata.N but not ...bss.N, so a whole band of
byte-exact player.c functions read as "2 STRUCTURAL" and a lane nearly
worked them (claim.law.PL_regnorm-census-two-structural-band-is-a-byte-
exact-reloc-naming-signature.20260901.v1).

Every artifact class therefore gets a canary here: a minimal input that
MUST produce that annotation, plus a negative twin that must NOT.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from regnorm import (analyze, split_reloc, summary_line, symbol_address)

ARTIFACT_CLASSES = {"reloc-naming", "reloc-value", "branch-disp",
                    "schedule-disp", "alignment"}

_DENSE = ["stfs f1,0(r5)", "stfs f2,4(r5)", "lwz r3,0(r4)",
          "addi r3,r3,1", "stw r3,8(r4)"]


def fake_resolver(names_by_side, addrs_by_side=None):
    addrs_by_side = addrs_by_side or {}

    def resolve(side, symbol, addend):
        return (frozenset(names_by_side.get((side, symbol), {symbol})),
                frozenset(addrs_by_side.get((side, symbol), set())))
    return resolve


CANARY_INPUTS = [
    ("reloc-value / shared name",
     ["lis r3,0", "    R_PPC_ADDR16_HA\tpotionicon_tab", "blr"],
     ["lis r3,0", "    R_PPC_ADDR16_HA\t...bss.0", "blr"],
     fake_resolver({
         ("target", "potionicon_tab"): {"potionicon_tab"},
         ("ours", "...bss.0"): {"...bss.0", "potionicon_tab", ".bss"}})),
    ("reloc-value / shared address",
     ["lis r3,0", "    R_PPC_ADDR16_HA\tlbl_80275534", "blr"],
     ["lis r3,0", "    R_PPC_ADDR16_HA\tgot_it", "blr"],
     fake_resolver({}, {("target", "lbl_80275534"): {0x80275534},
                        ("ours", "got_it"): {0x80275534}})),
    ("reloc-naming / pool vs named",
     ["lfs f1,0(r3)", "    R_PPC_EMB_SDA21\t@1234", "blr"],
     ["lfs f1,0(r3)", "    R_PPC_EMB_SDA21\tlbl_80345188", "blr"],
     None),
    ("branch-disp / displacement under a count delta",
     ["cmpwi r3,0", "beq <fn+0x20>", "li r3,1", "blr"],
     ["cmpwi r3,0", "beq <fn+0x24>", "li r3,1", "nop", "blr"],
     None),
    ("schedule-disp / instruction moved across the block",
     ["lwz r3,0(r4)", "stfs f1,0(r5)", "stfs f2,4(r5)"],
     ["stfs f1,0(r5)", "stfs f2,4(r5)", "lwz r3,0(r4)"],
     None),
    ("alignment / dense repeating block",
     _DENSE, [_DENSE[i] for i in (0, 4, 2, 3, 1)], None),
]


class SplitRelocTests(unittest.TestCase):
    def test_plain_symbol(self):
        self.assertEqual(split_reloc("R_PPC_ADDR16_HA\tpotionicon_tab"),
                         ("R_PPC_ADDR16_HA", "potionicon_tab", 0))

    def test_hex_addend(self):
        self.assertEqual(split_reloc("R_PPC_ADDR16_LO\tsFlags+0x4"),
                         ("R_PPC_ADDR16_LO", "sFlags", 4))

    def test_negative_decimal_addend(self):
        self.assertEqual(split_reloc("R_PPC_ADDR16_LO\tsFlags-8"),
                         ("R_PPC_ADDR16_LO", "sFlags", -8))


class SymbolAddressTests(unittest.TestCase):
    def test_label_carries_its_own_address(self):
        self.assertEqual(symbol_address("lbl_80275534"), 0x80275534)
        self.assertEqual(symbol_address("jumptable_8004FBC8"), 0x8004FBC8)

    def test_unknown_name_is_none(self):
        self.assertIsNone(symbol_address("definitely_not_a_symbol_xyz"))


class AnnotatorCanary(unittest.TestCase):
    """One canary per artifact class. Add a case here when adding a class."""

    def kinds(self, result):
        """Every annotated row — unpaired rows carry annotations too."""
        return {r.artifact for r in result.rows if r.artifact}

    def test_reloc_value_same_name_at_one_location(self):
        """Target names the datum, we name the private section symbol."""
        target = ["lis r3,0", "    R_PPC_ADDR16_HA\tpotionicon_tab", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA\t...bss.0", "blr"]
        resolver = fake_resolver({
            ("target", "potionicon_tab"): {"potionicon_tab"},
            ("ours", "...bss.0"): {"...bss.0", "potionicon_tab", ".bss"},
        })
        result = analyze(target, ours, resolver)
        self.assertEqual(self.kinds(result), {"reloc-value"})
        self.assertEqual(len(result.genuine), 0)

    def test_reloc_value_same_address_different_spelling(self):
        target = ["lis r3,0", "    R_PPC_ADDR16_HA\tlbl_80275534", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA\tgot_it", "blr"]
        resolver = fake_resolver(
            {},
            {("target", "lbl_80275534"): {0x80275534},
             ("ours", "got_it"): {0x80275534}})
        result = analyze(target, ours, resolver)
        self.assertEqual(self.kinds(result), {"reloc-value"})

    def test_reloc_value_does_NOT_fire_on_a_genuinely_different_target(self):
        """The negative twin: no shared name, no shared address."""
        target = ["lis r3,0", "    R_PPC_ADDR16_HA\tfoo", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA\tbar", "blr"]
        resolver = fake_resolver(
            {}, {("target", "foo"): {0x80000000},
                 ("ours", "bar"): {0x80001000}})
        result = analyze(target, ours, resolver)
        self.assertEqual(self.kinds(result), set())
        self.assertEqual(len(result.genuine), 1)

    def test_reloc_naming_pool_vs_named(self):
        target = ["lfs f1,0(r3)", "    R_PPC_EMB_SDA21\t@1234", "blr"]
        ours = ["lfs f1,0(r3)", "    R_PPC_EMB_SDA21\tlbl_80345188", "blr"]
        result = analyze(target, ours, None)
        self.assertEqual(self.kinds(result), {"reloc-naming"})

    def test_branch_displacement_under_a_count_delta(self):
        target = ["cmpwi r3,0", "beq <fn+0x20>", "li r3,1", "blr"]
        ours = ["cmpwi r3,0", "beq <fn+0x24>", "li r3,1", "nop", "blr"]
        result = analyze(target, ours, None)
        self.assertIn("branch-disp", self.kinds(result))

    def test_branch_displacement_does_NOT_fire_without_a_count_delta(self):
        target = ["cmpwi r3,0", "beq <fn+0x20>", "blr"]
        ours = ["cmpwi r3,0", "beq <fn+0x24>", "blr"]
        result = analyze(target, ours, None)
        self.assertNotIn("branch-disp", self.kinds(result))

    def test_schedule_displacement_when_both_lines_occur_opposite(self):
        target = ["lwz r3,0(r4)", "stfs f1,0(r5)", "stfs f2,4(r5)"]
        ours = ["stfs f1,0(r5)", "stfs f2,4(r5)", "lwz r3,0(r4)"]
        result = analyze(target, ours, None)
        self.assertIn("schedule-disp", self.kinds(result))

    def test_alignment_artifact_when_multisets_are_identical(self):
        """A dense repeating block where the LCS pairs differing opcodes.

        The permutation was found by exhaustive search over a 5-line
        block (22 of 120 permutations trigger the class); this one
        triggers 'alignment' and nothing else.
        """
        target = ["stfs f1,0(r5)", "stfs f2,4(r5)", "lwz r3,0(r4)",
                  "addi r3,r3,1", "stw r3,8(r4)"]
        ours = [target[i] for i in (0, 4, 2, 3, 1)]
        result = analyze(target, ours, None)
        self.assertEqual(self.kinds(result), {"alignment"})

    def test_every_artifact_class_actually_fires(self):
        """The canary proper: each class must FIRE on a known input.

        Asserting that a test method NAMED after a class exists would
        pass even with every annotator silent — which is the exact
        failure mode being guarded. This drives each input and inspects
        what actually came out.
        """
        fired = set()
        for label, target, ours, resolver in CANARY_INPUTS:
            result = analyze(target, ours, resolver)
            kinds = self.kinds(result)
            self.assertTrue(kinds, f"{label}: no annotation fired at all")
            fired |= kinds
        self.assertEqual(fired, ARTIFACT_CLASSES,
                         "annotator classes that never fired: "
                         f"{ARTIFACT_CLASSES - fired}")


class GenuineAccountingTests(unittest.TestCase):
    def test_genuine_excludes_annotated_rows(self):
        target = ["lis r3,0", "    R_PPC_ADDR16_HA\t@1234",
                  "addi r4,r4,8", "blr"]
        ours = ["lis r3,0", "    R_PPC_ADDR16_HA\tlbl_80345188",
                "addi r4,r4,12", "blr"]
        result = analyze(target, ours, None)
        self.assertEqual(len(result.structural), 2)
        self.assertEqual(len(result.genuine), 1)
        self.assertIn("(1 genuine)", summary_line("fn", result))

    def test_exact_streams_report_exact(self):
        lines = ["addi r3,r3,1", "blr"]
        result = analyze(lines, lines, None)
        self.assertEqual(result.verdict, "EXACT")
        self.assertEqual(len(result.genuine), 0)

    def test_pure_renaming_reports_clean_renaming(self):
        target = ["lwz r7,16(r3)", "add r8,r7,r4", "blr"]
        ours = ["lwz r9,16(r5)", "add r10,r9,r6", "blr"]
        result = analyze(target, ours, None)
        self.assertEqual(result.verdict, "CLEAN-RENAMING")
        self.assertEqual(len(result.renaming), 2)


if __name__ == "__main__":
    unittest.main()
