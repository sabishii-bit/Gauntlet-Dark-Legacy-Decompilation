"""fndiff --clean: the pool rows the view normalizes away (run 41 item 3).

`normalized_reloc_lines` collapses a dtk-named pool datum (lbl_ADDR) and our
object's anonymous compiler pool entry (@N) to the SAME "<local>" token, so a
row where the two sides read different data diffs to nothing. That row was
the whole mechanism of run-40's STRICT close: adsInitFromHeader's prologue
loaded four callee-saved FPRs from four constants, ours took two of them from
the wrong entries, and `fndiff --clean` printed
"MATCH (pool-name noise only) ... (+8 pool-name lines suppressed)" over it
(attempt.NM_adsinitfromheader-extern-scaffold-and-statement-order-close
.20260902.v1).

The reporting must never move a score: "naming a pool constant must never
change a score" is why the normalization exists, and the calibration census
found 3,222 kind-differing rows that are perfectly benign against 45
wrong-datum and 177 wrong-value rows.
"""

import io
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fndiff  # noqa: E402


# The recorded adsInitFromHeader prologue, target and ours-before-the-fix,
# verbatim from the law record's WORKED PROOF. Instruction words are
# identical on both sides -- only the relocation symbols differ, which is
# exactly the case --clean scored as zero.
TARGET = [
    "lfd     f28,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349318",
    "lfs     f29,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349308",
    "lfd     f30,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349320",
    "lfd     f31,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349310",
]
OURS = [
    "lfd     f28,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349310",
    "lfs     f29,0(0)",
    "    R_PPC_EMB_SDA21 lbl_80349308",
    "lfd     f30,0(0)",
    "    R_PPC_EMB_SDA21 @273",
    "lfd     f31,0(0)",
    "    R_PPC_EMB_SDA21 @276",
]

# Values at those addresses, read out of the retail image in the record.
VALUES = {
    "lbl_80349308": bytes.fromhex("46FA0000"),                # 32000.0f
    "lbl_80349310": bytes.fromhex("40F0000000000000"),        # 65536.0
    "lbl_80349318": bytes.fromhex("4330000000000000"),        # u32 bias
    "lbl_80349320": bytes.fromhex("4330000080000000"),        # s32 bias
}
# The record does not state which generated bias got which @-number before
# the fix, so the two anonymous entries below are assigned from the same
# four retail constants to exercise BOTH kind-differing outcomes: @273 holds
# what the target's row holds (benign), @276 does not (the defect).
OURS_VALUES = {
    "@273": bytes.fromhex("4330000080000000"),                # s32 bias
    "@276": bytes.fromhex("4330000000000000"),                # u32 bias
}
ADDR = {name: int(name[4:], 16) for name in VALUES}


class PoolRowPairing(unittest.TestCase):
    def test_every_differing_row_is_paired_and_scored_zero(self):
        rows = fndiff.suppressed_pool_rows(TARGET, OURS)
        self.assertEqual(
            [(row[1], row[2]) for row in rows],
            [("lbl_80349318", "lbl_80349310"),
             ("lbl_80349320", "@273"),
             ("lbl_80349310", "@276")])
        # ...and the --clean text really does score them as nothing: the
        # normalized lines are identical, which is the symptom.
        self.assertEqual(fndiff.normalized_reloc_lines(TARGET),
                         fndiff.normalized_reloc_lines(OURS))

    def test_instruction_offsets_follow_the_owning_instruction(self):
        rows = fndiff.suppressed_pool_rows(TARGET, OURS)
        offsets = fndiff._instruction_offsets(TARGET)
        self.assertEqual([offsets[row[0]] for row in rows], [0x0, 0x8, 0xC])


class PoolRowClassification(unittest.TestCase):
    def setUp(self):
        self._addr = fndiff._SYMBOL_ADDRESSES
        self._pool = fndiff._POOL_SYMBOLS
        fndiff._SYMBOL_ADDRESSES = dict(ADDR)
        # symbols.txt lists these lbl_ entries as .sdata2 objects, which is
        # what makes relocation_signature collapse them to "<local>" beside
        # our anonymous @N -- i.e. what makes --clean score the row as zero.
        fndiff._POOL_SYMBOLS = frozenset(VALUES)
        self._target_bytes = fndiff.target_datum_bytes
        self._ours_bytes = fndiff.ours_datum_bytes
        fndiff.target_datum_bytes = lambda sym: VALUES.get(sym.strip())
        fndiff.ours_datum_bytes = (
            lambda sym, obj: OURS_VALUES.get(sym.strip()))

    def tearDown(self):
        fndiff._SYMBOL_ADDRESSES = self._addr
        fndiff._POOL_SYMBOLS = self._pool
        fndiff.target_datum_bytes = self._target_bytes
        fndiff.ours_datum_bytes = self._ours_bytes

    def test_the_run40_prologue_reports_both_defect_shapes(self):
        findings = fndiff.pool_row_findings(TARGET, OURS, ours_object=None)
        self.assertEqual(
            [(row[0], row[1]) for row in findings],
            [("WRONG-POOL-DATUM", 0x0),      # lbl_...318 vs lbl_...310
             ("POOL-KIND-EQUAL", 0x8),       # lbl_...320 vs @273, same bytes
             ("WRONG-POOL-VALUE", 0xC)])     # 65536.0 vs the s32 bias

    def test_defect_rows_print_and_benign_rows_are_only_counted(self):
        findings = fndiff.pool_row_findings(TARGET, OURS, ours_object=None)
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            loud = fndiff.print_pool_findings("adsInitFromHeader", findings)
        text = buffer.getvalue()
        self.assertEqual(loud, 2)
        self.assertIn("POOL-DEFECT adsInitFromHeader", text)
        self.assertIn("pool@0x0", text)
        self.assertIn("ADDRESSES DIFFER", text)
        self.assertIn("VALUES DIFFER", text)
        self.assertNotIn("POOL-KIND-EQUAL", text)
        self.assertEqual(fndiff.pool_findings_note(findings),
                         "1 POOL-KIND-EQUAL")

    def test_reporting_does_not_change_the_score(self):
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            fndiff.clean_diff("adsInitFromHeader", TARGET, OURS)
        summary = [line for line in buffer.getvalue().splitlines()
                   if line.startswith("== ")]
        self.assertEqual(len(summary), 1)
        # The scored quantity is untouched: still 0 real diff lines, still
        # the MATCH status every consumer's regex parses.
        self.assertIn("MATCH (pool-name noise only), 0 real diff lines",
                      summary[0])
        self.assertIn("suppressed pool row(s) name DIFFERENT data",
                      summary[0])


class PoolValueGranularity(unittest.TestCase):
    """dtk names a whole .rodata run with ONE symbol; we emit per-literal."""

    def test_shorter_entry_that_is_a_prefix_is_the_same_constant(self):
        blob = b"\nNow, try to find memory info file...\n\n\x00"
        self.assertTrue(fndiff._datum_prefix_equal(blob, blob[:0x18]))
        self.assertTrue(fndiff._datum_prefix_equal(blob[:0x18], blob))

    def test_a_genuinely_different_string_still_differs(self):
        self.assertFalse(fndiff._datum_prefix_equal(
            b"AtreeNodeInit: Malloc't return 0 (too many nodes).\x00",
            b"AtreeNodeInit: Malloc't returned NULL.\x00"))

    def test_empty_or_unreadable_data_is_never_called_equal(self):
        self.assertFalse(fndiff._datum_prefix_equal(b"", b"\x00\x00\x00\x00"))
        self.assertFalse(fndiff._datum_prefix_equal(None, b"\x00"))

    def test_long_values_render_a_window_at_the_first_difference(self):
        target = b"AtreeNodeInit: Malloc't return 0 (too many nodes).\x00"
        rendered = fndiff._render_value(target, 0x21)
        self.assertIn("turn 0 (too many nodes)", rendered)
        self.assertNotIn("0x41747265", rendered)


class PoolRowsAgainstOtherViews(unittest.TestCase):
    def test_two_anonymous_entries_are_pool_renumbering_not_a_defect(self):
        target = ["lfd     f1,0(0)", "    R_PPC_EMB_SDA21 @13"]
        ours = ["lfd     f1,0(0)", "    R_PPC_EMB_SDA21 @11"]
        findings = fndiff.pool_row_findings(target, ours, ours_object=None)
        self.assertEqual([row[0] for row in findings], ["POOL-RENUMBER"])
        self.assertEqual(fndiff.print_pool_findings("f", findings), 0)

    def test_an_exact_function_reports_no_pool_rows_at_all(self):
        self.assertEqual(fndiff.pool_row_findings(TARGET, TARGET, None), [])


if __name__ == "__main__":
    unittest.main()
