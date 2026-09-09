"""fndiff --relocs word-identical pass and --gate (run-63 item 1).

THE DEFECT, reproduced at 77dd0fdef on the live game/game/player object
before a line of this was written:

    $ python tools/gdl/fndiff.py game/game/player --relocs
      == set_hidden_player: relocation sets IDENTICAL (111 reloc(s),
         addresses resolved)
    $ python tools/gdl/composed_census/wf_word_diff.py \
          game/game/player set_hidden_player --decode
      ... RELOC-SYMBOL MISMATCH = 4
        +0x0038  target lbl_803479C8   ours lbl_803479E0
        +0x0054  target lbl_803479D0   ours lbl_803479C8
        +0x006c  target lbl_803479D8   ours lbl_803479D0
        +0x0084  target lbl_803479E0   ours lbl_803479D8

Two readings of ONE object. The four rows are a four-way ROTATION of the
same `.sdata2` pool bases, so the relocation MULTISETS are equal by
construction and the set pass cannot see it; and 109 of 604 words differ,
so the positional pass refuses to run at all. Neither older pass had
anything to say, and the surviving line said "IDENTICAL".

That is not a cosmetic gap. A declaration sweeper gating on the TU-wide
`--relocs` count accepts exactly this class of candidate: lane P2 committed
`load_player_model_sub` real 110 -> 92 on that gate (e62b44d5d) and reverted
it (6f266fdf7) once the per-function line showed the "win" had transposed
two ADDR16 relocations and demoted the class RECOLOR -> SCHEDULE-REORDER,
and lane B's `items::SetItem` 516w -> 502w is the same trap.

THE FIX. A third `--relocs` pass pairs by INSTRUCTION INDEX and reports a
row only where the two instruction WORDS at that index are identical and
the relocation TYPES agree, which survives a diverged stream. It is
composed_census/wf_word_diff's own `reloc_symbol_mismatches` /
`anonymous_datum_rows` / `mnemonic_divergence`, imported rather than
re-implemented, so the two tools cannot drift; `test_the_gate_reloc_count_
IS_wf_word_diffs_headline_number` pins that equality on the live object.

TWO-SIDED. The positive side is the four live rows above plus the synthetic
transposition below; the negative side is every path that must NOT report or
must NOT pass — an equal stream, a function whose relocations agree, a
dropped baseline, an unreadable baseline, a count-asymmetric body, a
two-function invocation, and a `words`/`real` rise with `mnem`/`reloc` flat
(which a sweeper is expected to see and which must stay exit 0).
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import fndiff  # noqa: E402

TOOL = "tools/gdl/fndiff.py"
WF = "tools/gdl/composed_census/wf_word_diff.py"

#: The live reproduction. `set_hidden_player` carries FOUR pre-existing
#: RELOC-SYMBOL MISMATCH rows at 77dd0fdef and its streams diverge in 109 of
#: 604 words, so it exercises the exact combination both older passes miss.
UNIT = "game/game/player"
#: Integration 64-4 took set_hidden_player COUNT-ASYMMETRIC (603 of 604:
#: one address recompute is still open), so it can no longer carry the
#: gate's live measurement. kill_player is the same kind of body on the
#: same object -- count-equal, 23 differing words, MNEMONIC DIVERGENCE 8,
#: a clean relocation set -- and a lowered baseline still fails on it.
#: Repoint again when it closes (any count-equal open function with
#: mnem > 0 from `fndiff.py game/game/player --count` will do).
DEFECT_FN = "kill_player"
#: The LIVE carrier of the word-identical pass. `set_hidden_player`'s four
#: rotated `.sdata2` bases were the original one; 0e4963268 recovered the
#: target's cheat-name order and closed them, so its `--relocs` view is clean
#: and cannot demonstrate the pass any more. `damage_player` is the same
#: combination on the same object -- a diverged stream (MNEMONIC DIVERGENCE 2,
#: 9 differing words, so the positional pass refuses) that still carries
#: word-identical relocation rows, here of the anonymous-pool VALUE class
#: rather than the NAME class. It is a live function under repair: when it
#: closes, repoint this at whatever `fndiff.py <unit> --relocs` still lists.
RELOC_ROW_FN = "damage_player"
#: Lane P11 (integration 64-2) closed damage_player's anonymous-pool rows
#: too and swept every open player.c function: none carries a word-
#: identical relocation row any more. The live carrier therefore moves to
#: another NonMatching unit -- gauntworld::fn_8005FB48, a NAME-class row on
#: a diverged stream (MNEMONIC DIVERGENCE 2, 22 differing words). The
#: VALUE-class wording is pinned synthetically; the live case asserts the
#: pass, not the row class. Repoint again when that function closes.
#: fn_8005FB48 moved into items.c with the ITEMS.OBJ head (run 64 bounds pass).
RELOC_ROW_UNIT = "game/world/items"
RELOC_ROW_LIVE_FN = "fn_8005FB48"
RELOC_ROW_LIVE = (ROOT / "build/GUNE5D/src" / (RELOC_ROW_UNIT + ".o")).is_file()
EXACT_FN = "PlayerAttacking"
#: A count-asymmetric body: a determinate answer, not a measurement.
ASYM_FN = "setup_player_display"
LIVE = (ROOT / "build/GUNE5D/src" / (UNIT + ".o")).is_file()


def run(*args):
    return subprocess.run([sys.executable, TOOL, *args], cwd=str(ROOT),
                          capture_output=True, text=True)


def screen(rows=(), anon=(), mnem=0, words=0, insns=8, reason="",
           asymmetric=False):
    """A `word_identical_reloc_screen` result, without an object."""
    return {"rows": None if rows is None else list(rows),
            "anon": None if anon is None else list(anon),
            "mnem": mnem, "words": words, "insns": insns, "reason": reason,
            "asymmetric": asymmetric}


def capture(fn, *args, **kwargs):
    import contextlib
    import io
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        fn(*args, **kwargs)
    return out.getvalue()


# --------------------------------------------------------------- synthetic
# Synthetic instruction/relocation streams in `fndiff.parse`'s own shape:
# an instruction line, then its relocation lines indented four spaces.

def stream(*pairs):
    lines = []
    for insn, symbol in pairs:
        lines.append(insn)
        if symbol is not None:
            lines.append(f"    R_PPC_EMB_SDA21 {symbol}")
    return lines


class SyntheticRelocsView(unittest.TestCase):
    """`relocs_diff`'s verdict line, driven with a synthetic screen."""

    TARGET = stream(("lfs f1,0(r2)", "lbl_80345A28"),
                    ("lfs f2,0(r2)", "lbl_80345A48"))
    OURS = stream(("lfs f1,0(r2)", "lbl_80345A48"),
                  ("lfs f2,0(r2)", "lbl_80345A28"))

    def rows(self):
        return fndiff.reloc_rows_from_lines

    def test_a_transposition_replaces_the_IDENTICAL_verdict(self):
        text = capture(
            fndiff.relocs_diff, DEFECT_FN,
            self.rows()(self.TARGET), self.rows()(self.OURS),
            target_lines=self.TARGET, ours_lines=self.OURS,
            screen=screen(rows=[(0x38, "lbl_803479C8", "lbl_803479E0", True),
                                (0x54, "lbl_803479D0", "lbl_803479C8", True)],
                          mnem=51, words=109))
        self.assertIn("RELOC-SYMBOL MISMATCH " + DEFECT_FN, text)
        self.assertIn("+0x0038  target lbl_803479C8   ours lbl_803479E0",
                      text)
        self.assertIn("2 WORD-IDENTICAL row(s)", text)
        self.assertNotIn("relocation sets IDENTICAL", text)

    def test_a_clean_screen_leaves_the_IDENTICAL_verdict_alone(self):
        """The NEGATIVE side: the new pass must not editorialise a clean
        function into a suspicious one."""
        text = capture(
            fndiff.relocs_diff, "clean_fn",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET,
            screen=screen())
        self.assertIn("relocation sets IDENTICAL", text)
        self.assertNotIn("RELOC-SYMBOL MISMATCH", text)

    def test_no_screen_at_all_prints_exactly_the_old_two_passes(self):
        """Callers that never pass `screen` keep the pre-run-63 output."""
        with_none = capture(
            fndiff.relocs_diff, "clean_fn",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET)
        self.assertIn("relocation sets IDENTICAL", with_none)

    def test_an_unavailable_screen_says_so_instead_of_going_quiet(self):
        text = capture(
            fndiff.relocs_diff, "some_fn",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET,
            screen=screen(rows=None, anon=None, mnem=None, words=None,
                          reason="wf_word_diff unavailable (ImportError: x)"))
        self.assertIn("[word-identical pass: wf_word_diff unavailable", text)

    def test_a_count_asymmetric_body_is_not_reported_as_a_failure(self):
        """COUNT-ASYMMETRIC is a determinate answer about the function, so
        `--relocs` must not decorate it with a pass-skipped complaint."""
        text = capture(
            fndiff.relocs_diff, "asym_fn",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET,
            screen=screen(rows=None, anon=None, mnem=None, words=None,
                          asymmetric=True, reason="COUNT-ASYMMETRIC — ..."))
        self.assertNotIn("word-identical pass", text)
        self.assertIn("relocation sets IDENTICAL", text)

    def test_the_anon_pool_class_is_labelled_separately_from_the_name_class(
            self):
        text = capture(
            fndiff.relocs_diff, "do_got_it",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET,
            screen=screen(anon=[(4, "lbl_80113AE0", "@125", "VALUE-DIFFERS",
                                 True)], mnem=8, words=13))
        self.assertIn("1 by anonymous-pool VALUE", text)
        self.assertNotIn("by NAME", text)
        self.assertIn("ANON-POOL VALUE-DIFFERS", text)

    def test_an_unreliably_paired_row_is_MARKED_not_silently_asserted(self):
        text = capture(
            fndiff.relocs_diff, "camera_mode_follow",
            self.rows()(self.TARGET), self.rows()(self.TARGET),
            target_lines=self.TARGET, ours_lines=self.TARGET,
            screen=screen(rows=[(0x10, "get_cam_wpos", "calc_cam_pyr",
                                 False)], mnem=360, words=400))
        self.assertIn("PAIRING UNRELIABLE", text)


class GateVerdict(unittest.TestCase):
    """`gate_verdict` is the whole acceptance rule, tested without a build."""

    BASE = {"function": "f", "words": 55, "mnem": 0, "reloc": 0, "anon": 0,
            "real": 110, "reason": ""}

    def current(self, **over):
        row = dict(self.BASE)
        row.update(over)
        return row

    def test_the_lane_P2_candidate_FAILS(self):
        """words 55 -> 44 and real 110 -> 92 look like a win; mnem 0 -> 5
        and reloc 0 -> 2 are the defect that reverted it."""
        code, why = fndiff.gate_verdict(
            self.current(words=44, real=92, mnem=5, reloc=2), self.BASE)
        self.assertEqual(code, fndiff.GATE_FAILED)
        self.assertIn("mnem ROSE 0 -> 5", "; ".join(why))
        self.assertIn("reloc ROSE 0 -> 2", "; ".join(why))

    def test_the_lane_B_SetItem_candidate_FAILS_on_relocation_alone(self):
        code, why = fndiff.gate_verdict(
            self.current(words=502, real=1000, reloc=2), self.BASE)
        self.assertEqual(code, fndiff.GATE_FAILED)
        self.assertEqual(why, ["reloc ROSE 0 -> 2"])

    def test_an_anonymous_pool_rise_FAILS_too(self):
        code, _why = fndiff.gate_verdict(self.current(anon=1), self.BASE)
        self.assertEqual(code, fndiff.GATE_FAILED)

    def test_a_genuine_improvement_PASSES(self):
        code, why = fndiff.gate_verdict(
            self.current(words=17, real=34), self.BASE)
        self.assertEqual((code, why), (fndiff.GATE_OK, []))

    def test_a_words_and_real_RISE_alone_still_PASSES(self):
        """The NEGATIVE side of the rule: a sweeper ranks on words/real and
        must be free to measure a worse candidate without the gate calling
        it a defect."""
        code, why = fndiff.gate_verdict(
            self.current(words=999, real=999), self.BASE)
        self.assertEqual((code, why), (fndiff.GATE_OK, []))

    def test_no_baseline_is_a_measurement_not_a_verdict(self):
        code, why = fndiff.gate_verdict(self.current(), None)
        self.assertEqual((code, why), (fndiff.GATE_OK, []))

    def test_an_unmeasured_field_REFUSES_rather_than_passing(self):
        code, why = fndiff.gate_verdict(
            self.current(mnem=None, reloc=None, anon=None,
                         reason="COUNT-ASYMMETRIC — target 236, ours 241"),
            self.BASE)
        self.assertEqual(code, fndiff.GATE_REFUSED)
        self.assertIn("COUNT-ASYMMETRIC", "; ".join(why))

    def test_a_rise_beats_an_unmeasured_field(self):
        """A candidate that both regressed and went unmeasurable is FAILED,
        not merely refused: 1 is the code a sweeper rejects on."""
        code, _why = fndiff.gate_verdict(
            self.current(reloc=3, mnem=None), self.BASE)
        self.assertEqual(code, fndiff.GATE_FAILED)

    def test_the_reason_is_printed_ONCE_not_per_field(self):
        reason = "COUNT-ASYMMETRIC — target 236, ours 241 insns"
        _code, why = fndiff.gate_verdict(
            self.current(mnem=None, reloc=None, anon=None, reason=reason),
            self.BASE)
        self.assertEqual("; ".join(why).count(reason), 1)

    def test_the_printed_line_has_the_pinned_shape(self):
        text = capture(fndiff.print_gate, self.current())
        self.assertEqual(text.splitlines()[0],
                         "GATE: words 55 mnem 0 reloc 0 real 110")

    def test_an_unmeasured_field_prints_n_slash_a_not_zero(self):
        text = capture(fndiff.print_gate,
                       self.current(words=None, mnem=None, reloc=None))
        self.assertEqual(text.splitlines()[0],
                         "GATE: words n/a mnem n/a reloc n/a real 110")


class CleanRealAgreement(unittest.TestCase):
    """`--gate`'s `real` and `--clean`'s `real` are ONE computation."""

    T = ["stw r3,8(r1)", "    R_PPC_ADDR16_HA lbl_80345A28",
         "addi r4,r5,12", "blr"]
    B = ["stw r3,8(r1)", "    R_PPC_ADDR16_HA @125",
         "addi r4,r5,24", "blr"]

    def printed_real(self, t, b):
        text = capture(fndiff.clean_diff, "f", t, b)
        line = [ln for ln in text.splitlines() if ln.startswith("== f:")][0]
        return int(line.split(",")[1].split()[0])

    def test_clean_real_count_equals_what_clean_diff_prints(self):
        self.assertEqual(fndiff.clean_real_count(self.T, self.B),
                         self.printed_real(self.T, self.B))

    def test_it_is_zero_for_identical_streams(self):
        self.assertEqual(fndiff.clean_real_count(self.T, list(self.T)), 0)
        self.assertEqual(self.printed_real(self.T, list(self.T)), 0)

    def test_a_pool_rename_alone_is_not_real(self):
        """@125 vs lbl_ is normalized away by BOTH, or by neither."""
        ours = ["stw r3,8(r1)", "    R_PPC_ADDR16_HA @125",
                "addi r4,r5,12", "blr"]
        self.assertEqual(fndiff.clean_real_count(self.T, ours),
                         self.printed_real(self.T, ours))
        self.assertEqual(fndiff.clean_real_count(self.T, ours), 0)


class ScreenRefusals(unittest.TestCase):
    """`word_identical_reloc_screen` must never let a helper's refusal
    escape as a process exit inside a sweep (AGENTS.md: a `SystemExit`
    refusal is not caught by `except Exception`)."""

    class FakeCountAsymmetric(SystemExit):
        def __init__(self):
            self.target, self.ours = 236, 241
            super().__init__("asym")

    class Fake:
        CountAsymmetric = None

        def __init__(self, raiser):
            self.raiser = raiser

        def word_streams(self, unit, fn):
            raise self.raiser()

        def unit_bodies(self, path):
            return {"f": b""}

        def target_object(self, unit):
            return "t.o"

        def our_object(self, unit):
            return ("o.o", "raw")

    def drive(self, raiser):
        fake = self.Fake(raiser)
        fake.CountAsymmetric = self.FakeCountAsymmetric
        saved_module = list(fndiff._WORD_DIFF_MODULE)
        saved_cache = dict(fndiff._ELF_NAME_CACHE)
        fndiff._WORD_DIFF_MODULE[:] = [fake]
        fndiff._ELF_NAME_CACHE.clear()
        try:
            return fndiff.word_identical_reloc_screen("unit/x", "f")
        finally:
            fndiff._WORD_DIFF_MODULE[:] = saved_module
            fndiff._ELF_NAME_CACHE.clear()
            fndiff._ELF_NAME_CACHE.update(saved_cache)

    def test_a_count_asymmetric_refusal_is_reported_not_raised(self):
        got = self.drive(self.FakeCountAsymmetric)
        self.assertTrue(got["asymmetric"])
        self.assertIn("COUNT-ASYMMETRIC", got["reason"])
        self.assertIsNone(got["rows"])

    def test_a_plain_SystemExit_refusal_is_reported_not_raised(self):
        got = self.drive(lambda: SystemExit("missing object for unit/x"))
        self.assertFalse(got["asymmetric"])
        self.assertIn("missing object", got["reason"])

    def test_a_missing_symbol_KeyError_is_reported_not_raised(self):
        got = self.drive(lambda: KeyError("nosuchfn"))
        self.assertIn("nosuchfn", got["reason"])

    def test_an_unresolvable_name_never_reaches_word_streams(self):
        fake = self.Fake(AssertionError)
        fake.CountAsymmetric = self.FakeCountAsymmetric
        saved = list(fndiff._WORD_DIFF_MODULE)
        fndiff._WORD_DIFF_MODULE[:] = [fake]
        fndiff._ELF_NAME_CACHE.clear()
        try:
            got = fndiff.word_identical_reloc_screen("unit/x", "not_there")
        finally:
            fndiff._WORD_DIFF_MODULE[:] = saved
            fndiff._ELF_NAME_CACHE.clear()
        self.assertIn("no ELF symbol resolves", got["reason"])


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class LiveReproduction(unittest.TestCase):
    """The measured case the item was written from."""

    def test_the_TU_relocs_view_no_longer_calls_the_defect_IDENTICAL(self):
        """Word-identical relocation rows must be PRINTED on a real object
        whose stream has diverged, and the closing verdict must never read
        `relocation sets IDENTICAL` while they are.

        The set delta itself is not asserted either way: at 77dd0fdef
        `set_hidden_player`'s was clean (which is what made the rotation
        invisible) and after the small-data recovery it carried a real row
        of its own. Both are legitimate states of one object, and the claim
        under test is about the WORD-IDENTICAL pass surviving either.

        The carrier moved off `set_hidden_player` at 0e4963268, which
        recovered the target's cheat-name order and closed all four rotated
        rows; the rule itself is still pinned on the exact original rows by
        `SyntheticRelocsView.test_a_transposition_replaces_the_IDENTICAL_
        verdict`. Neither our anonymous pool index nor the instruction
        offset is asserted here: both move whenever the `.sdata2` pool or
        the body changes, and neither is part of the claim."""
        if not RELOC_ROW_LIVE:
            self.skipTest("needs a built %s object" % RELOC_ROW_UNIT)
        done = run(RELOC_ROW_UNIT, RELOC_ROW_LIVE_FN, "--relocs")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("RELOC-SYMBOL MISMATCH " + RELOC_ROW_LIVE_FN, done.stdout)
        self.assertIn("MNEMONIC DIVERGENCE", done.stdout)
        self.assertNotIn("relocation sets IDENTICAL", done.stdout)

    def test_the_gate_reloc_count_IS_wf_word_diffs_headline_number(self):
        """The calibration: one number, two tools, no second copy of the
        discriminator."""
        gate = run(UNIT, DEFECT_FN, "--gate")
        self.assertEqual(gate.returncode, 0, gate.stdout + gate.stderr)
        line = gate.stdout.splitlines()[0].split()
        words, mnem, reloc = line[2], line[4], line[6]
        theirs = subprocess.run(
            [sys.executable, WF, UNIT, DEFECT_FN],
            cwd=str(ROOT), capture_output=True, text=True)
        head = theirs.stdout.splitlines()[0]
        self.assertIn(f"DIFFERING WORDS = {words}", head)
        self.assertIn(f"MNEMONIC DIVERGENCE = {mnem}", head)
        self.assertIn(f"RELOC-SYMBOL MISMATCH = {reloc}", head)

    def test_an_exact_sibling_gates_at_all_zeroes(self):
        done = run(UNIT, EXACT_FN, "--gate")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(done.stdout.splitlines()[0],
                         "GATE: words 0 mnem 0 reloc 0 real 0")

    def test_save_then_compare_an_unchanged_tree_PASSES(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture_path = Path(tmp) / "gate.json"
            saved = run(UNIT, DEFECT_FN, "--gate",
                        "--save-baseline", str(capture_path))
            self.assertEqual(saved.returncode, 0, saved.stdout)
            self.assertTrue(capture_path.is_file())
            again = run(UNIT, DEFECT_FN, "--gate",
                        "--baseline", str(capture_path))
            self.assertEqual(again.returncode, 0, again.stdout)
            self.assertIn("GATE PASSED", again.stdout)

    def test_a_lowered_baseline_FAILS_with_exit_one(self):
        """Deliberately invalid input: a capture claiming the function had
        no relocation defect must make the current state fail."""
        with tempfile.TemporaryDirectory() as tmp:
            capture_path = Path(tmp) / "gate.json"
            run(UNIT, DEFECT_FN, "--gate",
                "--save-baseline", str(capture_path))
            row = json.loads(capture_path.read_text(encoding="utf-8"))
            row["reloc"], row["mnem"] = 0, 0
            capture_path.write_text(json.dumps(row), encoding="utf-8")
            done = run(UNIT, DEFECT_FN, "--gate",
                       "--baseline", str(capture_path))
            self.assertEqual(done.returncode, 1, done.stdout)
            self.assertIn("GATE FAILED", done.stdout)

    def test_a_BOM_capture_is_read_not_crashed_on(self):
        """PowerShell's `Out-File -Encoding utf8` and `ConvertTo-Json` write
        a BOM; a plain utf-8 read raised JSONDecodeError out of main() as
        exit 1 — the code a REGRESSION means."""
        with tempfile.TemporaryDirectory() as tmp:
            capture_path = Path(tmp) / "gate.json"
            run(UNIT, DEFECT_FN, "--gate",
                "--save-baseline", str(capture_path))
            body = capture_path.read_text(encoding="utf-8")
            capture_path.write_text("﻿" + body, encoding="utf-8")
            done = run(UNIT, DEFECT_FN, "--gate",
                       "--baseline", str(capture_path))
            self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
            self.assertNotIn("Traceback", done.stderr)

    def test_a_corrupt_capture_REFUSES_rather_than_failing(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture_path = Path(tmp) / "gate.json"
            capture_path.write_text("{not json", encoding="utf-8")
            done = run(UNIT, DEFECT_FN, "--gate",
                       "--baseline", str(capture_path))
            self.assertEqual(done.returncode, 2, done.stdout)
            self.assertIn("GATE REFUSED", done.stdout)
            self.assertNotIn("Traceback", done.stderr)

    def test_a_baseline_for_another_function_REFUSES(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture_path = Path(tmp) / "gate.json"
            run(UNIT, DEFECT_FN, "--gate",
                "--save-baseline", str(capture_path))
            done = run(UNIT, EXACT_FN, "--gate",
                       "--baseline", str(capture_path))
            self.assertEqual(done.returncode, 2, done.stdout)
            self.assertIn("GATE REFUSED", done.stdout)

    def test_a_missing_baseline_REFUSES_instead_of_silently_passing(self):
        done = run(UNIT, DEFECT_FN, "--gate",
                   "--baseline", "build/p5_no_such_capture.json")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("GATE REFUSED", done.stdout)

    def test_a_count_asymmetric_function_measures_but_cannot_be_gated(self):
        done = run(UNIT, ASYM_FN, "--gate")
        self.assertEqual(done.returncode, 0, done.stdout)
        self.assertIn("GATE: words n/a mnem n/a reloc n/a", done.stdout)
        self.assertIn("COUNT-ASYMMETRIC", done.stdout)

    def test_two_function_names_REFUSE(self):
        done = run(UNIT, DEFECT_FN, EXACT_FN, "--gate")
        self.assertEqual(done.returncode, 2, done.stdout)

    def test_no_function_name_REFUSES(self):
        done = run(UNIT, "--gate")
        self.assertEqual(done.returncode, 2, done.stdout)

    def test_an_unknown_function_REFUSES(self):
        done = run(UNIT, "no_such_function_here", "--gate")
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("GATE REFUSED", done.stdout)

    def test_a_valueless_baseline_flag_REFUSES(self):
        done = run(UNIT, DEFECT_FN, "--gate", "--baseline")
        self.assertEqual(done.returncode, 2, done.stdout)


@unittest.skipUnless(LIVE, "needs a built game/game/player object")
class ImportContract(unittest.TestCase):
    def test_importing_wf_word_diff_from_fndiff_is_side_effect_free(self):
        """Cheap enough to sit inside `--relocs`, and it prints nothing."""
        done = subprocess.run(
            [sys.executable, "-c",
             "import sys; sys.path.insert(0, 'tools/gdl');"
             " import fndiff;"
             " m = fndiff.word_diff_module();"
             " print('MODULE', m is not None)"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(done.stdout.strip(), "MODULE True")

    def test_the_help_text_documents_the_gate_and_exits_zero(self):
        done = run("--help")
        self.assertEqual(done.returncode, 0)
        self.assertIn("GATE: words N mnem M reloc R real X", done.stdout)
        self.assertIn("WORD-IDENTICAL", done.stdout)


if __name__ == "__main__":
    unittest.main()
