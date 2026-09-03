#!/usr/bin/env python3
"""T17 run-47 item 5: the IMMEDIATE-row arbiter on a multiset-IDENTICAL residual.

When `fndiff --ops` reports `opcode multiset: IDENTICAL`, probe's verdict is
decided by `real` alone -- the token count is structurally zero and the
headline reads "pure reorder, schedule-class residual". `real` counts
DIFFERING WORDS, so a register recolour rippling through forty instructions
swamps the single word a wrong literal occupies. NM had to tally the IMMEDIATE
rows by hand, and `real` said REVERT on the probe that unlocked the exact.

An IMMEDIATE row is a position where the OPCODE agrees and a LITERAL does not.
Those rows sit INSIDE the matcher's equal runs, so no --ops cluster covers
them and the opcode multiset cannot see them either -- which is exactly why
the multiset-IDENTICAL verdict is blind to the one thing that decides
postprocessor eligibility.

Two-sided calibration at e05f39017, over every function pair built in this
tree:

    multiset IDENTICAL                     2,844
      ... carrying IMMEDIATE rows             33   <- the line fires here
    multiset DIFFERS                         188   <- never fires (a cluster
      ... carrying IMMEDIATE rows             117      already covers it)

So the line is silent on 99% of the class it guards. Verified live: it fires
on game/world/camera::camera_mode_dest (16 rows, real 156, multiset 0t),
repeats as `(UNCHANGED)` on a second probe, and stays silent on
game/audio/adstream::AdsPutBuffer (real 198, multiset 0t, zero IMMEDIATE rows)
and on game/world/camera::camera_mode_follow (multiset 4t).
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402

OPS = """==== f: target 682 insns, ours 682
  opcode multiset: IDENTICAL (682/682) -- but 16 IMMEDIATE word(s) differ
  IMMEDIATE T[3]@c  O[3]@c   T: li r3,1   O: li r3,2
  16 IMMEDIATE row(s): the opcode agrees and a LITERAL does not. These sit
"""
OPS_SHAKY = """==== f: target 10 insns, ours 10
  opcode multiset: IDENTICAL (10/10)
  2 of 5 IMMEDIATE row(s) are marked PAIRING UNRELIABLE: they sit at the edge
  5 IMMEDIATE row(s): the opcode agrees and a LITERAL does not. These sit
"""
OPS_NONE = """==== f: target 254 insns, ours 254
  opcode multiset: IDENTICAL (254/254) -- pure reorder, schedule-class residual
"""


class Counting(unittest.TestCase):
    def test_the_count_is_read_from_fndiffs_own_summary_line(self):
        self.assertEqual(probe.immediate_row_count(OPS), 16)

    def test_the_PAIRING_UNRELIABLE_line_is_not_mistaken_for_the_count(self):
        """`2 of 5 IMMEDIATE row(s) are marked...` sits above the summary."""
        self.assertEqual(probe.immediate_row_count(OPS_SHAKY), 5)

    def test_an_ops_dump_with_no_summary_line_counts_zero(self):
        self.assertEqual(probe.immediate_row_count(OPS_NONE), 0)

    def test_no_ops_dump_is_None_not_zero(self):
        self.assertIsNone(probe.immediate_row_count(None))
        self.assertIsNone(probe.immediate_row_count("build failed"))


class ArbiterLine(unittest.TestCase):
    def test_silent_when_there_is_nothing_to_arbitrate(self):
        self.assertIsNone(probe.immediate_arbiter_line(0, 0, 100, 100))
        self.assertIsNone(probe.immediate_arbiter_line(0, None, 100, None))

    def test_silent_when_the_count_is_unknown(self):
        self.assertIsNone(probe.immediate_arbiter_line(None, 3, 100, 100))

    def test_a_first_probe_states_the_count_and_why_real_cannot_decide(self):
        text = probe.immediate_arbiter_line(16, None, 156, None)
        self.assertIn("16 row(s)", text)
        self.assertIn("IDENTICAL", text)
        self.assertIn("`real`", text)

    def test_an_unchanged_count_says_so(self):
        text = probe.immediate_arbiter_line(16, 16, 156, 156)
        self.assertIn("(UNCHANGED)", text)

    def test_a_fall_beside_a_real_RISE_is_called_out(self):
        """The shape NM measured: the verdict advises a revert on the probe
        that closed a literal."""
        text = probe.immediate_arbiter_line(2, 5, 160, 156)
        self.assertIn("-3 vs the last probe's 5", text)
        self.assertIn("`real` ROSE while the IMMEDIATE count FELL", text)

    def test_a_fall_beside_a_real_FALL_needs_no_warning(self):
        text = probe.immediate_arbiter_line(2, 5, 150, 156)
        self.assertIn("-3 vs the last probe's 5", text)
        self.assertNotIn("ROSE", text)

    def test_a_new_row_is_called_out_as_eligibility_deciding(self):
        text = probe.immediate_arbiter_line(3, 1, 150, 156)
        self.assertIn("+2 vs the last probe's 1", text)
        self.assertIn("eligibility-deciding", text)


class Wiring(unittest.TestCase):
    def test_the_line_is_gated_on_a_multiset_IDENTICAL_residual(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        self.assertIn("if multiset_tokens == 0 and real > 0:", text)
        self.assertIn("immediate_arbiter_line(", text)

    def test_the_count_is_banked_for_the_next_probes_delta(self):
        text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")
        self.assertIn('state["last_immediates"] = immediates', text)
        self.assertIn('prev_immediates = state.get("last_immediates")', text)


if __name__ == "__main__":
    unittest.main()
