"""T25 run-55 item 4: re-check a record's quoted numbers at commit time.

REPORTED (CR, run 54): "I quoted a record number one commit stale ... a
multi-probe pass invalidates its own earlier measurements, and nothing in
the loop re-checks them at commit time."

The extractor's two hardest cases are both false-verdict classes found by
READING the calibration output rather than counting it: an improvement
written as a TRANSITION (`real 68 -> 66`) whose right-hand side is the kept
value, and a record spanning several functions, where a metric cannot be
attributed to any one of them. Both are pinned here.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import t25_record_recheck as rc  # noqa: E402


class ParseAssertionTests(unittest.TestCase):
    def test_the_word_screen_metrics_are_read(self):
        got = rc.parse_assertions(
            "244 insns, DIFFERING WORDS = 53, MNEMONIC DIVERGENCE = 0,"
            " RELOC-SYMBOL MISMATCH = 0")
        self.assertEqual(got["differing_words"], [53])
        self.assertEqual(got["mnemonic_divergence"], [0])
        self.assertEqual(got["reloc_symbol_mismatch"], [0])

    def test_a_transition_contributes_both_sides(self):
        """`IMPROVED real 68 -> 66` quotes the BEFORE and the KEPT value;
        reading only the first calls the record stale against its own
        result, which it did on the CR record before this was added."""
        got = rc.parse_assertions("IMPROVED real 68 -> 66 (insns T148/O148)")
        self.assertIn(68, got["real"])
        self.assertIn(66, got["real"])

    def test_both_insn_spellings_are_read(self):
        """probe prints `insns T148/O148`; fndiff --count prints
        `insns 148/148`. A screen knowing one reports the other as
        NOT-MEASURED."""
        self.assertEqual(
            rc.parse_assertions("insns T148/O148")["insns"], [(148, 148)])
        self.assertEqual(
            rc.parse_assertions("insns 148/148")["insns"], [(148, 148)])

    def test_a_record_with_no_metric_is_empty(self):
        self.assertEqual(rc.parse_assertions("a prose park with no numbers"),
                         {})


class ParseCommandTests(unittest.TestCase):
    def test_a_quoted_command_names_the_unit_and_function(self):
        got = rc.parse_commands(
            "`python tools/gdl/composed_census/wf_word_diff.py"
            " game/enemy/critter CritterCollideItems`")
        self.assertEqual(got, [("wf_word_diff", "game/enemy/critter",
                                "CritterCollideItems")])

    def test_the_bare_spelling_is_recognised_too(self):
        got = rc.parse_commands("probe.py game/enemy/enemy.c move_logic22")
        self.assertEqual(got[0][1:], ("game/enemy/enemy.c", "move_logic22"))

    def test_probe_is_recognised_but_not_runnable(self):
        """It builds and it banks per-function state; reading the unit out
        of it is safe, running it from a re-check is not."""
        self.assertNotIn("probe", rc.RUNNABLE)


class CompareTests(unittest.TestCase):
    def test_a_live_value_among_the_quoted_ones_holds(self):
        rows = rc.compare({"real": [68, 66]}, {"real": 66})
        self.assertEqual(rows, [("real", [68, 66], 66, "HELD")])

    def test_a_live_value_nowhere_in_the_record_moved(self):
        rows = rc.compare({"differing_words": [122]},
                          {"differing_words": 66})
        self.assertEqual(rows[0][3], "MOVED")

    def test_an_unmeasurable_metric_is_not_a_pass(self):
        rows = rc.compare({"differing_words": [122]}, {})
        self.assertEqual(rows[0][3], "NOT-MEASURED")

    def test_a_metric_the_record_never_quotes_is_not_reported(self):
        self.assertEqual(rc.compare({}, {"real": 66}), [])


class MainVerdictTests(unittest.TestCase):
    def _run(self, record, *argv):
        path = Path(tempfile.mkdtemp(prefix="t25rec-")) / "draft.json"
        path.write_text(json.dumps(record), encoding="utf-8")
        out = []
        with mock.patch.object(sys, "argv",
                               ["t25_record_recheck.py", str(path), *argv]), \
                mock.patch("builtins.print", side_effect=out.append):
            code = rc.main()
        return code, "\n".join(str(line) for line in out)

    def test_a_multi_function_record_is_ambiguous_not_stale(self):
        """claim.CX_expiry-check-sweep-...20260903.v1 quotes four functions'
        differing-word counts and one function's command; every number was
        charged to that one function and it read STALE."""
        code, text = self._run({
            "id": "claim.multi.v1",
            "attributes": {"verification":
                           "DIFFERING WORDS = 122 and DIFFERING WORDS = 66;"
                           " fndiff.py game/a/b fn_one and"
                           " fndiff.py game/a/b fn_two"}})
        self.assertEqual(code, 0)
        self.assertIn("AMBIGUOUS", text)
        self.assertNotIn("STALE", text)

    def test_a_record_with_numbers_but_no_command_says_unanchored(self):
        code, text = self._run({
            "id": "attempt.unanchored.v1",
            "attributes": {"verification": "DIFFERING WORDS = 53"}})
        self.assertEqual(code, 0)
        self.assertIn("UNANCHORED", text)

    def test_a_record_with_no_metric_says_so(self):
        code, text = self._run({"id": "attempt.plain.v1",
                                "attributes": {"verification": "looks fine"}})
        self.assertEqual(code, 0)
        self.assertIn("NO RE-CHECKABLE METRIC", text)

    def test_gate_exits_one_only_when_something_moved(self):
        record = {"id": "attempt.stale.v1",
                  "attributes": {"verification":
                                 "DIFFERING WORDS = 122;"
                                 " wf_word_diff.py game/a/b fn_one"}}
        with mock.patch.object(rc, "live_metrics",
                               return_value=({"differing_words": 66}, ["x"])):
            code, text = self._run(record, "--gate")
        self.assertEqual(code, 1)
        self.assertIn("STALE", text)
        with mock.patch.object(rc, "live_metrics",
                               return_value=({"differing_words": 122}, ["x"])):
            code, text = self._run(record, "--gate")
        self.assertEqual(code, 0)
        self.assertIn("HELD", text)

    def test_without_gate_a_stale_record_still_exits_zero(self):
        """wf_word_diff's rule: a nonzero exit means the measurement did not
        happen, never that its answer was unwelcome."""
        record = {"id": "attempt.stale2.v1",
                  "attributes": {"verification":
                                 "real 68; fndiff.py game/a/b fn_one"}}
        with mock.patch.object(rc, "live_metrics",
                               return_value=({"real": 66}, ["x"])):
            code, _text = self._run(record)
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
