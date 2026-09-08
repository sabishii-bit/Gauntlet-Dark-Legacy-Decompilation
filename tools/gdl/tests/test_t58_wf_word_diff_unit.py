"""Run-58 item 4: the whole-TU sibling screen nobody had.

THE OBSERVATION (EN and CR lanes, independently). AGENTS.md's matching
loop mandates "screen the whole TU, not only the most promising function",
and no tool did it: both lanes composed `fnsurvey` with one `wf_word_diff`
subprocess per function over 84-86 functions and wrote a throwaway scraper
each time -- three each, all lost with the EN lane's worktree, which is why
this had to be rebuilt from a description rather than promoted.

MEASURED COST at 59fe8f6b3, game/movie/movieplayer (52 paired functions):

  52 per-function subprocesses (build/T2_item4_calibrate.py)   43.3 s
  `wf_word_diff.py --unit game/movie/movieplayer`               7.4 s

TWO-SIDED CALIBRATION, the brief's own gate -- `--unit` totals must equal
the sum of the per-function runs, on two units. Each function's HEADLINE
was re-read out of a real subprocess and compared field by field with the
`--unit` row (insns, differing words, mnemonic divergence, PINNED, and for
a count-asymmetric function the labelled count pair):

  game/enemy/enemy         84 paired, per-function sum 835 words,
                           --unit sum 835, 0 mismatched rows
  game/movie/movieplayer   52 paired, per-function sum 648 words,
                           --unit sum 648, 0 mismatched rows

movieplayer is the unit that exercises the third row shape:
DTextInitColorRamp is T51/O52, and it keeps its row as COUNT-ASYMMETRIC
with `differing_words: null` rather than being dropped -- the same
determinate answer the single-function mode gives at exit 0.

The tests below run over synthetic ELF-free inputs where they can, and
over the live objects where the question IS the live roster.
"""
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import wf_word_diff as wd                                   # noqa: E402

TOOL = os.path.join("tools", "gdl", "composed_census", "wf_word_diff.py")


class UnitRowShape(unittest.TestCase):
    """The printed table and the JSON, over hand-built rows."""

    ROWS = [
        {"function": "open_one", "pinned": False, "target_insns": 40,
         "ours_insns": 40, "verdict": "MEASURED", "differing_words": 12,
         "mnemonic_divergence": 0, "klass": "RECOLOR",
         "decode": {c: (12 if c == "REGFIELD-ONLY" else 0)
                    for c in wd.DECODE_CLASSES}},
        {"function": "pinned_one", "pinned": True, "target_insns": 105,
         "ours_insns": 105, "verdict": "MEASURED", "differing_words": 16,
         "mnemonic_divergence": 0, "klass": "RECOLOR",
         "decode": {c: (16 if c == "REGFIELD-ONLY" else 0)
                    for c in wd.DECODE_CLASSES}},
        {"function": "asym_one", "pinned": False, "target_insns": 51,
         "ours_insns": 52, "verdict": "COUNT-ASYMMETRIC",
         "differing_words": None, "mnemonic_divergence": None,
         "klass": None, "decode": None},
        {"function": "exact_one", "pinned": False, "target_insns": 8,
         "ours_insns": 8, "verdict": "MEASURED", "differing_words": 0,
         "mnemonic_divergence": 0, "klass": "EXACT",
         "decode": {c: 0 for c in wd.DECODE_CLASSES}},
    ]

    def render(self):
        import io
        import contextlib
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            wd.print_unit("game/x/y", self.ROWS, "raw")
        return buffer.getvalue()

    def test_the_totals_exclude_pinned_words_from_the_open_count(self):
        # The PIN SCREEN is the whole reason a sweep can be ranked at all:
        # a pinned row's words are the residual its rule ALREADY closes,
        # and 4 of 8 functions in the run-39 recolor class were pinned and
        # took first, third, fifth and eighth place.
        text = self.render()
        self.assertIn("1 OPEN function(s) carrying 12 differing word(s)",
                      text)
        self.assertIn("1 pinned (16 words their rules already close", text)

    def test_a_count_asymmetric_row_is_kept_and_named(self):
        text = self.render()
        self.assertIn("asym_one", text)
        self.assertIn("COUNT-ASYMMETRIC", text)
        self.assertIn("1 count-asymmetric", text)

    def test_the_exact_rows_are_counted(self):
        self.assertIn("1 exact.", self.render())

    def test_pinned_rows_sort_after_open_ones(self):
        lines = [line for line in self.render().splitlines()
                 if line.startswith("  ") and "TU TOTALS" not in line]
        self.assertLess([i for i, l in enumerate(lines)
                         if "open_one" in l][0],
                        [i for i, l in enumerate(lines)
                         if "pinned_one" in l][0])

    def test_every_row_carries_the_count_pair_and_the_decode(self):
        text = self.render()
        self.assertIn("T40/O40", text)
        self.assertIn("T51/O52", text)
        self.assertIn("DECODE: REGFIELD-ONLY 12", text)


class UnitCliContract(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        if not (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").exists():
            raise unittest.SkipTest("checkout is not built")

    def run_cli(self, *argv):
        return subprocess.run([sys.executable, TOOL, *argv],
                              cwd=str(ROOT), capture_output=True, text=True)

    def test_the_unit_flag_and_the_bare_positional_agree(self):
        # Run-59 item 10: the paired count is DERIVED from the objects, not
        # pinned at 84, so a retirement (or any source change that adds or
        # drops a function) cannot break a test about flag equivalence.
        a = self.run_cli("--unit", "game/enemy/enemy")
        b = self.run_cli("game/enemy/enemy")
        self.assertEqual(a.returncode, 0, a.stdout + a.stderr)
        self.assertEqual(a.stdout, b.stdout)
        paired = len(set(wd.unit_bodies(wd.our_object("game/enemy/enemy")[0]))
                     & set(wd.unit_bodies(
                         wd.target_object("game/enemy/enemy"))))
        self.assertGreater(paired, 0)
        self.assertIn(f"{paired} function(s) paired", a.stdout)

    def test_giving_the_unit_twice_is_refused(self):
        # Ambiguous about which argument is the unit; a tool that picked
        # one silently would measure a different thing than was typed.
        proc = self.run_cli("game/enemy/enemy", "--unit", "game/x/y")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("give the unit once", proc.stderr)

    def test_json_is_refused_for_a_single_function(self):
        proc = self.run_cli("game/enemy/enemy", "closest_enemy", "--json")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("whole-TU output", proc.stderr)

    def test_json_carries_one_row_per_paired_function(self):
        proc = self.run_cli("--unit", "game/enemy/enemy", "--json")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        payload = json.loads(proc.stdout)
        self.assertEqual(payload["unit"], "game/enemy/enemy")
        rows, _kind = wd.unit_rows("game/enemy/enemy")
        self.assertEqual(len(payload["rows"]), len(rows))
        self.assertGreater(len(rows), 0)
        # Run-61 item 3 added `count_asymmetric`, `instruction_delta` and the
        # two body hashes to every row, and a `totals` block beside them, so
        # a census can sum the column a COUNT-ASYMMETRIC row leaves null and
        # can tell "same word count" from "same bytes".
        self.assertEqual(sorted(payload), ["object", "rows", "totals", "unit"])
        for row in payload["rows"]:
            self.assertEqual(
                sorted(row),
                sorted(["function", "pinned", "target_insns", "ours_insns",
                        "verdict", "differing_words", "count_asymmetric",
                        "instruction_delta", "body_sha256_ours",
                        "body_sha256_target",
                        "mnemonic_divergence", "klass", "decode"]))

    def test_a_single_function_still_prints_its_headline(self):
        """THE NEGATIVE SIDE: the per-function mode is untouched.

        Run-59 item 10: the word count is DERIVED from the module's own
        measurement of the same function rather than pinned at 16, so this
        gates CLI-vs-library agreement (what it is for) instead of the
        current residual of one function in a TU other lanes are editing.
        """
        proc = self.run_cli("game/enemy/enemy", "closest_enemy")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        _kind, _insns, diffs, _mnem = wd.word_diff(
            "game/enemy/enemy", "closest_enemy")
        self.assertIn(f"DIFFERING WORDS = {len(diffs)}", proc.stdout)
        self.assertIn("PINNED = no", proc.stdout)


class UnitTotalsMatchPerFunctionRuns(unittest.TestCase):
    """The brief's calibration, asserted for one unit inside the suite.

    The full two-unit run is build/T2_item4_calibrate.py; this keeps the
    cheaper half permanently, using the module's OWN single-function
    measurement rather than 84 subprocesses.
    """

    @classmethod
    def setUpClass(cls):
        if not (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").exists():
            raise unittest.SkipTest("checkout is not built")
        os.chdir(ROOT)

    def test_each_row_equals_the_single_function_measurement(self):
        """The `--unit` table equals the per-function measurements, row by
        row and in total.

        RUN-59 ITEM 10. The total was asserted as `== 835`, the unit's
        differing-word count on the day this was written, so the next
        successful RETIREMENT in game/enemy/enemy (which lowers it to 830)
        broke a test about mode agreement. That is WR's run-53 defect —
        a fixture hardcoding a TU's live state — and the cure is to compare
        the two SIDES rather than either side against a constant.

        Both numbers are now derived in this test: the row count from the
        paired functions the module itself finds, and the total from the
        per-function sum computed here. A retirement moves both together; a
        real disagreement between the two modes still fails, and names the
        function it disagreed on.
        """
        rows, _kind = wd.unit_rows("game/enemy/enemy")
        paired = set(wd.unit_bodies(wd.our_object("game/enemy/enemy")[0])) & \
            set(wd.unit_bodies(wd.target_object("game/enemy/enemy")))
        self.assertEqual(len(rows), len(paired))
        self.assertGreater(len(rows), 0, "no paired functions to compare")
        checked = per_function_total = 0
        for row in rows:
            try:
                kind, insns, diffs, mnem = wd.word_diff(
                    "game/enemy/enemy", row["function"])
            except wd.CountAsymmetric:
                self.assertEqual(row["verdict"], "COUNT-ASYMMETRIC")
                continue
            self.assertEqual(insns, row["ours_insns"], row["function"])
            self.assertEqual(len(diffs), row["differing_words"],
                             row["function"])
            self.assertEqual(mnem, row["mnemonic_divergence"],
                             row["function"])
            per_function_total += len(diffs)
            checked += 1
        asymmetric = sum(1 for r in rows
                         if r["verdict"] == "COUNT-ASYMMETRIC")
        self.assertEqual(checked, len(rows) - asymmetric)
        self.assertEqual(sum(r["differing_words"] or 0 for r in rows),
                         per_function_total)


if __name__ == "__main__":
    unittest.main()
