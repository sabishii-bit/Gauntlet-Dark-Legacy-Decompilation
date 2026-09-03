#!/usr/bin/env python3
"""T19 run-49 item 2: --discard must re-score the tree it restored.

THE DEFECT (FT). `--discard` restores the source, rebuilds the object and
RETURNS. The per-function state keeps the DISCARDED probe's numbers, and
`count_class_line` is a TRANSITION report, so the next probe of the restored
tree announces a class change between two states nothing ever went through.

REPRODUCED at 21fdf082b on game/world/camera::camera_mode_dest, with the
pre-fix probe.py taken from HEAD, one count-changing edit
(`volatile s32 t19_marker; t19_marker = camIdx;`) and a whole-file discard:

    A  COUNT-PARITY LOST  insns T682/O682 -> T682/O683 ...
    B  discarded: src\\game\\world\\camera.c restored to HEAD
       [object rebuilt after --discard: ...]
    C  COUNT-PARITY GAINED  insns T682/O683 -> T682/O682: ... this function
       has just become eligible for one.
       IMMEDIATE-ROW ARBITER: 16 row(s) (-91 vs the last probe's 107) ...

C is measured on a tree byte-identical to HEAD, which was never asymmetric
and gained nothing; the arbiter's -91 is the same staleness in the other
instrument. After the fix, B carries the re-score and names the banner it
suppressed, and C reads `NEUTRAL real 156 (insns T682/O682, multiset 0t)`
with the arbiter back to `(UNCHANGED)`.

TWO-SIDED over probe's restore surface (every `rebuild_after_restore` caller
plus `--revert`, at 21fdf082b):

    --discard             early return, stale state   POSITIVE, fixed
    --discard --function  early return, stale state   POSITIVE, fixed
    --revert-baseline     early return, stale state   POSITIVE, fixed
    --revert / --restore  falls through to main()'s   NEGATIVE, untouched
                          own build and re-score

and two fail-closed sub-cases where re-scoring is impossible (`--no-rebuild`,
and a rebuild or score that fails): the PREV base is DROPPED, never left
describing the discarded probe.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import probe  # noqa: E402

DISCARDED = {
    "last_real": 489, "last_insns": "T682/O683", "last_multiset": 1,
    "last_bytes": "deadbeef", "last_data": "cafe", "last_immediates": 107,
    "last_words": 62, "last_fuzzy": 91.0, "last_fuzzy_bytes": "deadbeef",
    "last_verdict": "REGRESSED vs best 156: real 156 -> 489  [revert advised]",
    "count_class": "COUNT-PARITY LOST  insns T682/O682 -> T682/O683: ...",
    "best_real": 156, "baseline_real": 156,
}


class Harness(unittest.TestCase):
    """rescore_after_restore with its two measuring calls stubbed."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.state = Path(self.tmp.name) / "probe_state.json"
        self.state.write_text(json.dumps(DISCARDED), encoding="utf-8")
        self._score = probe.score_function
        self._digest = probe.object_digest
        self._data = probe.data_digest
        probe.score_function = lambda *a, **k: (156, "T682/O682")
        probe.object_digest = lambda *a, **k: "restored"
        probe.data_digest = lambda *a, **k: "restored-data"

    def tearDown(self):
        probe.score_function = self._score
        probe.object_digest = self._digest
        probe.data_digest = self._data
        self.tmp.cleanup()

    def after(self):
        return json.loads(self.state.read_text(encoding="utf-8"))


class Rescore(Harness):
    def test_the_prev_base_describes_the_restored_tree(self):
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        got = self.after()
        self.assertEqual(got["last_real"], 156)
        self.assertEqual(got["last_insns"], "T682/O682")

    def test_the_stale_class_change_banner_is_cleared(self):
        """`count_class` is printed ABOVE the verdict on the next probe."""
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        self.assertNotIn("count_class", self.after())

    def test_the_suppressed_false_banner_is_NAMED_not_silently_dropped(self):
        """A suppression nobody can see is indistinguishable from a bug."""
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertIn("SUPPRESSED, false", out.text)
        self.assertIn("COUNT-PARITY GAINED", out.text)
        self.assertIn("T682/O683 -> T682/O682", out.text)

    def test_no_parity_transition_means_no_suppression_note(self):
        """The negative side: a discard of a same-parity edit refreshes the
        base and says nothing about a banner that was never going to fire."""
        self.state.write_text(
            json.dumps(dict(DISCARDED, last_insns="T682/O682")),
            encoding="utf-8")
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertNotIn("SUPPRESSED", out.text)
        self.assertIn("PREV comparison base refreshed", out.text)

    def test_the_immediate_row_count_is_refreshed_not_left_at_107(self):
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        self.assertNotEqual(self.after().get("last_immediates"), 107)

    def test_the_discarded_verdict_is_not_left_for_a_RE_SCORE_to_replay(self):
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        self.assertNotIn("revert advised", self.after()["last_verdict"])
        self.assertIn("RESTORED", self.after()["last_verdict"])

    def test_last_bytes_is_DROPPED_so_the_next_probe_is_not_IDENTICAL(self):
        """Refreshing it traded one false banner for another: the next probe
        found prev_digest == digest and annotated its NEUTRAL as `the edit
        FOLDED AWAY before codegen`, about an edit that does not exist.
        Measured live on camera_mode_dest while building this fix."""
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        self.assertNotIn("last_bytes", self.after())

    def test_an_unmeasured_fuzzy_and_word_count_are_dropped_not_kept(self):
        probe.rescore_after_restore("game/world/camera", "camera_mode_dest",
                                    self.state, "--discard")
        got = self.after()
        for key in ("last_fuzzy", "last_fuzzy_bytes", "last_words"):
            self.assertNotIn(key, got)

    def test_the_BEST_anchor_is_untouched_and_the_risk_is_stated(self):
        """A discard does not roll the BEST anchor back — there is no
        snapshot anchor for HEAD — so the next probe is still classified
        against it. Say so rather than move it silently."""
        self.state.write_text(json.dumps(dict(DISCARDED, best_real=120)),
                              encoding="utf-8")
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertEqual(self.after()["best_real"], 120)
        self.assertIn("BEST anchor", out.text)

    def test_a_BEST_anchor_that_already_describes_this_state_is_silent(self):
        """The negative side: nothing to warn about when the anchor and the
        restored tree are the same number."""
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertEqual(self.after()["best_real"], 156)
        self.assertNotIn("BEST anchor", out.text)

    def test_nothing_is_banked_and_no_verdict_is_computed(self):
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertIn("no verdict computed, nothing banked", out.text)

    def test_a_function_that_cannot_be_scored_DROPS_the_base(self):
        probe.score_function = lambda *a, **k: (None, None)
        with _capture() as out:
            probe.rescore_after_restore(
                "game/world/camera", "camera_mode_dest", self.state,
                "--discard")
        self.assertIn("could not re-score", out.text)
        self.assertNotIn("last_real", self.after())


class FailClosed(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.state = Path(self.tmp.name) / "probe_state.json"
        self.state.write_text(json.dumps(DISCARDED), encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def test_every_prev_field_is_dropped_and_the_anchors_survive(self):
        with _capture() as out:
            dropped = probe.invalidate_prev_state(self.state, "--discard")
        got = json.loads(self.state.read_text(encoding="utf-8"))
        for key in probe.PREV_STATE_KEYS:
            self.assertNotIn(key, got)
        self.assertEqual(got["best_real"], 156)
        self.assertEqual(got["baseline_real"], 156)
        self.assertEqual(len(dropped), len(probe.PREV_STATE_KEYS))
        self.assertIn("NOT re-measured", out.text)

    def test_a_state_with_no_prev_fields_prints_nothing(self):
        self.state.write_text(json.dumps({"best_real": 5}), encoding="utf-8")
        with _capture() as out:
            self.assertEqual(
                probe.invalidate_prev_state(self.state, "--discard"), [])
        self.assertEqual(out.text.strip(), "")

    def test_a_missing_or_corrupt_state_file_is_survivable(self):
        missing = Path(self.tmp.name) / "nope.json"
        self.assertEqual(probe.invalidate_prev_state(missing, "x"), [])
        self.state.write_text("{not json", encoding="utf-8")
        self.assertEqual(probe.invalidate_prev_state(self.state, "x"), [])


class Wiring(unittest.TestCase):
    SRC = (REPO / "tools" / "gdl" / "probe.py").read_text(encoding="utf-8")

    def test_all_three_early_returning_restores_re_score(self):
        """Every rebuild_after_restore caller is followed by a re-score."""
        callers = [line for line in self.SRC.splitlines()
                   if "rebuild_after_restore(unit," in line
                   and not line.startswith("def ")]
        self.assertEqual(len(callers), 3, callers)
        for why in ('"--discard"', '"--discard --function"',
                    '"--revert-baseline"'):
            body = self.SRC.split("def main(")[-1]
            self.assertIn(why, body)
        rescores = [line for line in self.SRC.splitlines()
                    if "rescore_after_restore(unit, fn, state_file" in line
                    and not line.startswith("def ")]
        self.assertEqual(len(rescores), 3, rescores)

    def test_no_rebuild_falls_back_to_invalidation_not_silence(self):
        """Three restore paths x two fail-closed branches (--no-rebuild and
        a failed rebuild), plus the one inside rescore_after_restore for a
        score that names no such function."""
        body = self.SRC.split("def main(")[-1]
        self.assertEqual(body.count("invalidate_prev_state(state_file,"), 6)

    def test_revert_is_NOT_given_a_second_re_score(self):
        """The negative side of the calibration: --revert already re-scores
        by falling through to main(). A second one would double the build."""
        head, _, tail = self.SRC.partition('if "--revert" in sys.argv'
                                           ' or restore_tag:')
        self.assertNotIn("rescore_after_restore", tail.split(
            "coupled_scope = False")[0])


class _capture:
    def __enter__(self):
        import io
        self._old = sys.stdout
        sys.stdout = self._buf = io.StringIO()
        return self

    def __exit__(self, *exc):
        sys.stdout = self._old
        self.text = self._buf.getvalue()
        return False


if __name__ == "__main__":
    unittest.main()
