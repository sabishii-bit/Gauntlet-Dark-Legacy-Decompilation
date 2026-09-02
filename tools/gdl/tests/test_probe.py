"""probe.py verdict-table tests.

The regression this file exists for: probe's CONFLICT verdict compared the
opcode-multiset token count against the PREVIOUS probe, and every probe
banked its own count into that slot. Re-scoring an already-scored state
therefore flipped CONFLICT -> "REGRESSED ... [revert advised]" on bytes
that had not moved (measured on game/sys/memcard get_vmu_directory during
run 29: real 65 -> 65, insns and multiset both unchanged).
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from probe import (REPLAN_AT, annotate_neutral, classify, count_distance,
                   function_span, fuzzy_anchor_note, moved_sections,
                   parse_section_digests, replan_hint, scaffold_rows,
                   scoped_revert, split_lines, strip_noncode,
                   update_neutral_identical_streak)


TU = """\
#include "game.h"

static int helper(int a)
{
    return a + 1;
}

void alpha(Player* p)
{
    p->x = 1;
    p->y = 2;
}

void beta(Player* p)
{
    /* a } brace in a comment must not close the body */
    const char* s = "} neither must this one";
    p->z = 3;
}
"""



class CountDistanceTests(unittest.TestCase):
    def test_parses_insns_string(self):
        self.assertEqual(count_distance("T116/O115"), 1)
        self.assertEqual(count_distance("T290/O290"), 0)

    def test_unparseable_is_none(self):
        self.assertIsNone(count_distance("exact"))
        self.assertIsNone(count_distance(None))


class ClassifyTests(unittest.TestCase):
    def test_baseline_banks_best_real_and_best_multiset(self):
        verdict, state = classify({}, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("BASELINE"))
        self.assertEqual(state["best_real"], 65)
        self.assertEqual(state["best_multiset"], 3)

    def test_improvement_rebanks_both_best_fields(self):
        state = {"best_real": 65, "best_multiset": 3,
                 "last_real": 65, "last_insns": "T116/O115",
                 "last_multiset": 3}
        verdict, state = classify(state, 48, "T116/O116", 2)
        self.assertTrue(verdict.startswith("IMPROVED"), verdict)
        self.assertEqual(state["best_real"], 48)
        self.assertEqual(state["best_multiset"], 2)

    def test_conflict_is_anchored_on_best_not_prev(self):
        """A real rise with structure converging against BEST is CONFLICT."""
        state = {"best_real": 48, "best_multiset": 4,
                 "last_real": 48, "last_insns": "T116/O116",
                 "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("4t -> 3t vs best", verdict)

    def test_rescoring_a_conflict_state_does_not_flip_to_regressed(self):
        """THE run-29 REGRESSION TEST.

        Feed classify() exactly the state a CONFLICT probe leaves behind,
        then re-score the same measurement. Under the prev-anchored
        comparison this returned REGRESSED with '[revert advised]'.
        """
        _, after_conflict = classify(
            {"best_real": 48, "best_multiset": 4, "last_real": 48,
             "last_insns": "T116/O116", "last_multiset": 4},
            65, "T116/O115", 3)
        verdict, _ = classify(after_conflict, 65, "T116/O115", 3)
        self.assertNotIn("REGRESSED", verdict)
        self.assertNotIn("revert advised", verdict)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)

    def test_real_rise_without_structure_gain_is_still_regressed(self):
        state = {"best_real": 48, "best_multiset": 4,
                 "last_real": 48, "last_insns": "T116/O116",
                 "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O120", 5)
        self.assertTrue(verdict.startswith("REGRESSED"), verdict)

    def test_legacy_state_without_best_multiset_says_so(self):
        state = {"best_real": 48, "last_real": 48,
                 "last_insns": "T116/O116", "last_multiset": 4}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("vs prev", verdict)
        self.assertIn("no best_multiset banked", verdict)

    def test_legacy_fallback_is_flagged_on_the_regressed_half_too(self):
        """The half that tells a worker to throw work away must say it.

        This is the exact legacy shape measured live in run 29: the state
        a CONFLICT left behind, re-scored, reads REGRESSED because prev
        already carries the improved multiset.
        """
        state = {"best_real": 48, "last_real": 65,
                 "last_insns": "T116/O115", "last_multiset": 3}
        verdict, _ = classify(state, 65, "T116/O115", 3)
        self.assertTrue(verdict.startswith("REGRESSED"), verdict)
        self.assertIn("no best_multiset banked", verdict)

    def test_rebase_best_banks_current_as_best(self):
        state = {"best_real": 48, "best_multiset": 4, "last_real": 65,
                 "last_insns": "T116/O115", "last_multiset": 3}
        verdict, state = classify(state, 65, "T116/O115", 3,
                                  rebase_best=True)
        self.assertTrue(verdict.startswith("REBASED"), verdict)
        self.assertEqual(state["best_real"], 65)
        self.assertEqual(state["best_multiset"], 3)

    def test_blown_out_count_distance_refuses_to_bank_a_real_win(self):
        state = {"best_real": 949, "best_multiset": 20, "last_real": 949,
                 "last_insns": "T500/O500", "last_multiset": 20}
        verdict, state = classify(state, 802, "T500/O343", 25)
        self.assertIn("IMPROVED?", verdict)
        self.assertEqual(state["best_real"], 949)

    def test_parity_held_improvement_demands_fuzzy_arbitration(self):
        state = {"best_real": 30, "best_multiset": 0, "last_real": 30,
                 "last_insns": "T47/O47", "last_multiset": 0}
        verdict, _ = classify(state, 24, "T47/O47", 0)
        self.assertIn("PARITY-HELD IMPROVEMENT", verdict)


class MultisetOutranksRealTests(unittest.TestCase):
    """run-31 item 3: the headline verdict is decided by STRUCTURE first.

    The two shapes where a real-only headline actively misadvises, both
    measured against the pre-item classify():

      real 100 -> 90 with the multiset 2t -> 8t   read "IMPROVED [best
      updated]" — indistinguishable from a probe whose structure also
      converged, and it banked the diverged state as the revert point.

      real 100 -> 100 with the multiset 8t -> 0t  read "NEUTRAL", hiding a
      complete structural convergence, and left best_multiset stale at 8.
    """

    DIVERGED = {"best_real": 100, "best_multiset": 2, "last_multiset": 2,
                "last_real": 100, "last_insns": "T120/O120"}
    CONVERGED = {"best_real": 100, "best_multiset": 8, "last_multiset": 8,
                 "last_real": 100, "last_insns": "T120/O120"}

    def test_real_win_with_a_growing_multiset_is_a_CONFLICT(self):
        verdict, state = classify(dict(self.DIVERGED), 90, "T120/O120", 8)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("2t -> 8t vs best", verdict)
        self.assertIn("DIVERGED", verdict)
        self.assertIn("do NOT auto-bank", verdict)

    def test_a_diverged_real_win_does_not_become_the_new_best(self):
        _, state = classify(dict(self.DIVERGED), 90, "T120/O120", 8)
        self.assertEqual(state["best_real"], 100)
        self.assertEqual(state["best_multiset"], 2)

    def test_real_win_with_a_falling_multiset_is_still_IMPROVED(self):
        verdict, state = classify(dict(self.CONVERGED), 90, "T120/O120", 2)
        self.assertTrue(verdict.startswith("IMPROVED"), verdict)
        self.assertEqual(state["best_real"], 90)
        self.assertEqual(state["best_multiset"], 2)

    def test_real_win_with_a_flat_multiset_is_still_IMPROVED(self):
        verdict, state = classify(dict(self.CONVERGED), 90, "T120/O120", 8)
        self.assertTrue(verdict.startswith("IMPROVED"), verdict)
        self.assertEqual(state["best_real"], 90)

    def test_flat_real_with_a_falling_multiset_is_IMPROVED_STRUCTURE(self):
        verdict, state = classify(dict(self.CONVERGED), 100, "T120/O120", 0)
        self.assertTrue(verdict.startswith("IMPROVED-STRUCTURE"), verdict)
        self.assertIn("8t -> 0t vs best", verdict)

    def test_a_flat_real_structural_gain_rebanks_the_multiset_anchor(self):
        """The stale-anchor half: best_multiset stayed at 8 before."""
        _, state = classify(dict(self.CONVERGED), 100, "T120/O120", 0)
        self.assertEqual(state["best_multiset"], 0)
        self.assertEqual(state["best_real"], 100)

    def test_flat_real_with_a_flat_multiset_is_still_NEUTRAL(self):
        verdict, _ = classify(dict(self.CONVERGED), 100, "T120/O120", 8)
        self.assertTrue(verdict.startswith("NEUTRAL"), verdict)

    def test_flat_real_with_a_growing_multiset_is_still_NEUTRAL(self):
        """annotate_neutral owns that case (NEUTRAL-WORSE); classify must
        not steal it out from under the byte-identity check."""
        verdict, _ = classify(dict(self.CONVERGED), 100, "T120/O120", 12)
        self.assertTrue(verdict.startswith("NEUTRAL"), verdict)

    def test_no_multiset_measurement_leaves_every_verdict_unchanged(self):
        self.assertTrue(classify(dict(self.CONVERGED), 90, "T120/O120",
                                 None)[0].startswith("IMPROVED"))
        self.assertTrue(classify(dict(self.CONVERGED), 100, "T120/O120",
                                 None)[0].startswith("NEUTRAL"))

    def test_a_legacy_state_falls_back_to_prev_and_says_so(self):
        state = {"best_real": 100, "last_multiset": 2, "last_real": 100,
                 "last_insns": "T120/O120"}
        verdict, _ = classify(state, 90, "T120/O120", 8)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        self.assertIn("vs prev", verdict)
        self.assertIn("no best_multiset banked", verdict)


class ReplanHintTests(unittest.TestCase):
    """run-31 item 10.

    NEUTRAL-IDENTICAL means the object bytes did not move: the edit folded
    away BEFORE codegen, so the source text never reached the compiler's
    decision point. One is a strong negative on that spelling. Three in a
    row is evidence about the AXIS CLASS — the decision point is not
    reachable from this construct at all — and the loop said nothing,
    inviting a fourth spelling of the same dead lever.
    """

    IDENTICAL = ("NEUTRAL   real 30 (insns T47/O47, multiset 0t)"
                 "  [NEUTRAL-IDENTICAL: object bytes unchanged — the edit"
                 " FOLDED AWAY before codegen.]")
    REARRANGED = ("NEUTRAL   real 30  [NEUTRAL-REARRANGED: OBJECT BYTES"
                  " CHANGED]")

    def streak(self, verdicts, start=0):
        state = {"neutral_identical_streak": start}
        for verdict in verdicts:
            state["neutral_identical_streak"] = \
                update_neutral_identical_streak(state, verdict)
        return state["neutral_identical_streak"]

    def test_consecutive_identicals_accumulate(self):
        self.assertEqual(self.streak([self.IDENTICAL] * 3), 3)

    def test_any_other_verdict_resets_the_streak(self):
        self.assertEqual(
            self.streak([self.IDENTICAL, self.IDENTICAL,
                         "IMPROVED  real 30 -> 24", self.IDENTICAL]), 1)

    def test_a_rearranged_neutral_does_not_count(self):
        """Bytes MOVED there — the source did reach codegen."""
        self.assertEqual(self.streak([self.REARRANGED] * 3), 0)

    def test_a_rescore_neither_counts_nor_resets(self):
        """A re-score recomputes nothing, so it is not a probe."""
        rescored = ("RE-SCORE  real 30 — nothing moved since the last"
                    f" probe:\n{self.IDENTICAL}")
        self.assertEqual(self.streak([rescored, rescored], start=2), 2)

    def test_no_hint_below_the_threshold(self):
        for count in range(REPLAN_AT):
            self.assertIsNone(replan_hint(count))

    def test_the_hint_fires_at_the_threshold(self):
        hint = replan_hint(REPLAN_AT)
        self.assertIsNotNone(hint)
        self.assertIn("RE-PLAN THE AXIS CLASS", hint)
        self.assertIn(str(REPLAN_AT), hint)

    def test_the_hint_says_not_to_try_another_spelling(self):
        hint = replan_hint(REPLAN_AT + 2)
        self.assertIn("spelling", hint)
        self.assertIn(str(REPLAN_AT + 2), hint)

    def test_the_hint_persists_above_the_threshold(self):
        self.assertIsNotNone(replan_hint(9))


class RescoreGuardTests(unittest.TestCase):
    BASE = {"best_real": 48, "best_multiset": 4, "last_real": 65,
            "last_insns": "T116/O115", "last_multiset": 3,
            "last_bytes": "abc123", "last_verdict": "CONFLICT  standing"}

    def test_unchanged_source_and_digest_repeats_the_standing_verdict(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="abc123", source_changed=False)
        self.assertTrue(verdict.startswith("RE-SCORE"), verdict)
        self.assertIn("CONFLICT  standing", verdict)
        self.assertIn("REPEATED, not recomputed", verdict)

    def test_changed_source_recomputes_even_at_equal_scores(self):
        """An edit that folds away must still be classified, not swallowed."""
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="abc123", source_changed=True)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)

    def test_moved_bytes_at_equal_scores_recompute(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest="deadbeef", source_changed=False)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)

    def test_no_digest_means_no_guard(self):
        verdict, _ = classify(dict(self.BASE), 65, "T116/O115", 3,
                              digest=None, source_changed=False)
        self.assertFalse(verdict.startswith("RE-SCORE"), verdict)


class AnnotateNeutralTests(unittest.TestCase):
    def test_identical_bytes_flag_a_folded_away_edit(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O50, multiset 0t)",
                               4, "T50/O50", 0, 0, "T50/O50", "same", "same")
        self.assertIn("NEUTRAL-IDENTICAL", out)

    def test_moved_bytes_flag_a_rearrangement(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O50, multiset 0t)",
                               4, "T50/O50", 0, 0, "T50/O50", "old", "new")
        self.assertIn("NEUTRAL-REARRANGED", out)

    def test_structurally_worse_neutral_is_not_banked(self):
        out = annotate_neutral("NEUTRAL   real 4 (insns T50/O44, multiset 6t)",
                               4, "T50/O44", 6, 2, "T50/O50", "old", "new")
        self.assertTrue(out.startswith("NEUTRAL-WORSE"), out)
        self.assertIn("count distance 0 -> 6", out)
        self.assertIn("multiset 2t -> 6t", out)


class FuzzyAnchorTests(unittest.TestCase):
    """Run-32 item 3: CONFLICT ordered a fuzzy arbitration and printed no
    fuzzy.

    Reproduced before implementing, by driving classify() with a state that
    already carried last_fuzzy = 90.04: both CONFLICT branches produced a
    verdict containing neither "90.04" nor (in the converging branch) the
    word "fuzzy" at all. Every arbitration therefore cost two report builds
    — one on the current state, one after a revert — even when probe had
    already measured one of them. Three lanes paid that.
    """

    BEST = {"best_real": 48, "best_multiset": 4, "best_insns": "T116/O116",
            "last_real": 48, "last_insns": "T116/O116", "last_multiset": 4}

    def conflict_real_rose(self, **extra):
        state = dict(self.BEST, **extra)
        verdict, _ = classify(state, 65, "T116/O116", 3)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        return verdict

    def conflict_real_fell(self, **extra):
        state = dict(self.BEST, **extra)
        verdict, _ = classify(state, 40, "T116/O116", 6)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        return verdict

    def test_a_cached_anchor_is_printed_on_the_converging_conflict(self):
        verdict = self.conflict_real_rose(best_fuzzy=90.04)
        self.assertIn("90.0400", verdict)
        self.assertIn("BEST-STATE FUZZY", verdict)

    def test_a_cached_anchor_is_printed_on_the_diverging_conflict(self):
        verdict = self.conflict_real_fell(best_fuzzy=90.04)
        self.assertIn("90.0400", verdict)

    def test_both_halves_cached_print_the_delta_and_spend_no_build(self):
        state = dict(self.BEST, best_fuzzy=90.04)
        verdict, _ = classify(state, 65, "T116/O116", 3, fuzzy=92.72)
        self.assertIn("90.0400 -> 92.7200 (+2.6800)", verdict)
        self.assertIn("ROSE", verdict)
        self.assertIn("NO build spent", verdict)

    def test_a_missing_anchor_says_how_to_warm_it(self):
        verdict = self.conflict_real_rose()
        self.assertIn("no cached fuzzy anchor", verdict)
        self.assertIn("--fuzzy", verdict)

    def test_banking_a_best_records_the_bytes_it_describes(self):
        _, state = classify({}, 48, "T116/O116", 4, digest="cafe")
        self.assertEqual(state["best_bytes"], "cafe")

    def test_a_measured_fuzzy_is_banked_with_the_new_best(self):
        _, state = classify({}, 48, "T116/O116", 4, digest="cafe",
                            fuzzy=90.04)
        self.assertEqual(state["best_fuzzy"], 90.04)

    def test_a_new_best_without_a_fuzzy_CLEARS_the_stale_anchor(self):
        """A stale anchor would compare a fresh number against a number
        for different bytes — worse than having none."""
        state = dict(self.BEST, best_fuzzy=90.04)
        _, out = classify(state, 30, "T116/O116", 4, digest="beef")
        self.assertTrue(out["best_real"] == 30)
        self.assertNotIn("best_fuzzy", out)

    def test_a_fallen_fuzzy_reads_as_FELL(self):
        note = fuzzy_anchor_note(92.72, 90.04)
        self.assertIn("(-2.6800)", note)
        self.assertIn("FELL", note)

    def test_an_unchanged_fuzzy_reads_as_flat(self):
        self.assertIn("is FLAT", fuzzy_anchor_note(90.04, 90.04))

    def test_non_conflict_verdicts_carry_no_anchor(self):
        """The anchor belongs to the verdict that orders an arbitration."""
        for verdict, _ in (classify({}, 48, "T116/O116", 4),
                           classify(dict(self.BEST), 30, "T116/O116", 4),
                           classify(dict(self.BEST), 48, "T116/O116", 4)):
            self.assertNotIn("BEST-STATE FUZZY", verdict)
            self.assertNotIn("no cached fuzzy anchor", verdict)


class CountDistanceSuppressionTests(unittest.TestCase):
    """Run-32 item 4: an invalid predictor still printed its number.

    Reproduced before implementing. The CONFLICT branch bounds its own
    count-distance predictor to a FLAT multiset, and when the multiset had
    moved it printed "COUNT DISTANCE 1 -> 6 but the multiset moved — the
    predictor is NOT valid here". The figure led and the denial trailed;
    a number in a verdict reads as evidence whatever follows it.
    """

    # real 48 -> 65 (rose) with multiset 4t -> 3t (converging => CONFLICT).
    STATE = {"best_real": 48, "best_multiset": 4, "best_insns": "T116/O116",
             "last_real": 48, "last_insns": "T116/O115", "last_multiset": 4}

    def line(self, insns, tokens):
        verdict, _ = classify(dict(self.STATE), 65, insns, tokens)
        self.assertTrue(verdict.startswith("CONFLICT"), verdict)
        return next((ln.strip() for ln in verdict.splitlines()
                     if "COUNT DISTANCE" in ln), "")

    def test_a_moved_multiset_reports_no_figure_at_all(self):
        line = self.line("T116/O110", 3)          # distance 1 -> 6
        self.assertIn("WITHHELD", line)
        self.assertIsNone(re.search(r"COUNT DISTANCE:? \d+ -> \d+", line))
        self.assertNotRegex(line, r"\b1 -> 6\b")

    def test_it_says_why_the_predictor_is_unavailable(self):
        line = self.line("T116/O110", 3)
        self.assertIn("flat multiset", line)
        self.assertIn("fresh fuzzy", line)

    def test_a_flat_multiset_still_reports_the_figure(self):
        """The predictor is sound there — suppression must be scoped to the
        case that admitted invalidity, not applied to every CONFLICT."""
        state = dict(self.STATE, last_multiset=3)   # flat: 3t -> 3t
        verdict, _ = classify(state, 65, "T116/O110", 3)
        line = next(ln for ln in verdict.splitlines()
                    if "COUNT DISTANCE" in ln)
        self.assertIn("1 -> 6", line)
        self.assertIn("flat multiset", line)
        self.assertNotIn("WITHHELD", line)

    def test_an_unchanged_distance_reports_nothing_either_way(self):
        verdict, _ = classify(dict(self.STATE), 65, "T116/O115", 3)
        self.assertNotIn("COUNT DISTANCE", verdict)


class DataOnlyEditTests(unittest.TestCase):
    """The five-correct-fixes regression.

    Reproduced live on src/game/pb/pb_objregs.c::setChrome (the fix in
    commit adc292074, recorded in
    attempt.CS_image-wide-constant-sweep-five-fixes.20260901.v1):
    respelling the PI literal moved .sdata2+0x40 from 400921fb54442d18 to
    400921fb54524550 and probe printed "NEUTRAL-IDENTICAL: object bytes
    unchanged — the edit FOLDED AWAY before codegen ... a STRONGER
    negative than a regression". The instruction stream really is
    identical; the object is not, and the advice to treat it as a null
    probe is how five correct constant fixes nearly got reverted.
    """

    NEUTRAL = "NEUTRAL   real 0 (insns T362/O362, multiset 0t)"
    BEFORE = {".rodata": "aaa", ".sdata2": "111", ".bss": "zzz"}
    AFTER = {".rodata": "aaa", ".sdata2": "222", ".bss": "zzz"}

    def annotate(self, prev_data, data, source_changed=True):
        return annotate_neutral(self.NEUTRAL, 0, "T362/O362", 0, 0,
                                "T362/O362", "same", "same",
                                prev_data=prev_data, data=data,
                                source_changed=source_changed)

    def test_a_moved_data_section_is_not_a_fold_away(self):
        out = self.annotate(self.BEFORE, self.AFTER)
        self.assertIn("NEUTRAL-DATA-ONLY", out)
        self.assertNotIn("NEUTRAL-IDENTICAL", out)
        self.assertNotIn("FOLDED AWAY", out)

    def test_it_names_the_section_that_moved(self):
        out = self.annotate(self.BEFORE, self.AFTER)
        self.assertIn(".sdata2", out)
        self.assertNotIn(".rodata", out)

    def test_it_directs_to_a_value_audit_not_a_revert(self):
        out = self.annotate(self.BEFORE, self.AFTER)
        self.assertIn("VALUE AUDIT", out)
        self.assertIn("claim.law.SL_pool-constant-errors-are-score-invisible",
                      out)

    def test_flat_data_still_reads_as_a_fold_away(self):
        out = self.annotate(self.BEFORE, dict(self.BEFORE))
        self.assertIn("NEUTRAL-IDENTICAL", out)
        self.assertNotIn("NEUTRAL-DATA-ONLY", out)

    def test_an_unedited_source_is_never_called_data_only(self):
        """A sibling lane's rebuild can move a shared pool; only an edit
        of THIS source may be reported as a data-only change."""
        out = self.annotate(self.BEFORE, self.AFTER, source_changed=False)
        self.assertIn("NEUTRAL-IDENTICAL", out)

    def test_an_unmeasured_side_never_manufactures_the_verdict(self):
        for prev, cur in ((None, self.AFTER), (self.BEFORE, None),
                          (None, None)):
            out = self.annotate(prev, cur)
            self.assertIn("NEUTRAL-IDENTICAL", out)

    def test_a_moved_data_section_does_not_feed_the_axis_dead_streak(self):
        """Three data-only edits are three landed changes, not evidence
        that the axis cannot reach codegen."""
        state = {"neutral_identical_streak": 2}
        out = self.annotate(self.BEFORE, self.AFTER)
        self.assertEqual(update_neutral_identical_streak(state, out), 0)

    def test_moved_bytes_still_outrank_the_data_check(self):
        out = annotate_neutral(self.NEUTRAL, 0, "T362/O362", 0, 0,
                               "T362/O362", "old", "new",
                               prev_data=self.BEFORE, data=self.AFTER)
        self.assertIn("NEUTRAL-REARRANGED", out)
        self.assertNotIn("NEUTRAL-DATA-ONLY", out)


class SectionDigestTests(unittest.TestCase):
    DUMP = """\
pb_objregs.o:     file format elf32-powerpc

Contents of section .text:
 0000 9421ffd0 7c0802a6 90010034 bf610014  .!..|......4.a..
Contents of section .rodata:
 0000 79617700 70697463 68000000 00000000  yaw.pitch.......
Contents of section .sdata2:
 0040 400921fb 54442d18 3f008081 3f000000  @.!.TD-.?...?...
"""

    def test_text_sections_are_excluded(self):
        digests = parse_section_digests(self.DUMP)
        self.assertEqual(sorted(digests), [".rodata", ".sdata2"])

    def test_a_changed_pool_word_changes_only_its_own_section(self):
        after = self.DUMP.replace("54442d18", "54524550")
        moved = moved_sections(parse_section_digests(self.DUMP),
                               parse_section_digests(after))
        self.assertEqual(moved, [".sdata2"])

    def test_an_unchanged_dump_moves_nothing(self):
        digests = parse_section_digests(self.DUMP)
        self.assertEqual(moved_sections(digests, dict(digests)), [])

    def test_an_added_or_dropped_section_counts_as_moved(self):
        digests = parse_section_digests(self.DUMP)
        fewer = {k: v for k, v in digests.items() if k != ".rodata"}
        self.assertEqual(moved_sections(digests, fewer), [".rodata"])

    def test_a_dump_with_no_sections_is_empty_not_an_error(self):
        self.assertEqual(parse_section_digests("no sections here"), {})


class ScaffoldCensusTests(unittest.TestCase):
    SCAFFOLD = "\n".join(
        ["#pragma opt_propagation off"]
        + [f"    volatile int v{i};" for i in range(24)]
        + ["#pragma force_active on", "int plain = 0;"])

    def test_finds_pragmas_and_volatiles_but_not_force_active(self):
        rows = scaffold_rows(self.SCAFFOLD)
        self.assertEqual(len(rows), 25)
        self.assertTrue(any("opt_propagation" in r for r in rows))
        self.assertFalse(any("force_active" in r for r in rows))
        self.assertFalse(any("plain" in r for r in rows))

    def test_rows_are_line_numbered_from_one(self):
        rows = scaffold_rows(self.SCAFFOLD)
        self.assertTrue(rows[0].startswith("  L1: "))

    def test_more_rows_exist_than_the_twenty_row_head(self):
        """The cap is why --scaffold-all had to exist."""
        self.assertGreater(len(scaffold_rows(self.SCAFFOLD)), 20)


class SplitLinesTests(unittest.TestCase):
    def test_crlf_is_preserved_as_content(self):
        self.assertEqual(split_lines("a\r\nb\r\n", keepends=True),
                         ["a\r\n", "b\r\n"])

    def test_does_not_break_on_exotic_separators(self):
        """str.splitlines() breaks on U+0085; a latin-1 round-trip of an
        ordinary source byte can produce one, desynchronising indices."""
        text = "a\x85b\nc\n"
        self.assertEqual(len(split_lines(text)), 2)

    def test_roundtrip_is_byte_exact(self):
        for text in ("a\nb\n", "a\r\nb", "", "\n", "no trailing newline"):
            self.assertEqual("".join(split_lines(text, keepends=True)), text)


class StripNoncodeTests(unittest.TestCase):
    def test_preserves_length_and_line_count(self):
        out = strip_noncode(TU)
        self.assertEqual(len(out), len(TU))
        self.assertEqual(out.count("\n"), TU.count("\n"))

    def test_braces_in_comments_and_strings_are_erased(self):
        out = strip_noncode('int f(){ /* } */ char* s = "}"; }')
        self.assertEqual(out.count("}"), 1)


class FunctionSpanTests(unittest.TestCase):
    def test_locates_a_definition_and_its_body(self):
        start, end = function_span(TU, "alpha")
        lines = split_lines(TU)
        self.assertIn("void alpha", lines[start])
        self.assertEqual(lines[end - 1], "}")

    def test_brace_in_comment_or_string_does_not_close_the_body(self):
        start, end = function_span(TU, "beta")
        lines = split_lines(TU)
        self.assertIn("    p->z = 3;", lines[start:end])
        self.assertEqual(lines[end - 1], "}")

    def test_absent_function_is_none(self):
        self.assertIsNone(function_span(TU, "gamma"))

    def test_object_name_resolves_a_suffixed_source_spelling(self):
        """objdump says `SfxSkipItem`, the source says
        `SfxSkipItem_80096FF4`. 16 of 507 real functions across ten TUs
        are spelled this way; before this the span lookup missed all of
        them and --revert refused."""
        text = TU.replace("void alpha(", "void alpha_8007FC80(")
        self.assertIsNotNone(function_span(text, "alpha"))

    def test_suffixed_name_resolves_an_unsuffixed_source_spelling(self):
        self.assertIsNotNone(function_span(TU, "alpha_8007FC80"))

    def test_suffix_tolerance_does_not_match_a_different_function(self):
        text = TU.replace("void alpha(", "void alpha_8007FC80(")
        self.assertIsNone(function_span(text, "alph"))


class ScopedRevertTests(unittest.TestCase):
    def test_reverts_only_the_named_function(self):
        edited = TU.replace("p->x = 1;", "p->x = 99;") \
                   .replace("p->z = 3;", "p->z = 77;")
        out, notes = scoped_revert(TU, edited, "alpha")
        self.assertIn("p->x = 1;", out)     # alpha restored
        self.assertIn("p->z = 77;", out)    # beta's in-progress work kept
        self.assertIn("1 hunk(s) inside alpha reverted", notes)
        self.assertIn("1 hunk(s) elsewhere", notes)

    def test_insertions_and_deletions_inside_the_function(self):
        edited = TU.replace("    p->y = 2;\n", "")
        edited = edited.replace("    p->x = 1;\n",
                                "    p->x = 1;\n    p->w = 0;\n")
        out, _ = scoped_revert(TU, edited, "alpha")
        self.assertEqual(out, TU)

    def test_edit_outside_every_function_is_left_alone(self):
        edited = TU.replace('#include "game.h"', '#include "game.h"\n#include "x.h"')
        out, notes = scoped_revert(TU, edited, "alpha")
        self.assertEqual(out, edited)
        self.assertIn("0 hunk(s) inside alpha reverted", notes)

    def test_straddling_hunk_is_refused_not_guessed(self):
        """One contiguous hunk covering alpha's last line AND the line
        after it — the only shape a function-scoped revert cannot split."""
        edited = TU.replace("}\n\nvoid beta(Player* p)",
                            "}   /* end of alpha */\nvoid beta(Player* p)")
        with self.assertRaises(ValueError) as ctx:
            scoped_revert(TU, edited, "alpha")
        self.assertIn("straddle", str(ctx.exception))
        self.assertIn("--whole-file", str(ctx.exception))

    def test_missing_function_refuses_loudly(self):
        with self.assertRaises(ValueError) as ctx:
            scoped_revert(TU, TU.replace("void alpha", "void renamed"),
                          "alpha")
        self.assertIn("working source", str(ctx.exception))

    def test_crlf_source_round_trips(self):
        crlf = TU.replace("\n", "\r\n")
        edited = crlf.replace("p->x = 1;", "p->x = 99;")
        out, _ = scoped_revert(crlf, edited, "alpha")
        self.assertEqual(out, crlf)
        # every LF is still part of a CRLF — no line ending was rewritten
        self.assertEqual(out.count("\n"), out.count("\r\n"))


if __name__ == "__main__":
    unittest.main()
