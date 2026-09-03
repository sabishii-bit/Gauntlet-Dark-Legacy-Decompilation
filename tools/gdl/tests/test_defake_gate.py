"""defake_gate structure-arbiter and durable-baseline tests.

The gate scores only `real`, and `real` cannot see structure. Closing one
compensating error re-aligns every instruction after it, so a stream that
moved strictly NEARER target can score worse: get_vmu_directory went real
48 -> 65 while its opcode multiset went 4t -> 3t and its fresh fuzzy went
90.04 -> 92.72. The gate called that a REGRESSION and the lane had to
override it by hand.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from defake_gate import (arbitrate_regressions, arbitration_event, compare,
                         data_section_verdicts, format_arbitrations,
                         load_baseline, log_arbitration,
                         naming_drift_is_benign, parse_section_digests,
                         format_roster, parse_clean,
                         read_arbitrations, read_report_fuzzy,
                         relocation_change_direction, roster_rows,
                         summarize_arbitrations)


def no_ops(_unit, _name):
    """--ops output with a DIFFERING multiset: the naming-churn route off."""
    return "  opcode multiset: DIFFERS  target-only: +1 b  ours-only: -1 beq"


def identical_ops(_unit, _name):
    return "  opcode multiset: IDENTICAL (116/116)"


class StructureArbiterTests(unittest.TestCase):
    VERDICTS = [("get_vmu_directory", "REGRESSION", "real 48 -> 65")]

    def arb(self, baseline, genuine_now, ops_fn=no_ops):
        return arbitrate_regressions(
            list(self.VERDICTS), "game/sys/memcard", baseline,
            genuine_fn=lambda unit, names: genuine_now, ops_fn=ops_fn)

    def test_real_rise_with_genuine_rows_falling_is_a_CONFLICT(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 1})
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("genuine structural rows 5 -> 1 FELL", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_real_rise_with_genuine_rows_holding_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("genuine structural rows 5 -> 5", out[0][2])

    def test_real_rise_with_genuine_rows_rising_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 2}},
                       {"get_vmu_directory": 9})
        self.assertEqual(out[0][1], "REGRESSION")

    def test_legacy_baseline_without_genuine_counts_says_it_is_unavailable(self):
        out = self.arb({"get_vmu_directory": {}}, {"get_vmu_directory": 1})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("structure arbiter is UNAVAILABLE", out[0][2])

    def test_identical_multiset_route_still_wins_first(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5}, ops_fn=identical_ops)
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("opcode multiset IDENTICAL", out[0][2])

    def test_a_byte_exact_function_is_never_arbitrated(self):
        out = arbitrate_regressions(
            [("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
            {"f": {"genuine": 9}},
            genuine_fn=lambda unit, names: {"f": 0}, ops_fn=identical_ops)
        self.assertEqual(out[0][1], "REGRESSION")

    def test_improvements_pass_through_untouched(self):
        out = arbitrate_regressions(
            [("f", "IMPROVED", "real 9 -> 4")], "game/sys/memcard", {},
            genuine_fn=lambda unit, names: {}, ops_fn=no_ops)
        self.assertEqual(out, [("f", "IMPROVED", "real 9 -> 4")])


class FuzzyArbiterTests(unittest.TestCase):
    """run-31 item 2: when the genuine structural rows are FLAT the
    structure arbiter has nothing to say, so the gate printed a bare
    REGRESSION and every keep of this shape had to be overridden by hand
    with a fuzzy the tool never showed. --arbiter fuzzy measures it."""

    VERDICTS = [("get_vmu_directory", "REGRESSION", "real 48 -> 65")]

    def arb(self, baseline, genuine_now, fuzzy_now=None, arbiter=None):
        return arbitrate_regressions(
            list(self.VERDICTS), "game/sys/memcard", baseline,
            genuine_fn=lambda unit, names: genuine_now,
            ops_fn=no_ops,
            fuzzy_fn=(None if fuzzy_now is None
                      else (lambda unit, names: fuzzy_now)),
            arbiter=arbiter)

    def test_flat_genuine_without_the_fuzzy_arbiter_says_it_is_unmeasured(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5})
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("fuzzy delta UNMEASURED", out[0][2])
        self.assertIn("--arbiter fuzzy", out[0][2])

    def test_flat_genuine_with_fuzzy_RISING_is_a_CONFLICT(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("fuzzy 90.0400 -> 92.7200", out[0][2])
        self.assertIn("+2.6800", out[0][2])
        self.assertIn("do NOT auto-revert", out[0][2])

    def test_flat_genuine_with_fuzzy_FALLING_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 92.72}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 90.04}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        # The delta is PRINTED either way — that is the point of the item.
        self.assertIn("fuzzy 92.7200 -> 90.0400", out[0][2])
        self.assertIn("-2.6800", out[0][2])

    def test_flat_genuine_with_fuzzy_EQUAL_stays_a_REGRESSION(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 90.04}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("+0.0000", out[0][2])

    def test_baseline_without_a_fuzzy_anchor_says_so(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5}},
                       {"get_vmu_directory": 5},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")
        self.assertIn("no fuzzy anchor", out[0][2])

    def test_fuzzy_arbiter_never_rescues_a_byte_exact_function(self):
        out = arbitrate_regressions(
            [("f", "REGRESSION", "real 0 -> 4")], "game/sys/memcard",
            {"f": {"genuine": 5, "fuzzy": 100.0}},
            genuine_fn=lambda unit, names: {"f": 5},
            ops_fn=no_ops, fuzzy_fn=lambda unit, names: {"f": 100.0},
            arbiter="fuzzy")
        self.assertEqual(out[0][1], "REGRESSION")

    def test_genuine_FELL_conflict_also_reports_the_measured_fuzzy(self):
        out = self.arb({"get_vmu_directory": {"genuine": 5, "fuzzy": 90.04}},
                       {"get_vmu_directory": 1},
                       {"get_vmu_directory": 92.72}, arbiter="fuzzy")
        self.assertEqual(out[0][1], "CONFLICT")
        self.assertIn("genuine structural rows 5 -> 1 FELL", out[0][2])
        self.assertIn("fuzzy 90.0400 -> 92.7200", out[0][2])


class ReportFuzzyReadTests(unittest.TestCase):
    REPORT = {
        "units": [
            {"name": "src/game/sys/memcard",
             "functions": [{"name": "get_vmu_directory",
                            "fuzzy_match_percent": 92.7155},
                           {"name": "saveLoad",
                            "fuzzy_match_percent": 100.0}]},
            {"name": "src/game/ui/select",
             "functions": [{"name": "serve_blits",
                            "fuzzy_match_percent": 50.0}]},
        ]
    }

    def _report(self, tmp):
        path = Path(tmp) / "report.json"
        path.write_text(json.dumps(self.REPORT), encoding="utf-8")
        return path

    def test_reads_the_named_units_functions(self):
        with tempfile.TemporaryDirectory() as tmp:
            got = read_report_fuzzy("game/sys/memcard", self._report(tmp))
            self.assertEqual(got, {"get_vmu_directory": 92.7155,
                                   "saveLoad": 100.0})

    def test_unknown_unit_reads_empty_not_wrong(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(
                read_report_fuzzy("game/nope/nope", self._report(tmp)), {})

    def test_missing_report_is_not_fatal(self):
        self.assertEqual(
            read_report_fuzzy("game/sys/memcard", Path("no/such.json")), {})


class NamingDriftSoundnessTests(unittest.TestCase):
    """run-31 item 12, per claim.law.HV_defake-gate-naming-drift-is-a-false-
    benign-on-a-wrong-callee.20260901.v1.

    In an UNLINKED object a REL24 `bl` word carries no target — the callee
    lives entirely in the relocation symbol. So "instruction words
    unchanged" is trivially true for ANY callee substitution, and the gate
    called a genuine wrong-callee bug (gamemain fn_80054E78 +0x28c binding
    fn_8009FCA8 where the target binds DoAudioTallySFX, two distinct
    functions at 0x8009FCA8 and 0x8009FB84) a benign NAMING-DRIFT.

    Sound rule: benign only if both names resolve to the SAME ADDRESS.
    """

    ADDRESSES = {
        "fn_8009FCA8": 0x8009FCA8,
        "DoAudioTallySFX": 0x8009FB84,
        "get_attn_pos": 0x8002C9A8,
        "get_attn_pos_8002C9A8": 0x8002C9A8,
    }

    def resolve(self, symbol):
        return self.ADDRESSES.get(symbol)

    def benign(self, base, cur):
        return naming_drift_is_benign(base, cur, resolve=self.resolve)

    def test_the_measured_wrong_callee_is_NOT_benign(self):
        ok, why = self.benign([["R_PPC_REL24", "DoAudioTallySFX"]],
                              [["R_PPC_REL24", "fn_8009FCA8"]])
        self.assertFalse(ok)
        self.assertIn("DoAudioTallySFX", why)
        self.assertIn("fn_8009FCA8", why)

    def test_the_address_suffix_convention_stays_benign(self):
        """The real class the heuristic was built for: one datum, two
        spellings, same address."""
        ok, _why = self.benign([["R_PPC_REL24", "get_attn_pos_8002C9A8"]],
                               [["R_PPC_REL24", "get_attn_pos"]])
        self.assertTrue(ok)

    def test_an_unresolvable_symbol_fails_closed(self):
        ok, why = self.benign([["R_PPC_REL24", "get_attn_pos"]],
                              [["R_PPC_REL24", "mysteryFn"]])
        self.assertFalse(ok)
        self.assertIn("does not resolve", why)

    def test_a_changed_relocation_TYPE_is_never_benign(self):
        ok, why = self.benign([["R_PPC_ADDR16_HA", "get_attn_pos"]],
                              [["R_PPC_REL24", "get_attn_pos"]])
        self.assertFalse(ok)
        self.assertIn("type", why)

    def test_a_different_relocation_COUNT_is_never_benign(self):
        ok, why = self.benign([["R_PPC_REL24", "get_attn_pos"]], [])
        self.assertFalse(ok)
        self.assertIn("count", why)

    def test_a_legacy_baseline_without_relocation_symbols_fails_closed(self):
        ok, why = self.benign(None, [["R_PPC_REL24", "get_attn_pos"]])
        self.assertFalse(ok)
        self.assertIn("re-take", why)

    def test_identical_symbols_are_benign(self):
        rows = [["R_PPC_REL24", "get_attn_pos"]]
        self.assertTrue(self.benign(rows, list(rows))[0])


class NamingDriftInCompareTests(unittest.TestCase):
    """The verdict wiring: the same two cases through compare()."""

    def rows(self, symbol):
        return {"status": "STRUCTURAL", "real": 0, "bytes": symbol,
                "words": "same-words", "relocs": [["R_PPC_REL24", symbol]]}

    def resolve(self, symbol):
        return NamingDriftSoundnessTests.ADDRESSES.get(symbol)

    def test_a_wrong_callee_is_a_REGRESSION_not_NAMING_DRIFT(self):
        verdicts = compare({"f": self.rows("DoAudioTallySFX")},
                           {"f": self.rows("fn_8009FCA8")},
                           resolve=self.resolve)
        self.assertEqual(verdicts[0][1], "REGRESSION")
        self.assertIn("relocation symbol", verdicts[0][2])

    def test_a_true_rename_is_still_NAMING_DRIFT(self):
        verdicts = compare({"f": self.rows("get_attn_pos_8002C9A8")},
                           {"f": self.rows("get_attn_pos")},
                           resolve=self.resolve)
        self.assertEqual(verdicts[0][1], "NAMING-DRIFT")


class RelocationDirectionTests(unittest.TestCase):
    """run-38 item 1, per claim.law.RS_defake-gate-wrong-callee-check-is-
    ours-vs-ours-so-a-correction-toward-the-target-reads-as-a-regression.
    20260902.v1.

    The measured case: game/enemy/critter::CritterFirePlayerCollide loaded
    the wrong .sdata2 pool constant lbl_80346470 (0.0) where the target
    relocates lbl_803464E8 (0.5), at reloc index 9 of 24. Repairing it and
    breaking it produced the SAME `GATE FAILED ... revert or fix` verdict,
    because the check compared our object against our own earlier baseline
    and never read the target's relocation at that instruction.
    """

    ADDRESSES = {
        "lbl_80346470": 0x80346470,
        "lbl_803464E8": 0x803464E8,
        "DoAudioTallySFX": 0x8009FB84,
        "fn_8009FCA8": 0x8009FCA8,
    }
    WRONG, RIGHT = "lbl_80346470", "lbl_803464E8"

    def resolve(self, symbol):
        return self.ADDRESSES.get(symbol)

    def rows(self, symbol):
        """The critter shape: the changed reloc at index 1 of 3."""
        return [["R_PPC_EMB_SDA21", "gCritterFlags"],
                ["R_PPC_EMB_SDA21", symbol],
                ["R_PPC_REL24", "DoAudioTallySFX"]]

    def direction(self, base_sym, cur_sym, target_sym):
        return relocation_change_direction(
            self.rows(base_sym), self.rows(cur_sym), self.rows(target_sym),
            resolve=self.resolve)

    def test_a_repair_toward_the_target_is_TOWARD(self):
        direction, why = self.direction(self.WRONG, self.RIGHT, self.RIGHT)
        self.assertEqual(direction, "toward")
        self.assertIn("reloc[1]", why)

    def test_a_drift_away_from_the_target_is_AWAY(self):
        direction, why = self.direction(self.RIGHT, self.WRONG, self.RIGHT)
        self.assertEqual(direction, "away")
        self.assertIn("reloc[1]", why)

    def test_neither_symbol_matching_the_target_is_UNKNOWN(self):
        direction, _why = self.direction(
            self.WRONG, self.RIGHT, "DoAudioTallySFX")
        self.assertEqual(direction, "unknown")

    def test_no_target_relocations_stays_UNKNOWN_and_fails_closed(self):
        direction, why = relocation_change_direction(
            self.rows(self.WRONG), self.rows(self.RIGHT), None,
            resolve=self.resolve)
        self.assertEqual(direction, "unknown")
        self.assertIn("no target relocation list", why)

    def test_AWAY_dominates_a_mixed_change(self):
        """One repaired relocation never launders a broken sibling."""
        base = [["R_PPC_EMB_SDA21", self.WRONG],
                ["R_PPC_REL24", "DoAudioTallySFX"]]
        cur = [["R_PPC_EMB_SDA21", self.RIGHT],
               ["R_PPC_REL24", "fn_8009FCA8"]]
        target = [["R_PPC_EMB_SDA21", self.RIGHT],
                  ["R_PPC_REL24", "DoAudioTallySFX"]]
        direction, _why = relocation_change_direction(
            base, cur, target, resolve=self.resolve)
        self.assertEqual(direction, "away")

    def test_unaligned_lists_fall_back_to_target_address_counts(self):
        base = [["R_PPC_EMB_SDA21", self.WRONG]]
        cur = [["R_PPC_EMB_SDA21", self.RIGHT]]
        target = [["R_PPC_EMB_SDA21", self.RIGHT],
                  ["R_PPC_REL24", "DoAudioTallySFX"]]
        direction, why = relocation_change_direction(
            base, cur, target, resolve=self.resolve)
        self.assertEqual(direction, "toward")
        self.assertIn("do not line up", why)


class RelocationDirectionInCompareTests(unittest.TestCase):
    """The verdict wiring: the two directions through compare()."""

    def resolve(self, symbol):
        return RelocationDirectionTests.ADDRESSES.get(symbol)

    def entry(self, symbol):
        return {"status": "STRUCTURAL", "real": 0, "bytes": symbol,
                "words": "identical-words",
                "relocs": [["R_PPC_EMB_SDA21", symbol]]}

    def verdict(self, base_sym, cur_sym, target_sym=None):
        target = ({"f": [["R_PPC_EMB_SDA21", target_sym]]}
                  if target_sym else None)
        rows = compare({"f": self.entry(base_sym)},
                       {"f": self.entry(cur_sym)},
                       resolve=self.resolve, target_relocs=target)
        return rows[0][1], rows[0][2]

    def test_the_measured_repair_now_PASSES_as_RELOC_TOWARD_TARGET(self):
        verdict, detail = self.verdict(
            RelocationDirectionTests.WRONG, RelocationDirectionTests.RIGHT,
            RelocationDirectionTests.RIGHT)
        self.assertEqual(verdict, "RELOC-TOWARD-TARGET")
        self.assertIn("MOVED-TOWARD-TARGET", detail)

    def test_the_same_change_reversed_is_still_a_REGRESSION(self):
        verdict, detail = self.verdict(
            RelocationDirectionTests.RIGHT, RelocationDirectionTests.WRONG,
            RelocationDirectionTests.RIGHT)
        self.assertEqual(verdict, "REGRESSION")
        self.assertIn("MOVED-AWAY-FROM-TARGET", detail)

    def test_without_a_target_object_it_fails_closed_as_before(self):
        verdict, detail = self.verdict(
            RelocationDirectionTests.WRONG, RelocationDirectionTests.RIGHT)
        self.assertEqual(verdict, "REGRESSION")
        self.assertIn("DIRECTION-UNKNOWN", detail)

    def test_a_true_rename_never_reaches_the_direction_check(self):
        """Same address, two spellings: still the benign NAMING-DRIFT."""
        rows = compare(
            {"f": self.entry("get_attn_pos_8002C9A8")},
            {"f": self.entry("get_attn_pos")},
            resolve=NamingDriftSoundnessTests.ADDRESSES.get,
            target_relocs={"f": [["R_PPC_EMB_SDA21", "get_attn_pos"]]})
        self.assertEqual(rows[0][1], "NAMING-DRIFT")


class RosterModeTests(unittest.TestCase):
    """run-38 item 7: every number a per-function sweep needs was already
    computed to take a gate baseline and reachable no other way, so a lane
    ran fndiff once per function instead (UC: 15 subprocess calls for one
    mandated sweep)."""

    CLEAN = """\
== CritterCollideEnemies: EXACT, 0 real diff lines
== CritterDamage: STRUCTURAL, 505 real diff lines [artifact-filtered; raw 9]
== ProcessCritter: MATCH (pool-name noise only), 0 real diff lines
== CritterDoTexmodNode: MATCH-MODULO-RELOC-NAMING, 4 real diff lines
"""

    def test_parse_clean_reads_one_row_per_function(self):
        rows = parse_clean(self.CLEAN)
        self.assertEqual(rows["CritterCollideEnemies"], ("EXACT", 0))
        self.assertEqual(rows["CritterDamage"], ("STRUCTURAL", 505))

    def test_parse_clean_keeps_a_parenthesised_status_whole(self):
        self.assertEqual(parse_clean(self.CLEAN)["ProcessCritter"],
                         ("MATCH (pool-name noise only)", 0))

    def snap(self):
        return {
            "__sections__": {"data": {}},
            "CritterDamage": {"status": "STRUCTURAL", "ti": 604, "bi": 604,
                              "real": 476, "genuine": 37, "fuzzy": 92.54},
            "CritterCollideEnemies": {"status": "EXACT", "ti": None,
                                      "bi": None, "real": 0},
            "ProcessCritter": {"status": "RELOCATION_ONLY", "ti": 420,
                               "bi": 420, "real": 0, "fuzzy": 100.0},
        }

    def test_the_reserved_sections_key_is_never_a_roster_row(self):
        names = [row[0] for row in
                 roster_rows(self.snap(), parse_clean(self.CLEAN), set())]
        self.assertNotIn("__sections__", names)

    def test_rows_sort_by_real_descending(self):
        names = [row[0] for row in
                 roster_rows(self.snap(), parse_clean(self.CLEAN), set())]
        self.assertEqual(names[0], "CritterDamage")

    def test_a_pinned_row_sorts_LAST_whatever_its_real(self):
        """Its real 0 is a construction artifact, not closed work."""
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN),
                           {"CritterDamage"})
        self.assertEqual(rows[-1][0], "CritterDamage")
        self.assertTrue(rows[-1][8])

    def test_a_baseline_supplies_the_real_delta(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set(),
                           baseline={"CritterDamage": {"real": 500}})
        by_name = {row[0]: row for row in rows}
        self.assertEqual(by_name["CritterDamage"][9], "500->476")

    def test_an_unchanged_real_shows_no_delta(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set(),
                           baseline={"CritterDamage": {"real": 476}})
        self.assertIsNone({r[0]: r for r in rows}["CritterDamage"][9])

    def test_the_header_counts_pinned_rows_separately_from_exact(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN),
                           {"ProcessCritter"})
        out = format_roster("game/enemy/critter", rows, has_baseline=True)
        self.assertIn("1 at real 0", out)
        self.assertIn("1 with a residual", out)
        self.assertIn("1 WebFrank-PINNED", out)

    def test_a_missing_baseline_says_so_instead_of_showing_no_delta(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set())
        out = format_roster("game/x/y", rows, has_baseline=False)
        self.assertIn("no gate baseline", out)

    def test_an_exact_row_prints_a_dash_not_None_over_None(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set())
        out = format_roster("game/x/y", rows, has_baseline=True)
        self.assertNotIn("None/None", out)

    # --- run-42 item 7: the SLOT column -------------------------------
    # Both of run-41 CL's slot wins on game/mb/mb_camera were visible in
    # probe's BASELINE banner ("frame size target 40 vs ours 48; slots
    # differ (4t/4o exclusive)") before any source was read, and the roster
    # carried nothing about it — so a lane ranking a TU on `real` pointed a
    # frame residual at the metric that fights it
    # (claim.law.real-can-underweight-a-large-alignment-gain-so-arbitrate-
    # conflicts-on-fuzzy.20260831.v1).
    #
    # CALIBRATED over every unit with both objects on disk: 256 units,
    # 3032 functions, 353 open rows (real > 0, unpinned). The column flags
    # 58 of them — 16.4% — as 17 save-set deltas, 15 frame deltas and 26
    # exclusive-slot-only rows, and flags ZERO of the closed rows, so it
    # cannot manufacture noise on finished work.

    def test_the_slot_verdict_reaches_the_row_and_the_table(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set(),
                           slots={"CritterDamage": "frame 40/48,4T/4O"})
        self.assertEqual("frame 40/48,4T/4O",
                         {r[0]: r for r in rows}["CritterDamage"][10])
        out = format_roster("game/enemy/critter", rows, has_baseline=True)
        self.assertIn("frame 40/48,4T/4O", out)
        self.assertIn("SLOT", out)

    def test_a_row_with_no_slot_signal_reads_as_a_dash(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN), set())
        self.assertEqual("-", {r[0]: r for r in rows}["CritterDamage"][10])

    def test_the_footer_fires_only_on_OPEN_slot_rows(self):
        """A closed row's slot shape is not open work, and the column must
        not promise any."""
        closed = roster_rows(self.snap(), parse_clean(self.CLEAN), set(),
                             slots={"ProcessCritter": "frame 40/48"})
        self.assertNotIn("SLOT COLUMN:",
                         format_roster("u", closed, has_baseline=True))
        opened = roster_rows(self.snap(), parse_clean(self.CLEAN), set(),
                             slots={"CritterDamage": "frame 40/48"})
        out = format_roster("u", opened, has_baseline=True)
        self.assertIn("SLOT COLUMN: 1 open row(s)", out)
        self.assertIn("NOT ON `real`", out)

    def test_a_pinned_slot_row_does_not_count_as_open_work(self):
        rows = roster_rows(self.snap(), parse_clean(self.CLEAN),
                           {"CritterDamage"},
                           slots={"CritterDamage": "frame 40/48"})
        self.assertNotIn("SLOT COLUMN:",
                         format_roster("u", rows, has_baseline=True))


class BaselineFormatTests(unittest.TestCase):
    def test_reads_the_pre_run29_bare_dict(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "b.json"
            path.write_text(json.dumps({"f": {"real": 3}}), encoding="utf-8")
            functions, meta = load_baseline(path)
            self.assertEqual(functions, {"f": {"real": 3}})
            self.assertEqual(meta, {})

    def test_reads_the_commit_anchored_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "b.json"
            path.write_text(json.dumps({
                "meta": {"head": "abc123", "unit": "game/x/y"},
                "functions": {"f": {"real": 3, "genuine": 1}}}),
                encoding="utf-8")
            functions, meta = load_baseline(path)
            self.assertEqual(functions["f"]["genuine"], 1)
            self.assertEqual(meta["head"], "abc123")


class CompareRegressionTests(unittest.TestCase):
    """The pre-existing gate rules must be unaffected by the new field."""

    def test_byte_exact_demotion_is_still_a_regression(self):
        base = {"f": {"status": "EXACT", "real": 0}}
        cur = {"f": {"status": "RELOCATION_ONLY", "real": 0}}
        self.assertEqual(compare(base, cur)[0][1], "REGRESSION")

    def test_real_growth_is_still_a_regression(self):
        base = {"f": {"status": "STRUCTURAL", "real": 4}}
        cur = {"f": {"status": "STRUCTURAL", "real": 9}}
        self.assertEqual(compare(base, cur)[0][1], "REGRESSION")

    def test_real_fall_is_still_an_improvement(self):
        base = {"f": {"status": "STRUCTURAL", "real": 9}}
        cur = {"f": {"status": "STRUCTURAL", "real": 4}}
        self.assertEqual(compare(base, cur)[0][1], "IMPROVED")


class DataSectionTests(unittest.TestCase):
    """Run 34 item 1: a moved non-text section is its own verdict class.

    A frame-widening keep improved every .text arbiter (real, --ops, the
    multiset, fuzzy) while destroying a 208-byte .extab match no per-function
    verdict could see (claim.law.WS_frame-widening-silently-breaks-the-tus-
    extab-match). The per-TU DATA baseline banks the object's non-text
    sections so `compare` can surface exactly that.
    """

    DUMP = (
        "build/x.o:     file format elf32-powerpc\n"
        "Contents of section .text:\n"
        " 0000 3c608000 60630001                    <`..`c..\n"
        "Contents of section .extab:\n"
        " 0000 00000001 00000000                    ........\n"
        "Contents of section .rodata:\n"
        " 0000 40490fdb                             @I..\n"
    )

    def test_text_sections_are_excluded(self):
        digests = parse_section_digests(self.DUMP)
        self.assertNotIn(".text", digests)
        self.assertIn(".extab", digests)
        self.assertIn(".rodata", digests)

    def test_a_moved_extab_is_its_own_verdict_class(self):
        base = {"__sections__": {"data": {".extab": "aaa", ".rodata": "eee"}}}
        cur = {"__sections__": {"data": {".extab": "bbb", ".rodata": "eee"}}}
        verdicts = compare(base, cur)
        self.assertEqual(len(verdicts), 1)
        name, verdict, detail = verdicts[0]
        self.assertEqual(verdict, "DATA-CHANGED")
        self.assertIn(".extab", detail)
        self.assertNotIn(".rodata", detail)

    def test_an_exception_table_move_is_called_out(self):
        base = {"__sections__": {"data": {".extab": "aaa"}}}
        cur = {"__sections__": {"data": {".extab": "bbb"}}}
        self.assertIn("exception", compare(base, cur)[0][2].lower())

    def test_a_plain_pool_move_is_not_called_an_exception_table(self):
        rows = data_section_verdicts({"data": {".sdata2": "1"}},
                                     {"data": {".sdata2": "2"}})
        self.assertIn(".sdata2", rows[0][2])
        self.assertNotIn("exception", rows[0][2].lower())

    def test_flat_sections_produce_no_row(self):
        same = {"__sections__": {"data": {".extab": "aaa"}}}
        self.assertEqual(
            [v for v in compare(same, dict(same)) if v[1] == "DATA-CHANGED"],
            [])

    def test_a_missing_side_never_manufactures_a_row(self):
        cur = {"__sections__": {"data": {".extab": "bbb"}}}
        self.assertEqual(data_section_verdicts(None, cur), [])
        self.assertEqual(data_section_verdicts({"data": {".extab": "a"}},
                                               None), [])

    def test_the_reserved_key_never_reads_as_a_function(self):
        """A baseline with the reserved key and one real function must not
        emit a spurious NEW/vanished row for __sections__."""
        base = {"__sections__": {"data": {".extab": "a"}},
                "f": {"status": "STRUCTURAL", "real": 4}}
        cur = {"__sections__": {"data": {".extab": "a"}},
               "f": {"status": "STRUCTURAL", "real": 4}}
        names = {v[0] for v in compare(base, cur)}
        self.assertNotIn("__sections__", names)


class ArbitrationLogTests(unittest.TestCase):
    """Run-32 item 2: an override rate nothing records is unfalsifiable.

    Measured before implementing: `grep -c jsonl tools/gdl/defake_gate.py`
    was 0 and every `log|append|audit` hit in the file was
    `verdicts.append(...)`. The tool printed "record the arbitration in the
    attempt record" and then persisted nothing, so the project's own claims
    about how often it overrides its gate could not be checked against
    anything on disk. The single `.prev.json` archive is not a history —
    the next bank overwrites it.
    """

    ROWS = [("get_vmu_directory", "CONFLICT",
             "real 48 -> 65 BUT genuine structural rows FLAT 3 -> 3 while"
             " fuzzy 90.0400 -> 92.7200 (+2.6800) ROSE")]

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.log = Path(self.tmp.name) / "arbitrations.jsonl"
        self.addCleanup(self.tmp.cleanup)

    def write(self, *actions):
        for action in actions:
            log_arbitration(
                arbitration_event("game/sys/memcard.c", action, self.ROWS),
                path=self.log)

    def test_an_event_keeps_the_evidence_not_just_a_counter(self):
        event = arbitration_event("game/sys/memcard.c", "accepted", self.ROWS,
                                  arbiter="fuzzy", reanchored=True,
                                  head="abc123", at="2026-09-02T00:00:00Z")
        # Keyed extensionless, so `arbitrations game/sys/memcard` and
        # `... game/sys/memcard.c` scope to the same rows.
        self.assertEqual(event["unit"], "game/sys/memcard")
        self.assertEqual(event["arbiter"], "fuzzy")
        self.assertTrue(event["reanchored"])
        self.assertIn("fuzzy 90.0400 -> 92.7200",
                      event["functions"][0]["detail"])

    def test_appends_one_line_per_event(self):
        self.write("refused", "accepted")
        self.assertEqual(
            len(self.log.read_text(encoding="utf-8").strip().splitlines()), 2)
        self.assertEqual([e["action"] for e in read_arbitrations(self.log)],
                         ["refused", "accepted"])

    def test_a_malformed_line_does_not_poison_the_log(self):
        self.write("accepted")
        with self.log.open("a", encoding="utf-8") as handle:
            handle.write("{not json\n\n")
        self.write("refused")
        self.assertEqual(len(read_arbitrations(self.log)), 2)

    def test_a_missing_log_reads_as_empty_not_an_error(self):
        self.assertEqual(read_arbitrations(Path(self.tmp.name) / "nope"), [])

    def test_the_override_rate_needs_both_halves(self):
        self.write("accepted", "accepted", "refused", "refused")
        summary = summarize_arbitrations(read_arbitrations(self.log))
        self.assertEqual(summary["decided"], 4)
        self.assertAlmostEqual(summary["rate"], 0.5)

    def test_banked_rows_are_counted_apart_from_the_rate(self):
        """--bank-arbitrated re-anchors without a CONFLICT being raised in
        the same call, so it must not distort the accepted/raised ratio."""
        self.write("accepted", "refused", "banked", "banked")
        summary = summarize_arbitrations(read_arbitrations(self.log))
        self.assertEqual(summary["banked"], 2)
        self.assertEqual(summary["decided"], 2)
        self.assertAlmostEqual(summary["rate"], 0.5)

    def test_an_empty_log_reports_an_undefined_rate_not_zero(self):
        summary = summarize_arbitrations([])
        self.assertIsNone(summary["rate"])
        self.assertIn("UNDEFINED", format_arbitrations(summary))

    def test_the_summary_can_be_scoped_to_one_unit(self):
        self.write("accepted")
        log_arbitration(arbitration_event("game/ui/select.c", "refused",
                                          self.ROWS), path=self.log)
        events = read_arbitrations(self.log)
        self.assertEqual(
            summarize_arbitrations(events, "game/sys/memcard")["events"], 1)
        self.assertEqual(summarize_arbitrations(events)["events"], 2)

    def test_unit_spellings_normalize_so_scoping_actually_matches(self):
        log_arbitration(arbitration_event("src/game/sys/memcard.c", "accepted",
                                          self.ROWS), path=self.log)
        events = read_arbitrations(self.log)
        self.assertEqual(
            summarize_arbitrations(events, "game/sys/memcard.c")["events"], 1)

    def test_an_unwritable_log_reports_but_does_not_raise(self):
        """Losing an audit line must never turn a passing gate into a
        failing one."""
        blocked = Path(self.tmp.name) / "file.txt"
        blocked.write_text("not a directory", encoding="utf-8")
        self.assertFalse(log_arbitration(
            arbitration_event("game/x/y.c", "accepted", self.ROWS),
            path=blocked / "arbitrations.jsonl"))

    def test_the_rate_is_reported_as_a_percentage_of_raised_conflicts(self):
        self.write("accepted", "refused", "refused", "refused")
        text = format_arbitrations(
            summarize_arbitrations(read_arbitrations(self.log)))
        self.assertIn("25.0%", text)
        self.assertIn("1/4", text)


if __name__ == "__main__":
    unittest.main()
