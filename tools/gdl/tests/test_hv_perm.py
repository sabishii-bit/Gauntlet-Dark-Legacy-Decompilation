"""hv_perm: reopen per-offset absorption when composition refuses.

Run-35 item 12, against
claim.law.WF_per-offset-absorption-decided-before-window-selection-hides-the-
true-permutation-window.20260902.v1: absorption and movement are the two
competing explanations of the SAME word, so deciding absorption first at a
fixed offset forecloses movement. The absorbed word leaves the unabsorbed
set, the cluster window selection is built from shrinks, and the true window
is never offered to the prover — and the resulting refusal gets recorded as
a property of the FUNCTION.

The law's own scope limit is respected here: reopening changed WHICH refusal
its anchor produced, not WHETHER it closes. These tests pin the search
behaviour, not a close.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import hv_perm  # noqa: E402


class WidenAcrossAbsorbedTests(unittest.TestCase):
    def test_a_cluster_grows_over_adjacent_differing_words(self):
        # 0x10 and 0x24 differ but were ABSORBED, so the narrow cluster
        # stopped short of both.
        diffs = [0x10, 0x14, 0x18, 0x1C, 0x20, 0x24]
        self.assertEqual(
            hv_perm.widen_across_absorbed([(0x14, 0x24)], diffs),
            [(0x10, 0x28)])

    def test_an_identical_neighbour_stops_the_widening(self):
        """Only DIFFERING words are reopened; identical ones are not moved."""
        diffs = [0x14, 0x18, 0x1C, 0x20]
        self.assertEqual(
            hv_perm.widen_across_absorbed([(0x14, 0x24)], diffs),
            [(0x14, 0x24)])

    def test_widening_never_exceeds_the_window_atom_bound(self):
        diffs = [4 * i for i in range(200)]
        (lo, hi), = hv_perm.widen_across_absorbed([(0x100, 0x104)], diffs)
        self.assertLessEqual((hi - lo) // 4, hv_perm.MAX_WINDOW_ATOMS)

    def test_overlapping_widened_clusters_are_merged(self):
        """The combination step requires pairwise-disjoint windows."""
        diffs = [4 * i for i in range(16)]
        out = hv_perm.widen_across_absorbed([(0x10, 0x14), (0x18, 0x1C)],
                                            diffs)
        for first, second in zip(out, out[1:]):
            self.assertLessEqual(first[1], second[0])

    def test_an_oversized_merge_is_dropped_not_returned_too_wide(self):
        diffs = [4 * i for i in range(200)]
        out = hv_perm.widen_across_absorbed(
            [(0x10, 0x14)], diffs, limit=4)
        for lo, hi in out:
            self.assertLessEqual((hi - lo) // 4, 4)


class SpreadByWindowTests(unittest.TestCase):
    """The cap must not re-impose the narrowness the retry removes."""

    def _sols(self):
        # Ordered narrowest-window-first, as solve_cluster produces them.
        return ([{"lo": 0xC0, "hi": 0xD8, "order": [i]} for i in range(20)]
                + [{"lo": 0xA8, "hi": 0xD8, "order": [i]} for i in range(20)])

    def test_the_plain_truncation_would_drop_the_wider_window(self):
        """The defect, pinned: this is what the cap used to do."""
        windows = {(s["lo"], s["hi"]) for s in self._sols()[:8]}
        self.assertEqual(windows, {(0xC0, 0xD8)})

    def test_the_spread_represents_every_window(self):
        picked = hv_perm.spread_by_window(self._sols(), 8)
        self.assertEqual(len(picked), 8)
        self.assertEqual({(s["lo"], s["hi"]) for s in picked},
                         {(0xC0, 0xD8), (0xA8, 0xD8)})

    def test_it_never_exceeds_the_cap(self):
        self.assertEqual(len(hv_perm.spread_by_window(self._sols(), 3)), 3)

    def test_fewer_solutions_than_the_cap_are_all_returned(self):
        sols = [{"lo": 0, "hi": 8, "order": [0, 1]}]
        self.assertEqual(hv_perm.spread_by_window(sols, 8), sols)

    def test_no_solutions_is_empty(self):
        self.assertEqual(hv_perm.spread_by_window([], 8), [])


class DifferingTests(unittest.TestCase):
    def test_it_reports_every_differing_word_absorbed_or_not(self):
        ours = bytes.fromhex("00000001" "00000002" "00000003")
        tgt = bytes.fromhex("00000001" "000000FF" "00000003")
        self.assertEqual(hv_perm.differing(ours, tgt), [4])

    def test_identical_bodies_have_no_differing_words(self):
        body = bytes.fromhex("00000001" "00000002")
        self.assertEqual(hv_perm.differing(body, body), [])


if __name__ == "__main__":
    unittest.main()
