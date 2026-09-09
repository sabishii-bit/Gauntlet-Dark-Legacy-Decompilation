"""wf_word_diff: three states, three exit codes.

WHAT WAS ACTUALLY WRONG, measured at 0f3e1de09 before this suite was written.
The reported premise -- "exits 255 on any nonzero residual" -- did NOT
reproduce: a residual already exited 0, fixed in 928bd14a2 (run 42) and
narrowed in 7d8142f77 (run 51), and no invocation of any kind produced 255.

    $ wf_word_diff.py game/audio/dcsdrv dcsHandleRequest   # 67 words
    exit=0
    PS> ...; $LASTEXITCODE
    0

What run 42 left behind was the other half of the same confusion: every
REFUSAL also exited 1, the code the residual signal used to occupy.

    $ wf_word_diff.py game/audio/dcsdrv nosuchfn
    KeyError: "symbol 'nosuchfn' not found"      exit=1   (raw traceback)
    $ wf_word_diff.py game/nope/nope fn
    raw_object.RawObjectError: unit lacks ...    exit=1   (raw traceback)

So a `$LASTEXITCODE` gate could not distinguish "there is a residual" from
"the object is missing" from "you typed the function name wrong", two of the
three arrived as tracebacks rather than messages, and a gate that genuinely
WANTED to stop on a residual had no supported way to ask for it.

THE CONTRACT NOW: 0 = the measurement completed (residual is data),
2 = refusal with a `WF_WORD_DIFF REFUSED:` line, 1 = only under the opt-in
`--fail-on-residual`. A COUNT-ASYMMETRIC function has no word residual to
find, so it stays 0 even under that flag.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
CENSUS = TOOLS / "composed_census"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(CENSUS))

import wf_word_diff  # noqa: E402

TOOL = "tools/gdl/composed_census/wf_word_diff.py"
#: A NonMatching function with a real residual, and an exact sibling.
UNIT = "game/audio/dcsdrv"
RESIDUAL_FN = "dcsHandleRequest"
EXACT_FN = "dcsInit"
#: A count-asymmetric function: a determinate answer with no word residual.
ASYM_UNIT, ASYM_FN = "game/game/pmotion", "PlayerMotion"
LIVE = (ROOT / "build/GUNE5D/src" / (UNIT + ".o")).is_file()


def run(*args):
    return subprocess.run([sys.executable, TOOL, *args], cwd=str(ROOT),
                          capture_output=True, text=True)


class CodeConstants(unittest.TestCase):
    def test_the_three_codes_are_distinct_and_named(self):
        self.assertEqual((wf_word_diff.OK, wf_word_diff.RESIDUAL,
                          wf_word_diff.REFUSED), (0, 1, 2))

    def test_refuse_prints_a_greppable_prefix_and_returns_two(self):
        self.assertEqual(wf_word_diff.refuse("x"), 2)

    def test_refuse_writes_to_stdout_not_stderr(self):
        # A sweep that captured stdout and saw nothing read it as "no
        # residual"; the refusal has to land in the same stream as the data.
        import contextlib
        import io
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            wf_word_diff.refuse("the object is missing")
        self.assertIn("WF_WORD_DIFF REFUSED: the object is missing",
                      out.getvalue())
        self.assertEqual(err.getvalue(), "")


class ArgumentRefusals(unittest.TestCase):
    def test_an_unusable_range_refuses_with_two_not_one(self):
        done = run(UNIT, RESIDUAL_FN, "--range", "bogus")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("WF_WORD_DIFF REFUSED", done.stdout)

    def test_an_inverted_range_refuses(self):
        done = run(UNIT, RESIDUAL_FN, "--range", "0x40:0x10")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)

    def test_a_usage_error_keeps_argparses_own_two(self):
        done = run(UNIT, "--unit", UNIT)
        self.assertEqual(done.returncode, 2)

    def test_help_still_exits_zero(self):
        done = run("--help")
        self.assertEqual(done.returncode, 0)
        self.assertIn("--fail-on-residual", done.stdout)

    def test_an_unresolvable_unit_refuses_with_a_message_not_a_traceback(self):
        done = run("game/nope/nope", "fn")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("WF_WORD_DIFF REFUSED", done.stdout)
        self.assertNotIn("Traceback", done.stderr)

    def test_an_unresolvable_unit_refuses_in_whole_tu_mode_too(self):
        done = run("game/nope/nope")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertNotIn("Traceback", done.stderr)


@unittest.skipUnless(LIVE, "needs a built object")
class LiveExitContract(unittest.TestCase):
    def test_a_residual_measurement_exits_zero(self):
        done = run(UNIT, RESIDUAL_FN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("DIFFERING WORDS = ", done.stdout)
        self.assertNotIn("DIFFERING WORDS = 0,", done.stdout)

    def test_an_exact_function_exits_zero(self):
        done = run(UNIT, EXACT_FN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("DIFFERING WORDS = 0,", done.stdout)

    def test_the_whole_tu_screen_exits_zero_with_open_words(self):
        done = run(UNIT)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("OPEN function(s)", done.stdout)

    def test_an_unknown_function_refuses_with_two_and_names_it(self):
        done = run(UNIT, "nosuchfn")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("WF_WORD_DIFF REFUSED", done.stdout)
        self.assertIn("nosuchfn", done.stdout)
        self.assertNotIn("Traceback", done.stderr)

    def test_fail_on_residual_exits_one_only_when_words_differ(self):
        self.assertEqual(run(UNIT, RESIDUAL_FN, "--fail-on-residual")
                         .returncode, 1)
        self.assertEqual(run(UNIT, EXACT_FN, "--fail-on-residual")
                         .returncode, 0)

    def test_fail_on_residual_still_prints_the_measurement(self):
        done = run(UNIT, RESIDUAL_FN, "--fail-on-residual")
        self.assertIn("DIFFERING WORDS = ", done.stdout)

    def test_fail_on_residual_uses_the_open_total_in_whole_tu_mode(self):
        self.assertEqual(run(UNIT, "--fail-on-residual").returncode, 1)

    def test_the_opt_in_is_off_by_default(self):
        self.assertEqual(run(UNIT, RESIDUAL_FN).returncode, 0)
        self.assertEqual(run(UNIT).returncode, 0)

    def test_a_range_that_excludes_every_word_still_fails_on_the_whole_count(self):
        # --fail-on-residual gates on the WHOLE-function count, which is the
        # only one that decides candidacy; --range narrows the listing.
        done = run(UNIT, RESIDUAL_FN, "--range", "0x0:0x8",
                   "--fail-on-residual")
        self.assertEqual(done.returncode, 1, done.stdout)


@unittest.skipUnless(
    (ROOT / "build/GUNE5D/src" / (ASYM_UNIT + ".o")).is_file(),
    "needs pmotion built")
class CountAsymmetryIsNotAResidual(unittest.TestCase):
    def test_it_exits_zero(self):
        done = run(ASYM_UNIT, ASYM_FN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("COUNT-ASYMMETRIC", done.stdout)

    def test_it_exits_zero_even_under_fail_on_residual(self):
        done = run(ASYM_UNIT, ASYM_FN, "--fail-on-residual")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("COUNT-ASYMMETRIC", done.stdout)

    def test_it_is_not_reported_as_a_refusal(self):
        done = run(ASYM_UNIT, ASYM_FN)
        self.assertNotIn("REFUSED", done.stdout)


if __name__ == "__main__":
    unittest.main()
