"""fndiff --ops: the retype-scaling tell.

THE BUG CLASS. When a member changes from `u8 *` to a typed pointer, every
`+ N` on it silently starts counting RECORDS instead of BYTES. The source
compiles, the linter clears, and the object emits a displacement scaled by
sizeof(record). Four shipped past review in run 62
(from-fable-2026-09-11-run62-critter-lint-clean) and were caught only by a
whole-object byte comparison:

    CritterNewInst       lwz r3,1600(r3)   target lwz r3,20(r3)     x80
    CritterInitColnodes  lwz r5,4800(r5)   target lwz r5,60(r5)     x80
    CritterBossAI        header->movesPtr + moveIndex * 0x90    0x90 records
    CRITTER_DIE          hdr->descriptor  + 0x20                0x20 records

and a fifth from the same campaign, `+100` emitted as `+24000`. In `--ops`
each is ONE aligned IMMEDIATE row, which is exactly how they were dismissed
as regalloc noise: the finding's own advice is "a per-function `fndiff --ops`
will show it as a lone IMMEDIATE row, which is easy to dismiss".

CALIBRATION (build/e_lane/e_scaled_calib.py at 8c81b21f3, over every built
object: 258 TUs, 141 functions with IMMEDIATE rows, 2,345 rows):

    5 of 5 positive controls flagged
    0 of 10 negative controls flagged
    3 of 2,345 live rows flagged (0.13%)

TWO RULES WERE MEASURED AWAY and are asserted here as negatives, because a
first cut shipped both and produced 40 live hits (1.71%) of which at least 9
were plainly wrong:

  * `ours == target + k * stride` -- no positive control, and its 9 live hits
    were all frame slots and different bases: `ControlsUpdate addi r3,r1,20`
    against `addi r3,r1,108` read as "+1 * sizeof(SndVoice)" because 88 is a
    struct size somewhere in this tree.
  * "exactly one numeric field differs", without requiring the rest of the
    instruction to be identical: it paired `lwz r10,4(r31)` with
    `lwz r10,180(r1)` and called it x45. Those read different BASES.
  * a target literal of 1, which makes EVERY literal an integer multiple:
    `li r4,1` against `li r5,33` was reported as x33.
"""
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT))

import fndiff  # noqa: E402

#: A fixed stride table, so these tests do not move when a header does.
STRIDES = {0x3C: ("WorldObj",), 0x50: ("plyr_sfx",),
           0x58: ("SndVoice", "plyr_damage"), 0x80: ("mbnode",),
           0x180: ("plyr_data",), 0x18C: ("Camera",)}


def verdict(t_line, b_line, strides=STRIDES):
    return fndiff.scaled_displacement(t_line, b_line, strides)


class HeaderStrideTable(unittest.TestCase):
    def test_the_live_headers_assert_the_sizes_this_relies_on(self):
        table = fndiff.known_record_strides(ROOT)
        self.assertIn(0x18C, table)
        self.assertIn("Camera", table[0x18C])
        self.assertIn(0x80, table)
        self.assertIn("mbnode", table[0x80])
        self.assertIn(0x3C, table)
        self.assertIn("WorldObj", table[0x3C])

    def test_sizes_below_the_floor_are_not_strides(self):
        table = fndiff.known_record_strides(ROOT)
        self.assertTrue(all(size >= fndiff.SCALED_MIN_STRIDE
                            for size in table))

    def test_a_tree_with_no_game_headers_yields_an_empty_table(self):
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            self.assertEqual(fndiff.known_record_strides(Path(temp)), {})


class PositiveControls(unittest.TestCase):
    """Every known run-62 miss, replayed from its quoted words."""

    def test_critter_new_inst_types_offset(self):
        found = verdict("lwz     r3,20(r3)", "lwz     r3,1600(r3)")
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 80)
        self.assertEqual(found["difference"], 1580)

    def test_critter_init_colnodes_nodes_offset(self):
        found = verdict("lwz     r5,60(r5)", "lwz     r5,4800(r5)")
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 80)

    def test_critter_boss_ai_0x90_records(self):
        found = verdict("addi    r3,r3,144", "addi    r3,r3,20736")
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 144)

    def test_critter_die_0x20_records(self):
        found = verdict("addi    r4,r4,32", "addi    r4,r4,1024")
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 32)

    def test_the_plus_100_to_plus_24000_row(self):
        found = verdict("addi    r3,r3,100", "addi    r3,r3,24000")
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 240)
        self.assertEqual(found["difference"], 23900)

    def test_a_named_stride_is_named_but_only_as_a_candidate(self):
        found = verdict("lwz     r3,20(r3)", "lwz     r3,1600(r3)")
        self.assertEqual(found["names"], ("plyr_sfx",))
        self.assertIn("CANDIDATE", found["why"])

    def test_an_unnamed_ratio_says_so_instead_of_inventing_a_record(self):
        found = verdict("addi    r3,r3,100", "addi    r3,r3,24000")
        self.assertEqual(found["names"], ())
        self.assertIn("plausible", found["why"])
        self.assertNotIn("sizeof", found["why"])

    def test_a_named_stride_below_the_blind_factor_still_fires(self):
        # x60 (WorldObj) would pass the blind factor anyway; use a case that
        # only the NAME can reach, to prove the two rules are independent.
        found = verdict("lwz     r3,8(r3)", "lwz     r3,480(r3)",
                        {60: ("WorldObj",)})
        self.assertIsNotNone(found)
        self.assertEqual(found["factor"], 60)
        blind = verdict("lwz     r3,8(r3)", "lwz     r3,480(r3)", {})
        self.assertIsNotNone(blind, "60 >= SCALED_BLIND_FACTOR")


class NegativeControls(unittest.TestCase):
    """Each of these was, or would have been, a false positive."""

    def test_adjacent_stack_slots_are_silent(self):
        self.assertIsNone(verdict("lwz     r3,8(r1)", "lwz     r3,12(r1)"))

    def test_a_neighbouring_field_is_silent(self):
        self.assertIsNone(verdict("lwz     r3,20(r3)", "lwz     r3,24(r3)"))

    def test_a_shrinking_literal_is_silent(self):
        # A retype scales UP. A smaller literal is a different question.
        self.assertIsNone(verdict("addi    r3,r3,1600", "addi    r3,r3,20"))

    def test_a_sign_flip_is_silent(self):
        self.assertIsNone(verdict("addi    r3,r3,-20", "addi    r3,r3,1600"))

    def test_a_zero_on_either_side_is_silent(self):
        self.assertIsNone(verdict("addi    r3,r3,0", "addi    r3,r3,1600"))
        self.assertIsNone(verdict("addi    r3,r3,20", "addi    r3,r3,0"))

    def test_a_small_unnamed_factor_is_silent(self):
        self.assertIsNone(verdict("lwz     r3,4(r3)", "lwz     r3,32(r3)"))

    def test_two_literals_moving_is_silent(self):
        self.assertIsNone(verdict("rlwinm  r3,r3,2,0,29",
                                  "rlwinm  r3,r3,4,0,27"))

    def test_a_different_base_register_is_silent(self):
        # Live false positive of the first cut: reported as x45.
        self.assertIsNone(verdict("lwz     r10,4(r31)", "lwz     r10,180(r1)"))

    def test_a_different_destination_register_is_silent(self):
        self.assertIsNone(verdict("lwz     r3,20(r3)", "lwz     r4,1600(r3)"))

    def test_a_target_literal_of_one_is_silent(self):
        # Live false positive of the first cut: reported as x33.
        self.assertIsNone(verdict("li      r4,1", "li      r4,33"))
        self.assertIsNone(verdict("addi    r0,r4,1", "addi    r0,r4,96"))

    def test_a_frame_slot_difference_is_silent(self):
        # Live false positive of the DROPPED `target + k * stride` rule.
        self.assertIsNone(verdict("addi    r3,r1,20", "addi    r3,r1,108"))
        self.assertIsNone(verdict("stw     r0,72(r1)", "stw     r0,152(r1)"))

    def test_a_different_opcode_is_silent(self):
        self.assertIsNone(verdict("lwz     r3,20(r3)", "lbz     r3,1600(r3)"))

    def test_the_dropped_offset_rule_is_not_reachable_by_any_stride(self):
        # AddLocatorInstList, the one live hit that survived every other
        # filter of the dropped rule: +3080 == 35 * sizeof(SndVoice).
        self.assertIsNone(verdict("addi    r4,r4,12", "addi    r4,r4,3092"))


class RowsAndFormatting(unittest.TestCase):
    def test_only_immediate_rows_are_screened_not_branch_rows(self):
        rows = [(0, 0, "branch", "b       <fn+0x20>", "b       <fn+0x800>")]
        self.assertEqual(fndiff.scaled_displacement_rows(rows, STRIDES), {})

    def test_the_row_map_is_keyed_by_target_index(self):
        rows = [(7, 7, "immediate", "lwz     r3,20(r3)",
                 "lwz     r3,1600(r3)"),
                (9, 9, "immediate", "lwz     r3,8(r1)", "lwz     r3,12(r1)")]
        found = fndiff.scaled_displacement_rows(rows, STRIDES)
        self.assertEqual(list(found), [7])

    def test_the_tag_carries_both_the_factor_and_the_difference(self):
        tag = fndiff.format_scaled_displacement(
            verdict("lwz     r3,20(r3)", "lwz     r3,1600(r3)"))
        self.assertIn("SCALED-DISPLACEMENT", tag)
        self.assertIn("x80", tag)
        self.assertIn("+1580", tag)

    def test_a_truncated_ops_view_announces_a_suppressed_scaled_row(self):
        text = "\n".join(
            ["  replace T[0:1]@0-4=['a']  O[0:1]@0-4=['b']"] * 3
            + ["  IMMEDIATE T[7]@1c  O[7]@1c   T: x   O: y"
               "   [SCALED-DISPLACEMENT (x80 / +1580): z]"])
        cut = fndiff.truncate_ops(text, 3)
        self.assertIn("SCALED-DISPLACEMENT row(s) suppressed", cut)

    def test_an_untruncated_view_adds_no_suppression_note(self):
        text = "  IMMEDIATE T[7]@1c  O[7]@1c   [SCALED-DISPLACEMENT (x80)]"
        self.assertNotIn("suppressed", fndiff.truncate_ops(text, 10))


LIVE = (ROOT / "build/GUNE5D/src/game/sound/sounds.o").is_file()


@unittest.skipUnless(LIVE, "needs a built object")
class LiveCalibration(unittest.TestCase):
    """The live positives the calibration sweep found, pinned as fixtures."""

    def test_init_name_audio_carries_two_scaled_rows(self):
        target = fndiff.parse(ROOT / "build/GUNE5D/obj/game/sound/sounds.o")
        ours = fndiff.parse(ROOT / "build/GUNE5D/src/game/sound/sounds.o")
        rows = fndiff.immediate_deltas(target["InitNameAudio"],
                                       ours["InitNameAudio"])
        found = fndiff.scaled_displacement_rows(rows)
        self.assertEqual(sorted(found), [54, 108])
        self.assertEqual(found[54]["factor"], 16)

    def test_the_live_rate_stays_a_tell_not_a_footnote(self):
        # If this ever exceeds a few percent the classifier stopped
        # discriminating and must be recalibrated before it is trusted.
        total = flagged = 0
        for unit in ("game/sound/sounds", "game/game/controls",
                     "game/game/combat", "game/world/gauntworld"):
            target = ROOT / "build/GUNE5D/obj" / (unit + ".o")
            ours = ROOT / "build/GUNE5D/src" / (unit + ".o")
            if not (target.is_file() and ours.is_file()):
                continue
            t, o = fndiff.parse(target), fndiff.parse(ours)
            for name in sorted(set(t) & set(o)):
                rows = fndiff.immediate_deltas(t[name], o[name])
                rows = [row for row in rows if row[2] == "immediate"]
                total += len(rows)
                flagged += len(fndiff.scaled_displacement_rows(rows))
        self.assertGreater(total, 100, "not enough rows to calibrate against")
        self.assertLess(flagged, total * 0.05,
                        "%d of %d rows flagged" % (flagged, total))


if __name__ == "__main__":
    unittest.main()
