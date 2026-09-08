"""fnasm: which stream did that dump come from?

THE DEFECT, reproduced live on game/audio/dcsdrv dcsHandleRequest before this
suite was written:

    $ python tools/gdl/fnasm.py game/audio/dcsdrv dcsHandleRequest --raw | head
      10: addi r29,r5,0
    $ python tools/gdl/fnasm.py game/audio/dcsdrv dcsHandleRequest 0x0:0x40
      10: addi r30,r5,0

Two different streams. The help documented `build/GUNE5D/obj/<unit>.o` (the
dtk-extracted TARGET) as the default and documented `--ours` as the flag that
switches away from it, but `ours = "--ours" in sys.argv or raw` made a bare
`--raw` switch too -- silently. The plain dump printed no header at all, and
its only stream marker was a trailing `[411 insns (raw, pre-postprocess)]`
that `| head` discards. A reader comparing that against the target's own dump
reads our register allocation as the target's and invents a recolour.

THE FIX IS NOT "--raw should print the target". `--raw` is a qualifier naming
WHICH of our objects to read, so selecting our stream is correct; the
dishonesty was that nothing said so. `select_stream` is now the one decision
point, `--raw implies --ours` is in the help, and `stream_header` prints the
label and the actual file on the FIRST line of every dump.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT))

import fnasm  # noqa: E402

UNIT = "game/audio/dcsdrv"
FN = "dcsHandleRequest"
LIVE = ((ROOT / "build/GUNE5D/obj" / (UNIT + ".o")).is_file()
        and (ROOT / "build/GUNE5D/src" / (UNIT + ".o")).is_file())


class StreamSelection(unittest.TestCase):
    def test_no_flag_is_the_target(self):
        self.assertEqual(fnasm.select_stream([UNIT, FN]), "target")

    def test_ours_selects_our_built_object(self):
        self.assertEqual(fnasm.select_stream([UNIT, FN, "--ours"]), "ours")

    def test_raw_selects_our_compiler_object_on_its_own(self):
        # The documented behaviour now; previously an undocumented side
        # effect of `ours = "--ours" in sys.argv or raw`.
        self.assertEqual(fnasm.select_stream([UNIT, FN, "--raw"]), "raw")

    def test_raw_wins_over_ours_when_both_are_given(self):
        self.assertEqual(fnasm.select_stream([UNIT, FN, "--ours", "--raw"]),
                         "raw")

    def test_a_slice_argument_is_never_read_as_a_flag(self):
        self.assertEqual(fnasm.select_stream([UNIT, FN, "0x0:0x40"]), "target")
        self.assertEqual(fnasm.select_stream([UNIT, FN, "40:120", "--raw"]),
                         "raw")

    def test_diff_reads_our_side_but_the_left_column_stays_the_target(self):
        self.assertEqual(fnasm.select_stream([UNIT, FN], diff=True), "ours")
        self.assertEqual(fnasm.select_stream([UNIT, FN, "--raw"], diff=True),
                         "raw")

    def test_every_stream_key_has_a_label_and_a_description(self):
        for key in ("target", "ours", "raw"):
            label, what = fnasm.STREAMS[key]
            self.assertIn(label, ("TARGET", "OURS"))
            self.assertTrue(what)

    def test_raw_is_labelled_ours_never_target(self):
        self.assertEqual(fnasm.STREAMS["raw"][0], "OURS")
        self.assertNotEqual(fnasm.STREAMS["raw"][0], "TARGET")


class HeaderText(unittest.TestCase):
    def test_the_header_names_the_stream_and_the_file(self):
        line = fnasm.stream_header(UNIT, FN, "raw",
                                   ROOT / "build/GUNE5D/src/game/audio/dcsdrv.o")
        self.assertTrue(line.startswith("# "))
        self.assertIn("OURS", line)
        self.assertIn(FN, line)
        self.assertIn("build/GUNE5D/src/game/audio/dcsdrv.o", line)

    def test_the_target_header_says_target(self):
        line = fnasm.stream_header(UNIT, FN, "target",
                                   "build/GUNE5D/obj/game/audio/dcsdrv.o")
        self.assertIn("TARGET", line)
        self.assertNotIn("OURS", line)

    def test_the_header_is_a_comment_so_offset_parsers_skip_it(self):
        import re
        line = fnasm.stream_header(UNIT, FN, "ours", "x.o")
        self.assertIsNone(
            re.match(r"\s*([0-9a-f]+):\s+(\S+)", line),
            "the header must not parse as an instruction row")

    def test_an_absolute_in_repo_path_is_shown_repo_relative(self):
        line = fnasm.stream_header(
            UNIT, FN, "raw", ROOT / "build/GUNE5D/src/game/audio/dcsdrv.o")
        self.assertNotIn(str(ROOT).replace("\\", "/"), line)


@unittest.skipUnless(LIVE, "needs the extracted target and our built object")
class LiveStreams(unittest.TestCase):
    def dump(self, *flags):
        done = subprocess.run(
            [sys.executable, "tools/gdl/fnasm.py", UNIT, FN, *flags],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        return done.stdout

    def test_the_first_line_of_a_bare_raw_dump_says_OURS(self):
        first = self.dump("--raw").splitlines()[0]
        self.assertIn("OURS", first)
        self.assertNotIn("TARGET", first)

    def test_the_first_line_of_a_default_dump_says_TARGET(self):
        first = self.dump("0x0:0x40").splitlines()[0]
        self.assertIn("TARGET", first)
        self.assertNotIn("OURS", first)

    def test_the_label_survives_a_pipe_to_head(self):
        # The whole failure mode: the old marker was the LAST line.
        self.assertIn("OURS", "\n".join(self.dump("--raw").splitlines()[:5]))

    def test_the_two_streams_really_do_differ_here(self):
        # If they ever stop differing this test is meaningless, so it asserts
        # the premise the defect report rests on rather than assuming it.
        target = [line for line in self.dump("0x0:0x40").splitlines()
                  if line.strip().startswith("10:")]
        ours = [line for line in self.dump("--raw", "0x0:0x40").splitlines()
                if line.strip().startswith("10:")]
        self.assertTrue(target and ours)
        self.assertNotEqual(target[0], ours[0])

    def test_the_footer_agrees_with_the_header(self):
        for flags, label in ((("--raw",), "OURS"), (("--ours",), "OURS"),
                             ((), "TARGET")):
            with self.subTest(flags=flags):
                lines = self.dump(*flags).splitlines()
                self.assertIn(label, lines[0])
                self.assertIn(label, lines[-1])

    def test_diff_prints_both_stream_headers_before_the_columns(self):
        lines = self.dump("--raw", "--diff", "0x0:0x20").splitlines()
        self.assertIn("TARGET", lines[0])
        self.assertIn("OURS", lines[1])
        self.assertIn("T = TARGET", lines[2])

    def test_listing_functions_is_still_bare_names(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/fnasm.py", UNIT],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertFalse(done.stdout.startswith("#"))
        self.assertIn(FN, done.stdout.split())


if __name__ == "__main__":
    unittest.main()
