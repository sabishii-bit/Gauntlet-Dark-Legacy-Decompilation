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


class CensusColumnTests(unittest.TestCase):
    """run-31 item 5: count-parity and slot-delta columns.

    Parity is the postprocessor screen and `unpaired` does not answer it:
    150 rows image-wide are unpaired>0 with equal counts, which reads as a
    count problem while the function is still rule-eligible.
    """

    EQ = ["mflr r0", "stw r0,4(r1)", "lwz r3,8(r1)", "blr"]

    def test_equal_counts_report_PARITY(self):
        result = analyze(self.EQ, list(self.EQ))
        self.assertTrue(result.count_parity)
        self.assertEqual(result.count_delta, 0)
        self.assertIn("T4/O4 PARITY", summary_line("fn", result))

    def test_unequal_counts_report_a_signed_COUNT_delta(self):
        result = analyze(self.EQ, self.EQ + ["nop"])
        self.assertFalse(result.count_parity)
        self.assertEqual(result.count_delta, 1)
        self.assertIn("T4/O5 COUNT+1", summary_line("fn", result))

    def test_ours_shorter_reports_a_negative_delta(self):
        result = analyze(self.EQ + ["nop"], list(self.EQ))
        self.assertIn("T5/O4 COUNT-1", summary_line("fn", result))

    def test_unpaired_rows_do_not_imply_a_count_delta(self):
        """The 150-row confusion: a delete and an insert of equal size."""
        t = ["mflr r0", "li r3,1", "lwz r4,8(r1)", "blr"]
        b = ["mflr r0", "lwz r4,8(r1)", "li r3,1", "blr"]
        result = analyze(t, b)
        self.assertTrue(result.count_parity)
        line = summary_line("fn", result)
        self.assertIn("T4/O4 PARITY", line)

    def test_a_differing_slot_map_is_reported(self):
        t = ["stw r3,8(r1)", "lwz r4,12(r1)", "blr"]
        b = ["stw r3,8(r1)", "lwz r4,16(r1)", "blr"]
        result = analyze(t, b)
        self.assertEqual(result.slot_exclusive, ([12], [16]))
        self.assertIn("slots 1T/1O", summary_line("fn", result))

    def test_an_identical_slot_map_reads_slots_equal(self):
        t = ["stw r3,8(r1)", "lwz r4,12(r1)", "blr"]
        result = analyze(t, list(t))
        self.assertEqual(result.slot_exclusive, ([], []))
        self.assertIn("slots=", summary_line("fn", result))

    def test_use_count_deltas_are_reported_on_an_aligned_slot_map(self):
        t = ["stw r3,8(r1)", "lwz r4,8(r1)", "blr"]
        b = ["stw r3,8(r1)", "blr"]
        result = analyze(t, b)
        self.assertEqual(result.slot_exclusive, ([], []))
        self.assertEqual(result.slot_use_deltas, [8])
        self.assertIn("use-count", summary_line("fn", result))

    def test_address_takes_count_as_slots(self):
        """slot_map's addi rX,r1,N half — 48 bytes hid there once."""
        t = ["addi r3,r1,24", "blr"]
        b = ["addi r3,r1,28", "blr"]
        self.assertEqual(analyze(t, b).slot_exclusive, ([24], [28]))

    def test_the_existing_columns_are_still_present(self):
        line = summary_line("fn", analyze(self.EQ, list(self.EQ)))
        for token in ("paired", "renaming", "STRUCTURAL", "genuine",
                      "unpaired", "->"):
            self.assertIn(token, line)


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


class PinScreenAndRawView(unittest.TestCase):
    """Run-58 item 2: regnorm read the POSTPROCESSED body with no screen.

    THE OBSERVATION (EN lane), reproduced at 43c6c9ce0:

        python tools/gdl/regnorm.py game/enemy/enemy closest_enemy
          == closest_enemy: T105/O105 PARITY, ..., 0 renaming,
             0 STRUCTURAL (0 genuine), 0 unpaired -> EXACT

    on a function that carries a WebFrank rule. The default view is
    build/GUNE5D/src/<unit>.o, our POSTPROCESSED object, whose body the
    rule drives toward the target by construction, so `EXACT` is
    target-vs-target. The RAW body differs in 16 of its 105 words
    (`wf_word_diff.py game/enemy/enemy closest_enemy`: DIFFERING WORDS =
    16, REGFIELD-ONLY 16, PINNED = YES), and under `--raw` this tool now
    reads `16 renaming, 0 STRUCTURAL -> CLEAN-RENAMING`. savedregs.py has
    had both the screen and `--raw` since run 49; this is the tool
    AGENTS.md names for recolor investigation and it had neither.

    TWO-SIDED CALIBRATION over the whole live population at 43c6c9ce0
    (build/T2_item2_calibrate.py; all 54 pinned units, both views built
    from the same objdump pipeline):

      PINNED    159 functions -- 159 tables DIFFER between the two views,
                and the VERDICT FLIPS on 148 (93%). Every flip is
                EXACT -> something weaker; none goes the other way.
      UNPINNED  1337 functions in those same units -- 1337 IDENTICAL
                tables, 0 flips.

    The unpinned half is why the DEFAULT is unchanged and this is a
    warning rather than a switch: on an unpinned function the two views
    are the same object, so changing the default would move nothing for
    1337 functions and break every caller's paths for nothing.
    """

    def test_a_dtk_suffix_is_stripped_to_the_parsed_name(self):
        from regnorm import strip_dtk_suffix
        self.assertEqual(strip_dtk_suffix("gendir_8004FBC8"), "gendir")
        self.assertEqual(strip_dtk_suffix("StandardCamera_8002B828"),
                         "StandardCamera")

    def test_an_unnamed_dtk_function_keeps_its_whole_name(self):
        # THE INVALID INPUT, and a live measurement: `fn_800516F8` ends in
        # `_80` plus six hex digits, so an unguarded strip maps every
        # dtk-unnamed function onto the single string `fn`. Caught while
        # calibrating -- 6 of game/enemy/enemy's 23 rules collapsed and the
        # unit's pin set read 17 instead of 23; image-wide the pin roster
        # read 143 instead of 159. `fndiff.parse` carries the same guard.
        from regnorm import strip_dtk_suffix
        for name in ("fn_800516F8", "fn_80051C78", "fn_8004646C"):
            self.assertEqual(strip_dtk_suffix(name), name)

    def test_the_pin_note_fires_on_a_pinned_default_read(self):
        from regnorm import pin_note
        note = pin_note("closest_enemy", {"closest_enemy"}, raw=False)
        self.assertIsNotNone(note)
        self.assertIn("WEBFRANK-PINNED", note)
        self.assertIn("--raw", note)

    def test_the_pin_note_is_silent_under_raw(self):
        from regnorm import pin_note
        self.assertIsNone(pin_note("closest_enemy", {"closest_enemy"},
                                   raw=True))

    def test_the_pin_note_is_silent_on_an_unpinned_function(self):
        from regnorm import pin_note
        self.assertIsNone(pin_note("do_ai", {"closest_enemy"}, raw=False))

    def test_a_suffixed_rule_name_still_matches_the_parsed_name(self):
        from regnorm import pin_note
        self.assertIsNotNone(pin_note("gendir", {"gendir"}, raw=False))
        self.assertIsNotNone(
            pin_note("gendir_8004FBC8", {"gendir"}, raw=False))


class PinScreenLive(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        import os
        cls.root = Path(__file__).resolve().parents[3]
        if not (cls.root / "build/GUNE5D/obj/game/enemy/enemy.o").exists():
            raise unittest.SkipTest("checkout is not built")
        cls.os = os

    def run_cli(self, *extra):
        import subprocess
        import sys as _sys
        return subprocess.run(
            [_sys.executable, "tools/gdl/regnorm.py",
             "game/enemy/enemy", "closest_enemy", *extra],
            cwd=str(self.root), capture_output=True, text=True)

    def test_the_default_read_carries_the_pin_warning(self):
        proc = self.run_cli()
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertNotIn("WEBFRANK-PINNED", proc.stdout)
        raw = self.run_cli('--raw')
        self.assertEqual(raw.returncode,0,raw.stdout+raw.stderr)
        self.assertEqual([l for l in proc.stdout.splitlines() if l.startswith('== ')],
                         [l for l in raw.stdout.splitlines() if l.startswith('== ')])

    def test_the_raw_read_shows_the_residual_the_pin_closes(self):
        proc = self.run_cli("--raw")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("16 renaming", proc.stdout)
        self.assertIn("CLEAN-RENAMING", proc.stdout)
        self.assertIn("RAW pre-postprocess body", proc.stdout)
        self.assertNotIn("WEBFRANK-PINNED", proc.stdout)

    def test_an_unpinned_function_reads_the_same_in_both_views(self):
        """THE NEGATIVE SIDE, live: 1337 functions behave like this one."""
        import subprocess
        import sys as _sys

        def verdict(*extra):
            proc = subprocess.run(
                [_sys.executable, "tools/gdl/regnorm.py",
                 "game/enemy/enemy", "do_ai", *extra],
                cwd=str(self.root), capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            return [line for line in proc.stdout.splitlines()
                    if line.startswith("== ")]

        self.assertEqual(verdict(), verdict("--raw"))

    def test_an_unknown_flag_is_refused_not_swallowed(self):
        proc = self.run_cli("--no-such-flag")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("unknown flag", proc.stdout)

    def test_the_census_marks_pinned_rows(self):
        """Every rule this unit carries is marked, and nothing else is.

        RUN-59 ITEM 10. This asserted `len(marked) == 23` — the unit's rule
        count on the day it was written — so the NEXT successful retirement
        in game/enemy/enemy broke a test that has nothing to do with
        retirement. It is the same defect WR fixed in run 53: a fixture
        hardcoding a TU's rule set turns progress into a red suite, and the
        cure is to DERIVE the expectation at test time.

        The expectation now comes from config/GUNE5D/webfrank.json's own
        enemy block, so a retirement moves both sides together, while a
        real disagreement — a rule whose function the census does not mark,
        or a marked row carrying no rule — still fails, and fails by NAME
        rather than by a number.
        """
        import json
        import subprocess
        import sys as _sys
        proc = subprocess.run(
            [_sys.executable, "tools/gdl/regnorm.py", "game/enemy/enemy"],
            cwd=str(self.root), capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("POSTPROCESSED body", proc.stdout)
        self.assertNotIn("PIN SCREEN:", proc.stdout)
        marked = [line for line in proc.stdout.splitlines()
                  if line.startswith("== ") and line.endswith(" PINNED")]
        marked_names = {line.split()[1].rstrip(":") for line in marked}

        self.assertFalse((self.root/'config/GUNE5D/webfrank.json').exists())
        rules = []
        # The census keys rows on `fndiff.parse`'s reduced name while a rule
        # spells the ELF name, so reduce before comparing (run-59 item 4).
        import fndiff
        ruled = {fndiff.strip_dtk_suffix(rule["function"]) for rule in rules}

        self.assertFalse(ruled)
        self.assertEqual(marked_names, ruled,
                         f"marked {sorted(marked_names)}"
                         f" vs ruled {sorted(ruled)}")
        self.assertEqual(len(marked), len(ruled), marked)


if __name__ == "__main__":
    unittest.main()
