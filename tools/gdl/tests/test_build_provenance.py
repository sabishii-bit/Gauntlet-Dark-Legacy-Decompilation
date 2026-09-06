"""Controls for truthful provenance, independent of private retail assets."""
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl import build_provenance as bp


class ProvenanceTests(unittest.TestCase):
    def test_recording_writer_is_transparent(self):
        plain, recorded = io.StringIO(), io.StringIO()
        from tools.ninja_syntax import Writer
        writers = [Writer(plain), bp.RecordingWriter(recorded)]
        for writer in writers:
            writer.rule("mwcc", "$compiler $cflags -c $in -o $out", depfile="$out.d", deps="gcc")
            writer.build(Path("build/a.o"), "mwcc", Path("a.c"),
                         implicit=[Path("compiler.exe"), None], variables={"cflags": ["-O4,p", "-lang=c"]})
        self.assertEqual(plain.getvalue(), recorded.getvalue())
        self.assertEqual(writers[1].edges[0]["inputs"], ["a.c"])

    def test_compiler_label_is_not_authentication(self):
        self.assertEqual(bp.compiler_class("GC/1.2.5"), "stock_configured")
        self.assertEqual(bp.compiler_class("GC\\1.2.5n"), "derived_configured")
        self.assertEqual(bp.compiler_class("GC/1.2.5s"), "derived_configured")
        self.assertEqual(bp.compiler_class("GC/new"), "unknown")

    def test_ninja_deps_preserves_stale_and_spaces(self):
        parsed = bp.parse_ninja_deps("build/a.o: #deps 2, deps mtime 12 (STALE)\n    a.c\n    W:/with spaces/a.h\n")
        self.assertEqual(parsed["build/a.o"]["state"], "STALE")
        self.assertEqual(parsed["build/a.o"]["paths"], ["a.c", "W:/with spaces/a.h"])
        self.assertEqual(bp.parse_ninja_deps("build/a.o: deps not found\n"), {})

    def test_rule_chain_is_deduplicated_and_manual_wins(self):
        wf = {"units": {"game/a": [{"function": "f"}, {"function": "f", "unproven_recolor_audit": "reviewed"}]}}
        result = bp.postprocessor_functions("game/a", [{"rule": "webfrank_globalize_atree"}], wf, {})
        self.assertEqual(result["f"], {"kind": "manual_exception", "rule_count": 2})
        self.assertEqual(bp.postprocessor_functions("game/a", [], wf, {}), {})

    def test_p6_is_included_and_mod_pipeline_bypasses_it(self):
        p6 = {"units": {"game/a": {"function": "f"}}}
        result = bp.postprocessor_functions("game/a", [{"rule": "p6frank"}], {}, p6)
        self.assertEqual(result["f"]["kind"], "p6frank_guarded_declared")
        self.assertEqual(bp.postprocessor_functions("game/a", [], {}, p6), {})
        with self.assertRaises(ValueError):
            bp.postprocessor_functions("game/a", [{"rule": "p6frank"}], {}, {})
        with self.assertRaises(ValueError):
            bp.postprocessor_functions("game/a", [{"rule": "webfrank"}], {}, {})

    def test_disjoint_score_tiers_do_not_confuse_linkage(self):
        report = {"units": [{"name": "main/game/a", "functions": [
            {"name": "raw", "size": 12, "fuzzy_match_percent": 100},
            {"name": "p6", "size": 8, "fuzzy_match_percent": 100},
            {"name": "open", "size": 20, "fuzzy_match_percent": 99}]}]}
        units = {"main/game/a": {"compiler_class": "derived_configured", "linkage": "extracted_fallback",
                                "function_postprocessors": {"p6": {"kind": "p6frank_guarded_declared"}}}}
        result = bp.report_accounting(report, units)
        self.assertEqual(result["total_code_bytes"], 40)
        self.assertEqual(sum(t["bytes"] for t in result["tiers"].values()), 20)
        self.assertEqual(sum(t["linked_source_bytes"] for t in result["tiers"].values()), 0)
        self.assertIn("NOT raw-byte", result["scope"])

    def fixture(self, root, *, inplace=False):
        paths = ["build.ninja", "configure.py", "a.c", "a.h", "build/compilers/GC/1.2.5/mwcceppc.exe",
                 "build/compilers/GC/1.3.2/mwldeppc.exe", "build/GUNE5D/src/a.o", "build/GUNE5D/obj/a.o",
                 "build/GUNE5D/main.elf", "build/GUNE5D/fix.stamp", "tools/fix_exception_objects.py"]
        for path in paths:
            dest = root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text("#pragma scheduling on\n", encoding="utf-8")
        for name in ("webfrank", "p6frank"):
            dest = root / "config/GUNE5D" / (name + ".json")
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text('{"units": {}}', encoding="utf-8")
        compile_edge = {"rule": "mwcc", "outputs": ["build/GUNE5D/src/a.o"], "inputs": ["a.c"],
                        "implicit": [], "variables": {"mw_version": "GC/1.2.5", "cflags": "-O4,p"}}
        link_edge = {"rule": "link", "outputs": ["build/GUNE5D/main.elf"], "inputs": ["build/GUNE5D/src/a.o"], "implicit": [], "variables": {}}
        snapshot = {"schema_version": 1, "non_matching": False, "compilers": "build/compilers",
                    "linker_version": "GC/1.3.2", "ninja": "ninja", "ninja_sha256": bp.sha256(root / "build.ninja"),
                    "generator_inputs": {"configure.py": bp.sha256(root / "configure.py")},
                    "rules": {"mwcc": {}, "link": {}}, "edges": [compile_edge, link_edge],
                    "units": [{"name": "a.c", "module": "main", "autogenerated": False,
                               "code_size": 4, "data_size": 0, "configured": True, "configured_completed": True,
                               "source_object": "build/GUNE5D/src/a.o", "extracted_object": "build/GUNE5D/obj/a.o",
                               "linked_object": "build/GUNE5D/src/a.o", "linkage": "source"}]}
        if inplace:
            snapshot["edges"].append({"rule": "fix_exception_objects", "outputs": ["build/GUNE5D/fix.stamp"],
                "inputs": ["build/GUNE5D/src/a.o"], "implicit": ["tools/fix_exception_objects.py"], "variables": {}})
        (root / "build/GUNE5D/build_edges.json").write_text(json.dumps(snapshot), encoding="utf-8")
        (root / "build/GUNE5D/report.json").write_text(json.dumps({"units": [{"name": "main/a", "functions": [
            {"name": "f", "size": 4, "fuzzy_match_percent": 100}]}]}), encoding="utf-8")
        commands = [{"output": edge["outputs"][0], "command": "expanded " + edge["rule"]} for edge in snapshot["edges"]]
        deps = "build/GUNE5D/src/a.o: #deps 2, deps mtime 12 (" + ("STALE" if inplace else "VALID") + ")\n    a.c\n    a.h\n"

        def runner(argv, **kwargs):
            from subprocess import CompletedProcess
            return CompletedProcess(argv, 0, json.dumps(commands) if "compdb" in argv else deps, "")
        return runner

    def test_manifest_pass_and_hash_missing_artifact_refusal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runner = self.fixture(root)
            with patch.object(bp.subprocess, "run", runner):
                result = bp.collect_manifest(root)
                self.assertEqual(result["status"], "PASS", result["failures"])
                self.assertEqual(result["link_reconciliation"]["selected_count"], 1)
                self.assertTrue(result["link_reconciliation"]["ordered_inputs_equal"])
                self.assertEqual(result["units"][0]["pipeline"][0]["dependencies"], ["a.c", "a.h"])
                self.assertEqual(result["units"][0]["pipeline"][0]["pragma_observations"][0]["line"], 1)
                (root / "a.h").unlink()
                self.assertEqual(bp.collect_manifest(root)["status"], "UNRESOLVED")
                (root / "build.ninja").write_text("drift", encoding="utf-8")
                self.assertEqual(bp.collect_manifest(root)["status"], "FAIL")

    def test_inplace_fixup_never_claims_raw_or_valid_dependencies(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runner = self.fixture(root, inplace=True)
            with patch.object(bp.subprocess, "run", runner):
                result = bp.collect_manifest(root)
            self.assertEqual(result["status"], "PASS", result["failures"])
            stage = result["units"][0]["pipeline"][0]
            self.assertFalse(stage["raw_compiler_output_retained"])
            self.assertEqual(stage["dependency_status"], "UNRESOLVED")
            self.assertEqual(result["units"][0]["raw_text_class"], "exception_runtime_rewrite_declared")
            self.assertTrue(result["known_limitations"])

    def test_missing_snapshot_is_fail_not_empty_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = bp.collect_manifest(tmp)
            self.assertEqual(result["execution_status"], "FAIL")
            self.assertTrue(result["failures"])

    def test_separate_runtime_output_retains_raw_and_classifies_rewrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root)
            path = root / 'build/GUNE5D/build_edges.json'
            snapshot = json.loads(path.read_text())
            raw = 'build/GUNE5D/src/raw.o'
            (root / raw).write_bytes(b'raw')
            snapshot['edges'][0]['outputs'] = [raw]
            snapshot['edges'].append({'rule': 'fix_exception_object', 'inputs': [raw],
                'outputs': ['build/GUNE5D/src/a.o'], 'implicit': ['tools/fix_exception_objects.py'],
                'variables': {'exception_kind': 'nmw'}})
            path.write_text(json.dumps(snapshot), encoding='utf-8')

            def query(argv, **kwargs):
                from subprocess import CompletedProcess
                if 'compdb' in argv:
                    text = json.dumps([{'output': e['outputs'][0], 'command': 'expanded ' + e['rule']}
                                       for e in snapshot['edges']])
                else:
                    text = f'{raw}: #deps 1, deps mtime 12 (VALID)\n    a.c\n'
                return CompletedProcess(argv, 0, text, '')

            with patch.object(bp.subprocess, 'run', query):
                result = bp.collect_manifest(root)
            self.assertEqual(result['status'], 'PASS', result['failures'])
            unit = result['units'][0]
            self.assertEqual([s['rule'] for s in unit['pipeline']], ['mwcc', 'fix_exception_object'])
            self.assertTrue(unit['pipeline'][0]['raw_compiler_output_retained'])
            self.assertEqual(unit['pipeline'][0]['dependency_status'], 'PASS')
            self.assertEqual(unit['raw_text_class'], 'exception_runtime_rewrite_declared')
            self.assertFalse(result['known_limitations'])
            snapshot['non_matching'] = True
            path.write_text(json.dumps(snapshot), encoding='utf-8')
            with patch.object(bp.subprocess, 'run', query):
                bad = bp.collect_manifest(root)
            self.assertEqual(bad['status'], 'FAIL')
            self.assertEqual(bad['editable_postprocess_isolation']['status'], 'FAIL')

    def test_editable_no_target_transform_is_only_an_edge_certificate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runner = self.fixture(root)
            path = root / 'build/GUNE5D/build_edges.json'
            snapshot = json.loads(path.read_text())
            snapshot['non_matching'] = True
            path.write_text(json.dumps(snapshot), encoding='utf-8')
            with patch.object(bp.subprocess, 'run', runner):
                result = bp.collect_manifest(root)
            self.assertEqual(result['status'], 'PASS')
            self.assertEqual(result['editable_postprocess_isolation']['status'], 'PASS')
            self.assertEqual(result['reconstruction_status'], 'UNRESOLVED')

    def test_report_inventory_and_numeric_corruption_fail(self):
        for corruption in ("duplicate_unit", "extra_unit", "missing_unit", "duplicate_function",
                           "nan_score", "string_nan", "negative_score", "excess_score", "negative_size", "fractional_size", "wrong_total"):
            with self.subTest(corruption=corruption), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                runner = self.fixture(root)
                path = root / "build/GUNE5D/report.json"
                report = json.loads(path.read_text())
                unit = report["units"][0]
                fn = unit["functions"][0]
                if corruption == "duplicate_unit":
                    report["units"].append(unit)
                elif corruption == "extra_unit":
                    report["units"].append({"name": "main/ghost", "functions": []})
                elif corruption == "missing_unit":
                    report["units"] = []
                elif corruption == "duplicate_function":
                    unit["functions"].append(fn)
                elif corruption in {"nan_score", "string_nan", "negative_score", "excess_score"}:
                    fn["fuzzy_match_percent"] = {"nan_score": float("nan"), "string_nan": "nan",
                                                "negative_score": -1, "excess_score": 101}[corruption]
                else:
                    fn["size"] = {"negative_size": -1, "fractional_size": 4.5, "wrong_total": 8}[corruption]
                path.write_text(json.dumps(report), encoding="utf-8")
                with patch.object(bp.subprocess, "run", runner):
                    result = bp.collect_manifest(root)
                self.assertEqual(result["status"], "FAIL", corruption)
                self.assertEqual(result["execution_status"], "FAIL", corruption)
                self.assertTrue(result["failures"], corruption)

    def test_missing_link_or_postcompile_command_cannot_pass(self):
        for target in ("build/GUNE5D/main.elf", "build/GUNE5D/fix.stamp"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                runner = self.fixture(root, inplace=True)

                def query(argv, **kwargs):
                    result = runner(argv, **kwargs)
                    if "compdb" in argv:
                        result.stdout = json.dumps([row for row in json.loads(result.stdout) if row["output"] != target])
                    return result

                with patch.object(bp.subprocess, "run", query):
                    result = bp.collect_manifest(root)
                self.assertEqual(result["execution_status"], "FAIL", result)
                self.assertTrue(any(target in failure for failure in result["failures"]))

    def test_atree_metadata_does_not_hide_webfrank_or_claim_raw_final_object(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            runner = self.fixture(root)
            path = root / "build/GUNE5D/build_edges.json"
            snapshot = json.loads(path.read_text())
            raw = "build/GUNE5D/src/raw.o"
            (root / raw).write_bytes(b"raw object")
            snapshot["edges"][0]["outputs"] = [raw]
            snapshot["edges"].append({"rule": "webfrank_globalize_atree", "inputs": [raw],
                                      "outputs": ["build/GUNE5D/src/a.o"], "implicit": [], "variables": {}})
            path.write_text(json.dumps(snapshot), encoding="utf-8")
            (root / "config/GUNE5D/webfrank.json").write_text(
                json.dumps({"units": {"a": [{"function": "f"}]}}), encoding="utf-8")

            def query(argv, **kwargs):
                from subprocess import CompletedProcess
                if "compdb" in argv:
                    payload = json.dumps([{"output": e["outputs"][0], "command": "expanded " + e["rule"]}
                                          for e in snapshot["edges"]])
                else:
                    payload = f"{raw}: #deps 1, deps mtime 12 (VALID)\n    a.c\n"
                return CompletedProcess(argv, 0, payload, "")

            with patch.object(bp.subprocess, "run", query):
                result = bp.collect_manifest(root)
            self.assertEqual(result["status"], "PASS", result["failures"])
            row = result["units"][0]
            self.assertEqual(len(row["pipeline"]), 2)
            self.assertIn("atree symbol visibility", row["metadata_operations"][0])
            self.assertEqual(row["function_postprocessors"]["f"]["kind"], "webfrank_guarded_declared")


if __name__ == "__main__":
    unittest.main()
