"""cs_constsweep set-corroboration ranking (run 34 item 8).

A wrong constant that recurs across a SET of functions is far likelier to be
a systematic source error (a shared #define read wrong in every consumer)
than a one-off reconstruction difference in a single NonMatching body. The
sweep now ranks those corroborated rows first instead of printing findings in
whatever order the units swept.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import cs_constsweep as cs  # noqa: E402


def vm(unit, fn, target_only, ours_only):
    return {"verdict": "VALUE_MISMATCH", "unit": unit, "function": fn,
            "target_only": target_only, "ours_only": ours_only}


class CorroborationTests(unittest.TestCase):
    def test_a_shared_substitution_pair_is_counted_across_functions(self):
        findings = [vm("u/a", "f1", ["32.0f"], ["8.0"]),
                    vm("u/b", "f2", ["32.0f"], ["8.0"])]
        corro = cs.value_corroboration(findings)
        self.assertEqual(len(corro[("32.0f", "8.0")]), 2)

    def test_a_unique_substitution_scores_one(self):
        findings = [vm("u/a", "f1", ["1.5f"], ["2.5f"])]
        best, shared = cs.corroboration_score(
            findings[0], cs.value_corroboration(findings))
        self.assertEqual(best, 1)
        self.assertEqual(shared, [])

    def test_a_common_value_alone_does_not_corroborate(self):
        """100.0 recurs everywhere as debt; only the SAME swap is signal."""
        findings = [vm("u/a", "f1", ["100.0"], ["0.5"]),
                    vm("u/b", "f2", ["100.0"], ["0.25"])]
        best, _ = cs.corroboration_score(
            findings[0], cs.value_corroboration(findings))
        self.assertEqual(best, 1)   # different ours-value -> not the same swap

    def test_a_shared_swap_scores_the_function_count(self):
        findings = [vm("u/a", "f1", ["32.0f"], ["8.0"]),
                    vm("u/b", "f2", ["32.0f"], ["8.0"]),
                    vm("u/c", "f3", ["32.0f"], ["8.0"])]
        best, shared = cs.corroboration_score(
            findings[0], cs.value_corroboration(findings))
        self.assertEqual(best, 3)
        self.assertIn((("32.0f", "8.0"), 3), shared)

    def test_corroborated_rows_rank_first(self):
        findings = [
            vm("u/z", "lone", ["1.0f"], ["2.0f"]),            # unique swap
            vm("u/a", "sysA", ["32.0f"], ["8.0"]),            # shared swap
            vm("u/b", "sysB", ["32.0f"], ["8.0"]),            # shared swap
        ]
        ranked = cs.rank_value_mismatches(findings)
        self.assertEqual(ranked[0]["corroboration"], 2)
        self.assertEqual(ranked[-1]["function"], "lone")
        self.assertEqual(ranked[-1]["corroboration"], 1)

    def test_ranking_is_stable_within_a_tier(self):
        findings = [vm("u/b", "f2", ["5.0f"], ["6.0f"]),
                    vm("u/a", "f1", ["7.0f"], ["8.0f"])]
        ranked = cs.rank_value_mismatches(findings)
        # both uncorroborated -> ordered by (unit, function)
        self.assertEqual([r["function"] for r in ranked], ["f1", "f2"])

    def test_the_original_findings_are_not_mutated(self):
        findings = [vm("u/a", "f1", ["32.0f"], ["8.0"])]
        cs.rank_value_mismatches(findings)
        self.assertNotIn("corroboration", findings[0])


if __name__ == "__main__":
    unittest.main()
