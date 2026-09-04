"""The COUNT-PARITY banner names its reference, and it is the baseline.

Run-52 item 4.  Parity is a property of the FUNCTION against the target, so
"this function has just become eligible for a postprocessor class" is a
claim about the function — and it was computed against whatever the last
probe left in the state file.

REPRODUCTION, verbatim from attempt.CR_critterremovecolnodesub-the-two-
pointer-hitnode-split-costs-one-instruction-or-folds-back.20260904.v1:

    NOTE ON A MISLEADING BANNER, recorded because it nearly entered this
    record as a result: probe (2) printed `COUNT-PARITY GAINED insns
    T77/O78 -> T77/O77`, which is true only against probe (1)'s failed
    state -- parity was never lost against HEAD, which was T77/O77
    throughout. The banner compares to the PREVIOUS PROBE, not to the
    commit, and reads like an achievement.

Run 49 fixed the `--discard` path by RE-SCORING the restored tree
(test_t19_discard_rescore).  That closed one route to a stale `last_insns`;
it did not make the reference right, so any path that leaves a different
previous probe behind still produces the flattering line.

NEGATIVE SIDE, and the reason this is not just a suppression: a REAL parity
gain must still announce itself.  attempt.NC_draw-power-meter-pooled-base-
buys-count-parity-and-the-tu-wide-pooling-claim-is-false.20260903.v1 went
BASELINE T289/O290 -> FINAL T289/O289 and quotes probe's own class banner as
the result.  That case is pinned below and must keep printing GAINED.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from probe import count_class_line  # noqa: E402


class CritterRemoveColnodeSubTests(unittest.TestCase):
    """Baseline T77/O77, a failed probe at T77/O78, then back to T77/O77."""

    BASE = "T77/O77"

    def test_the_old_reference_still_produces_the_false_banner(self):
        # Pinned as the defect, so a regression cannot restore it quietly.
        line = count_class_line("T77/O78", "T77/O77")
        self.assertTrue(line.startswith("COUNT-PARITY GAINED"))

    def test_against_the_baseline_nothing_was_gained(self):
        line = count_class_line("T77/O78", "T77/O77",
                                baseline_insns=self.BASE)
        self.assertNotIn("GAINED", line)
        self.assertIn("COUNT-PARITY HELD", line)
        self.assertIn("NOT a class change", line)

    def test_it_names_both_states_so_the_reader_can_check(self):
        line = count_class_line("T77/O78", "T77/O77",
                                baseline_insns=self.BASE)
        self.assertIn("the SESSION BASELINE T77/O77", line)
        self.assertIn("the PREVIOUS PROBE was T77/O78", line)

    def test_the_failing_probe_itself_is_still_a_real_LOSS(self):
        # Probe (1): baseline T77/O77 -> T77/O78. That IS a class change.
        line = count_class_line("T77/O77", "T77/O78",
                                baseline_insns=self.BASE)
        self.assertTrue(line.startswith("COUNT-PARITY LOST"))
        self.assertIn("the SESSION BASELINE", line)


class GenuineGainStillAnnouncesTests(unittest.TestCase):
    """draw_power_meter: T289/O290 -> T289/O289, a real class change."""

    def test_a_gain_against_the_baseline_still_prints_GAINED(self):
        line = count_class_line("T289/O290", "T289/O289",
                                baseline_insns="T289/O290")
        self.assertTrue(line.startswith("COUNT-PARITY GAINED"))
        self.assertIn("eligible", line)

    def test_a_gain_survives_an_intermediate_probe_that_also_lost_it(self):
        # baseline T289/O290 (asymmetric), a probe at T289/O292, now
        # T289/O289: still a genuine GAIN against the baseline.
        line = count_class_line("T289/O292", "T289/O289",
                                baseline_insns="T289/O290")
        self.assertTrue(line.startswith("COUNT-PARITY GAINED"))
        self.assertIn("T289/O290 -> T289/O289", line)

    def test_a_loss_against_the_baseline_still_prints_LOST(self):
        line = count_class_line("T682/O682", "T682/O680",
                                baseline_insns="T682/O682")
        self.assertTrue(line.startswith("COUNT-PARITY LOST"))
        self.assertIn("differ by 2", line)


class SilenceAndFallbackTests(unittest.TestCase):
    def test_no_movement_anywhere_is_still_silent(self):
        self.assertEqual(
            count_class_line("T100/O100", "T100/O100",
                             baseline_insns="T100/O100"), "")
        self.assertEqual(
            count_class_line("T100/O98", "T100/O95",
                             baseline_insns="T100/O97"), "")

    def test_without_a_baseline_the_old_reference_is_used_and_NAMED(self):
        line = count_class_line("T100/O98", "T100/O100")
        self.assertTrue(line.startswith("COUNT-PARITY GAINED"))
        self.assertIn("no session baseline banked", line)

    def test_an_unparseable_baseline_falls_back_to_the_previous_probe(self):
        line = count_class_line("T100/O98", "T100/O100",
                                baseline_insns="100/98")
        self.assertIn("no session baseline banked", line)

    def test_an_unparseable_current_count_says_nothing(self):
        self.assertEqual(
            count_class_line("T100/O100", "1174/1172",
                             baseline_insns="T100/O100"), "")

    def test_a_baseline_alone_is_enough_on_a_first_reprobe(self):
        line = count_class_line(None, "T77/O78", baseline_insns="T77/O77")
        self.assertTrue(line.startswith("COUNT-PARITY LOST"))
        self.assertNotIn("PREVIOUS PROBE was", line)


if __name__ == "__main__":
    unittest.main()
