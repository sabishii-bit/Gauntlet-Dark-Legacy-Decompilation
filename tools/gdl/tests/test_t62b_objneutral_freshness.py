"""objneutral: staleness, provenance and the report that hides nothing.

THREE DEFECTS, each reproduced on the live critter object before this suite
was written (build/e_lane/e_repro_objneutral.py, e_repro_stale.py):

1. `check --limit 0` -- the documented "show everything" flag -- died with
   `TypeError: '>' not supported between instances of 'int' and 'NoneType'`
   on EVERY check, clean or not, because `args.limit or None` turned 0 into
   None and `len(interesting) > limit` then compared against it.

2. The default text report listed only the first 12 non-identical functions.
   Twenty patched functions produced twelve rows and a one-line footnote, and
   the only flag that would have shown the other eight was defect 1. A
   not-neutral edit therefore read as a short, tidy report.

3. `check` answered `VERDICT: NEUTRAL` on a STALE object. With the object's
   mtime pushed a week into the past, `ninja -n` printed
   `[1/1] MWCC build\\GUNE5D\\src\\game\\enemy\\critter.o` and `ninja -t deps`
   listed nine inputs all newer than it; objneutral compared the previous
   compile against a bank of the previous compile and exited 0.

THE FRESHNESS DISCRIMINANT, and why the exit code is not it: `ninja -n
<target>` exits 0 whether or not it printed pending work. The verdict is its
TEXT ("no work to do"), and a nonzero exit means only that the question
failed. The `-MMD` depfiles do not survive on disk either -- the build
declares `deps = gcc`, so Ninja consumes each `.d` into `.ninja_deps` -- so
the recorded dependency list is read back with `ninja -t deps`.

Staleness is asserted only on POSITIVE evidence. Anything that merely stops
the question from being answered is UNVERIFIED, reported as such, and never
silently treated as fresh.
"""
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))

from tools.gdl import objneutral  # noqa: E402

#: Verbatim shape of `ninja -t deps` output, two targets so the parser has to
#: select rather than take whatever it saw last.
DEPS_BLOB = """\
build/GUNE5D/src/game/enemy/enemy.o: #deps 2, deps mtime 8105566708734281 (VALID)
    src/game/enemy/enemy.c
    W:/Repositories/GDL-Claude-Tools62/include/game/enemy.h

build/GUNE5D/src/game/enemy/critter.o: #deps 3, deps mtime 8105566708734281 (VALID)
    src/game/enemy/critter.c
    W:/Repositories/GDL-Claude-Tools62/include/types.h
    W:/Repositories/GDL-Claude-Tools62/include/game/critter.h
"""


def result(functions, **extra):
    """A minimal check() result for format_check, with the new keys present."""
    counts = {}
    for row in functions:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
    base = {"unit": "game/enemy/critter", "tag": "t", "neutral": False,
            "banked": {"object": "build/GUNE5D/src/game/enemy/critter.o",
                       "object_sha256": "a" * 64, "head": "b" * 40,
                       "banked_at": "2026-09-08T00:00:00"},
            "current_object": "active compiler-stage output (raw)",
            "current_sha256": "c" * 64, "object_identical": False,
            "freshness": {"state": "FRESH", "reason": "test"},
            "git": {"head": "b" * 40}, "warnings": [],
            "counts": counts, "functions": functions, "limits": ["l"]}
    base.update(extra)
    return base


def changed(name, words=1):
    return {"function": name, "verdict": "CHANGED", "size_before": 64,
            "size_after": 64, "first_differing_byte": 8,
            "differing_words": words, "renumbered_relocations": [],
            "changed_relocations": []}


def renumbered(name, entries=1):
    return {"function": name, "verdict": "RENUMBERED", "size_before": 64,
            "size_after": 64, "renumbered_relocations":
            [{"offset": 4 * i, "before": "@%d" % i, "after": "@%d" % (i + 90)}
             for i in range(entries)],
            "changed_relocations": []}


class NinjaDepsParsing(unittest.TestCase):
    def test_it_selects_the_requested_target_not_the_last_block(self):
        inputs, state = objneutral.parse_ninja_deps(
            DEPS_BLOB, "build/GUNE5D/src/game/enemy/enemy.o")
        self.assertEqual(inputs, [
            "src/game/enemy/enemy.c",
            "W:/Repositories/GDL-Claude-Tools62/include/game/enemy.h"])
        self.assertEqual(state, "VALID")

    def test_it_reads_the_whole_recorded_header_list(self):
        inputs, state = objneutral.parse_ninja_deps(
            DEPS_BLOB, "build/GUNE5D/src/game/enemy/critter.o")
        self.assertEqual(len(inputs), 3)
        self.assertIn("W:/Repositories/GDL-Claude-Tools62/include/game/critter.h",
                      inputs)
        self.assertEqual(state, "VALID")

    def test_windows_separators_still_select_the_same_target(self):
        blob = DEPS_BLOB.replace("build/GUNE5D/src/game/enemy/critter.o",
                                 "build\\GUNE5D\\src\\game\\enemy\\critter.o")
        inputs, _ = objneutral.parse_ninja_deps(
            blob, "build/GUNE5D/src/game/enemy/critter.o")
        self.assertEqual(len(inputs), 3)

    def test_an_unknown_target_yields_no_inputs_not_another_targets(self):
        inputs, state = objneutral.parse_ninja_deps(DEPS_BLOB, "build/nope.o")
        self.assertEqual(inputs, [])
        self.assertIsNone(state)

    def test_a_stale_deps_record_is_reported_not_swallowed(self):
        blob = DEPS_BLOB.replace("(VALID)", "(STALE)")
        _, state = objneutral.parse_ninja_deps(
            blob, "build/GUNE5D/src/game/enemy/critter.o")
        self.assertEqual(state, "STALE")


class DryRunVerdict(unittest.TestCase):
    def test_no_work_to_do_is_up_to_date(self):
        self.assertEqual(objneutral.dry_run_verdict("ninja: no work to do.\n"),
                         "up-to-date")

    def test_a_printed_build_line_is_out_of_date_despite_exit_zero(self):
        # The whole trap: ninja -n exits 0 either way. Reproduced live as
        # `[1/1] MWCC build\GUNE5D\src\game\enemy\critter.o` with exit 0.
        self.assertEqual(objneutral.dry_run_verdict(
            "[1/1] MWCC build\\GUNE5D\\src\\game\\enemy\\critter.o\n", 0),
            "out-of-date")

    def test_a_nonzero_exit_is_unknown_never_up_to_date(self):
        for stdout in ("", "ninja: no work to do.\n", "ninja: error: unknown"):
            with self.subTest(stdout=stdout):
                self.assertEqual(objneutral.dry_run_verdict(stdout, 1),
                                 "unknown")


class NewerInputs(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def _write(self, name, mtime):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("x", encoding="utf-8")
        os.utime(path, (mtime, mtime))
        return path

    def test_a_newer_relative_input_is_named(self):
        self._write("src/a.c", 2000)
        rows = objneutral.newer_inputs(1000, ["src/a.c"], self.root)
        self.assertEqual([row["path"] for row in rows], ["src/a.c"])

    def test_a_newer_absolute_input_is_named_too(self):
        path = self._write("include/a.h", 2000)
        rows = objneutral.newer_inputs(1000, [str(path)], self.root)
        self.assertEqual(len(rows), 1)

    def test_an_older_input_is_not_named(self):
        self._write("src/a.c", 500)
        self.assertEqual(objneutral.newer_inputs(1000, ["src/a.c"], self.root),
                         [])

    def test_a_missing_input_does_not_fabricate_staleness(self):
        self.assertEqual(
            objneutral.newer_inputs(1000, ["src/gone.c"], self.root), [])

    def test_rows_are_ordered_newest_first(self):
        self._write("src/a.c", 2000)
        self._write("src/b.c", 3000)
        rows = objneutral.newer_inputs(1000, ["src/a.c", "src/b.c"], self.root)
        self.assertEqual([row["path"] for row in rows], ["src/b.c", "src/a.c"])


class FreshnessVerdict(unittest.TestCase):
    def test_both_signals_agreeing_is_fresh(self):
        verdict = objneutral.freshness_verdict("up-to-date", [], "VALID", True)
        self.assertEqual(verdict["state"], "FRESH")
        self.assertFalse(verdict["stale"])

    def test_ninja_pending_work_alone_is_stale(self):
        verdict = objneutral.freshness_verdict("out-of-date", [], "VALID", True)
        self.assertTrue(verdict["stale"])
        self.assertIn("ninja -n", verdict["reason"])

    def test_a_newer_recorded_input_alone_is_stale(self):
        verdict = objneutral.freshness_verdict(
            "up-to-date", [{"path": "include/game/critter.h", "mtime": 9}],
            "VALID", True)
        self.assertTrue(verdict["stale"])
        self.assertIn("newer", verdict["reason"])

    def test_an_unanswerable_dry_run_is_unverified_never_fresh(self):
        verdict = objneutral.freshness_verdict("unknown", [], "VALID", True)
        self.assertEqual(verdict["state"], "UNVERIFIED")
        self.assertFalse(verdict["stale"])

    def test_no_recorded_dependency_list_is_unverified_not_fresh(self):
        verdict = objneutral.freshness_verdict("up-to-date", [], None, False)
        self.assertEqual(verdict["state"], "UNVERIFIED")

    def test_ninjas_own_stale_deps_marker_is_unverified_not_fresh(self):
        verdict = objneutral.freshness_verdict("up-to-date", [], "STALE", True)
        self.assertEqual(verdict["state"], "UNVERIFIED")

    def test_fresh_requires_every_signal(self):
        for args in (("out-of-date", [], "VALID", True),
                     ("unknown", [], "VALID", True),
                     ("up-to-date", [{"path": "x", "mtime": 1}], "VALID", True),
                     ("up-to-date", [], "STALE", True),
                     ("up-to-date", [], None, False)):
            with self.subTest(args=args):
                self.assertNotEqual(
                    objneutral.freshness_verdict(*args)["state"], "FRESH")


class HeadProvenance(unittest.TestCase):
    def test_an_unmoved_head_warns_about_nothing(self):
        self.assertEqual(objneutral.head_warnings(
            {"head": "a" * 40, "head_tree": "t" * 40},
            {"head": "a" * 40, "head_tree": "t" * 40}), [])

    def test_a_moved_head_warns_and_names_both_commits(self):
        warnings = objneutral.head_warnings({"head": "a" * 40},
                                            {"head": "b" * 40})
        self.assertEqual(len(warnings), 1)
        self.assertIn("HEAD MOVED", warnings[0])
        self.assertIn("a" * 12, warnings[0])
        self.assertIn("b" * 12, warnings[0])

    def test_a_bank_without_a_recorded_head_warns_rather_than_passing(self):
        warnings = objneutral.head_warnings({}, {"head": "b" * 40})
        self.assertEqual(len(warnings), 1)
        self.assertIn("no HEAD commit", warnings[0])

    def test_an_amended_commit_at_the_same_id_is_impossible_but_a_new_tree_is_not(self):
        warnings = objneutral.head_warnings(
            {"head": "a" * 40, "head_tree": "1" * 40},
            {"head": "a" * 40, "head_tree": "2" * 40})
        self.assertEqual(len(warnings), 1)
        self.assertIn("different tree id", warnings[0])

    def test_an_unreadable_current_head_is_reported_not_ignored(self):
        warnings = objneutral.head_warnings({"head": "a" * 40}, {"head": None})
        self.assertEqual(len(warnings), 1)
        self.assertIn("could not be read", warnings[0])


class ReportHidesNothing(unittest.TestCase):
    def test_every_changed_function_prints_at_the_default_limit(self):
        rows = [changed("fn%02d" % i) for i in range(20)]
        text = objneutral.format_check(result(rows), limit=12)
        for row in rows:
            self.assertIn(row["function"], text)
        self.assertEqual(text.count("    CHANGED    "), 20)

    def test_every_changed_function_prints_even_at_limit_one(self):
        rows = [changed("fn%02d" % i) for i in range(20)]
        text = objneutral.format_check(result(rows), limit=1)
        self.assertEqual(text.count("    CHANGED    "), 20)

    def test_added_and_removed_are_never_elided_either(self):
        rows = [{"function": "gone", "verdict": "REMOVED", "size": 4},
                {"function": "new", "verdict": "ADDED", "size": 4}]
        rows += [changed("fn%02d" % i) for i in range(30)]
        text = objneutral.format_check(result(rows), limit=1)
        self.assertIn("REMOVED    gone", text)
        self.assertIn("ADDED      new", text)

    def test_limit_none_does_not_raise_and_prints_all_detail(self):
        rows = [renumbered("fn%02d" % i, entries=9) for i in range(20)]
        text = objneutral.format_check(result(rows, neutral=True), limit=None)
        self.assertEqual(text.count("    RENUMBERED "), 20)
        self.assertNotIn("... ", text)

    def test_limit_caps_the_detail_listing_and_says_how_much_it_withheld(self):
        text = objneutral.format_check(
            result([renumbered("fn", entries=9)], neutral=True), limit=2)
        self.assertEqual(text.count("renumbered +0x"), 2)
        self.assertIn("... 7 more renumbered pool entr", text)

    def test_the_neutral_renumbered_listing_says_it_was_capped(self):
        rows = [renumbered("fn%02d" % i) for i in range(20)]
        text = objneutral.format_check(result(rows, neutral=True), limit=12)
        self.assertIn("... 8 more RENUMBERED function(s)", text)

    def test_warnings_print_above_the_verdict(self):
        text = objneutral.format_check(
            result([changed("fn")], warnings=["HEAD MOVED since ..."]))
        self.assertIn("WARNING: HEAD MOVED", text)
        self.assertLess(text.index("WARNING:"), text.index("VERDICT:"))

    def test_the_freshness_state_is_always_on_the_report(self):
        text = objneutral.format_check(
            result([changed("fn")],
                   freshness={"state": "UNVERIFIED", "reason": "no ninja"}))
        self.assertIn("freshness UNVERIFIED -- no ninja", text)


UNIT = "game/enemy/critter"
OBJECT = ROOT / "build/GUNE5D/src/game/enemy/critter.o"
LIVE = OBJECT.is_file()


@unittest.skipUnless(LIVE, "needs a built object")
class LiveFreshnessAndLimit(unittest.TestCase):
    TAG = "e62b_test"

    def setUp(self):
        self.object_path, self.meta_path = objneutral.bank_paths(UNIT, self.TAG)
        self.addCleanup(self.cleanup)
        # The fixture is refreshed by CI before the suite, but any test that
        # runs before this module can dirty it again (CI measured exactly
        # that: `ninja -n` clean right after the rebuild, STALE at bank
        # time). Refresh it here so freshness is measured when it matters,
        # and let a refusal carry ninja's own explanation.
        target = OBJECT.relative_to(ROOT).as_posix()
        subprocess.run(["ninja", target], cwd=str(ROOT),
                       capture_output=True, text=True)
        done = self.run_tool("bank", "--tag", self.TAG)
        if done.returncode != 0:
            explain = subprocess.run(["ninja", "-d", "explain", "-n", target],
                                     cwd=str(ROOT), capture_output=True,
                                     text=True)
            self.fail(done.stdout + done.stderr
                      + "\n--- ninja -d explain -n ---\n"
                      + explain.stdout + explain.stderr)

    def cleanup(self):
        for path in (self.object_path, self.meta_path):
            if path.exists():
                path.unlink()

    def run_tool(self, action, *flags):
        return subprocess.run(
            [sys.executable, "tools/gdl/objneutral.py", action, UNIT, *flags],
            cwd=str(ROOT), capture_output=True, text=True)

    def test_a_fresh_object_is_reported_fresh_and_the_bank_records_head(self):
        meta = json.loads(self.meta_path.read_text(encoding="utf-8"))
        self.assertEqual(meta["schema_version"], 2)
        self.assertEqual(meta["freshness"]["state"], "FRESH")
        self.assertRegex(str(meta["head"]), r"^[0-9a-f]{40}$")
        self.assertRegex(str(meta["head_tree"]), r"^[0-9a-f]{40}$")
        self.assertIn("worktree_status_sha256", meta)
        self.assertIn("banked_at", meta)

    def test_limit_zero_prints_the_report_instead_of_crashing(self):
        done = self.run_tool("check", "--tag", self.TAG, "--limit", "0")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertNotIn("Traceback", done.stderr)
        self.assertIn("VERDICT: NEUTRAL", done.stdout)

    def test_a_stale_object_refuses_with_exit_two_and_names_an_input(self):
        stat = OBJECT.stat()
        original = (stat.st_atime, stat.st_mtime)
        past = time.time() - 7 * 24 * 3600
        os.utime(OBJECT, (past, past))
        try:
            done = self.run_tool("check", "--tag", self.TAG)
        finally:
            os.utime(OBJECT, original)
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("REFUSED", done.stdout)
        self.assertIn("STALE", done.stdout)
        self.assertIn("critter.c", done.stdout)

    def test_banking_a_stale_object_refuses_too(self):
        stat = OBJECT.stat()
        original = (stat.st_atime, stat.st_mtime)
        past = time.time() - 7 * 24 * 3600
        os.utime(OBJECT, (past, past))
        try:
            done = self.run_tool("bank", "--tag", self.TAG + "_stale")
        finally:
            os.utime(OBJECT, original)
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("STALE", done.stdout)
        stale_object, stale_meta = objneutral.bank_paths(UNIT,
                                                         self.TAG + "_stale")
        self.assertFalse(stale_object.exists())
        self.assertFalse(stale_meta.exists())

    def test_a_moved_head_warns_but_still_compares(self):
        meta = json.loads(self.meta_path.read_text(encoding="utf-8"))
        meta["head"] = "0" * 40
        self.meta_path.write_text(json.dumps(meta), encoding="utf-8")
        done = self.run_tool("check", "--tag", self.TAG, "--json")
        self.assertEqual(done.returncode, 0, done.stdout[:400])
        payload = json.loads(done.stdout)
        self.assertTrue(payload["neutral"])
        self.assertTrue(any("HEAD MOVED" in w for w in payload["warnings"]))

    def test_the_live_freshness_probe_agrees_with_ninja(self):
        state = objneutral.freshness(OBJECT)
        self.assertTrue(state["checked"], state)
        self.assertEqual(state["state"], "FRESH", state)
        self.assertGreater(state["inputs"], 0)


if __name__ == "__main__":
    unittest.main()
