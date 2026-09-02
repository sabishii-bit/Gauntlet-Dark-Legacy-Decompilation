"""normalize_report.py: protobuf default-skipping across the WHOLE report.

Run-35 item 11 (ZI): a completed unit's per-function figures read as zero
or absent, and the report was misread. The measured cause, over the live
327-scope report (one top-level `measures` plus 326 units): NOT ONE of the
sixteen measure keys ever appears with an explicit zero — every zero in the
file is an ABSENCE, and seven different unit-measure key-shapes exist
purely as a function of which values happened to be zero. The original
script normalized exactly one field (`functions[].fuzzy_match_percent`) and
left the other fifteen to the same defect.
"""

import json
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import normalize_report as nr  # noqa: E402


class MeasureDefaultsTests(unittest.TestCase):
    def test_every_key_the_report_uses_has_a_default(self):
        """The 16 keys measured live on 2026-09-02."""
        self.assertEqual(sorted(nr.MEASURE_DEFAULTS), [
            "complete_code", "complete_code_percent", "complete_data",
            "complete_data_percent", "complete_units", "fuzzy_match_percent",
            "matched_code", "matched_code_percent", "matched_data",
            "matched_data_percent", "matched_functions",
            "matched_functions_percent", "total_code", "total_data",
            "total_functions", "total_units",
        ])

    def test_byte_counts_default_to_a_STRING_zero(self):
        """objdiff serializes int64 as JSON strings; so must the fill.

        Filling with 0 rather than "0" would make the filled rows the only
        ones a consumer has to special-case — the inconsistency this script
        exists to remove.
        """
        for key in ("total_code", "matched_code", "complete_code",
                    "total_data", "matched_data", "complete_data"):
            self.assertEqual(nr.MEASURE_DEFAULTS[key], "0", key)

    def test_percents_default_to_float_and_counts_to_int(self):
        for key in ("fuzzy_match_percent", "matched_code_percent",
                    "matched_data_percent", "complete_code_percent",
                    "complete_data_percent", "matched_functions_percent"):
            self.assertIsInstance(nr.MEASURE_DEFAULTS[key], float, key)
        for key in ("total_functions", "matched_functions", "total_units",
                    "complete_units"):
            self.assertIsInstance(nr.MEASURE_DEFAULTS[key], int, key)
            self.assertNotIsInstance(nr.MEASURE_DEFAULTS[key], bool, key)


class NormalizeTests(unittest.TestCase):
    def _report(self):
        """A report shaped like the real one's sparsest observed unit."""
        return {
            "version": 2,
            "measures": {"fuzzy_match_percent": 98.4, "total_units": 326},
            "units": [
                {   # data-only unit: the whole code family is omitted
                    "name": "main/auto_11_80345720_sdata2",
                    "measures": {"fuzzy_match_percent": 100.0,
                                 "total_data": "72", "total_units": 1},
                },
                {   # complete code unit with no data at all
                    "name": "main/game/anim/anim",
                    "measures": {"fuzzy_match_percent": 100.0,
                                 "total_code": "3392", "total_functions": 10,
                                 "matched_functions": 10, "total_units": 1},
                    "functions": [
                        {"name": "AnimInit", "size": "40",
                         "fuzzy_match_percent": 100.0, "address": "0"},
                        {"name": "Divergent"},   # every field defaulted away
                    ],
                },
            ],
        }

    def test_every_scope_ends_with_every_measure_key(self):
        report = self._report()
        nr.normalize(report)
        for scope in [report] + report["units"]:
            self.assertEqual(set(scope["measures"]),
                             set(nr.MEASURE_DEFAULTS), scope.get("name"))

    def test_a_scored_zero_function_is_no_longer_indistinguishable(self):
        """The original defect: an absent key read as 'not measured'."""
        report = self._report()
        nr.normalize(report)
        divergent = report["units"][1]["functions"][1]
        self.assertEqual(divergent["fuzzy_match_percent"], 0.0)
        self.assertEqual(divergent["size"], "0")
        self.assertEqual(divergent["address"], "0")

    def test_present_values_are_never_overwritten(self):
        report = self._report()
        nr.normalize(report)
        anim = report["units"][1]
        self.assertEqual(anim["measures"]["total_functions"], 10)
        self.assertEqual(anim["measures"]["matched_functions"], 10)
        self.assertEqual(anim["measures"]["total_code"], "3392")
        self.assertEqual(anim["functions"][0]["fuzzy_match_percent"], 100.0)
        self.assertEqual(report["measures"]["fuzzy_match_percent"], 98.4)

    def test_the_counts_report_what_was_actually_filled(self):
        report = self._report()
        counts = nr.normalize(report)
        # 3 scopes x 16 keys, minus the 2 + 3 + 5 already present.
        self.assertEqual(counts["measures"], 48 - 10)
        self.assertEqual(counts["functions"], 3)   # the bare Divergent row

    def test_it_is_idempotent(self):
        report = self._report()
        nr.normalize(report)
        frozen = json.dumps(report, sort_keys=True)
        counts = nr.normalize(report)
        self.assertEqual(counts, {"measures": 0, "functions": 0})
        self.assertEqual(json.dumps(report, sort_keys=True), frozen)

    def test_a_unit_with_no_functions_list_is_not_invented(self):
        report = self._report()
        nr.normalize(report)
        self.assertNotIn("functions", report["units"][0])

    def test_missing_measures_scope_is_tolerated(self):
        report = {"units": [{"name": "bare"}]}
        counts = nr.normalize(report)
        self.assertEqual(counts, {"measures": 0, "functions": 0})


if __name__ == "__main__":
    unittest.main()
