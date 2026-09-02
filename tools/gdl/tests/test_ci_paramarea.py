"""Derived frame metrics must assert their floors (run-37 item 8).

A DERIVED frame figure is a heuristic over disassembly, and when the
heuristic breaks it does not fail — it returns a number. The run-34
`param area` column is (lowest `addi rN,r1,K`) - 8, which assumes every
such addi addresses a declared local; on a by-value aggregate argument
MWCC emits `addi r5,r1,8` as the argument-COPY cursor, so the column
printed `shape_struct_byval ... param area 0`. Zero is below the PPC EABI
minimum of 8 and therefore impossible, but it was read as "does not reach
48" — as evidence — and shipped into
claim.law.CI_mwcc-outgoing-param-area-is-sized-only-by-stack-spilled-args
.20260902.v1.

Re-running the probe with the floor in place flagged a SECOND impossible
row the prose review had never named: shape_struct_return reported
`param area 4`.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

from ci_paramarea_probe import (EABI_MIN_PARAM_AREA,  # noqa: E402
                                frame_metric_floor_violation)


class FrameMetricFloorTests(unittest.TestCase):

    def test_the_eabi_floor_is_eight(self):
        """Back-chain word + LR save word at r1+0."""
        self.assertEqual(EABI_MIN_PARAM_AREA, 8)

    def test_the_measured_zero_is_refused(self):
        message = frame_metric_floor_violation("param area", 0)
        self.assertIsNotNone(message)
        self.assertIn("IMPOSSIBLE", message)
        self.assertIn("BROKEN MEASUREMENT", message)

    def test_the_second_impossible_row_is_also_refused(self):
        """shape_struct_return's 4 — found only once the floor existed."""
        self.assertIsNotNone(frame_metric_floor_violation("param area", 4))

    def test_the_floor_itself_is_allowed(self):
        self.assertIsNone(frame_metric_floor_violation("param area", 8))

    def test_a_real_value_passes(self):
        self.assertIsNone(frame_metric_floor_violation("param area", 48))

    def test_none_is_not_a_violation(self):
        """An unmeasurable case is already reported as unmeasured; it must
        not be re-reported as an impossible number."""
        self.assertIsNone(frame_metric_floor_violation("param area", None))

    def test_the_message_names_the_metric(self):
        self.assertIn("locals@",
                      frame_metric_floor_violation("locals@", 0))

    def test_the_message_forbids_reading_it_as_did_not_reach(self):
        """The exact misreading that shipped the law."""
        message = frame_metric_floor_violation("param area", 0)
        self.assertIn("did not reach", message)

    def test_the_floor_is_overridable_for_other_metrics(self):
        self.assertIsNone(frame_metric_floor_violation("below", 0, floor=0))
        self.assertIsNotNone(
            frame_metric_floor_violation("below", -4, floor=0))


if __name__ == "__main__":
    unittest.main()
