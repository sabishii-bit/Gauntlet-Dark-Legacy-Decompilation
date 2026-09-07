import io
import json
import tempfile
from contextlib import redirect_stdout
import subprocess
import sys
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lowmatch import missing_fuzzy_is_exact_zero, object_function_name_counts
import lowmatch


class LowMatchMissingFuzzyTests(unittest.TestCase):
    def test_exact_unique_base_name_identifies_serialized_zero(self):
        counts = Counter({"fn_800DBA80": 1})
        self.assertTrue(missing_fuzzy_is_exact_zero("fn_800DBA80", counts))

    def test_different_source_name_remains_unpaired(self):
        counts = Counter({"init_next_level": 1})
        self.assertFalse(
            missing_fuzzy_is_exact_zero("init_next_level_8005638C", counts))

    def test_duplicate_base_name_fails_closed(self):
        counts = Counter({"duplicate": 2})
        self.assertFalse(missing_fuzzy_is_exact_zero("duplicate", counts))

    @patch("lowmatch.subprocess.run")
    def test_symbol_table_parser_counts_only_functions(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout="""
00000000 l     F .text  00000050 local_fn
00000050 g     F .text  00000020 global_fn
00000000 l     O .data  00000004 not_a_function
00000000 l    df *ABS*  00000000 source.c
00000070 l     F .text  00000010 local_fn
""",
            stderr="",
        )
        self.assertEqual(
            object_function_name_counts(Path("example.o")),
            Counter({"local_fn": 2, "global_fn": 1}),
        )

    @patch("lowmatch.subprocess.run")
    def test_objdump_failure_fails_closed(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[], returncode=1, stdout="", stderr="error")
        self.assertEqual(object_function_name_counts(Path("bad.o")), Counter())


    def test_queue_runs_without_knowledge_package(self):
        report = {"units": [
            {"name": "main/game/test/foo", "functions": [
                {"name": "near", "size": 100, "fuzzy_match_percent": 99},
                {"name": "low", "size": 80, "fuzzy_match_percent": 20},
                {"name": "exact", "size": 40, "fuzzy_match_percent": 100}]},
            {"name": "main/game/test/linked", "metadata": {"complete": True},
             "functions": [
                 {"name": "linked_noise", "size": 40,
                  "fuzzy_match_percent": 10}]}]}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "report.json"
            path.write_text(json.dumps(report), encoding="utf-8")
            output = io.StringIO()
            with patch.object(lowmatch, "REPORT", path), \
                 patch.object(sys, "argv", ["lowmatch", "--limit", "0"]), \
                 patch.dict(sys.modules, {"memory_graph": None,
                                          "memory_graph.core": None}), \
                 redirect_stdout(output):
                self.assertEqual(lowmatch.main(), 0)
        text = output.getvalue()
        self.assertLess(text.index("low "), text.index("near "))
        self.assertNotIn("exact ", text)
        self.assertNotIn("linked_noise", text)
        self.assertNotIn("PARKED", text)
        self.assertNotIn("rec=", text)
        self.assertIn("prior attempts and ownership not assessed", text)


if __name__ == "__main__":
    unittest.main()
