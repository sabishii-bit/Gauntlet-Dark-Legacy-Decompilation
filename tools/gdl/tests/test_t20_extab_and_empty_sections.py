"""Exception-table visibility and explicit empty/missing measurement results.

TWO OBSERVATIONS, both reproduced verbatim before anything was designed.

(1) A COUNT-PARITY LOST verdict on a `-Cpp_exceptions` TU is also a DATA
    event, and probe never said so: the byte figure was reachable only by
    running defake_gate afterwards (`-84 B`, game/pb/dbgtext).

(2) `python tools/gdl/datadiff.py --sections game/pb/dbgtext` printed
    NOTHING AT ALL and exited 0 -- output indistinguishable from a mistyped
    unit or a broken tool.

R60 REFUTED the old absence claim in this test: its census depended on
datadiff's dot-only parser, which discarded extab/extabindex before counting.
The tested object header now explicitly includes those dotless names.
Report snapshots remain supplemental and need not reflect record order in
the direct function-indexed metadata comparison.

TWO-SIDED CALIBRATION at run-50 HEAD (scratch t20_empty_sections_census.py):
  (2) POSITIVE 78 of 257 units print NOTHING AT ALL today; NEGATIVE 178
      print rows and must be untouched, 1 prints SKIP for a missing object.
  (1) POSITIVE 95 report units carry an extab-family section; NEGATIVE the
      rest get no note at all, and a parity transition on them reads exactly
      as before.
"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import datadiff                                                # noqa: E402
import probe                                                   # noqa: E402


class ExtabNoteOnCountParity(unittest.TestCase):

    DBGTEXT = [("extab", 56, 100.0), ("extabindex", 84, 100.0)]

    def test_a_parity_loss_on_an_exceptions_tu_names_the_bytes(self):
        line = probe.count_class_line("T682/O682", "T682/O680", self.DBGTEXT)
        self.assertIn("COUNT-PARITY LOST", line)
        self.assertIn("DATA AT RISK", line)
        self.assertIn("140 bytes", line)        # 56 + 84
        self.assertIn("extabindex 84 B", line)

    def test_a_parity_gain_names_them_too(self):
        line = probe.count_class_line("T682/O680", "T682/O682", self.DBGTEXT)
        self.assertIn("COUNT-PARITY GAINED", line)
        self.assertIn("DATA AT RISK", line)

    def test_a_tu_without_exception_tables_reads_exactly_as_before(self):
        with_none = probe.count_class_line("T682/O682", "T682/O680", [])
        legacy = probe.count_class_line("T682/O682", "T682/O680")
        self.assertEqual(with_none, legacy)
        self.assertNotIn("DATA AT RISK", legacy)

    def test_zero_sized_sections_are_not_a_note(self):
        self.assertEqual(probe.extab_note([("extab", 0, 100.0)]), "")

    def test_no_parity_change_stays_silent_even_with_tables(self):
        # The banner is a TRANSITION report; the note must not turn it into
        # a line printed on every probe.
        self.assertEqual(
            probe.count_class_line("T682/O682", "T682/O682", self.DBGTEXT), "")

    def test_extab_at_risk_reads_the_report(self):
        report = Path(tempfile.mkdtemp(prefix="t20_extab_")) / "report.json"
        report.write_text(json.dumps({"units": [
            {"name": "main/game/pb/dbgtext",
             "sections": [{"name": ".text", "size": 3676},
                          {"name": "extab", "size": 56,
                           "fuzzy_match_percent": 100.0},
                          {"name": "extabindex", "size": 84,
                           "fuzzy_match_percent": 100.0}]}]}),
            encoding="utf-8")
        rows = probe.extab_at_risk("game/pb/dbgtext.c", report)
        self.assertEqual(rows, [("extab", 56, 100.0),
                                ("extabindex", 84, 100.0)])
        self.assertEqual(probe.extab_at_risk("game/nope/nothing", report), [])

    def test_a_missing_report_is_not_an_exception(self):
        self.assertEqual(probe.extab_at_risk("game/pb/dbgtext",
                                             "no/such/report.json"), [])


class SectionsNeverPrintsNothing(unittest.TestCase):
    """`fndiff.clean_diff`'s rule, applied to the tool that broke it:
    empty output can never mean success."""

    def setUp(self):
        self.dir = Path(tempfile.mkdtemp(prefix="t20_sections_"))
        self.cwd = os.getcwd()
        self._repo, self._report = datadiff.REPO, datadiff.REPORT

    def tearDown(self):
        datadiff.REPO, datadiff.REPORT = self._repo, self._report
        os.chdir(self.cwd)

    def test_dotless_exception_sections_are_detected(self):
        listing = ("  0 extab 00000098 00000000 00000000 00000040 2**2\n"
                   "  1 extabindex 000000e4 00000000 00000000 000000e0 2**2\n"
                   "  2 .text 00002e34 00000000 00000000 000001e0 2**2\n")
        with patch.object(datadiff, "dump_object", return_value=listing):
            sizes = datadiff.section_sizes("fixture.o")
        self.assertEqual(sizes, {"extab": 0x98, "extabindex": 0xE4, ".text": 0x2E34})

    def test_a_unit_with_no_data_sections_still_prints_a_verdict(self):
        import io
        from contextlib import redirect_stdout
        datadiff.REPORT = self.dir / "missing_report.json"
        buf = io.StringIO()
        with redirect_stdout(buf), patch.object(Path, "exists", return_value=True), \
                patch.object(datadiff, "section_sizes", return_value={}), \
                patch.object(datadiff, "parse_splits", return_value={}):
            datadiff.section_table("game/pb/dbgtext.c")
        text = buf.getvalue()
        self.assertTrue(text.strip(), "section_table printed nothing")
        self.assertIn("--sections:", text)

    def test_the_zero_compared_verdict_says_it_is_a_verdict(self):
        import io
        from contextlib import redirect_stdout
        datadiff.REPORT = self.dir / "missing_report.json"
        buf = io.StringIO()
        with redirect_stdout(buf), patch.object(Path, "exists", return_value=True), \
                patch.object(datadiff, "section_sizes", return_value={}), \
                patch.object(datadiff, "parse_splits", return_value={}):
            datadiff.section_table("game/pb/dbgtext.c")
        text = buf.getvalue()
        self.assertIn("NOTHING TO COMPARE", text)
        self.assertIn("not silence", text)

    def test_missing_object_refuses_instead_of_printing_a_zero_comparison(self):
        with patch.object(Path, "exists", return_value=False):
            with self.assertRaisesRegex(datadiff.MeasurementUnavailable, "missing"):
                datadiff.section_table("game/pb/dbgtext.c")


if __name__ == "__main__":
    unittest.main()
