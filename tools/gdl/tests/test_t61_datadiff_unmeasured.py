"""datadiff renders an unmeasured EH comparison and a stale snapshot (item 7).

THE OBSERVATION (T3/ER2). `--sections` printed
`UNRESOLVED ... missing=0 changed=0 extra=0` -- three confident zeros beside
`target_records=unreadable`, for a comparison that did not happen -- and it
printed report.json's extab percentages beside the live binding verdict with
a fixed "may be stale" caveat that is equally true in both states.

The defect is in the caller, not the measurement: `exception_table` returns
`{'schema_version': 1, 'status': 'UNRESOLVED', 'error': "[Errno 2] ..."}`
with NO count keys when it cannot read an object, and the old line's
`len(eh.get("missing", []))` turned each absent key into a confident zero:

    [unit] exception metadata: UNRESOLVED target_records=unreadable
           ours_records=unreadable missing=0 changed=0 extra=0

`missing=0` reads as "nothing is missing", which is exactly what was NOT
established. The live population has no error-branch row: every UNRESOLVED
in it carries REAL measured counts and must keep them, which is the negative
side below. Re-measured on the native-only tree at 9626c9bb4 with
`python tools/gdl/datadiff.py --sections <unit>`:

    [game/mb/mb_objects.c]     UNRESOLVED target_records=16 ours_records=17
                               missing=0 changed=0 extra=1
    [dolphin/demo/DEMOInit.c]  UNRESOLVED target_records=5 ours_records=11
                               missing=0 changed=0 extra=6
"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

# datadiff.ours_object imports `raw_object` unqualified, so tools/gdl must be
# importable the way the CLI has it.
TOOLS = Path(__file__).resolve().parent.parent
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from tools.gdl import datadiff  # noqa: E402


class ExceptionCounts(unittest.TestCase):
    def test_an_unreadable_comparison_renders_every_count_as_n_a(self):
        line = datadiff.exception_counts_line(
            {"schema_version": 1, "status": "UNRESOLVED", "error": "boom"})
        self.assertEqual(
            line, "target_records=n/a ours_records=n/a missing=n/a"
                  " changed=n/a extra=n/a")
        self.assertNotIn("=0", line)

    def test_a_measured_zero_is_still_printed_as_zero(self):
        line = datadiff.exception_counts_line(
            {"status": "PASS", "target_records": 12, "ours_records": 12,
             "missing": [], "changed": {}, "extra": {}})
        self.assertEqual(
            line, "target_records=12 ours_records=12 missing=0 changed=0"
                  " extra=0")

    def test_a_measured_unresolved_keeps_its_real_counts(self):
        # The live rows this must not damage: mb_objects (extra 1) and
        # DEMOInit (extra 6), both re-measured at 9626c9bb4.
        line = datadiff.exception_counts_line(
            {"status": "UNRESOLVED", "target_records": 16, "ours_records": 17,
             "missing": [], "changed": {}, "extra": {"fn": 1}})
        self.assertEqual(
            line, "target_records=16 ours_records=17 missing=0 changed=0"
                  " extra=1")

    def test_a_partially_populated_result_mixes_numbers_and_n_a(self):
        line = datadiff.exception_counts_line(
            {"status": "FAIL", "target_records": 5,
             "changed": {"a": 1, "b": 2}})
        self.assertIn("target_records=5", line)
        self.assertIn("ours_records=n/a", line)
        self.assertIn("changed=2", line)
        self.assertIn("missing=n/a", line)


class SnapshotFreshness(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def write(self, name, when):
        path = self.root / name
        path.write_text("x", encoding="utf-8")
        os.utime(path, ns=(when, when))
        return path

    def test_a_snapshot_older_than_an_object_is_named_STALE(self):
        report = self.write("report.json", 1_000)
        obj = self.write("ours.o", 2_000)
        label, detail = datadiff.snapshot_freshness(report, obj)
        self.assertEqual(label, "STALE")
        self.assertIn("ours.o", detail)

    def test_a_snapshot_newer_than_every_object_is_current(self):
        report = self.write("report.json", 3_000)
        a, b = self.write("t.o", 1_000), self.write("o.o", 2_000)
        self.assertEqual(datadiff.snapshot_freshness(report, a, b)[0],
                         "current")

    def test_the_newest_object_decides_not_the_first(self):
        report = self.write("report.json", 2_500)
        a, b = self.write("t.o", 1_000), self.write("o.o", 3_000)
        self.assertEqual(datadiff.snapshot_freshness(report, a, b)[0], "STALE")

    def test_a_missing_snapshot_or_object_says_so_rather_than_current(self):
        obj = self.write("ours.o", 1_000)
        self.assertEqual(
            datadiff.snapshot_freshness(self.root / "absent.json", obj)[0],
            "no snapshot")
        report = self.write("report.json", 1_000)
        self.assertEqual(
            datadiff.snapshot_freshness(report, self.root / "absent.o")[0],
            "unknown")


REPO = Path(__file__).resolve().parents[3]
UNIT = "game/pb/pb_diag"


@unittest.skipUnless(
    (REPO / f"build/GUNE5D/obj/{UNIT}.o").is_file()
    and Path(datadiff.OBJDUMP).exists(),
    "needs the split target objects and a built tree")
class LiveSectionTable(unittest.TestCase):
    def test_the_json_row_carries_the_measured_snapshot_state(self):
        rows = []
        datadiff.section_table(UNIT + ".c", report=rows)
        self.assertEqual(len(rows), 1)
        snapshot = rows[0]["report_snapshot"]
        self.assertIn(snapshot["state"],
                      ("current", "STALE", "unknown", "no snapshot"))
        self.assertTrue(snapshot["detail"])
        json.dumps(rows)          # the row must stay serialisable


if __name__ == "__main__":
    unittest.main()
