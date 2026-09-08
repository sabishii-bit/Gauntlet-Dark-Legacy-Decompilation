"""fndiff gates the POOL-DEFECT banner on alignment (run 62 item 3a).

THE OBSERVATION (CP lane). `--clean` announced a wrong-constant defect over
a pairing the sequence matcher had merely CHOSEN:

    python tools/gdl/fndiff.py game/enemy/critter CritterCollidePlayers --clean
    POOL-DEFECT CritterCollidePlayers  (1 relocation row(s) ...)
        pool@0x7c  target lbl_80346490 = 0x3FF0000000000000 (f64 1.0)
                   ours @131 = 0x4008000000000000 (f64 3.0)
                   VALUES DIFFER (first at +0x0)

That function is count-asymmetric (target 150, ours 151), every `--ops` row
pairs T[n] with O[n+1], and the pool row is the FIRST line of a six-line
equal run that abuts an unpaired block: 1.0 and 3.0 are two different
instructions' constants, not one instruction's wrong constant.

THE DISCRIMINANT is adjacency to an unpaired block -- the rule run 39
already applied to the IMMEDIATE rows -- and NOT count asymmetry. Calibrated
over 255 configured units at 33f9f8da8 (build/c62_pool_align_census.py):
125 loud rows in 49 functions; 79 asymmetric / 46 symmetric; 108 at an edge
/ 17 interior. 7 asymmetric rows are interior (still evidence) and 36
symmetric rows are at an edge (still a guess), so neither half of that
split follows from the other.

The demoted rows are still printed in full, under a CANDIDATE banner.
"""
import io
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import fndiff  # noqa: E402
from tools.gdl.tests.test_pool_rows import (ADDR, OURS, OURS_VALUES,  # noqa: E402
                                            TARGET, VALUES)

# One instruction inserted on our side, then a short equal run carrying a
# pool row: the shape the matcher has to guess at.
ASYM_TARGET = [
    "lfs     f1,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349308",
    "fadds   f0,f1,f0",
    "lfd     f2,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349310",
    "blr",
]
ASYM_OURS = [
    "lfs     f1,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349308",
    "fadds   f0,f1,f0",
    "addi    r3,r3,0",
    "lfd     f2,0(0)",
    "    R_PPC_EMB_SDA21 @276",
    "blr",
]


class AdjacencyRule(unittest.TestCase):
    def test_the_pool_gate_uses_the_same_rule_as_the_immediate_gate(self):
        self.assertEqual(fndiff.POOL_ADJACENCY, fndiff.IMMEDIATE_ADJACENCY)

    def test_reliability_is_aligned_with_the_rows_it_describes(self):
        rows = fndiff.suppressed_pool_rows(TARGET, OURS)
        findings = fndiff.pool_row_findings(TARGET, OURS, ours_object=None)
        reasons = fndiff.pool_row_reliability(TARGET, OURS)
        self.assertEqual(len(reasons), len(rows))
        self.assertEqual(len(reasons), len(findings))

    def test_one_equal_run_with_no_unpaired_neighbour_is_all_evidence(self):
        # The recorded adsInitFromHeader prologue: nothing to guess at.
        self.assertEqual(fndiff.pool_row_reliability(TARGET, OURS),
                         [None, None, None])

    def test_a_row_beside_an_inserted_instruction_is_marked(self):
        reasons = fndiff.pool_row_reliability(ASYM_TARGET, ASYM_OURS)
        self.assertEqual(len(reasons), 1)
        self.assertIn("after an unpaired block", reasons[0])
        self.assertIn("drifted by", reasons[0])

    def test_a_wider_adjacency_marks_more_and_a_zero_marks_none(self):
        self.assertEqual(
            fndiff.pool_row_reliability(ASYM_TARGET, ASYM_OURS, adjacency=0),
            [None])
        self.assertTrue(
            fndiff.pool_row_reliability(ASYM_TARGET, ASYM_OURS,
                                        adjacency=99)[0])

    def test_an_exact_function_has_no_rows_to_judge(self):
        self.assertEqual(fndiff.pool_row_reliability(TARGET, TARGET), [])


class Banner(unittest.TestCase):
    def setUp(self):
        self._addr = fndiff._SYMBOL_ADDRESSES
        self._pool = fndiff._POOL_SYMBOLS
        self._target_bytes = fndiff.target_datum_bytes
        self._ours_bytes = fndiff.ours_datum_bytes
        fndiff._SYMBOL_ADDRESSES = dict(ADDR)
        fndiff._POOL_SYMBOLS = frozenset(VALUES)
        fndiff.target_datum_bytes = lambda sym: VALUES.get(sym.strip())
        fndiff.ours_datum_bytes = (
            lambda sym, obj: OURS_VALUES.get(sym.strip()))

    def tearDown(self):
        fndiff._SYMBOL_ADDRESSES = self._addr
        fndiff._POOL_SYMBOLS = self._pool
        fndiff.target_datum_bytes = self._target_bytes
        fndiff.ours_datum_bytes = self._ours_bytes

    def render(self, t, b, name="f", counts=None):
        findings = fndiff.pool_row_findings(t, b, ours_object=None)
        reasons = fndiff.pool_row_reliability(t, b)
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            confirmed = fndiff.print_pool_findings(name, findings, reasons,
                                                   counts=counts)
        return confirmed, buffer.getvalue(), findings, reasons

    def test_an_unreliable_row_is_a_candidate_not_a_defect(self):
        confirmed, text, findings, reasons = self.render(
            ASYM_TARGET, ASYM_OURS, "CritterCollidePlayers", counts=(150, 151))
        self.assertEqual(confirmed, 0)
        self.assertEqual(fndiff.pool_candidate_count(findings, reasons), 1)
        self.assertIn("POOL-DEFECT CANDIDATE CritterCollidePlayers", text)
        self.assertNotIn("POOL-DEFECT CritterCollidePlayers", text)
        self.assertIn("PAIRING UNRELIABLE", text)

    def test_a_candidate_still_prints_its_values_and_how_to_confirm(self):
        _confirmed, text, _f, _r = self.render(ASYM_TARGET, ASYM_OURS)
        self.assertIn("VALUES DIFFER", text)
        self.assertIn("65536.0", text.replace("(f64 ", ""))
        self.assertIn("--relocs", text)
        self.assertIn("--raw --diff", text)

    def test_count_asymmetry_is_context_beside_the_candidate(self):
        _confirmed, text, _f, _r = self.render(ASYM_TARGET, ASYM_OURS,
                                               counts=(150, 151))
        self.assertIn("COUNT-ASYMMETRIC (target 150, ours 151", text)
        _confirmed, equal_text, _f, _r = self.render(ASYM_TARGET, ASYM_OURS,
                                                     counts=(151, 151))
        self.assertNotIn("COUNT-ASYMMETRIC", equal_text)

    def test_an_anchored_defect_still_prints_the_original_verdict(self):
        # The negative side: the recorded true positive must not be demoted.
        confirmed, text, findings, reasons = self.render(
            TARGET, OURS, "adsInitFromHeader")
        self.assertEqual(confirmed, 2)
        self.assertEqual(fndiff.pool_candidate_count(findings, reasons), 0)
        self.assertIn("POOL-DEFECT adsInitFromHeader", text)
        self.assertNotIn("CANDIDATE", text)

    def test_without_a_reliability_list_nothing_changes(self):
        findings = fndiff.pool_row_findings(TARGET, OURS, ours_object=None)
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            confirmed = fndiff.print_pool_findings("adsInitFromHeader",
                                                   findings)
        self.assertEqual(confirmed, 2)
        self.assertNotIn("CANDIDATE", buffer.getvalue())

    def test_a_mismatched_reliability_list_is_refused_not_ignored(self):
        findings = fndiff.pool_row_findings(TARGET, OURS, ours_object=None)
        with self.assertRaisesRegex(ValueError, "does not match"):
            fndiff.print_pool_findings("f", findings, [None])


LIVE = (ROOT / "build/GUNE5D/obj/game/enemy/critter.o").is_file()


@unittest.skipUnless(LIVE, "needs the split target objects and a built tree")
class LiveCritter(unittest.TestCase):
    """The reported function, end to end through --clean's own printer."""

    def clean(self, unit, function):
        target = fndiff.parse(Path("build/%s/obj/%s.o"
                                   % (fndiff.VERSION, unit)))
        ours, _raw = fndiff.ours_object_path(unit)
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            fndiff.clean_diff(function, target[function],
                              fndiff.parse(ours)[function], ours_object=ours)
        return buffer.getvalue()

    def test_the_reported_row_is_now_a_candidate_naming_its_asymmetry(self):
        text = self.clean("game/enemy/critter", "CritterCollidePlayers")
        self.assertIn("POOL-DEFECT CANDIDATE CritterCollidePlayers", text)
        self.assertNotIn("POOL-DEFECT CritterCollidePlayers", text)
        self.assertIn("COUNT-ASYMMETRIC (target 150, ours 151", text)
        self.assertIn("CANDIDATES only", text)

    def test_an_interior_row_elsewhere_is_still_reported_as_a_defect(self):
        # game/game/player::set_hidden_player carries three interior rows
        # (runs of 36 lines) and one edge row -- both banners, one function.
        text = self.clean("game/game/player", "set_hidden_player")
        self.assertIn("POOL-DEFECT set_hidden_player", text)
        self.assertIn("POOL-DEFECT CANDIDATE set_hidden_player", text)
        self.assertIn("ADDRESSES DIFFER", text)


if __name__ == "__main__":
    unittest.main()
