"""Fail-closed compiler sweep contracts; no private input or compiler required."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import cv_probe as cv


class SweepTests(unittest.TestCase):
    def run_fixture(self, root, *, args=(), drift=False, reject=False, empty=False):
        (root / "source.c").write_text("int f(void) { return 1; }\n#pragma optimization_level 4\n")
        (root / "body.o").write_bytes(b"BASE")
        target = root / "build/GUNE5D/obj/fake.o"
        target.parent.mkdir(parents=True)
        target.write_bytes(b"TARGET")
        edge = dict(src="source.c", mw="GC/fake", cflags="-O4", body_o="body.o", rule="mwcc")
        calls = []

        def compile_fake(edge, mw, flags, output, workdir):
            calls.append(flags)
            if reject and len(calls) > 1:
                return None, "FAIL compile (exit 1): unknown option"
            Path(output).write_bytes(b"WRONG" if drift else b"BASE")
            return Path(output), None

        def parse_fake(path):
            return {} if empty else {"f": ["li r3, 1", "blr"], "control": ["blr"]}

        with patch.object(cv, "REPO", root), \
             patch.object(cv, "read_edges", return_value={"fake": edge}), \
             patch.object(cv, "compile_with", side_effect=compile_fake), \
             patch.object(cv.matchtool, "parse", side_effect=parse_fake):
            rc = cv.main(["fake", "--out", str(root / "result.json"), *args])
        return rc, json.loads((root / "result.json").read_text()), calls

    def test_wrong_baseline_refuses_every_mode_before_variants(self):
        for axis in cv.AXES:
            with self.subTest(axis=axis), tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
                rc, result, calls = self.run_fixture(Path(td), args=["--axes", axis], drift=True)
                self.assertEqual((rc, result["status"], len(calls)), (2, "UNRESOLVED", 1))
                self.assertFalse(result["baseline"]["fidelity"])
                self.assertEqual(result["variants"], [])

    def test_check_success_is_not_target_match_claim(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            rc, result, calls = self.run_fixture(Path(td))
            self.assertEqual((rc, result["status"], len(calls)), (0, "PASS", 1))
            self.assertEqual(result["schema_version"], 1)
            self.assertFalse(result["coverage"]["exhaustive"])

    def test_unknown_axis_and_invalid_jobs_refuse_without_compiling(self):
        for args in (["--axes", "everything"], ["-j", "0"]):
            with self.subTest(args=args), tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
                rc, result, calls = self.run_fixture(Path(td), args=args)
                self.assertEqual((rc, result["status"], calls), (2, "UNRESOLVED", []))

    def test_rejected_variants_not_counted_as_negatives_or_success(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            rc, result, calls = self.run_fixture(Path(td), args=["--axes", "align", "--fn", "f"], reject=True)
            self.assertEqual((rc, result["status"]), (1, "FAIL"))
            self.assertEqual(result["coverage"]["completed_variants"], 1)
            self.assertEqual(result["coverage"]["requested_variants"], 5)
            self.assertEqual(len(calls), 5)  # baseline is reused, not compiled twice

    def test_duplicate_outputs_and_insensitive_control_are_explicit(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            rc, result, calls = self.run_fixture(Path(td), args=["--axes", "align", "--fn", "f", "--control-fn", "control"])
            self.assertEqual(rc, 0)
            self.assertEqual(len(result["duplicate_outputs"][0]["variants"]), 5)
            self.assertEqual({row["status"] for row in result["controls"]}, {"UNRESOLVED"})
            self.assertEqual(result["source_pragmas"]["effective_options"], "UNRESOLVED")

    def test_empty_parser_is_not_vacuous_success(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            rc, result, calls = self.run_fixture(Path(td), args=["--axes", "align"], empty=True)
            self.assertEqual(rc, 2)
            self.assertIn("empty", result["reasons"][0])
            self.assertEqual(len(calls), 1)

    def test_unknown_function_refuses_not_miss_score(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            rc, result, calls = self.run_fixture(Path(td), args=["--axes", "align", "--fn", "missing"])
            self.assertEqual(rc, 2)
            self.assertIn("not present", result["reasons"][0])
            self.assertEqual(len(calls), 1)

    def test_unknown_programmatic_axis_raises(self):
        with self.assertRaisesRegex(ValueError, "unknown axes"):
            cv.variants({"mw": "GC/fake", "cflags": ""}, "bogus")


class CompilerExecutionTests(unittest.TestCase):
    def run_compile(self, root, responses, *, extab=False):
        output = root / "out.o"
        output.write_bytes(b"STALE")
        trace = []
        commands = [("compile", ["compiler", "-bad-option"])]
        if extab:
            commands.append(("extab_clean", ["dtk", "extab", "clean"]))

        def fake_run(argv, **kwargs):
            response = responses.pop(0)
            if isinstance(response, Exception):
                raise response
            rc, messages, emit = response
            if emit:
                output.write_bytes(b"NEW")
            return subprocess.CompletedProcess(argv, rc, stdout=messages, stderr="")

        with patch.object(cv, "compile_commands", return_value=commands), \
             patch.object(cv.subprocess, "run", side_effect=fake_run):
            got, err = cv.compile_with({"_command_trace": trace}, "GC/fake", "", output, root)
        return got, err, trace, output

    def test_cleanup_failure_is_not_ignored(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            got, err, trace, _ = self.run_compile(Path(td), [(0, "", True), (1, "bad padding", True)], extab=True)
            self.assertIsNone(got)
            self.assertIn("FAIL extab_clean", err)
            self.assertEqual([row["status"] for row in trace], ["PASS", "FAIL"])

    def test_stale_output_cannot_mask_no_output(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            got, err, trace, output = self.run_compile(Path(td), [(0, "", False)])
            self.assertIsNone(got)
            self.assertIn("without object", err)
            self.assertFalse(output.exists())

    def test_zero_exit_ignored_option_is_failure(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            got, err, trace, _ = self.run_compile(Path(td), [(0, "warning: unknown option -nonsense", True)])
            self.assertIsNone(got)
            self.assertTrue(err.startswith("FAIL"))
            self.assertEqual(trace[0]["returncode"], 0)

    def test_missing_executable_is_unresolved(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
            got, err, trace, _ = self.run_compile(Path(td), [FileNotFoundError("missing runner")])
            self.assertIsNone(got)
            self.assertIn("UNRESOLVED", err)

    def test_missing_compiler_refuses_before_launch(self):
        with tempfile.TemporaryDirectory(prefix="r66_cv_") as td, \
             patch.object(cv, "REPO", Path(td)), \
             patch.object(cv.subprocess, "run", side_effect=AssertionError("launched")):
            got, err = cv.compile_with({"rule": "mwcc"}, "GC/missing", "", Path(td) / "out.o", td)
            self.assertIsNone(got)
            self.assertIn("missing compiler", err)

    def test_ninja_linux_and_windows_edges_use_same_unit_and_runner(self):
        for separator, runner in (("/", "build/tools/wibo "), ("\\", "")):
            with self.subTest(separator=separator), tempfile.TemporaryDirectory(prefix="r66_cv_") as td:
                root = Path(td)
                ninja = ("rule mwcc_sjis\n  command = RUNNERbuild/tools/sjiswrap.exe build/compilers/$mw_version/mwcceppc.exe $cflags -c $in -o $out\n"
                         "build build/GUNE5D/src/game/sys/.postprocess/body/sysservice.o: mwcc_sjis src/game/sys/sysservice.c | compiler\n"
                         "  mw_version = GC/1.2.5\n  cflags = -O4\n")
                (root / "build.ninja").write_text(ninja.replace("/", separator).replace("RUNNER", runner))
                compiler = root / "build/compilers/GC/1.2.5/mwcceppc.exe"
                compiler.parent.mkdir(parents=True)
                compiler.write_bytes(b"COMPILER")
                with patch.object(cv, "REPO", root):
                    edge = cv.read_edges()["game/sys/sysservice"]
                    self.assertTrue(edge["raw"])
                    self.assertEqual(edge["src"], "src/game/sys/sysservice.c")
                    # Windows templates are deliberately unsupported on a
                    # non-Windows host. The Linux runner case is host-agnostic.
                    if runner or cv.os.name == "nt":
                        command = cv.compile_commands(edge, edge["mw"], edge["cflags"], root / "out.o")[0][1]
                        self.assertEqual(command[0], "build/tools/wibo" if runner else str(root / "build/tools/sjiswrap.exe"))


MWCC_STDOUT = (
    "### mwcceppc.exe Compiler:\n"
    "#    File: src/game/sys/sysservice.c\n"
    "# -----------------------------------------\n"
    "#     412: static void f(void) { return ; ; ) }\n"
    "#   Error:                                  ^\n"
    "#   declaration syntax error\n")
MWCC_STDERR = "\nUser break, cancelled...\n"


class DiagnosticPreference(unittest.TestCase):
    """Run-59 item 8: the compiler's message, not the shell's noise.

    MWCC writes its diagnostic to STDOUT and only `User break,
    cancelled...` to stderr, and `compile_with` rendered
    `r.stderr + r.stdout` and took the first non-empty line — so a
    REJECTED SOURCE was reported to the lane as a CANCELLED probe, with
    the file, line and message already in hand and thrown away.

    Live reproduction at c52699758 (build/t3_scratch/t3_item8_repro.py,
    one-line syntax error compiled through the real game/sys/sysservice
    edge, GC/1.2.5 mwcc_sjis): the returned string was
    `FAIL compile (exit 2): User break, cancelled...` while stdout held
    `expression syntax error` with the file and the source line.

    Two-sided: stdout wins when it has content, stderr is the fallback
    the `extab_clean` (dtk) stage needs, and neither leaves an explicit
    `no diagnostic` rather than an empty tail.
    """

    def test_the_stdout_block_is_preferred_over_the_stderr_noise(self):
        text = cv.diagnostic(MWCC_STDOUT, MWCC_STDERR)
        self.assertIn("declaration syntax error", text)
        self.assertIn("src/game/sys/sysservice.c", text)
        self.assertIn("412", text)
        self.assertNotIn("User break", text)

    def test_the_gutter_and_the_rule_line_are_dropped(self):
        text = cv.diagnostic(MWCC_STDOUT, MWCC_STDERR)
        self.assertNotIn("#", text)
        self.assertNotIn("-----", text)

    def test_stderr_is_the_fallback_when_stdout_is_empty(self):
        # The extab_clean stage is dtk: it reports on stderr.
        self.assertEqual(cv.diagnostic("", "bad padding value\n"),
                         "bad padding value")

    def test_no_output_at_all_says_so(self):
        self.assertEqual(cv.diagnostic("", ""), "no diagnostic")
        self.assertEqual(cv.diagnostic(None, None), "no diagnostic")

    def test_the_rendering_is_bounded(self):
        text = cv.diagnostic("x" * 5000, "")
        self.assertEqual(len(text), cv.DIAGNOSTIC_LIMIT)

    def test_compile_with_reports_the_compiler_message(self):
        """End to end through the real refusal path."""
        with tempfile.TemporaryDirectory(prefix="t3_cv_") as td:
            root = Path(td)
            output = root / "out.o"
            trace = []

            def fake_run(argv, **kwargs):
                return subprocess.CompletedProcess(
                    argv, 2, stdout=MWCC_STDOUT, stderr=MWCC_STDERR)

            with patch.object(cv, "compile_commands",
                              return_value=[("compile", ["mwcceppc"])]), \
                 patch.object(cv.subprocess, "run", side_effect=fake_run):
                got, err = cv.compile_with(
                    {"_command_trace": trace}, "GC/fake", "", output, root)
        self.assertIsNone(got)
        self.assertTrue(err.startswith("FAIL compile (exit 2): "), err)
        self.assertIn("declaration syntax error", err)
        self.assertNotIn("User break", err)
        # The RAW streams stay in the trace: the rendering is for reading,
        # not a replacement for the evidence.
        self.assertEqual(trace[0]["stderr"], MWCC_STDERR)
        self.assertEqual(trace[0]["stdout"], MWCC_STDOUT)


if __name__ == "__main__":
    unittest.main()
