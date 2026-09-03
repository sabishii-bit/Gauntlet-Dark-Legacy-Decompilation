#!/usr/bin/env python3
"""T17 run-47 item 7: the TU-scope gate no longer promises a check it cannot run.

THE OBSERVATION, reproduced at a3d735404 on game/anim/atree::AtreeDelete with
one file-scope declaration added to the TU:

    [TU-scope gate: 1 file-scope change(s) in this diff -- cross-checking the
     whole TU against its defake_gate baseline (no build; --no-tu-gate skips
     it)]
    BASELINE  real 0 (insns exact, multiset 0t)
    [TU-scope gate: this BASELINE is banked over file-scope change(s) ...]

Two things are wrong with that, and the second is what cost NM a re-probe:

  1. A BASELINE is EXEMPT from this gate by construction -- it banks no
     improvement claim and is the session's only revert point, so
     `apply_tu_scope_gate` returns the verdict annotated and unchanged. The
     cross-check announced on the first line is measured and then discarded.
  2. The SAME OUTPUT appears when no cross-check was possible at all. Verified
     by moving build/GUNE5D/gate/game_anim_atree.json aside and re-running:
     byte-for-byte the same three blocks. A lane reads "cross-checking the
     whole TU against its defake_gate baseline", believes its siblings were
     screened at the first probe, and finds out later that they never were.

The gate fires on the mere EXISTENCE of a file-scope difference against HEAD,
which is true from a unit's first probe onward -- not on the edit that
introduced one, and not at a point where it can decide anything.

Two-sided:

  POSITIVE (BASELINE)      no cross-check is run, and the header says so;
                           when no TU baseline exists it also names the one
                           command that arms the gate for the next verdict.
  NEGATIVE (IMPROVED etc.) the cross-check path is untouched -- it still
                           announces, measures, and fails CLOSED when the
                           check could not run (`tu_scope_refusal` with
                           strict=None), which is the behaviour that exists
                           because a measurement nobody took cost nine
                           byte-exact functions.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402

CHANGES = [("decl ADDED", "extern void T17ProbeScopeMarker(int a)")]


class BaselineIsExempt(unittest.TestCase):
    def test_a_BASELINE_is_annotated_and_still_banks(self):
        verdict, state = probe.apply_tu_scope_gate(
            "BASELINE  real 0", {"best_real": 0}, {"best_real": None},
            CHANGES, None, None, "not run on a BASELINE", "u", "f")
        self.assertTrue(verdict.startswith("BASELINE"))
        self.assertEqual(state, {"best_real": 0})
        self.assertIn("TU-scope gate", verdict)

    def test_a_BASELINE_with_a_missing_cross_check_is_NOT_refused(self):
        """strict=None means 'could not run'. On any OTHER verdict that is a
        hard refusal; on a BASELINE it must not be, or a worker who edited
        before their first probe is left with no snapshot at all."""
        verdict, _ = probe.apply_tu_scope_gate(
            "BASELINE  real 0", {}, {}, CHANGES, None, None, "no baseline",
            "u", "f")
        self.assertNotIn("TU-SCOPE UNGATED", verdict)
        self.assertNotIn("NOTHING banked", verdict)


class NonBaselineIsUnchanged(unittest.TestCase):
    def test_an_IMPROVED_with_no_cross_check_still_fails_CLOSED(self):
        verdict, state = probe.apply_tu_scope_gate(
            "IMPROVED  real 10 -> 8", {"best_real": 8}, {"best_real": 10},
            CHANGES, None, None, "no baseline at build/x.json", "u", "f")
        self.assertIn("TU-SCOPE UNGATED", verdict)
        self.assertEqual(state["best_real"], 10)

    def test_an_IMPROVED_that_lost_a_byte_exact_sibling_is_refused(self):
        verdict, state = probe.apply_tu_scope_gate(
            "IMPROVED  real 10 -> 8", {"best_real": 8}, {"best_real": 10},
            CHANGES, [("sib", "was byte-identical")], [], "note", "u", "f")
        self.assertIn("TU-SCOPE REGRESSED", verdict)
        self.assertEqual(state["best_real"], 10)


class BaselineExistence(unittest.TestCase):
    def test_it_is_decided_from_a_path_not_a_measurement(self):
        self.assertIn(bool(probe.tu_baseline_exists("game/anim/atree")),
                      (True, False))

    def test_a_nonexistent_unit_has_no_baseline(self):
        self.assertFalse(probe.tu_baseline_exists("game/no/such/unit"))


class Wiring(unittest.TestCase):
    def setUp(self):
        self.text = (REPO / "tools" / "gdl" / "probe.py").read_text(
            encoding="utf-8")

    def test_the_BASELINE_branch_does_not_call_the_cross_check(self):
        start = self.text.index(
            'if scope_changes and verdict.startswith("BASELINE"):')
        end = self.text.index("        elif scope_changes:")
        branch = self.text[start:end]
        self.assertNotIn("tu_sibling_regressions", branch)
        self.assertIn("is NOT run on a BASELINE", branch)

    def test_the_other_branch_still_calls_it(self):
        start = self.text.index("        elif scope_changes:")
        branch = self.text[start:start + 1200]
        self.assertIn("tu_sibling_regressions(unit)", branch)
        self.assertIn("cross-checking the whole TU", branch)

    def test_the_missing_baseline_case_names_the_arming_command(self):
        start = self.text.index(
            'if scope_changes and verdict.startswith("BASELINE"):')
        end = self.text.index("        elif scope_changes:")
        branch = self.text[start:end]
        self.assertIn("no defake_gate baseline for this unit", branch)
        self.assertIn("defake_gate.py baseline {unit} --at-head", branch)
        self.assertIn("tu_baseline_exists(unit)", branch)


if __name__ == "__main__":
    unittest.main()
