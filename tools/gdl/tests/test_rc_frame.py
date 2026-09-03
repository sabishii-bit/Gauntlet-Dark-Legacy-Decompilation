"""rc_recolor_class's FRAME column (run 41 item 1).

A frame-size difference is STACK_LAYOUT, categorically outside both classes
the tool splits, and neither the mnemonic-divergence count nor the opcode
multiset can see it — so a stack-work function reads as schedule work.
Measured: 2 of the 9 rows of claim.CT_schedule-class-tier-2-remeasured-live-
with-pin-and-frame-columns.20260903.v1 were exactly that (AudioWithName,
target `stwu r1,-56(r1)` vs ours `stwu r1,-72(r1)`; fn_80093918, 8 bytes),
and they were the same two rows whose word counts failed to reproduce.

Corpus calibration before shipping: of 2,903 count-symmetric functions,
2,022 have equal frames, 875 have no `stwu` frame at all, and 6 differ — 5
of which already wore a schedule label. Exactly ONE row changes class
(game/ps2/fakelib::sDvdReadSync, 48 vs 56), and it is independently recorded
as STACK_LAYOUT, so the column reclassifies nothing it should not.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

from rc_recolor_class import frame_size, label_with_frame  # noqa: E402


def rows(*texts):
    return [{"off": i * 4, "mnem": text.split()[0], "regs": [], "text": text}
            for i, text in enumerate(texts)]


class FrameSizeTests(unittest.TestCase):
    def test_the_prologue_stwu_gives_the_frame(self):
        self.assertEqual(
            frame_size(rows("mflr r0", "stwu r1,-56(r1)", "stmw r27,36(r1)")),
            56)

    def test_the_ct_row_reproduces_both_sides(self):
        target = rows("mflr r0", "stwu r1,-56(r1)")
        ours = rows("mflr r0", "stwu r1,-72(r1)")
        self.assertEqual(frame_size(target) - frame_size(ours), -16)

    def test_a_leaf_function_with_no_frame_is_unknown_not_zero(self):
        """A silent 0 would compare equal to another 0 and read as
        'frames agree', which is the failure this column exists to stop."""
        self.assertIsNone(frame_size(rows("mflr r0", "add r3,r3,r4", "blr")))

    def test_a_later_stwu_on_r1_does_not_override_the_prologue(self):
        self.assertEqual(
            frame_size(rows("stwu r1,-32(r1)", "stwu r1,-16(r1)")), 32)

    def test_the_large_frame_stwux_form_is_unknown(self):
        self.assertIsNone(frame_size(rows("lis r0,-1", "stwux r1,r1,r0")))

    def test_a_store_to_another_register_is_not_a_frame(self):
        self.assertIsNone(frame_size(rows("stwu r5,-56(r4)")))


class FrameLabelTests(unittest.TestCase):
    def test_stack_layout_is_prefixed_so_a_truncated_label_still_shows_it(self):
        label = label_with_frame("SCHEDULE-REORDER (6 mnemonic diffs)", -16)
        self.assertTrue(label.startswith("STACK_LAYOUT (frame -16)"))
        self.assertIn("SCHEDULE-REORDER (6 mnemonic diffs)", label)

    def test_a_recolor_label_is_overridden_too(self):
        label = label_with_frame("CONSISTENT-RECOLOR", +8)
        self.assertTrue(label.startswith("STACK_LAYOUT (frame +8)"))

    def test_equal_frames_leave_the_label_alone(self):
        self.assertEqual(label_with_frame("CONSISTENT-RECOLOR", 0),
                         "CONSISTENT-RECOLOR")

    def test_an_unknown_frame_is_not_evidence_of_a_difference(self):
        self.assertEqual(label_with_frame("CONSISTENT-RECOLOR", None),
                         "CONSISTENT-RECOLOR")


if __name__ == "__main__":
    unittest.main()
