"""Shadow-report calibration and stale-output refusal; no private fixtures."""
import copy
import contextlib
import io
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


class ExceptionControlTests(unittest.TestCase):
    def control(self):
        return {"schema_version": 1, "status": "UNRESOLVED", "units_selected": 1,
                "rows": [{"unit": "game/world/newcam.c", "exception_metadata": {
                    "schema_version": 1, "status": "PASS", "target_records": 19,
                    "ours_records": 19, "missing": [], "changed": {}, "extra": {}}}]}

    def test_equal_exceptions_do_not_certify_the_whole_tu(self):
        row = self.control()
        self.assertIn("metadata only", preflight.validate_exception_control(row))
        self.assertEqual(row["status"], "UNRESOLVED")

    def test_missing_mismatched_or_empty_exception_population_refuses(self):
        for field, value in (("status", "UNRESOLVED"), ("target_records", 0),
                             ("ours_records", 18), ("missing", ["function"]),
                             ("changed", {"function": "differs"}), ("extra", {"new": {}})):
            with self.subTest(field=field):
                row = self.control()
                row["rows"][0]["exception_metadata"][field] = value
                with self.assertRaises(ValueError):
                    preflight.validate_exception_control(row)


class DatumControlTests(unittest.TestCase):
    def control(self, delta=False):
        state = "UNRESOLVED" if delta else "PASS"
        side = {"object": "build/enemy.o", "status": state,
                "verdict": "VALUE-DELTA" if delta else "VALUE-EQUAL",
                "target_relocs": 91, "ours_relocs": 91,
                "target_only": {"A:0x80250E00": 2} if delta else {},
                "ours_only": {"N:mbdesc": 2} if delta else {}}
        return {"schema_version": 1, "status": state,
                "selection": {"scope": "explicit-function", "units_selected": 1},
                "discovery_failures": [],
                "tally": dict(functions_selected=1, functions_screened_both=1,
                              raw_equal=int(not delta), raw_candidates=int(delta),
                              post_equal=int(not delta), post_candidates=int(delta),
                              raw_unreadable=0, post_unreadable=0,
                              disagreements=0, discovery_failures=0),
                "rows": [{"unit": "game/enemy/enemy", "function": "do_enemy_move",
                          "status": state, "raw": copy.deepcopy(side),
                          "post": copy.deepcopy(side)}]}

    def test_equal_and_delta_are_complete_measurements_not_equivalence(self):
        for delta in (False, True):
            payload = self.control(delta)
            before = copy.deepcopy(payload)
            self.assertIn("not a binding/equivalence certificate",
                          preflight.validate_datum_control(payload))
            self.assertEqual(payload, before)  # Never rewrite UNRESOLVED to PASS.

    def test_incomplete_stale_or_malformed_screens_fail(self):
        mutations = [
            lambda p: p.update(schema_version=2),
            lambda p: p.update(status="FAIL"),
            lambda p: p.update(rows=[]),
            lambda p: p.update(rows=None),
            lambda p: p.update(discovery_failures=[{"error": "missing object"}]),
            lambda p: p["selection"].update(units_selected=0),
            lambda p: p["rows"][0].update(function="different"),
            lambda p: p["rows"][0]["raw"].update(error="STALE OBJECT"),
            lambda p: p["rows"][0]["raw"].update(target_relocs=0),
            lambda p: p["rows"][0]["raw"].pop("ours_relocs"),
            lambda p: p["rows"][0]["raw"].update(verdict="VALUE-EQUAL"),
            lambda p: p["rows"][0]["raw"].update(target_only={"bad": -1}),
            lambda p: p["rows"][0]["raw"].update(target_only={"bad": 92}),
            lambda p: p["tally"].update(functions_screened_both=0),
            lambda p: p["tally"].update(raw_candidates=0),
        ]
        for i, mutate in enumerate(mutations):
            with self.subTest(case=i):
                payload = self.control(True)
                mutate(payload)
                with self.assertRaises(ValueError):
                    preflight.validate_datum_control(payload)

    def test_only_valid_exit_two_artifact_completes_the_stage(self):
        for incomplete in (False, True):
            with self.subTest(incomplete=incomplete), tempfile.TemporaryDirectory() as temp:
                folder = Path(temp)
                artifact = folder / "datum.json"
                payload = self.control(True)
                if incomplete:
                    payload["rows"][0]["raw"]["error"] = "missing object"

                def run(*args, **kwargs):
                    artifact.write_text(json.dumps(payload), encoding="utf-8")
                    return subprocess.CompletedProcess(args[0], 2, "review candidate", "")

                with patch.object(preflight.subprocess, "run", side_effect=run):
                    result = preflight.run_stage(
                        "datum", ["fixture"], folder, folder, artifact,
                        allowed_returncodes=(0, 2),
                        artifact_validator=preflight.validate_datum_control)
                self.assertEqual(result["status"], "FAIL" if incomplete else "PASS")
                if not incomplete:
                    self.assertEqual(result["tool_reported_status"], "UNRESOLVED")

    def test_console_distinguishes_review_findings_from_execution_failure(self):
        output = io.StringIO()
        result = {"status": "FAIL", "error": "input changed", "stages": [
            {"name": "datum_control", "status": "PASS",
             "tool_reported_status": "UNRESOLVED", "validated_scope": "measurement only"},
            {"name": "provenance", "status": "FAIL", "error": "bad graph",
             "log": "build/provenance.log"}]}
        with contextlib.redirect_stdout(output):
            preflight.print_result(result, "build/summary.json")
        for text in ("ERROR: input changed", "REVIEW UNRESOLVED: measurement only",
                     "provenance: FAIL", "bad graph", "build/provenance.log"):
            self.assertIn(text, output.getvalue())


class InputFingerprintTests(unittest.TestCase):
    def test_uses_validated_graph_inputs_without_retired_rule_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            paths = ("objdiff.json", "build.ninja", "build/GUNE5D/build_edges.json",
                     "tools/gdl/native_build.py", "build/control.o")
            for name in paths:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("fixture", encoding="utf-8")
            (root / "objdiff.json").write_text(json.dumps({"units": [
                {"target_path": "build/control.o"}]}), encoding="utf-8")
            graph = {"generator_inputs": {"tools/gdl/native_build.py": "validated"}}
            with patch.object(preflight, "load_graph", return_value=graph) as load:
                hashes = preflight.input_fingerprints(root)
                load.assert_called_once_with(root, "GUNE5D")
                self.assertEqual(set(hashes), {str(root / name) for name in paths})
                (root / "tools/gdl/native_build.py").unlink()
                with self.assertRaises(OSError):
                    preflight.input_fingerprints(root)

    def test_stale_graph_is_not_silently_accepted(self):
        with patch.object(preflight, "load_graph", side_effect=ValueError("stale graph")):
            with self.assertRaisesRegex(ValueError, "stale graph"):
                preflight.input_fingerprints(Path("unused"))


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
