"""Run-58 item 7: `matchtool probe` printed rows nobody could tell apart.

THE OBSERVATION (EN lane). A `probe` of game/enemy/enemy prints one row per
preset and a `best:` line, and some of those rows are byte-identical to each
other. A preset that scores exactly what the row above it scored carries no
evidence about the flag that separates them -- `demo` and `demo_nox` differ
only in `-Cpp_exceptions`, so on a unit whose functions raise nothing the two
rows are the same row twice. Nothing in the output said so, and finding it
out meant diffing 84-column lines by eye.

REPRODUCED, WITH THE REPORTED NUMBER CORRECTED. The brief said "12 rows that
are provably indistinguishable for the unit (all six `demo` presets tie;
1.2.5 == 1.2.5n)". Measured at f0c613686:

  python tools/gdl/matchtool.py probe game/enemy/enemy
      -> 10 of 16 rows distinct, 6 duplicates in 4 classes:
         demo_nox@1.2.5 == demo@1.2.5
         demo_nox@1.2.5n == demo@1.2.5n
         sdk_x@1.2.5n, sdk_ro@1.2.5n == sdk@1.2.5n
         sdk_x@1.2.5, sdk_ro@1.2.5 == sdk@1.2.5

  python tools/gdl/matchtool.py probe game/enemy/enemy --fn closest_enemy
      -> 4 of 16 distinct, 12 duplicates -- the reported 12, so the brief was
         quoting a SINGLE-FUNCTION probe as a whole-unit fact.

Neither of the brief's two characterisations holds. The six `demo` presets do
NOT tie: `demo_i1` separates from `demo` on the whole unit (fn_80046680 68 vs
48, damage_enemy 94 vs 44). 1.2.5 and 1.2.5n do NOT tie either, on the unit or
on closest_enemy (32 vs 34) -- the compiler revision is one of the axes this
unit DOES discriminate, and calling it indistinguishable would have retired a
live discriminator. The underlying defect -- redundant rows with no summary --
is real and is what the summary line reports.

TWO-SIDED CALIBRATION over live probes at f0c613686:
  game/enemy/enemy               10 of 16 distinct,  6 duplicates
  game/enemy/enemy --fn ...       4 of 16 distinct, 12 duplicates
  game/sys/gutil (4 fns, all OK)  1 of 16 distinct, 15 duplicates
The gutil row is the saturated end and the reason the line prints the COUNT
first: on a finished TU every preset ties, and "best: sdk@1.2.5n" there is an
alphabetical accident, not a measurement.
"""
import sys
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GDL))

import matchtool  # noqa: E402


FNS = ["a", "b", "c"]


def rows(**labelled):
    """[(label, {fn: cell})] in the order given."""
    return [(label, dict(zip(FNS, cells)))
            for label, cells in labelled.items()]


class PresetEquivalence(unittest.TestCase):

    def test_identical_rows_group_under_the_first_of_them(self):
        table = rows(demo=("OK", 32, "OK"),
                     demo_nox=("OK", 32, "OK"),
                     sdk=("OK", 44, "OK"))
        self.assertEqual(matchtool.preset_equivalence(table, FNS),
                         [("demo", ["demo_nox"]), ("sdk", [])])

    def test_a_single_differing_cell_breaks_the_tie(self):
        # THE INVALID INPUT. A grouper that compared anything coarser than
        # the whole row would fold these two, and retire a live
        # discriminator: 1.2.5 vs 1.2.5n differs on enemy::closest_enemy in
        # exactly one column (32 vs 34).
        table = rows(**{"demo@1.2.5": ("OK", 32, "OK"),
                        "demo@1.2.5n": ("OK", 34, "OK")})
        self.assertEqual(matchtool.preset_equivalence(table, FNS),
                         [("demo@1.2.5", []), ("demo@1.2.5n", [])])

    def test_every_row_distinct_reports_no_duplicates(self):
        table = rows(a=("OK", 1, "OK"), b=("OK", 2, "OK"), c=("OK", 3, "OK"))
        summary = matchtool.distinguishable_summary(table, FNS)
        self.assertIn("3 of 3 row(s) carry a distinct result", summary)
        self.assertIn("every row differs from every other", summary)
        self.assertNotIn("duplicate", summary)

    def test_the_summary_names_each_duplicate_against_its_row(self):
        table = rows(demo=("OK", 32, "OK"),
                     demo_nox=("OK", 32, "OK"),
                     sdk=("OK", 44, "OK"),
                     sdk_x=("OK", 44, "OK"),
                     sdk_ro=("OK", 44, "OK"))
        summary = matchtool.distinguishable_summary(table, FNS)
        self.assertIn("2 of 5 row(s) carry a distinct result", summary)
        self.assertIn("3 duplicate row(s)", summary)
        self.assertIn("demo_nox == demo", summary)
        self.assertIn("sdk_x,sdk_ro == sdk", summary)

    def test_the_verdict_is_taken_over_the_displayed_columns_only(self):
        # `--fn` narrows the table, and the claim narrows with it: on the
        # whole unit these two rows differ, on the printed single column they
        # do not, and the summary must say what the reader can see. This is
        # the measured difference between the run-58 brief's 12 and the
        # whole-unit 6.
        table = rows(demo=("OK", 32, "OK"), demo_i1=("OK", 32, 94))
        self.assertEqual(matchtool.preset_equivalence(table, FNS),
                         [("demo", []), ("demo_i1", [])])
        self.assertEqual(matchtool.preset_equivalence(table, ["a", "b"]),
                         [("demo", ["demo_i1"])])

    def test_an_absent_column_is_compared_as_absent_not_skipped(self):
        # A preset whose object lacks the function must not silently tie a
        # preset that produced it.
        table = [("demo", {"a": "OK", "b": 32}), ("sdk", {"a": "OK"})]
        self.assertEqual(matchtool.preset_equivalence(table, ["a", "b"]),
                         [("demo", []), ("sdk", [])])

    def test_no_scored_rows_prints_nothing(self):
        self.assertEqual(matchtool.distinguishable_summary([], FNS), "")

    def test_one_row_is_distinct_by_itself(self):
        table = rows(demo=("OK", 32, "OK"))
        self.assertIn("1 of 1 row(s) carry a distinct result",
                      matchtool.distinguishable_summary(table, FNS))


if __name__ == "__main__":
    unittest.main()
