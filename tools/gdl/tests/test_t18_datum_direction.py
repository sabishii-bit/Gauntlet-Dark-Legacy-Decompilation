#!/usr/bin/env python3
"""T18 run-48 item 2: defake_gate resolves relocation DATUMS before names.

THE DEFECT, from claim.law.PR_defake-gate-compares-relocation-names-while-
fndiff-relocs-resolves-them.20260903.v1, reproduced at c8cdf216d by driving
`_row_direction` with the record's own symbols (game/world/items is another
lane's claimed TU this run, so the source edit was NOT re-applied):

    resolve('sArrowFloorYOffset') = 0x80346FB0
    resolve('@433')               = None
    _row_direction(13, 'sArrowFloorYOffset', '@433', ...) -> away
      "target reloc[13] is 'sArrowFloorYOffset' (0x80346FB0) - our OLD symbol
       matched it, the new one does not"
    fndiff.target_datum_bytes('sArrowFloorYOffset') = 3fe0000000000000

i.e. an f64 0.5, which our `@433` also holds -- and on the same body
`fndiff --relocs` prints `relocation sets IDENTICAL (16 reloc(s), addresses
resolved)` and exits 0. The gate exited 1 and the integrator had to rule the
keep in by hand (work_claim.apply-rulings.20260903.v1: "the defake MOVED-AWAY
objection is the name-vs-datum defect (kind-equal-value rule)").

TWO-SIDED CALIBRATION at c8cdf216d, fndiff's own `pool_row_findings` over all
257 built unit pairs -- the class census, not a firing count:

    POOL-KIND-EQUAL       3,317 rows / 634 fns   -> now PASSES
    WRONG-POOL-VALUE         81 rows /  32 fns   -> still FAILS, now with a
                                                    VALUE reason instead of
                                                    DIRECTION-UNKNOWN
    POOL-KIND-UNDECIDED       0 rows             -> unchanged
    RENAME                   81 rows /  52 fns   -> untouched (both concrete)
    WRONG-POOL-DATUM         36 rows /  18 fns   -> untouched (both concrete)
    POOL-RENUMBER            24 rows /  10 fns   -> untouched (both anonymous)

Verified on LIVE objects afterwards, both directions:
  dolphin/demo/DEMOInit  lbl_80116BD8 (180 bytes) vs @33 (40 bytes)
      -> datum-equal, the granularity mismatch handled by _datum_prefix_equal
  game/anim/action       lbl_80115840 vs @1274 ("  SEQ:%s  frame:%.1f/%d   "
      versus "  SEQ:%s  frame:%.1f/%d") -> away, with the VALUE reason
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import defake_gate  # noqa: E402

HALF = bytes.fromhex("3fe0000000000000")      # f64 0.5
THREE = bytes.fromhex("4008000000000000")     # f64 3.0
TYPE = "R_PPC_EMB_SDA21"


class KindEqualValueRule(unittest.TestCase):
    """`pool_datum_direction` -- pure over the two byte strings."""

    def test_named_versus_anonymous_same_bytes_is_datum_equal(self):
        result = defake_gate.pool_datum_direction(
            "sArrowFloorYOffset", "@433", None,
            target_bytes=HALF, ours_bytes=HALF)
        self.assertIsNotNone(result)
        self.assertEqual(result[0], "datum-equal")
        self.assertIn("SAME datum", result[1])

    def test_named_versus_anonymous_differing_bytes_is_away(self):
        result = defake_gate.pool_datum_direction(
            "sNewtonThree", "@432", None,
            target_bytes=THREE, ours_bytes=HALF)
        self.assertEqual(result[0], "away")
        self.assertIn("BYTES DISAGREE", result[1])

    def test_granularity_mismatch_is_a_prefix_comparison(self):
        # dtk names a whole contiguous .rodata run with ONE lbl_ symbol while
        # our compiler emits each literal as its own @N. Measured live:
        # lbl_80116BD8 is 180 bytes, our @33 is 40.
        result = defake_gate.pool_datum_direction(
            "lbl_80116BD8", "@33", None,
            target_bytes=b"\x0aNow, try to find memory info file\x00" + b"X" * 40,
            ours_bytes=b"\x0aNow, try to find memory info file\x00")
        self.assertEqual(result[0], "datum-equal")

    def test_two_concrete_symbols_are_never_decided_by_value(self):
        # Same value at two ADDRESSES is two different data objects
        # (fndiff's WRONG-POOL-DATUM). Deciding those by bytes would let a
        # wrong-datum bug through, so the rule declines and the address
        # comparison keeps them.
        self.assertIsNone(defake_gate.pool_datum_direction(
            "sArrowFloorYOffset", "sNewtonHalf", None,
            target_bytes=HALF, ours_bytes=HALF))

    def test_two_anonymous_symbols_are_declined(self):
        # A pure pool renumber has no cross-object identity to decide.
        self.assertIsNone(defake_gate.pool_datum_direction(
            "@432", "@433", None, target_bytes=HALF, ours_bytes=HALF))

    def test_unreadable_bytes_fail_closed_to_the_name_comparison(self):
        # POOL-KIND-UNDECIDED: an unread measurement must never manufacture
        # a pass. 0 rows in the corpus census, and it is still the shape the
        # record's own row takes when no `ours` object is available.
        self.assertIsNone(defake_gate.pool_datum_direction(
            "sArrowFloorYOffset", "@433", None,
            target_bytes=HALF, ours_bytes=None))
        self.assertIsNone(defake_gate.pool_datum_direction(
            "sArrowFloorYOffset", "@433", None,
            target_bytes=None, ours_bytes=HALF))

    def test_none_is_an_answer_not_an_omission(self):
        # `None` is a REAL result from both byte readers, so it must not mean
        # "not supplied" — with None as the default, the assertion above
        # silently ran a LIVE symbols.txt lookup for sArrowFloorYOffset
        # (which succeeds) and the case went untested. UNREAD is the
        # not-supplied sentinel.
        self.assertIsNot(defake_gate.UNREAD, None)
        self.assertIsNone(defake_gate.pool_datum_direction(
            "sArrowFloorYOffset", "@433", None,
            target_bytes=None, ours_bytes=None))


def _rows(index, cur_sym, target_sym, count):
    target = [(TYPE, f"pad{i}") for i in range(count)]
    target[index] = (TYPE, target_sym)
    ours = [(TYPE, f"pad{i}") for i in range(count)]
    ours[index] = (TYPE, cur_sym)
    return ours, target


class RowDirection(unittest.TestCase):
    """`_row_direction` with the datum rule wired in front of the names."""

    def setUp(self):
        self._target = defake_gate.fndiff.target_datum_bytes
        self._ours = defake_gate.fndiff.ours_datum_bytes

    def tearDown(self):
        defake_gate.fndiff.target_datum_bytes = self._target
        defake_gate.fndiff.ours_datum_bytes = self._ours

    def test_the_records_row_now_reads_datum_equal(self):
        defake_gate.fndiff.target_datum_bytes = lambda sym: HALF
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: HALF
        ours, target = _rows(13, "@433", "sArrowFloorYOffset", 16)
        direction, detail = defake_gate._row_direction(
            13, "sArrowFloorYOffset", "@433", ours, target,
            defake_gate.resolve_symbol, ours_object="anything")
        self.assertEqual(direction, "datum-equal")
        self.assertIn("target reloc[13] is 'sArrowFloorYOffset'", detail)

    def test_without_an_ours_object_it_is_still_away(self):
        # Fail-closed: with no object to read, the name comparison decides
        # exactly as it did before. This is the verbatim pre-fix output.
        defake_gate.fndiff.target_datum_bytes = lambda sym: HALF
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: None
        ours, target = _rows(13, "@433", "sArrowFloorYOffset", 16)
        direction, detail = defake_gate._row_direction(
            13, "sArrowFloorYOffset", "@433", ours, target,
            defake_gate.resolve_symbol, ours_object=None)
        self.assertEqual(direction, "away")
        self.assertIn("our OLD symbol matched it, the new one does not",
                      detail)

    def test_a_wrong_pool_value_row_is_away_with_a_value_reason(self):
        defake_gate.fndiff.target_datum_bytes = lambda sym: THREE
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: HALF
        ours, target = _rows(3, "@432", "sNewtonThree", 8)
        direction, detail = defake_gate._row_direction(
            3, "sNewtonThree", "@432", ours, target,
            defake_gate.resolve_symbol, ours_object="anything")
        self.assertEqual(direction, "away")
        self.assertIn("BYTES DISAGREE", detail)
        # The reason names both VALUES, which the name comparison could not:
        # it had only "neither the old nor the new symbol matches it".
        self.assertIn("f64 3.0", detail)
        self.assertIn("f64 0.5", detail)


class ChangeDirection(unittest.TestCase):
    """`relocation_change_direction` aggregation over rows."""

    def setUp(self):
        self._target = defake_gate.fndiff.target_datum_bytes
        self._ours = defake_gate.fndiff.ours_datum_bytes
        defake_gate.fndiff.target_datum_bytes = lambda sym: HALF
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: HALF

    def tearDown(self):
        defake_gate.fndiff.target_datum_bytes = self._target
        defake_gate.fndiff.ours_datum_bytes = self._ours

    def test_all_rows_datum_equal_passes(self):
        base = [(TYPE, "sA"), (TYPE, "sB")]
        cur = [(TYPE, "@1"), (TYPE, "@2")]
        target = [(TYPE, "sA"), (TYPE, "sB")]
        direction, _detail = defake_gate.relocation_change_direction(
            base, cur, target, ours_object="anything")
        self.assertEqual(direction, "datum-equal")

    def test_a_repair_plus_a_respelling_is_still_a_repair(self):
        # `toward` outranks `datum-equal`: the change fixed something and
        # re-spelled something else. Both names here resolve in the live
        # symbols.txt, which is what the name half of the check needs.
        base = [(TYPE, "sNewtonThree"), (TYPE, "sB")]
        cur = [(TYPE, "sArrowFloorYOffset"), (TYPE, "@2")]
        target = [(TYPE, "sArrowFloorYOffset"), (TYPE, "sB")]
        direction, _detail = defake_gate.relocation_change_direction(
            base, cur, target, ours_object="anything")
        self.assertEqual(direction, "toward")

    def test_away_still_dominates_everything(self):
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: (
            THREE if sym == "@2" else HALF)
        base = [(TYPE, "sA"), (TYPE, "sB")]
        cur = [(TYPE, "@1"), (TYPE, "@2")]
        target = [(TYPE, "sA"), (TYPE, "sB")]
        direction, _detail = defake_gate.relocation_change_direction(
            base, cur, target, ours_object="anything")
        self.assertEqual(direction, "away")


class CompareVerdict(unittest.TestCase):
    """The verdict `compare` emits, and that it is not a regression."""

    def setUp(self):
        self._target = defake_gate.fndiff.target_datum_bytes
        self._ours = defake_gate.fndiff.ours_datum_bytes
        defake_gate.fndiff.target_datum_bytes = lambda sym: HALF
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: HALF

    def tearDown(self):
        defake_gate.fndiff.target_datum_bytes = self._target
        defake_gate.fndiff.ours_datum_bytes = self._ours

    def _row(self, relocs):
        return {"status": "OK", "real": 0, "bytes": "h", "words": "w",
                "relocs": [list(r) for r in relocs]}

    def test_kind_equal_value_is_reloc_datum_equal_not_a_regression(self):
        base = {"f": self._row([(TYPE, "sArrowFloorYOffset")])}
        cur = {"f": dict(self._row([(TYPE, "@433")]), bytes="h2")}
        verdicts = defake_gate.compare(
            base, cur, target_relocs={"f": [(TYPE, "sArrowFloorYOffset")]},
            ours_object="anything")
        self.assertEqual([v[1] for v in verdicts], ["RELOC-DATUM-EQUAL"])
        self.assertIn("KIND-EQUAL-VALUE", verdicts[0][2])
        self.assertNotIn("REGRESSION", [v[1] for v in verdicts])

    def test_the_verdict_is_bankable_by_update_improved(self):
        # RELOC-TOWARD-TARGET and NAMING-DRIFT leave the baseline holding the
        # OLD symbols and re-report on every later check unless they are in
        # the bankable set; this verdict has the same shape.
        self.assertIn("RELOC-DATUM-EQUAL",
                      Path(defake_gate.__file__).read_text(encoding="utf-8"))
        source = Path(defake_gate.__file__).read_text(encoding="utf-8")
        bankable = source.split('improved = [v for v in verdicts')[1][:200]
        self.assertIn("RELOC-DATUM-EQUAL", bankable)

    def test_a_differing_value_stays_a_regression(self):
        defake_gate.fndiff.ours_datum_bytes = lambda sym, obj: THREE
        base = {"f": self._row([(TYPE, "sArrowFloorYOffset")])}
        cur = {"f": dict(self._row([(TYPE, "@433")]), bytes="h2")}
        verdicts = defake_gate.compare(
            base, cur, target_relocs={"f": [(TYPE, "sArrowFloorYOffset")]},
            ours_object="anything")
        self.assertEqual([v[1] for v in verdicts], ["REGRESSION"])
        self.assertIn("MOVED-AWAY-FROM-TARGET", verdicts[0][2])


if __name__ == "__main__":
    unittest.main()
