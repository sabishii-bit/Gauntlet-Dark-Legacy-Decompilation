"""composed_census/dispdelta.py: immediate-delta families and their facts.

WHAT IT IS FOR. A residual of paired same-opcode instructions differing only
in a DISPLACEMENT is not register allocation, and when many rows share one
signed delta that delta is a DATA fact. Lane P1 turned game/game/player's
"allocation residuals" into two exact constants this way.

THE FOUR REFUSALS ARE THE CALIBRATION. Each was a family in the predecessor
(build/p1_lane/p1_delta.txt, lane P1, 2026-09-08) that was not one, and each
gets a test below:

  branch displacements   `beq <fn+0x1ec>` vs `beq <fn+0x200>` produced
                         `+199 x3` / `-199 x2` from the last DIGIT RUN of a
                         hex branch target.
  relocation rows        `R_PPC_ADDR16_HA jumptable_80120BA8` vs `@3763`
                         produced `delta -3755` — two symbol NAMES
                         subtracted.
  hex vs decimal         the same value read base-10 from one side and as a
                         digit run from the other.
  stack slots            `addi r4,r1,24` vs `addi r4,r1,12` is a missing
                         12-byte LOCAL and landed in the same `+12` family
                         as the `.bss` rows, so one number stood for two
                         unrelated facts.

TWO-SIDED. Positive: the live `.bss` (+12, -768), `.rodata` front-deficit
(+840) and section-size (+3136) families on game/game/player, and the
synthetic displacement family below. Negative: the four refusals, a delta
with no matching fact reported as `unexplained` rather than attached to the
nearest number, and a `same base` share that says when a family is not one
object at two offsets.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import dispdelta  # noqa: E402

TOOL = "tools/gdl/composed_census/dispdelta.py"
UNIT = "game/game/player"
LIVE = (ROOT / "build/GUNE5D/src" / (UNIT + ".o")).is_file()


def kinds(rows):
    return [row.kind for row in rows]


def deltas(rows):
    return [row.delta for row in rows if row.kind == "IMMEDIATE"]


class ImmediateParsing(unittest.TestCase):
    def test_d_form_gives_the_displacement_and_the_base(self):
        self.assertEqual(dispdelta.immediate_of("lfs f0,2356(r31)"),
                         (2356, "r31"))
        self.assertEqual(dispdelta.immediate_of("stw r0,16232(r28)"),
                         (16232, "r28"))
        self.assertEqual(dispdelta.immediate_of("lwz r3,-4(r1)"), (-4, "r1"))

    def test_addi_gives_the_immediate_and_the_base(self):
        self.assertEqual(dispdelta.immediate_of("addi r7,r29,868"),
                         (868, "r29"))
        self.assertEqual(dispdelta.immediate_of("addi r4,r1,24"), (24, "r1"))

    def test_a_form_with_no_base_reports_None(self):
        self.assertEqual(dispdelta.immediate_of("li r3,0"), (0, None))
        self.assertEqual(dispdelta.immediate_of("cmpwi r3,10"), (10, None))

    def test_hex_immediates_are_read_as_hex(self):
        """0x1000 is 4096, not the digit run '1000'."""
        self.assertEqual(dispdelta.immediate_of("andi. r0,r0,0x1000"),
                         (4096, "r0"))
        self.assertEqual(dispdelta.immediate_of("andi. r0,r0,0x800"),
                         (2048, "r0"))

    def test_a_mask_field_instruction_has_NO_immediate_here(self):
        """`rlwinm r3,r3,2,0,29` ends in a mask field; subtracting two mask
        fields is not a displacement."""
        self.assertIsNone(dispdelta.immediate_of("rlwinm r3,r3,2,0,29"))
        self.assertIsNone(dispdelta.immediate_of("rlwimi r3,r4,8,16,23"))

    def test_a_register_only_instruction_has_no_immediate(self):
        self.assertIsNone(dispdelta.immediate_of("mr r3,r4"))
        self.assertIsNone(dispdelta.immediate_of("add r3,r4,r5"))
        self.assertIsNone(dispdelta.immediate_of("blr"))

    def test_a_branch_target_is_not_an_immediate(self):
        self.assertIsNone(dispdelta.immediate_of("beq <fn+0x1ec>"))
        self.assertIsNone(dispdelta.immediate_of("b <fn+0x200>"))


class RowClassification(unittest.TestCase):
    """Synthetic streams in `fndiff.parse`'s shape, through regnorm."""

    def rows(self, target, ours):
        return dispdelta.rows_for_function(target, ours)

    def test_a_displacement_family_is_counted(self):
        rows = self.rows(["addi r7,r29,868", "addi r7,r29,884", "blr"],
                         ["addi r7,r29,28", "addi r7,r29,44", "blr"])
        self.assertEqual(deltas(rows), [840, 840])
        self.assertTrue(all(row.same_base for row in rows
                            if row.kind == "IMMEDIATE"))

    def test_a_differing_base_register_is_MARKED_not_hidden(self):
        rows = self.rows(["addi r18,r31,3136", "blr"],
                         ["addi r19,r18,0", "blr"])
        self.assertEqual(deltas(rows), [3136])
        self.assertFalse([row for row in rows
                          if row.kind == "IMMEDIATE"][0].same_base)

    def test_a_BRANCH_row_never_enters_a_family(self):
        """p1_delta read '1' and '200' out of these and reported +199."""
        rows = self.rows(["cmpwi r3,1", "beq <fn+0x1ec>", "nop", "blr"],
                         ["cmpwi r3,1", "beq <fn+0x200>", "nop", "blr"])
        self.assertEqual(deltas(rows), [])
        self.assertIn(dispdelta.CLASS_BRANCH, kinds(rows))

    def test_a_RELOCATION_row_never_enters_a_family(self):
        """p1_delta subtracted two symbol NAMES and reported -3755. Two
        DIFFERENT named callees are the case regnorm leaves unannotated, so
        this bucket has to catch them itself."""
        rows = self.rows(["bl <reloc>", "    R_PPC_REL24 fooCallee", "blr"],
                         ["bl <reloc>", "    R_PPC_REL24 barCallee", "blr"])
        self.assertEqual(deltas(rows), [])
        self.assertIn(dispdelta.CLASS_RELOC, kinds(rows))

    def test_the_pool_vs_jumptable_reloc_pair_produces_no_family_either(self):
        """regnorm's own `reloc-naming` annotator already refuses these."""
        rows = self.rows(
            ["lis r3,0", "    R_PPC_ADDR16_HA jumptable_80120BA8", "blr"],
            ["lis r3,0", "    R_PPC_ADDR16_HA @3763", "blr"])
        self.assertEqual(deltas(rows), [])

    def test_a_HEX_immediate_pair_subtracts_as_hex(self):
        rows = self.rows(["andi. r0,r0,0x1000", "blr"],
                         ["andi. r0,r0,0x800", "blr"])
        self.assertEqual(deltas(rows), [2048])

    def test_a_STACK_row_is_labelled_r1_so_the_caller_can_split_it(self):
        rows = self.rows(["addi r4,r1,24", "blr"], ["addi r4,r1,12", "blr"])
        immediate = [row for row in rows if row.kind == "IMMEDIATE"]
        self.assertEqual([row.delta for row in immediate], [12])
        self.assertEqual([row.base for row in immediate], ["r1"])

    def test_differing_mnemonics_are_their_own_bucket(self):
        rows = self.rows(["lwz r3,8(r4)", "add r5,r6,r7", "blr"],
                         ["lfs f3,8(r4)", "nop", "blr"])
        self.assertEqual(deltas(rows), [])
        self.assertIn(dispdelta.CLASS_MNEMONIC, kinds(rows))

    def test_equal_immediates_are_not_a_family(self):
        rows = self.rows(["addi r7,r29,868", "blr"],
                         ["addi r8,r29,868", "blr"])
        self.assertEqual(deltas(rows), [])

    def test_an_identical_stream_produces_no_rows_at_all(self):
        rows = self.rows(["addi r7,r29,868", "blr"],
                         ["addi r7,r29,868", "blr"])
        self.assertEqual(rows, [])


class FactBinding(unittest.TestCase):
    DISPLACEMENTS = {12: [("frame_blit", ".bss", 0xBD4, 0xBE0, 96),
                          ("lbl_802757E0", ".bss", 0x934, 0x940, 672)],
                     -768: [("gDefaultPlayerPosition", ".bss", 0xC34,
                             0x934, 12)]}
    DEFICITS = {".rodata": 840}
    SIZES = {12: ["gDefaultPlayerPosition"]}
    SECTION_SIZES = {3136: [".bss"]}

    def explain(self, delta):
        return dispdelta.explain(delta, self.DISPLACEMENTS, self.DEFICITS,
                                 self.SIZES, self.SECTION_SIZES)

    def test_a_moved_object_is_named_with_its_direction(self):
        facts = self.explain(12)
        self.assertIn("2 named object(s)", facts[0])
        self.assertIn("LATER in the TARGET", facts[0])
        self.assertIn("frame_blit", facts[0])

    def test_the_other_half_of_the_same_fact_reads_EARLIER(self):
        self.assertIn("EARLIER in the TARGET", self.explain(-768)[0])

    def test_a_front_deficit_is_named_as_such(self):
        self.assertIn("FRONT DEFICIT", self.explain(840)[0])
        self.assertIn(".rodata", self.explain(840)[0])

    def test_a_whole_section_size_says_it_reaches_PAST_the_end(self):
        self.assertIn("WHOLE SIZE of our .bss", self.explain(3136)[0])
        self.assertIn("NEXT object in the link", self.explain(3136)[0])

    def test_an_unmatched_delta_gets_NO_fact_rather_than_the_nearest_one(self):
        """The negative side: 884 is close to 840 and must not borrow it."""
        self.assertEqual(self.explain(884), [])
        self.assertEqual(self.explain(1184), [])

    def test_a_datum_SIZE_match_is_labelled_a_candidate_not_a_proof(self):
        facts = dispdelta.explain(12, {}, {}, self.SIZES, {})
        self.assertIn("not yet a proof", facts[0])

    def test_the_object_displacement_fact_states_its_own_blind_spot(self):
        self.assertIn("DEFINED IN BOTH objects", self.explain(12)[0])


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class LivePlayer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.record = dispdelta.measure(UNIT)
        cls.by_delta = {row["delta"]: row for row in cls.record["families"]}

    def test_the_bss_plus_twelve_family_is_bound_to_the_moved_objects(self):
        row = self.by_delta[12]
        self.assertGreaterEqual(row["rows"], 20)
        self.assertIn("lbl_802757E0", row["facts"][0])
        self.assertIn("LATER in the TARGET", row["facts"][0])

    def test_the_minus_768_family_names_gDefaultPlayerPosition(self):
        row = self.by_delta[-768]
        self.assertIn("gDefaultPlayerPosition", row["facts"][0])
        self.assertEqual(row["same_base"], row["rows"])

    def test_the_plus_840_family_is_the_rodata_front_deficit(self):
        row = self.by_delta[840]
        self.assertIn("FRONT DEFICIT", row["facts"][0])
        self.assertIn(".rodata", row["facts"][0])

    def test_the_plus_3136_family_is_our_whole_bss_size(self):
        row = self.by_delta[3136]
        self.assertIn("WHOLE SIZE of our .bss", row["facts"][0])
        self.assertLess(row["same_base"], row["rows"] // 2)

    def test_no_plus_or_minus_199_branch_family_survives(self):
        """The predecessor's branch artifact, on the live object."""
        self.assertNotIn(199, self.by_delta)
        self.assertNotIn(-199, self.by_delta)

    def test_no_minus_3755_jumptable_family_survives(self):
        self.assertNotIn(-3755, self.by_delta)

    def test_stack_rows_are_reported_APART_from_the_bss_twelve(self):
        stack = {row["delta"]: row["rows"]
                 for row in self.record["stack_families"]}
        self.assertIn(12, stack)
        self.assertNotEqual(stack[12], self.by_delta[12]["rows"])

    def test_the_count_asymmetric_functions_are_named(self):
        self.assertIn("do_players", self.record["count_asymmetric"])

    def test_the_excluded_buckets_are_reported_not_dropped(self):
        self.assertIn("MNEMONIC-DIFFERS", self.record["excluded"])
        self.assertIn("RELOCATION-ROW", self.record["excluded"])


class CommandLine(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, TOOL, *args], cwd=str(ROOT),
                              capture_output=True, text=True)

    def test_help_exits_zero_on_stdout(self):
        done = self.run_tool("--help")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("FOUR THINGS IT REFUSES TO COUNT", done.stdout)

    def test_an_unknown_flag_is_refused(self):
        self.assertEqual(self.run_tool(UNIT, "--nope").returncode, 2)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_a_single_function_run_reports_its_own_rows(self):
        done = self.run_tool(UNIT, "SetPlayerWindows", "--no-facts")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("1 function pair(s) measured", done.stdout)
        self.assertIn("+12", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_an_unknown_function_REFUSES_with_two(self):
        done = self.run_tool(UNIT, "no_such_fn")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("no function", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_no_facts_still_reports_families(self):
        done = self.run_tool(UNIT, "--no-facts", "--min", "5",
                             "--no-examples")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("unexplained", done.stdout)


if __name__ == "__main__":
    unittest.main()
