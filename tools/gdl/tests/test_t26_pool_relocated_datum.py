"""Run-56 item 6: a jump table cannot be byte-compared across the link.

THE REPORTED OBSERVATION (PJ, run 55): "a jump-table @N renumber is reported
as a WRONG-POOL-VALUE regression ... our zeros are relocation placeholders in
the object; the target side is linked bytes".

IT DOES NOT REPRODUCE AT HEAD, and the measurement says so precisely: over
all 257 paired units, `pool_row_findings` produces 76 WRONG-POOL-VALUE rows
and NONE of them is a relocated datum (0 by the relocation table, and the
single row the shape heuristic flags is a false positive, below). The
MECHANISM is real anyway and was one pooled jump table away from firing:
`pool_row_findings` compared our UNLINKED object's bytes against the LINKED
retail DOL with no relocation check at all, while `--datum` has had a guard
for exactly this since `is_pointer_table` was written. One hazard, two
layers, a guard in only one -- the shape AGENTS.md discipline 18 exists for.

TWO-SIDED CALIBRATION at 28c884db3, over all 257 unit pairs:
  POSITIVE  0 pool rows have a relocated datum today, so shipping the guard
            reclassifies NOTHING and can hide nothing. Subjects exist: in
            `build/GUNE5D/src/game/ui/auxscreen.o` the anonymous entries
            `@164` and `@165` ARE relocation-covered .data tables, which is
            what the guard is for the moment one of them pairs with a named
            target datum.
  NEGATIVE  all 76 WRONG-POOL-VALUE rows and all 3,432 POOL-KIND-EQUAL rows
            keep their class exactly.

DESIGN REVERSAL, and the reason this is not the obvious one-liner. The
obvious cure is to call the existing `is_pointer_table` from
`pool_row_findings`. Measured, it fires on exactly ONE of the 76 rows --
game/ui/auxscreen::CaptionTextSub, where OUR `@355` holds
`0000000000000000` against the target's `403e000000000000`, an f64 0.0
where retail has 30.0. That is a genuine wrong constant, and the heuristic
calls it benign because `not any(blob)` treats every all-zero datum as a
pointer table. The obvious cure would have hidden a real defect and fixed
nothing measurable. The shipped discriminant is the object's own relocation
table.
"""
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS))

import fndiff                                                    # noqa: E402

AUXSCREEN = REPO / "build" / "GUNE5D" / "src" / "game" / "ui" / "auxscreen.o"


class RelocationEvidence(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        if not AUXSCREEN.exists():
            raise unittest.SkipTest("auxscreen.o not built in this tree")

    def test_a_relocated_data_table_is_reported_relocated(self):
        for symbol in ("@164", "@165"):
            self.assertIs(fndiff.datum_is_relocated(symbol, AUXSCREEN), True,
                          symbol)

    def test_a_plain_constant_pool_entry_is_not(self):
        for symbol in ("@218", "@219", "@355"):
            self.assertIs(fndiff.datum_is_relocated(symbol, AUXSCREEN), False,
                          symbol)

    def test_an_unknown_symbol_is_undecidable_not_false(self):
        """`None` and `False` mean different things and a guard must not
        confuse a symbol it cannot find with one it cleared."""
        self.assertIsNone(fndiff.datum_is_relocated("lbl_80345A80", AUXSCREEN))
        self.assertIsNone(fndiff.datum_is_relocated("@164", None))

    def test_relocations_are_read_per_section(self):
        table = fndiff.object_relocation_offsets(AUXSCREEN)
        self.assertIn(".data", table)
        self.assertIn(".text", table)
        self.assertIn(0, table[".data"])
        self.assertNotIn(".sdata2", table)


class TheShapeHeuristicIsNotTheDiscriminant(unittest.TestCase):
    """Pin the false positive that vetoed the obvious design."""

    def test_an_all_zero_f64_is_called_a_pointer_table_by_shape(self):
        self.assertTrue(fndiff.is_pointer_table(bytes(8), "@355"))

    def test_but_it_is_a_real_constant_disagreement(self):
        """0.0 where retail has 30.0 -- the row the heuristic would hide."""
        target = bytes.fromhex("403e000000000000")
        self.assertFalse(fndiff._datum_prefix_equal(target, bytes(8)))

    def test_the_relocation_discriminant_does_not_clear_it(self):
        if not AUXSCREEN.exists():
            self.skipTest("auxscreen.o not built in this tree")
        self.assertIs(fndiff.datum_is_relocated("@355", AUXSCREEN), False)


class PoolRowClasses(unittest.TestCase):

    def test_the_new_class_is_benign_and_counted(self):
        self.assertNotIn("POOL-RELOCATED", fndiff.LOUD_POOL_CLASSES)
        note = fndiff.pool_findings_note(
            [("POOL-RELOCATED", 0, "lbl_1", "@1", b"\x80\x00\x00\x00",
              bytes(4))])
        self.assertIn("POOL-RELOCATED", note)

    def test_a_relocated_row_never_prints_as_a_defect(self):
        loud = fndiff.print_pool_findings(
            "fake", [("POOL-RELOCATED", 0, "lbl_1", "@1",
                      b"\x80\x00\x00\x00", bytes(4))])
        self.assertEqual(loud, 0)


if __name__ == "__main__":
    unittest.main()
