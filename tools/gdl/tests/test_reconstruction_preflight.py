"""Shadow-report calibration and stale-output refusal; no private fixtures."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl import reconstruction_preflight as preflight


def report(score=100):
    return {"measures": {"total_code": "8", "total_data": "0", "total_functions": 1, "total_units": 1},
            "units": [{"name": "main/sdk/control", "metadata": {"complete": True},
                       "functions": [{"name": "control", "size": "8", "fuzzy_match_percent": score}]}]}


class ShadowReportTests(unittest.TestCase):
    def test_same_reports_are_not_an_original_source_certificate(self):
        row = preflight.compare_reports(report(), report())
        self.assertEqual(row["status"], "PASS")
        self.assertEqual(row["scores_changed"], 0)
        self.assertIn("not a corrected matching percentage", row["interpretation"])

    def test_linked_demotions_are_unadjudicated_not_proven_defects(self):
        row = preflight.compare_reports(report(), report(90))
        self.assertEqual(row["lost_report_100"], 1)
        self.assertEqual(row["source_linked_demotions"], 1)
        self.assertEqual(row["adjudication_status"], "UNRESOLVED")
        self.assertEqual(row["changes"][0]["disposition"], "UNRESOLVED")

    def test_no_missing_score_is_silently_counted_as_zero(self):
        row = preflight.compare_reports(report(None), report())
        self.assertEqual(row["scores_changed"], 0)
        self.assertEqual(len(row["unknown_scores"]), 1)

    def test_mismatched_populations_and_totals_are_refused(self):
        for kind in ("name", "size", "total", "linked", "duplicate", "empty"):
            with self.subTest(kind=kind):
                other = report()
                if kind == "name":
                    other["units"][0]["functions"][0]["name"] = "different"
                elif kind == "size":
                    other["units"][0]["functions"][0]["size"] = "12"
                elif kind == "total":
                    other["measures"]["total_code"] = "12"
                elif kind == "linked":
                    other["units"][0]["metadata"]["complete"] = False
                elif kind == "duplicate":
                    other["units"].append(copy.deepcopy(other["units"][0]))
                else:
                    other["units"] = []
                with self.assertRaises(ValueError):
                    preflight.compare_reports(report(), other)

    def test_invalid_scores_fail(self):
        for value in (float("nan"), float("inf"), -1, 101):
            with self.assertRaises(ValueError):
                preflight.compare_reports(report(), report(value))


class CommandArtifactTests(unittest.TestCase):
    def test_previous_artifact_is_never_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            path = folder / "result.json"
            path.write_text('{"status":"PASS"}')
            with patch.object(preflight.subprocess, "run") as run:
                row = preflight.run_stage("test", ["not-run"], folder, folder, path)
            run.assert_not_called()
            self.assertEqual(row["status"], "FAIL")

    def test_process_failure_missing_artifact_and_bad_schema_refuse(self):
        for case in ("failure", "missing", "unresolved"):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temp:
                folder = Path(temp)
                path = folder / "result.json"

                def run(*args, **kwargs):
                    if case == "unresolved":
                        path.write_text(json.dumps({"schema_version": 1, "status": "UNRESOLVED"}))
                    return subprocess.CompletedProcess(args[0], 1 if case == "failure" else 0, "", "")

                with patch.object(preflight.subprocess, "run", side_effect=run):
                    row = preflight.run_stage("test", ["mock"], folder, folder, path, True)
                self.assertNotEqual(row["status"], "PASS")

    def test_real_cli_log_success_and_refusal_without_assets(self):
        script = str(Path(preflight.__file__))
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            ok = preflight.run_stage("help", [sys.executable, script, "--help"], folder, folder)
            bad = preflight.run_stage("bad", [sys.executable, script, "--no-such-option"], folder, folder)
            self.assertEqual(ok["status"], "PASS")
            self.assertEqual(bad["status"], "FAIL")
            self.assertIn("unrecognized arguments", Path(bad["log"]).read_text())


if __name__ == "__main__":
    unittest.main()
