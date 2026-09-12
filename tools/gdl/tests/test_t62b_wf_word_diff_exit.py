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
import contextlib
import hashlib
import io
import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
CENSUS = TOOLS / "composed_census"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(CENSUS))

import wf_word_diff  # noqa: E402

TOOL = "tools/gdl/composed_census/wf_word_diff.py"
#: Live integration examples; neither is required to remain mismatched.
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


class ControlledExitContract(unittest.TestCase):
    """Exercise actual measurement/CLI logic with stable input boundaries.

    Matching dcsHandleRequest must not break the tests. Only object/relocation
    reads are replaced here: word comparison, decoding, totals, printing and
    exit selection remain real, for both exact and unequal input streams.
    """

    def measure(self, *args, all_exact=False):
        target = bytes.fromhex("60000000 60000000 38600000 4e800020")
        residual = bytes.fromhex("60000000 60000000 38800000 4e800020")

        def streams(unit, fn):
            if fn not in (RESIDUAL_FN, EXACT_FN):
                raise KeyError(fn)
            ours = target if all_exact or fn == EXACT_FN else residual
            return "controlled raw input", ours, target

        def unit_rows(unit):
            rows = []
            for fn in (RESIDUAL_FN, EXACT_FN):
                kind, ours, tgt = streams(unit, fn)
                words = sum(ours[i:i + 4] != tgt[i:i + 4]
                            for i in range(0, len(ours), 4))
                rows.append(dict(
                    function=fn, pinned=False, target_insns=4, ours_insns=4,
                    instruction_delta=0, count_asymmetric=False,
                    verdict="MEASURED", differing_words=words,
                    mnemonic_divergence=0, klass="RECOLOR" if words else "EXACT",
                    body_sha256_ours=hashlib.sha256(ours).hexdigest(),
                    body_sha256_target=hashlib.sha256(tgt).hexdigest()))
            return rows, kind

        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(wf_word_diff, "word_streams", side_effect=streams), \
                mock.patch.object(wf_word_diff, "unit_rows", side_effect=unit_rows), \
                mock.patch.object(wf_word_diff, "reloc_symbol_mismatches", return_value=[]), \
                mock.patch.object(wf_word_diff, "anonymous_datum_rows", return_value=([], {})), \
                mock.patch.object(wf_word_diff, "rule_served_functions", return_value=set()), \
                mock.patch.object(wf_word_diff, "reloc_types_by_index", return_value={}), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = wf_word_diff.main(list(args))
        return subprocess.CompletedProcess(args, code, out.getvalue(), err.getvalue())

    def test_a_residual_measurement_exits_zero(self):
        done = self.measure(UNIT, RESIDUAL_FN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("DIFFERING WORDS = ", done.stdout)
        self.assertNotIn("DIFFERING WORDS = 0,", done.stdout)

    def test_an_exact_function_exits_zero(self):
        done = self.measure(UNIT, EXACT_FN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("DIFFERING WORDS = 0,", done.stdout)

    def test_the_whole_tu_screen_exits_zero_with_open_words(self):
        done = self.measure(UNIT)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("OPEN function(s)", done.stdout)

    def test_an_unknown_function_refuses_with_two_and_names_it(self):
        done = self.measure(UNIT, "nosuchfn")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("WF_WORD_DIFF REFUSED", done.stdout)
        self.assertIn("nosuchfn", done.stdout)
        self.assertNotIn("Traceback", done.stderr)

    def test_fail_on_residual_exits_one_only_when_words_differ(self):
        self.assertEqual(self.measure(UNIT, RESIDUAL_FN, "--fail-on-residual")
                         .returncode, 1)
        self.assertEqual(self.measure(UNIT, EXACT_FN, "--fail-on-residual")
                         .returncode, 0)

    def test_fail_on_residual_still_prints_the_measurement(self):
        done = self.measure(UNIT, RESIDUAL_FN, "--fail-on-residual")
        self.assertIn("DIFFERING WORDS = ", done.stdout)

    def test_fail_on_residual_uses_the_open_total_in_whole_tu_mode(self):
        self.assertEqual(self.measure(UNIT, "--fail-on-residual").returncode, 1)
        self.assertEqual(self.measure(UNIT, "--fail-on-residual", all_exact=True)
                         .returncode, 0)

    def test_the_opt_in_is_off_by_default(self):
        self.assertEqual(self.measure(UNIT, RESIDUAL_FN).returncode, 0)
        self.assertEqual(self.measure(UNIT).returncode, 0)

    def test_a_range_that_excludes_every_word_still_fails_on_the_whole_count(self):
        # --fail-on-residual gates on the WHOLE-function count, which is the
        # only one that decides candidacy; --range narrows the listing.
        done = self.measure(UNIT, RESIDUAL_FN, "--range", "0x0:0x8",
                   "--fail-on-residual")
        self.assertEqual(done.returncode, 1, done.stdout)
        self.assertIn("0 of 1 differing word(s)", done.stdout)


@unittest.skipUnless(LIVE, "needs a built object")
class LiveExitContract(unittest.TestCase):
    def test_live_exit_codes_agree_with_the_current_object_measurement(self):
        # Calibrate against current raw bytes, not a permanent residual count.
        # ControlledExitContract separately guarantees both branches are tested.
        for fn in (RESIDUAL_FN, EXACT_FN):
            with self.subTest(function=fn):
                _, ours, target = wf_word_diff.word_streams(UNIT, fn)
                words = sum(ours[i:i + 4] != target[i:i + 4]
                            for i in range(0, len(ours), 4))
                for flags in ((), ("--fail-on-residual",)):
                    done = run(UNIT, fn, *flags)
                    self.assertEqual(done.returncode, int(bool(flags and words)),
                                     done.stdout + done.stderr)
                    self.assertIn(f"DIFFERING WORDS = {words},", done.stdout)

    def test_live_whole_tu_exit_agrees_with_the_printed_inventory(self):
        done = run(UNIT, "--json")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        report = json.loads(done.stdout)
        self.assertTrue(report["rows"])
        expected = int(bool(report["totals"]["open_differing_words"]))
        gated = run(UNIT, "--fail-on-residual")
        self.assertEqual(gated.returncode, expected, gated.stdout + gated.stderr)

    def test_missing_live_symbol_still_refuses(self):
        done = run(UNIT, "nosuchfn")
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("WF_WORD_DIFF REFUSED", done.stdout)
        self.assertNotIn("Traceback", done.stderr)


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
