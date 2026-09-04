"""T25 run-55 item 6: a real win that LOSES COUNT PARITY is not bare IMPROVED.

REPORTED (MC, run 54): "defake_gate arbitrates regressions but reports
improvements bare ... IMPROVED writeGauntletSave real 137 -> 108
unqualified - that row is a fuzzy regression and a parity loss".

The existing IMPROVED-CARRIER arbiter cannot reach that row: it REQUIRES
`base.ti == cur.ti and base.bi == cur.bi`, i.e. counts that did not move,
and a parity loss is precisely counts that did. CALIBRATED at f0b1b3fe1
over the 3,032 built pairs: 2,918 are at parity (the population that can
lose it) and 114 are already off it (the population a cruder `ti != bi`
test would false-flag, and which the gap comparison excludes).

`compare` is IMPORTABLE CORE and pure over two roster dicts, so these run
with no build.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from defake_gate import compare  # noqa: E402


def roster(real, ti, bi, opset="A", status="OPERAND_DIFF"):
    return {"fn": {"status": status, "real": real, "ti": ti, "bi": bi,
                   "opset": opset}}


class ParityQualifierTests(unittest.TestCase):
    def kind(self, base, cur):
        rows = [row for row in compare(base, cur) if row[0] == "fn"]
        self.assertEqual(len(rows), 1, rows)
        return rows[0][1], rows[0][2]

    def test_a_real_win_that_loses_parity_is_qualified(self):
        kind, detail = self.kind(roster(137, 190, 190),
                                 roster(108, 190, 191))
        self.assertEqual(kind, "IMPROVED-PARITY-LOST")
        self.assertIn("190/190 -> 190/191", detail)
        self.assertIn("COUNT PARITY LOST", detail)

    def test_a_real_win_at_held_parity_is_a_plain_improvement(self):
        kind, detail = self.kind(roster(137, 190, 190),
                                 roster(108, 190, 190))
        self.assertEqual(kind, "IMPROVED")
        self.assertEqual(detail, "real 137 -> 108")

    def test_a_function_already_off_parity_is_not_flagged(self):
        """The 114 rows a bare `ti != bi` test would eat: the gap did not
        widen, so the edit did not lose anything."""
        kind, _detail = self.kind(roster(137, 190, 191),
                                  roster(108, 190, 191))
        self.assertEqual(kind, "IMPROVED")

    def test_regaining_parity_is_not_flagged(self):
        kind, _detail = self.kind(roster(137, 190, 191),
                                  roster(108, 190, 190))
        self.assertEqual(kind, "IMPROVED")

    def test_the_carrier_arbiter_still_fires_at_equal_counts(self):
        kind, _detail = self.kind(roster(137, 190, 190, opset="A"),
                                  roster(108, 190, 190, opset="B"))
        self.assertEqual(kind, "IMPROVED-CARRIER")

    def test_parity_is_checked_before_the_carrier(self):
        """Both conditions cannot hold at once — the carrier needs counts
        that did not move — but the ORDER is what guarantees it."""
        kind, _detail = self.kind(roster(137, 190, 190, opset="A"),
                                  roster(108, 190, 192, opset="B"))
        self.assertEqual(kind, "IMPROVED-PARITY-LOST")

    def test_missing_counts_fall_back_to_a_plain_improvement(self):
        kind, _detail = self.kind(roster(137, None, None),
                                  roster(108, None, None))
        self.assertEqual(kind, "IMPROVED")

    def test_a_regression_is_untouched(self):
        kind, _detail = self.kind(roster(108, 190, 190),
                                  roster(137, 190, 191))
        self.assertEqual(kind, "REGRESSION")


if __name__ == "__main__":
    unittest.main()
