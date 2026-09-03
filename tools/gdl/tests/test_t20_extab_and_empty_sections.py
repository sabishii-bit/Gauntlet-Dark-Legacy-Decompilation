"""Run-50 item 3: the exception-table byte figure, and datadiff's silence.

TWO OBSERVATIONS, both reproduced verbatim before anything was designed.

(1) A COUNT-PARITY LOST verdict on a `-Cpp_exceptions` TU is also a DATA
    event, and probe never said so: the byte figure was reachable only by
    running defake_gate afterwards (`-84 B`, game/pb/dbgtext).

(2) `python tools/gdl/datadiff.py --sections game/pb/dbgtext` printed
    NOTHING AT ALL and exited 0 -- output indistinguishable from a mistyped
    unit or a broken tool.

WHAT THE MEASUREMENT CHANGED IN THE DESIGN.  The obvious fix for (1) was to
compare the extab sections of the two OBJECTS: `datadiff.section_table` has
probed for them since it was written.  Measured image-wide over all 257
paired units, ZERO target objects and ZERO of ours carry an extab-family
section under ANY spelling -- exception tables are a LINK-level artifact and
live only in objdiff's report.  (The probe keys were also spelled `"extab"`
/ `"extabindex"` without the leading dot, which is splits.txt's spelling,
while `section_sizes` reads `objdump -h` and every key it can produce starts
with a dot -- so they were dead twice over.  They are removed, not
respelled.)

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

    def test_the_extab_probe_keys_are_gone_from_the_object_loop(self):
        # They could never match: section_sizes' keys all start with a dot,
        # and no object carries an extab section under any spelling.
        source = Path(datadiff.__file__).read_text(encoding="utf-8")
        self.assertNotIn('("extab", "extabindex",', source)

    def test_a_unit_with_no_data_sections_still_prints_a_verdict(self):
        import io
        from contextlib import redirect_stdout
        datadiff.REPORT = self.dir / "missing_report.json"
        buf = io.StringIO()
        with redirect_stdout(buf):
            # A unit whose objects do not exist takes the SKIP path, which
            # already printed; the zero-compared path is what was silent.
            datadiff.section_table("game/pb/dbgtext.c")
        text = buf.getvalue()
        self.assertTrue(text.strip(), "section_table printed nothing")
        self.assertIn("--sections:", text)

    def test_the_zero_compared_verdict_says_it_is_a_verdict(self):
        import io
        from contextlib import redirect_stdout
        datadiff.REPORT = self.dir / "missing_report.json"
        buf = io.StringIO()
        with redirect_stdout(buf):
            datadiff.section_table("game/pb/dbgtext.c")
        text = buf.getvalue()
        if "SKIP --sections" not in text:
            self.assertIn("NOTHING TO COMPARE", text)
            self.assertIn("not silence", text)


if __name__ == "__main__":
    unittest.main()
