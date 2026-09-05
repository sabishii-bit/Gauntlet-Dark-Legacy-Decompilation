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
        self.assertEqual(ce.function_key({"DiffRate": []}, "DiffRate_8002951C"), "DiffRate")
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


if __name__ == "__main__":
    unittest.main()
