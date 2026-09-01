"""probe.py verdict-table tests.

The regression this file exists for: probe's CONFLICT verdict compared the
opcode-multiset token count against the PREVIOUS probe, and every probe
banked its own count into that slot. Re-scoring an already-scored state
therefore flipped CONFLICT -> "REGRESSED ... [revert advised]" on bytes
that had not moved (measured on game/sys/memcard get_vmu_directory during
run 29: real 65 -> 65, insns and multiset both unchanged).
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from probe import annotate_neutral, classify, count_distance


class CountDistanceTests(unittest.TestCase):
    def test_parses_insns_string(self):
        self.assertEqual(count_distance("T116/O115"), 1)
        self.assertEqual(count_distance("T290/O290"), 0)

    def test_unparseable_is_none(self):
        self.assertIsNone(count_distance("exact"))
        self.assertIsNone(count_distance(None))


class ClassifyTests(unittest.TestCase):
    def test_baseline_banks_best_real_and_best_multiset(self):
        verdict, state = classify({}, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("BASELINE"))
        self.assertEqual(state["best_real"], 65)
        self.assertEqual(state["best_multiset"], 3)

    def test_improvement_rebanks_both_best_fields(self):
        state = {"best_real": 65, "best_multiset": 3,
                 "last_real": 65, "last_insns": "T116/O115",
                 "last_multiset": 3}
        verdict, state = classify(state, 48, "T116/O116", 2)
        self.assertTrue(verdict.startswith("IMPROVED"), verdict)
        self.assertEqual(state["best_real"], 48)
        self.assertEqual(state["best_multiset"], 2)

    def test_conflict_is_anchored_on_best_not_prev(self):
        """A real rise with structure converging against BEST is CONFLICT."""
        state = {"best_real": 48, "best_multiset": 4,
                 "last_real": 48, "last_insns": "T116/O116",
                 "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("4t -> 3t vs best", verdict)

    def test_rescoring_a_conflict_state_does_not_flip_to_regressed(self):
        """THE run-29 REGRESSION TEST.

        Feed classify() exactly the state a CONFLICT probe leaves behind,
        then re-score the same measurement. Under the prev-anchored
        comparison this returned REGRESSED with '[revert advised]'.
        """
        _, after_conflict = classify(
            {"best_real": 48, "best_multiset": 4, "last_real": 48,
             "last_insns": "T116/O116", "last_multiset": 4},
            65, "T116/O115", 3)
        verdict, _ = classify(after_conflict, 65, "T116/O115", 3)
        self.assertNotIn("REGRESSED", verdict)
        self.assertNotIn("revert advised", verdict)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)

    def test_real_rise_without_structure_gain_is_still_regressed(self):
        state = {"best_real": 48, "best_multiset": 4,
                 "last_real": 48, "last_insns": "T116/O116",
                 "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O120", 5)
        self.assertTrue(verdict.startswith("REGRESSED"), verdict)

    def test_legacy_state_without_best_multiset_says_so(self):
        state = {"best_real": 48, "last_real": 48,
                 "last_insns": "T116/O116", "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("vs prev", verdict)
        self.assertIn("no best_multiset banked", verdict)

    def test_legacy_fallback_is_flagged_on_the_regressed_half_too(self):
        """The half that tells a worker to throw work away must say it.

        This is the exact legacy shape measured live in run 29: the state
        a CONFLICT left behind, re-scored, reads REGRESSED because prev
        already carries the improved multiset.
        """
        state = {"best_real": 48, "last_real": 65,
                 "last_insns": "T116/O115", "last_multiset": 3}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("REGRESSED"), verdict)
        self.assertIn("no best_multiset banked", verdict)

    def test_rebase_best_banks_current_as_best(self):
        state = {"best_real": 48, "best_multiset": 4, "last_real": 65,
                 "last_insns": "T116/O115", "last_multiset": 3}
        verdict, state = classify(state, 65, "T116/O115", 3,
                                  rebase_best=True)
        self.assertTrue(verdict.startswith("REBASED"), verdict)
        self.assertEqual(state["best_real"], 65)
        self.assertEqual(state["best_multiset"], 3)

    def test_blown_out_count_distance_refuses_to_bank_a_real_win(self):
        state = {"best_real": 949, "best_multiset": 20, "last_real": 949,
                 "last_insns": "T500/O500", "last_multiset": 20}
        verdict, state = classify(state, 802, "T500/O343", 25)
        self.assertIn("IMPROVED?", verdict)
        self.assertEqual(state["best_real"], 949)

    def test_parity_held_improvement_demands_fuzzy_arbitration(self):
        state = {"best_real": 30, "best_multiset": 0, "last_real": 30,
                 "last_insns": "T47/O47", "last_multiset": 0}
        verdict, _ = classify(state, 24, "T47/O47", 0)
        self.assertIn("PARITY-HELD IMPROVEMENT", verdict)


class RescoreGuardTests(unittest.TestCase):
    BASE = {"best_real": 48, "best_multiset": 4, "last_real": 65,
            "last_insns": "T116/O115", "last_multiset": 3,
            "last_bytes": "abc123", "last_verdict": "CONFLICT  standing"}

    def test_unchanged_source_and_digest_repeats_the_standing_verdict(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="abc123", source_changed=False)
        self.assertTrue(verdict.startswith("RE-SCORE"), verdict)
        self.assertIn("CONFLICT  standing", verdict)
        self.assertIn("REPEATED, not recomputed", verdict)

    def test_changed_source_recomputes_even_at_equal_scores(self):
        """An edit that folds away must still be classified, not swallowed."""
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="abc123", source_changed=True)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)

    def test_moved_bytes_at_equal_scores_recompute(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="deadbeef", source_changed=False)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)

    def test_no_digest_means_no_guard(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest=None, source_changed=False)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)


class AnnotateNeutralTests(unittest.TestCase):
    def test_identical_bytes_flag_a_folded_away_edit(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O50, multiset 0t)",
                               4, "T50/O50", 0, 0, "T50/O50", "same", "same")
        self.assertIn("NEUTRAL-IDENTICAL", out)

    def test_moved_bytes_flag_a_rearrangement(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O50, multiset 0t)",
                               4, "T50/O50", 0, 0, "T50/O50", "old", "new")
        self.assertIn("NEUTRAL-REARRANGED", out)

    def test_structurally_worse_neutral_is_not_banked(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O44, multiset 6t)",
                               4, "T50/O44", 6, 2, "T50/O50", "old", "new")
        self.assertTrue(out.startswith("NEUTRAL-WORSE"), out)
        self.assertIn("count distance 0 -> 6", out)
        self.assertIn("multiset 2t -> 6t", out)


if __name__ == "__main__":
    unittest.main()
