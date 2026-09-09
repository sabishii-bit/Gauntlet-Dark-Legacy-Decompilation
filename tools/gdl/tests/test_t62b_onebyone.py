"""onebyone: apply each candidate edit ALONE and measure it.

WHY THE TOOL EXISTS. Run 62 produced eight private one-off drivers for the
same loop -- `a_paddrive.py`, `a_pragmadrive.py`, `a_paddrive2.py`,
`a_pragmadrive2.py`, `a_pairprobe.py`, `a_castdrive.py`, `a_offedrive.py`,
`d_hdrgate.py` -- and each rediscovered the same failure modes. The three
this suite pins are the ones that produced wrong FINDINGS, not just wrong
runs:

  * A substitution that binds NOTHING compiles fine and reports NEUTRAL. That
    verdict reads as "this edit is inert" when it means "the edit never
    happened". `apply_edits` refuses instead.
  * A scoped pragma pair measured one half at a time is a different
    experiment: `a_pairprobe.py` had to re-probe the pairs
    `a_pragmadrive2.py` had already measured singly and found 4 inert pairs
    neither half showed. Same `group` = applied together.
  * A `static` with no symbol in the object was inlined, so ITS edit shows up
    in its CALLERS' bodies -- and the changed-function list then names the
    callers. Attributing the change to the caller's own source is the
    caller/inlinee trap, and the report warns on it.

The restore path is the other half: every probe restores the file in a
`finally`, and the run refuses to start when a file it would touch is dirty,
because the committed content is what it restores to.
"""
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS / "composed_census"))
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT))

import objneutral  # noqa: E402
import onebyone  # noqa: E402


class CandidateParsing(unittest.TestCase):
    def test_each_row_without_a_group_is_its_own_candidate(self):
        rows = onebyone.load_candidates([
            {"file": "a.c", "old": "x", "new": "y"},
            {"file": "a.c", "old": "p", "new": "q"}])
        self.assertEqual(len(rows), 2)
        self.assertEqual([len(row["edits"]) for row in rows], [1, 1])

    def test_rows_sharing_a_group_become_one_candidate(self):
        rows = onebyone.load_candidates([
            {"file": "a.c", "old": "#pragma opt off", "new": "",
             "group": "opt-pair"},
            {"file": "a.c", "old": "#pragma opt reset", "new": "",
             "group": "opt-pair"}])
        self.assertEqual(len(rows), 1)
        self.assertEqual(len(rows[0]["edits"]), 2)
        self.assertEqual(rows[0]["id"], "opt-pair")

    def test_group_order_is_the_order_the_author_wrote(self):
        rows = onebyone.load_candidates([
            {"file": "a.c", "old": "1", "new": "one", "group": "g"},
            {"file": "a.c", "old": "2", "new": "two", "group": "g"}])
        self.assertEqual([edit["old"] for edit in rows[0]["edits"]],
                         ["1", "2"])

    def test_a_group_may_span_several_files(self):
        rows = onebyone.load_candidates([
            {"file": "h.h", "old": "u8 *p;", "new": "Rec *p;", "group": "g"},
            {"file": "a.c", "old": "p + 4", "new": "&p[1]", "group": "g"}])
        self.assertEqual(len(rows), 1)
        self.assertEqual(sorted(e["file"] for e in rows[0]["edits"]),
                         ["a.c", "h.h"])

    def test_a_non_list_payload_refuses(self):
        with self.assertRaisesRegex(onebyone.Refused, "JSON LIST"):
            onebyone.load_candidates({"file": "a.c"})

    def test_a_missing_field_refuses(self):
        with self.assertRaises(onebyone.Refused):
            onebyone.load_candidates([{"file": "a.c", "old": "x"}])

    def test_an_empty_old_refuses(self):
        with self.assertRaises(onebyone.Refused):
            onebyone.load_candidates([{"file": "a.c", "old": "", "new": "y"}])

    def test_a_no_op_candidate_refuses(self):
        with self.assertRaisesRegex(onebyone.Refused, "changes nothing"):
            onebyone.load_candidates([{"file": "a.c", "old": "x", "new": "x"}])

    def test_a_deletion_to_empty_text_is_legal(self):
        rows = onebyone.load_candidates(
            [{"file": "a.c", "old": "#pragma opt off\n", "new": ""}])
        self.assertEqual(rows[0]["edits"][0]["new"], "")


class Substitution(unittest.TestCase):
    def test_it_substitutes_and_counts(self):
        text, counts = onebyone.apply_edits(
            "a b a", [{"file": "a.c", "old": "a", "new": "z"}])
        self.assertEqual(text, "z b z")
        self.assertEqual(counts, [2])

    def test_a_substitution_that_binds_nothing_refuses(self):
        # The whole point: a silent no-op compiles and reports NEUTRAL.
        with self.assertRaisesRegex(onebyone.Refused, "text not found"):
            onebyone.apply_edits(
                "a b c", [{"file": "a.c", "old": "zzz", "new": "y"}])

    def test_an_unexpected_occurrence_count_refuses(self):
        with self.assertRaisesRegex(onebyone.Refused, "expected 1"):
            onebyone.apply_edits(
                "a b a", [{"file": "a.c", "old": "a", "new": "z",
                           "count": 1}])

    def test_a_matching_expected_count_is_accepted(self):
        text, _ = onebyone.apply_edits(
            "a b a", [{"file": "a.c", "old": "a", "new": "z", "count": 2}])
        self.assertEqual(text, "z b z")

    def test_edits_in_a_group_apply_in_order_and_all_bind(self):
        text, counts = onebyone.apply_edits("off ... reset", [
            {"file": "a.c", "old": "off", "new": "OFF"},
            {"file": "a.c", "old": "reset", "new": "RESET"}])
        self.assertEqual(text, "OFF ... RESET")
        self.assertEqual(counts, [1, 1])


class RebuildSettling(unittest.TestCase):
    """Wibo's first output timestamp may need one real Ninja retry."""

    @staticmethod
    def completed(returncode=0, stdout="", stderr=""):
        return subprocess.CompletedProcess([], returncode, stdout, stderr)

    def test_a_settled_first_build_is_not_repeated(self):
        responses = [
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="ninja: no work to do.\n"),
        ]
        with mock.patch.object(onebyone.subprocess, "run",
                               side_effect=responses) as run:
            ok, _message = onebyone.rebuild("x.o")
        self.assertTrue(ok)
        self.assertEqual(run.call_count, 2)

    def test_one_pending_dry_run_gets_one_real_retry(self):
        responses = [
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="ninja: no work to do.\n"),
        ]
        with mock.patch.object(onebyone.subprocess, "run",
                               side_effect=responses) as run:
            ok, _message = onebyone.rebuild("x.o")
        self.assertTrue(ok)
        self.assertEqual(run.call_count, 4)

    def test_persistent_pending_work_fails_closed(self):
        responses = [
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="[1/1] MWCC x.o\n"),
            self.completed(stdout="[1/1] MWCC x.o\n"),
        ]
        with mock.patch.object(onebyone.subprocess, "run",
                               side_effect=responses):
            ok, message = onebyone.rebuild("x.o")
        self.assertFalse(ok)
        self.assertIn("remained pending", message)

class SourceIO(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "a.c"

    def test_crlf_survives_a_read_and_write_round_trip(self):
        self.path.write_bytes(b"int a;\r\nint b;\r\n")
        text = onebyone.read_source(self.path)
        self.assertIn("\r\n", text)
        onebyone.write_source(self.path, text)
        self.assertEqual(self.path.read_bytes(), b"int a;\r\nint b;\r\n")

    def test_lf_stays_lf(self):
        self.path.write_bytes(b"int a;\nint b;\n")
        onebyone.write_source(self.path, onebyone.read_source(self.path))
        self.assertEqual(self.path.read_bytes(), b"int a;\nint b;\n")

    def test_writing_moves_the_mtime_forward(self):
        self.path.write_bytes(b"x")
        past = time.time() - 3600
        os.utime(self.path, (past, past))
        onebyone.write_source(self.path, "y")
        self.assertGreater(self.path.stat().st_mtime, past + 1)


SOURCE = """\
#include "x.h"

static inline int Helper(int v)
{
    return v + 1;
}

void Caller(int v)
{
    use(Helper(v));
}

void Other(void)
{
    use(0);
}
"""


class InlineeTrap(unittest.TestCase):
    def test_static_spans_are_found_with_their_line_range(self):
        spans = onebyone.static_function_spans(SOURCE)
        self.assertEqual([span["name"] for span in spans], ["Helper"])
        self.assertEqual(spans[0]["first_line"], 3)
        self.assertEqual(spans[0]["last_line"], 6)

    def test_a_static_prototype_is_not_a_definition(self):
        self.assertEqual(
            onebyone.static_function_spans("static int Helper(int v);\n"), [])

    def test_edit_lines_reports_every_occurrence(self):
        self.assertEqual(onebyone.edit_lines(SOURCE, "return v + 1;"), [5])
        self.assertEqual(onebyone.edit_lines(SOURCE, "use("), [10, 15])

    def test_it_warns_when_an_inlined_static_holds_the_edit(self):
        warnings = onebyone.inlining_warnings(
            SOURCE, [{"file": "a.c", "old": "return v + 1;", "new": "return v;"}],
            emitted_names={"Caller", "Other"}, changed_names={"Caller"})
        self.assertEqual(len(warnings), 1)
        self.assertIn("Helper", warnings[0])
        self.assertIn("Caller", warnings[0])

    def test_it_stays_quiet_when_the_static_has_its_own_symbol(self):
        warnings = onebyone.inlining_warnings(
            SOURCE, [{"file": "a.c", "old": "return v + 1;", "new": "return v;"}],
            emitted_names={"Caller", "Other", "Helper"},
            changed_names={"Caller"})
        self.assertEqual(warnings, [])

    def test_it_stays_quiet_when_nothing_changed(self):
        warnings = onebyone.inlining_warnings(
            SOURCE, [{"file": "a.c", "old": "return v + 1;", "new": "return v;"}],
            emitted_names={"Caller"}, changed_names=set())
        self.assertEqual(warnings, [])

    def test_it_stays_quiet_when_the_edit_is_outside_every_static(self):
        warnings = onebyone.inlining_warnings(
            SOURCE, [{"file": "a.c", "old": "use(0);", "new": "use(1);"}],
            emitted_names={"Caller", "Other"}, changed_names={"Other"})
        self.assertEqual(warnings, [])


class RunRefusals(unittest.TestCase):
    def test_a_missing_file_refuses_before_anything_is_touched(self):
        with self.assertRaisesRegex(onebyone.Refused, "no such file"):
            onebyone.run("game/sound/sounds", [
                {"id": "x", "group": "x", "edits": [
                    {"file": "src/game/e62b_absent.c", "old": "a",
                     "new": "b"}]}])

    def test_a_dirty_file_refuses_and_names_it(self):
        original = onebyone.dirty_files
        onebyone.dirty_files = lambda paths: ["src/game/sound/sounds.c"]
        try:
            with self.assertRaisesRegex(onebyone.Refused, "refusing to start"):
                onebyone.run("game/sound/sounds", [
                    {"id": "x", "group": "x", "edits": [
                        {"file": "src/game/sound/sounds.c", "old": "a",
                         "new": "b"}]}])
        finally:
            onebyone.dirty_files = original

    def test_dirty_files_sees_a_real_untracked_file(self):
        probe = TOOLS / "tests" / "e62b_dirty_probe.txt"
        probe.write_text("scratch\n", encoding="utf-8")
        self.addCleanup(lambda: probe.exists() and probe.unlink())
        rel = probe.relative_to(ROOT).as_posix()
        self.assertIn(rel, onebyone.dirty_files([rel]))

    def test_dirty_files_is_empty_for_a_committed_file(self):
        # raw_object.py is not this lane's to edit, so it is committed and
        # clean in any worktree this suite runs in.
        self.assertEqual(onebyone.dirty_files(["tools/gdl/raw_object.py"]), [])

    def test_the_cli_refuses_a_bad_candidate_file_with_exit_two(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "c.json"
            path.write_text(json.dumps({"not": "a list"}), encoding="utf-8")
            done = subprocess.run(
                [sys.executable, "tools/gdl/composed_census/onebyone.py",
                 "game/sound/sounds", str(path)],
                cwd=str(ROOT), capture_output=True, text=True)
            self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
            self.assertIn("ONEBYONE REFUSED", done.stdout)

    def test_the_cli_refuses_an_out_path_outside_build(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/composed_census/onebyone.py",
             "game/sound/sounds", str(FIXTURE), "--out", "src/leak.json"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 2)


class Formatting(unittest.TestCase):
    def test_the_report_names_every_verdict_and_the_limits(self):
        text = onebyone.format_run({
            "unit": "u", "tag": "t", "candidates": 2, "files": ["a.c"],
            "limits": ["no link is run"],
            "rows": [{"id": "one", "verdict": "NEUTRAL", "files": ["a.c"]},
                     {"id": "two", "verdict": "CHANGED", "files": ["a.c"],
                      "changed": [{"function": "f", "verdict": "CHANGED",
                                   "differing_words": 3, "size_before": 8,
                                   "size_after": 12}],
                      "warnings": ["static H was inlined"]}]})
        self.assertIn("NEUTRAL", text)
        self.assertIn("CHANGED       two", text)
        self.assertIn("3 word(s)", text)
        self.assertIn("WARNING: static H was inlined", text)
        self.assertIn("LIMITS:", text)

    def test_a_compile_fail_prints_its_detail(self):
        text = onebyone.format_run({
            "unit": "u", "tag": "t", "candidates": 1, "files": ["a.c"],
            "limits": [], "rows": [{"id": "one", "verdict": "COMPILE-FAIL",
                                    "files": ["a.c"],
                                    "detail": "FAILED: build/x.o"}]})
        self.assertIn("COMPILE-FAIL", text)
        self.assertIn("FAILED: build/x.o", text)


UNIT = "game/sound/sounds"
SRC = ROOT / "src/game/sound/sounds.c"
#: The candidate list the tool's docstring documents its live run with.
FIXTURE = TOOLS / "tests" / "fixtures" / "t62b_onebyone_live.json"
LIVE = (SRC.is_file()
        and (ROOT / "build/GUNE5D/src/game/sound/sounds.o").is_file())


@unittest.skipUnless(LIVE, "needs the sounds unit built")
class LiveLoop(unittest.TestCase):
    """The documented live run, re-executed: one inert edit, one that moves.

    Two real compiles plus a restoring rebuild, so it is the slowest test
    here; it is also the only one that proves the Ninja edge, the bank
    comparison and the restore actually compose.
    """

    TAG = "e62b_unittest"

    def setUp(self):
        self.before = SRC.read_bytes()
        self.paths = objneutral.bank_paths(UNIT, self.TAG)
        self.addCleanup(self.cleanup)

    def cleanup(self):
        if SRC.read_bytes() != self.before:
            SRC.write_bytes(self.before)
        for path in self.paths:
            if path.exists():
                path.unlink()

    def test_the_two_candidate_run_separates_inert_from_load_bearing(self):
        candidates = onebyone.load_candidates(
            json.loads(FIXTURE.read_text(encoding="utf-8")))
        result = onebyone.run(UNIT, candidates, self.TAG, bank_first=True)
        verdicts = {row["id"]: row["verdict"] for row in result["rows"]}
        self.assertEqual(verdicts["comment-only"], "NEUTRAL")
        self.assertEqual(verdicts["welcome-extra-224-to-225"], "CHANGED")
        changed = next(row for row in result["rows"]
                       if row["verdict"] == "CHANGED")["changed"]
        self.assertEqual([row["function"] for row in changed],
                         ["AudioWelcome"])
        self.assertEqual(changed[0]["differing_words"], 1)

    def test_the_source_is_byte_identical_afterwards(self):
        candidates = onebyone.load_candidates(
            json.loads(FIXTURE.read_text(encoding="utf-8")))
        onebyone.run(UNIT, candidates, self.TAG, bank_first=True)
        self.assertEqual(SRC.read_bytes(), self.before)

    def test_a_binding_failure_mid_run_still_restores_the_file(self):
        candidates = onebyone.load_candidates([
            {"id": "good", "file": "src/game/sound/sounds.c",
             "old": "/* --- sound-engine callees",
             "new": "/* --- sound engine callees"},
            {"id": "unbindable", "file": "src/game/sound/sounds.c",
             "old": "THIS TEXT IS NOT IN THE FILE", "new": "x"}])
        result = onebyone.run(UNIT, candidates, self.TAG, bank_first=True)
        verdicts = {row["id"]: row["verdict"] for row in result["rows"]}
        self.assertEqual(verdicts["good"], "NEUTRAL")
        self.assertEqual(verdicts["unbindable"], "REFUSED")
        self.assertEqual(SRC.read_bytes(), self.before)


if __name__ == "__main__":
    unittest.main()
