"""CE shared-core integration and explicit incomplete-measurement controls."""
import json
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS / "composed_census"))
import ce_eq_datum_audit as ce
import ce_rank_rows as rank
import datadiff
import exception_metadata as eh


def screen(delta=False):
    return {"verdict": "VALUE-DELTA" if delta else "VALUE-EQUAL",
            "target_only": Counter({"B:00000000": 1}) if delta else Counter(),
            "ours_only": Counter(), "labels": {}, "target_relocs": 1,
            "ours_relocs": 0 if delta else 1}


class SharedDatumTests(unittest.TestCase):
    def test_screen_calls_shared_core_with_exact_object_pair(self):
        target_lines, our_lines = ["target"], ["ours"]
        ours = ce.ROOT / "build/GUNE5D/src/u.o"
        with patch.object(ce, "parsed", side_effect=[{"f": target_lines}, {"f": our_lines}]), \
                patch.object(ce.fndiff, "datum_screen_from_lines", return_value=screen()) as core:
            self.assertEqual(ce.screen_against("u", "f", ours), screen())
        core.assert_called_once_with(target_lines, our_lines,
                                     ce.ROOT / "build/GUNE5D/obj/u.o", ours)

    def test_suffix_resolution_does_not_strip_identity_placeholders(self):
        with patch.object(ce.fndiff, "objdump", return_value="00000000 g F .text 00000004 DiffRate_8002951C"):
            self.assertEqual(ce.function_key({"DiffRate": []}, "DiffRate_8002951C", "fixture.o"), "DiffRate")
            with self.assertRaises(KeyError):
                ce.function_key({"DiffRate": []}, "DiffRate_8000BAD0", "fixture.o")
        with self.assertRaises(KeyError):
            ce.function_key({"fn": []}, "fn_8002951C")
        self.assertEqual(ce.function_key({"dtor_800DB21C": []}, "dtor_800DB21C"), "dtor_800DB21C")

    def test_rule_chains_are_counted_once(self):
        pins, count = ce.pinned_functions({"units": {"u": [{"function": "f"}, {"function": "f"}]}})
        self.assertEqual(pins, [("u", "f")])
        self.assertEqual(count, 2)

    def test_malformed_rules_are_not_an_empty_clean_scope(self):
        for config in ([], {"units": []}, {"u": {}}, {"u": [{}]}):
            with self.assertRaises(ValueError):
                ce.pinned_functions(config)

    def test_missing_required_raw_body_never_falls_back_to_final(self):
        with patch.object(Path, "is_file", return_value=False):
            final, raw, has_raw = ce.object_paths("u", requires_raw=True)
        self.assertNotEqual(final, raw)
        self.assertTrue(has_raw)
        self.assertIn(".postprocess", str(raw))

    def test_active_p6_edge_requires_raw_even_without_webfrank_pin(self):
        edges = {"game/sys/registry": {"body_o": "build/GUNE5D/src/game/sys/.postprocess/body/registry.o"}}
        with patch.object(Path, "is_file", return_value=False):
            final, raw, has_raw = ce.object_paths("game/sys/registry", edges=edges)
        self.assertNotEqual(final, raw)
        self.assertTrue(has_raw)
        self.assertIn(".postprocess", str(raw))

    def test_direct_compile_mode_does_not_read_leftover_raw_body(self):
        edges = {"u": {"body_o": "build/GUNE5D/src/u.o"}}
        with patch.object(Path, "is_file", return_value=True):
            final, raw, has_raw = ce.object_paths("u", requires_raw=True, edges=edges)
        self.assertEqual(final, raw)
        self.assertFalse(has_raw)

    def test_missing_compile_edge_is_an_unresolved_measurement(self):
        result = ce.audit([], 0, [("u", "f")], [], {}, edges={})
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertEqual(result["tally"]["functions_selected"], 1)
        self.assertEqual(result["tally"]["functions_screened_both"], 0)

    def test_inplace_rewrite_cannot_masquerade_as_raw_compiler_output(self):
        edges = {"u": {"body_o": "build/GUNE5D/src/u.o",
                       "raw_unavailable": "declared in-place runtime rewrite"}}
        with patch.object(ce, "screen_side", side_effect=AssertionError("must not read rewritten body")):
            result = ce.audit([], 0, [("u", "f")], [], {}, edges=edges)
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertIn("raw compiler bytes unavailable", result["discovery_failures"][0]["error"])

    def test_raw_snapshot_must_match_current_ninja(self):
        with patch.object(Path, "read_text", return_value='{"schema_version":1,"ninja_sha256":"stale"}'), \
                patch.object(Path, "read_bytes", return_value=b"current graph"):
            with self.assertRaisesRegex(ValueError, "stale"):
                ce.active_raw_edges()

    def test_missing_and_stale_objects_refuse_before_parse(self):
        with patch.object(Path, "is_file", return_value=False), patch.object(ce.fndiff, "parse") as parse:
            with self.assertRaises(FileNotFoundError):
                ce.parsed(Path("missing.o"))
            parse.assert_not_called()
        with patch.object(Path, "is_file", return_value=True), \
                patch.object(ce.fndiff, "stale_object_warning", return_value="STALE OBJECT"), \
                patch.object(ce.fndiff, "parse") as parse:
            with self.assertRaisesRegex(ValueError, "STALE"):
                ce.parsed(Path("stale.o"))
            parse.assert_not_called()

    def test_failed_dump_is_fail_not_a_clean_empty_result(self):
        with patch.object(ce, "screen_against", side_effect=ce.fndiff.ObjdumpFailed("bad dump")):
            result = ce.screen_side("u", "f", ce.ROOT / "build/f.o")
        self.assertEqual(result["status"], "FAIL")
        self.assertNotIn("verdict", result)

    def test_missing_function_is_unresolved_not_zero_relocations(self):
        with patch.object(ce, "screen_against", side_effect=KeyError("f")):
            result = ce.screen_side("u", "f", ce.ROOT / "build/f.o")
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertNotIn("verdict", result)

    def test_delta_is_review_candidate_not_confirmed_source_bug(self):
        with patch.object(ce, "screen_against", return_value=screen(True)):
            result = ce.screen_side("u", "f", ce.ROOT / "build/f.o")
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertIn("not a proven", result["interpretation"])

    def test_post_read_failure_is_counted_independently(self):
        raw = {"status": "PASS", **screen()}
        post = {"status": "UNRESOLVED", "error": "missing"}
        with patch.object(ce, "screen_side", side_effect=[raw, post]):
            result = ce.audit([("u", "f")], 2, [("u", "f")], [], {"scope": "test"})
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertEqual(result["tally"]["post_unreadable"], 1)
        self.assertEqual(result["tally"]["raw_equal"], 1)
        self.assertEqual(result["tally"]["functions_screened_both"], 0)

    def test_delta_residuals_differ_even_if_both_sides_have_delta(self):
        raw = {"status": "UNRESOLVED", **screen(True)}
        post = {**raw, "target_only": Counter({"B:11111111": 1})}
        with patch.object(ce, "screen_side", side_effect=[raw, post]):
            result = ce.audit([("u", "f")], 2, [("u", "f")], [], {})
        self.assertEqual(result["tally"]["disagreements"], 1)

    def test_empty_scope_never_passes(self):
        result = ce.audit([], 0, [], [], {})
        self.assertEqual(result["status"], "UNRESOLVED")

    def test_image_keeps_missing_target_as_discovery_failure_and_excludes_auto(self):
        report = {"units": [{"name": "main/u", "metadata": {}},
                             {"name": "main/auto", "metadata": {"auto_generated": True}},
                             {"name": "main/done", "metadata": {"complete": True}}]}
        with patch.object(Path, "read_text", return_value=json.dumps(report)), \
                patch.object(ce, "parsed", side_effect=FileNotFoundError("missing")):
            roster, failures, details = ce.select_functions([], image=True)
        self.assertEqual(roster, [])
        self.assertEqual(len(failures), 1)
        self.assertEqual(failures[0]["unit"], "u")
        self.assertEqual(details["units_selected"], 1)

    def test_shared_core_detects_wrong_literal_but_not_transposition(self):
        # Exercise the real shared multiset arithmetic, not a CE replacement.
        relocs = ["    R_PPC_ADDR16_LO one", "    R_PPC_ADDR16_LO two"]
        local = {"one": (".sdata2", 4, bytes.fromhex("3f800000")),
                 "two": (".sdata2", 4, bytes.fromhex("40000000"))}
        wrong = {**local, "two": (".sdata2", 4, bytes.fromhex("40400000"))}
        with patch.object(ce.fndiff, "object_datum_table", side_effect=[local, wrong]):
            delta = ce.fndiff.datum_screen_from_lines(relocs, relocs, "t.o", "o.o")
        self.assertEqual(delta["verdict"], "VALUE-DELTA")
        with patch.object(ce.fndiff, "object_datum_table", side_effect=[local, local]):
            same = ce.fndiff.datum_screen_from_lines(relocs, list(reversed(relocs)), "t.o", "o.o")
        self.assertEqual(same["verdict"], "VALUE-EQUAL")

    def test_cli_writes_versioned_failure_without_silent_empty_report(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(ce, "pinned_functions", side_effect=ValueError("bad config")):
            output = Path(tmp) / "report.json"
            code = ce.main(["--out", str(output)])
            result = json.loads(output.read_text())
        self.assertEqual(code, 1)
        self.assertEqual(result["schema_version"], 1)
        self.assertEqual(result["status"], "FAIL")

    def test_ranker_consumes_current_schema_and_keeps_candidates(self):
        with patch.object(ce, "screen_against", return_value=screen(True)):
            result = ce.audit([("u", "f")], 1, [("u", "f")], [], {})
        byte_rows, identity_rows, unreadable = rank.ranked_rows(result)
        self.assertEqual(len(byte_rows), 1)
        self.assertEqual(identity_rows, [])
        self.assertEqual(unreadable, [])

    def test_ranker_refuses_old_schema_and_contradictory_pass(self):
        for data in ({"rows": []}, {"schema_version": 2},
                     {"schema_version": 1, "status": "PASS", "rows": [], "tally": None}):
            with self.assertRaises(ValueError):
                rank.ranked_rows(data)
        row = {"unit": "u", "function": "f", "raw": {"status": "UNRESOLVED", **screen(True)}}
        with self.assertRaisesRegex(ValueError, "PASS audit"):
            rank.ranked_rows({"schema_version": 1, "status": "PASS", "rows": [row], "tally": {}})


class ExceptionAndSectionTests(unittest.TestCase):
    def test_r60_and_datadiff_share_the_same_decoder(self):
        import r60_enemy_source_probe
        # The import-safety suite deliberately reloads core modules. Identity
        # may differ across module generations; the implementation path must not.
        self.assertEqual(r60_enemy_source_probe.exception_records.__code__.co_filename,
                         eh.exception_records.__code__.co_filename)
        self.assertEqual(r60_enemy_source_probe.compare_exception_records.__code__.co_filename,
                         eh.compare_exception_records.__code__.co_filename)

    def test_extab_payload_relocation_refuses_even_when_placeholder_bytes_equal(self):
        from tools.gdl.tests import test_r60_enemy_probes
        data, sections = test_r60_enemy_probes.ExceptionMetadata().fixture()
        sections.append(eh.wf.Section(7, ".relaextab", 4, 128, 12, 5, 2, 12))
        with patch.object(eh.wf, "_sections", return_value=sections):
            with self.assertRaisesRegex(ValueError, "payload relocations"):
                eh.exception_records(data)

    def test_absent_eh_is_empty_but_a_missing_partner_refuses(self):
        from tools.gdl.tests import test_r60_enemy_probes
        data, sections = test_r60_enemy_probes.ExceptionMetadata().fixture()
        with patch.object(eh.wf, "_sections", return_value=sections[:2]):
            self.assertEqual(eh.exception_records(data), {})
        with patch.object(eh.wf, "_sections", return_value=sections[:3]):
            with self.assertRaisesRegex(ValueError, "one extab"):
                eh.exception_records(data)

    def test_record_order_is_not_a_metadata_difference(self):
        records = {"a": {"length": 8, "metadata": "00"},
                   "b": {"length": 12, "metadata": "11"}}
        result = eh.compare_exception_records(records, dict(reversed(list(records.items()))))
        self.assertEqual(result["changed"], {})
        self.assertEqual(result["missing"], [])
        self.assertEqual(result["extra"], {})

    def test_extra_records_are_unresolved_not_silently_dead_stripped(self):
        record = {"length": 8, "metadata": "00"}
        with patch.object(Path, "read_bytes", return_value=b"fixture"), \
                patch.object(eh, "exception_records", side_effect=[{"a": record}, {"a": record, "helper": record}]):
            result = datadiff.exception_table("target.o", "ours.o")
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertEqual(set(result["extra"]), {"helper"})

    def test_changed_records_are_a_failed_obligation(self):
        with patch.object(Path, "read_bytes", return_value=b"fixture"), \
                patch.object(eh, "exception_records", side_effect=[{"a": {"length": 8}}, {"a": {"length": 12}}]):
            result = datadiff.exception_table("target.o", "ours.o")
        self.assertEqual(result["status"], "FAIL")

    def test_failed_objdump_does_not_produce_empty_sections(self):
        from subprocess import CompletedProcess
        with patch.object(Path, "is_file", return_value=True), \
                patch.object(datadiff.subprocess, "run", return_value=CompletedProcess([], 1, "", "bad dump")):
            with self.assertRaisesRegex(datadiff.MeasurementUnavailable, "bad dump"):
                datadiff.section_sizes("fixture.o")

    def test_missing_input_writes_an_unresolved_json_artifact(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "audit.json"
            result = datadiff.main(["--sections", "game/no_such_unit", "--out", str(out)])
            report = json.loads(out.read_text())
        self.assertEqual(result, 2)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["status"], "UNRESOLVED")
        self.assertEqual(len(report["rows"]), 1)


if __name__ == "__main__":
    unittest.main()
