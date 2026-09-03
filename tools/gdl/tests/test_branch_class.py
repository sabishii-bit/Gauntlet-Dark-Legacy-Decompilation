"""fndiff --ops: BRANCH-TARGET DIVERGENCE as a headline class (run 41 #2).

These rows used to print LAST — below every cluster and every IMMEDIATE row,
under a parenthetical calling them "usually downstream of the clusters
above". Usually is not always: the mechanism of CT's real 60 -> 34
MBCameraUpdate win sat below twelve downstream IMMEDIATE rows, where
probe's truncated view drops it entirely
(claim.CT_schedule-class-tier-2-remeasured-live-with-pin-and-frame-columns
.20260903.v1).

CALIBRATION decided the discriminator. `parse` normalizes a branch to its
ABSOLUTE target, so two byte-identical branch words at different indices
normalize to different targets: ranking on the absolute target called 5,286
of 6,982 rows structural (76%, 164 functions). Requiring the DISPLACEMENT to
differ — which is what the instruction word encodes — and no unpaired
cluster between the branch and its target leaves 35 rows in 14 functions
(0.5%).
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fndiff  # noqa: E402


class BranchDivergenceTests(unittest.TestCase):
    def test_a_different_displacement_with_a_clear_span_is_structural(self):
        target = ["nop", "b <fn+0xc>", "nop", "nop"]
        ours = ["nop", "b <fn+0x8>", "nop", "nop"]
        rows = fndiff.branch_divergences(target, ours)
        self.assertEqual(len(rows), 1)
        t_index, o_index, t_target, o_target, structural = rows[0]
        self.assertEqual((t_index, o_index, t_target, o_target), (1, 1, 3, 2))
        self.assertTrue(structural)

    def test_the_same_displacement_at_a_shifted_index_is_an_artifact(self):
        """The two branch WORDS are identical; only `parse`'s absolute
        normalization makes them look different."""
        target = ["nop", "b <fn+0x14>", "nop", "nop", "nop"]
        ours = ["nop", "nop", "b <fn+0x18>", "nop", "nop", "nop"]
        rows = fndiff.branch_divergences(target, ours)
        self.assertTrue(rows)
        for _t, _o, t_target, o_target, structural in rows:
            self.assertEqual(t_target - 1, o_target - 2)   # one displacement
            self.assertFalse(structural)

    def test_an_unpaired_cluster_in_the_span_explains_the_difference(self):
        target = ["nop", "b <fn+0x14>", "nop", "nop", "nop", "nop"]
        ours = ["nop", "b <fn+0x18>", "addi r3,r3,1",
                "nop", "nop", "nop", "nop"]
        rows = fndiff.branch_divergences(target, ours)
        self.assertTrue(rows)
        self.assertFalse(any(row[4] for row in rows))

    def test_a_non_branch_immediate_row_is_not_a_branch_row(self):
        target = ["nop", "addi r3,r3,4", "nop"]
        ours = ["nop", "addi r3,r3,8", "nop"]
        self.assertEqual(fndiff.branch_divergences(target, ours), [])
        kinds = [row[2] for row in fndiff.immediate_deltas(target, ours)]
        self.assertEqual(kinds, ["immediate"])

    def test_identical_streams_have_no_rows(self):
        stream = ["nop", "b <fn+0xc>", "nop", "nop"]
        self.assertEqual(fndiff.branch_divergences(stream, stream), [])

    def test_a_backward_branch_target_parses(self):
        self.assertEqual(fndiff._branch_target_index("bdnz <fn+0x-8>"), -2)
        self.assertEqual(fndiff._branch_target_index("b <fn+0x20>"), 8)
        self.assertIsNone(fndiff._branch_target_index("nop"))


class HeadlinePlacementTests(unittest.TestCase):
    def test_the_class_prints_above_the_clusters(self):
        import io
        from contextlib import redirect_stdout
        target = ["nop", "b <fn+0xc>", "nop", "nop"]
        ours = ["nop", "b <fn+0x8>", "nop", "nop"]
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            fndiff.ops_diff("fn", target, ours)
        lines = buffer.getvalue().splitlines()
        head = next(i for i, line in enumerate(lines)
                    if "BRANCH-TARGET DIVERGENCE" in line)
        rows = [i for i, line in enumerate(lines)
                if line.lstrip().startswith(("IMMEDIATE ", "replace",
                                             "insert", "delete"))]
        self.assertTrue(all(head < i for i in rows))

    def test_a_truncated_ops_view_announces_dropped_branch_lines(self):
        text = "\n".join(
            ["==== fn: target 4 insns, ours 4"]
            + ["  BRANCH-TARGET DIVERGENCE: 1 of 1"]
            + ["  BRANCH T[1]@4 -> @c   O[1]@4 -> @8"])
        cut = fndiff.truncate_ops(text, 1)
        self.assertIn("BRANCH-TARGET DIVERGENCE line(s) suppressed", cut)
        self.assertIn("control-flow difference", cut)


if __name__ == "__main__":
    unittest.main()
