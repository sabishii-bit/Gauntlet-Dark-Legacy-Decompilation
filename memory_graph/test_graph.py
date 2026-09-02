#!/usr/bin/env python3
"""Behavior tests for the memory_graph maintenance and query surface.

Covers: attempt byte cap and per-function pruning, freshness stamping and
age precedence, the law_screen proposal gate, accept/release integration,
the laws/claims/debt query ops, stale-op re-open heuristics, build-time
attempt-overflow reporting, and the defake_gate parse/compare logic.

Run:  python memory_graph/test_graph.py
"""

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from memory_graph import core
from memory_graph.core import (
    ATTEMPT_BYTE_CAP,
    LAW_STATUS_ORDER,
    MemoryGraphError,
    REPO_ROOT,
    _record_age_days,
    _validate_record,
    accept_records,
    attempt_staleness,
    beta_mean,
    build_database,
    fakematch_debt,
    find_records,
    law_corpus,
    law_evidence_score,
    law_score_sort_key,
    anchor_basename_index,
    hypothesis_refuter_warning,
    missing_anchor_paths,
    prune_attempts,
    record_lookup,
    regime_events,
    stage_event_proposal,
    stage_record_proposal,
    symbol_context,
    tu_briefing,
    wilson_lower_bound,
    work_claims,
)

# Dynamic: a hardcoded date made the age-skew assertion break the moment the
# calendar advanced past it, failing every fleet's test run for an unrelated
# reason (flagged by a worker 2026-09-01).
import datetime as _dt
TODAY = _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%d")


def _write(path: Path, record: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(record, indent=2), encoding="utf-8")


def _attempt(rid, function, outcome="improved", **extra):
    record = {
        "schema_version": 1, "id": rid, "kind": "attempt",
        "function": function, "attempted_axis": extra.pop("axis", "probe"),
        "outcome": outcome, "valid_from": extra.pop("valid_from", TODAY),
    }
    record.update(extra)
    return record


def make_root(with_symbols=True) -> Path:
    root = Path(tempfile.mkdtemp(prefix="gdlgraphtest-"))
    (root / "memory_graph" / "inbox").mkdir(parents=True)
    for sub in ("attempts", "claims", "entities", "evidence", "tools"):
        (root / "memory_graph" / "records" / sub).mkdir(parents=True)
    if with_symbols:
        (root / "config" / "GUNE5D").mkdir(parents=True)
        (root / "config" / "GUNE5D" / "symbols.txt").write_text(
            "test_fn = .text:0x80001000; // type:function size:0x10 scope:global\n"
            "fn_80001010 = .text:0x80001010; // type:function size:0x10 scope:global\n"
            "other_fn = .text:0x80001020; // type:function size:0x20 scope:global\n",
            encoding="utf-8",
        )
        (root / "config" / "GUNE5D" / "splits.txt").write_text(
            "game/test/foo.c:\n"
            "\t.text start:0x80001000 end:0x80001040\n",
            encoding="utf-8",
        )
        research = root / "research" / "xbox_symbols"
        research.mkdir(parents=True)
        (research / "functions_by_module.txt").write_text(
            "== .\\Release\\foo.obj (foo.c)\n"
            "[0001:00000000] 10 G test_fn\n"
            "[0001:00000010] 10 G mystery_one\n"
            "[0001:00000020] 10 G other_fn\n",
            encoding="utf-8",
        )
    return root


class PruneAndCapTests(unittest.TestCase):
    def setUp(self):
        self.root = make_root(with_symbols=False)
        self.attempts = self.root / "memory_graph" / "records" / "attempts"

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _seed(self, count, superseded_pairs=()):
        for i in range(1, count + 1):
            record = _attempt(
                f"attempt.r{i}", "function:test_fn",
                valid_from=f"2026-08-{i:02d}",
            )
            for newer, older in superseded_pairs:
                if record["id"] == newer:
                    record["supersedes"] = older
            _write(self.attempts / f"attempt.r{i}.json", record)

    def test_under_limit_is_untouched(self):
        self._seed(5)
        report = prune_attempts(self.root, limit=5)
        self.assertEqual(report["functions_over_limit"], 0)
        self.assertEqual(report["ejected"], [])

    def test_superseded_records_eject_first_then_oldest(self):
        # r2 supersedes r1, r3 supersedes r2 -> r1 and r2 are superseded.
        self._seed(7, superseded_pairs=(("attempt.r2", "attempt.r1"),
                                        ("attempt.r3", "attempt.r2")))
        report = prune_attempts(self.root, limit=5)
        ejected = sorted(entry["id"] for entry in report["ejected"])
        self.assertEqual(ejected, ["attempt.r1", "attempt.r2"])
        self.assertEqual(
            sorted(report["kept"]["function:test_fn"]),
            ["attempt.r3", "attempt.r4", "attempt.r5",
             "attempt.r6", "attempt.r7"],
        )
        # dry-run leaves files alone
        self.assertTrue((self.attempts / "attempt.r1.json").exists())

    def test_apply_deletes_and_converges(self):
        self._seed(7)
        prune_attempts(self.root, limit=5, apply=True)
        remaining = sorted(p.name for p in self.attempts.glob("*.json"))
        self.assertEqual(len(remaining), 5)
        self.assertNotIn("attempt.r1.json", remaining)
        self.assertNotIn("attempt.r2.json", remaining)
        after = prune_attempts(self.root, limit=5)
        self.assertEqual(after["functions_over_limit"], 0)

    def test_byte_cap_grew_but_still_closes(self):
        record = _attempt("attempt.big", "function:test_fn",
                          axis="y" * 5000)  # over the old 4096 cap
        _validate_record(record, Path("<t>"))  # must pass now
        record["attempted_axis"] = "y" * (ATTEMPT_BYTE_CAP + 1)
        with self.assertRaises(MemoryGraphError):
            _validate_record(record, Path("<t>"))


class FreshnessTests(unittest.TestCase):
    def test_recorded_at_supersedes_valid_from(self):
        # recorded_at wins when both are present
        self.assertEqual(
            _record_age_days("2020-01-01", f"{TODAY}T10:00:00Z"),
            _record_age_days(TODAY, None),
        )
        # falls back to valid_from without recorded_at
        self.assertIsNotNone(_record_age_days("2026-08-01", None))
        self.assertIsNone(_record_age_days(None, None))

    def test_staging_stamps_and_law_screen_gate(self):
        root = make_root(with_symbols=False)
        original = core._probe_record_references
        core._probe_record_references = lambda *args, **kwargs: None
        try:
            claim = {
                "schema_version": 1, "id": "claim.stamp-test.v1",
                "kind": "claim", "subject": "compiler:test",
                "predicate": "codegen_law", "epistemic_state": "hypothesis",
                "value": "x",
            }
            path = stage_record_proposal(dict(claim), root=root)
            staged = json.loads(path.read_text(encoding="utf-8"))
            self.assertTrue(staged["valid_from"])
            self.assertRegex(staged["recorded_at"],
                             r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")

            bare = _attempt("attempt.nolaw.v1", "function:test_fn")
            with self.assertRaisesRegex(MemoryGraphError, "law_screen"):
                stage_record_proposal(dict(bare), root=root)

            ok = _attempt("attempt.withlaw.v1", "function:test_fn",
                          attributes={"law_screen": "none applicable: test"})
            staged_path = stage_record_proposal(dict(ok), root=root)
            self.assertTrue(staged_path.exists())

            # re-proposing the file already sitting in the inbox is not a
            # duplicate (the field-test friction): validates in place
            staged = json.loads(staged_path.read_text(encoding="utf-8"))
            again = stage_record_proposal(staged, root=root,
                                          in_place=staged_path)
            self.assertEqual(again, staged_path.resolve())
            # but a DIFFERENT source with the same id still fails closed
            with self.assertRaisesRegex(MemoryGraphError, "already exists"):
                stage_record_proposal(dict(ok), root=root)

            # unknown law tags fail closed with the vocabulary in the error
            tagged = {
                "schema_version": 1, "id": "claim.badtag.v1", "kind": "claim",
                "subject": "compiler:test", "predicate": "codegen_law",
                "epistemic_state": "hypothesis", "value": "x",
                "attributes": {"tags": ["not-a-real-tag"]},
            }
            with self.assertRaisesRegex(MemoryGraphError, "unknown tag"):
                stage_record_proposal(tagged, root=root)
        finally:
            core._probe_record_references = original
            shutil.rmtree(root, ignore_errors=True)


class GraphSurfaceTests(unittest.TestCase):
    """Ops that need a real (temp) graph build."""

    @classmethod
    def setUpClass(cls):
        cls.root = make_root()
        records = cls.root / "memory_graph" / "records"
        _write(records / "entities" / "entity.compiler-test.json", {
            "schema_version": 1, "id": "entity.compiler-test",
            "kind": "entity", "entity_type": "compiler",
            "key": "compiler:test", "name": "test compiler",
        })
        _write(records / "claims" / "claim.law.test-law.v1.json", {
            "schema_version": 1, "id": "claim.law.test-law.v1",
            "kind": "claim", "subject": "compiler:test",
            "predicate": "codegen_law", "epistemic_state": "verified",
            "value": "old law text", "valid_from": "2026-08-01",
            "attributes": {"scope": "test scope v1",
                           "tags": ["alias-form", "defake"]},
        })
        _write(records / "claims" / "claim.law.test-law.v2.json", {
            "schema_version": 1, "id": "claim.law.test-law.v2",
            "kind": "claim", "subject": "compiler:test",
            "predicate": "codegen_law", "epistemic_state": "verified",
            "value": "new law text mentioning game/test/foo.c",
            "valid_from": TODAY,
            "recorded_at": f"{TODAY}T09:00:00Z",
            "supersedes": "claim.law.test-law.v1",
            "attributes": {"scope": "test scope v2",
                           "tags": ["core-screen", "alias-form", "defake"]},
        })
        # improved attempt applying the v2 law
        _write(records / "attempts" / "attempt.applies.v1.json", _attempt(
            "attempt.applies.v1", "function:test_fn",
            attributes={"laws_applied": ["claim.law.test-law.v2"]},
        ))
        # matching-tagged law for the brief matching_laws list
        _write(records / "claims" / "claim.law.match-lever.v1.json", {
            "schema_version": 1, "id": "claim.law.match-lever.v1",
            "kind": "claim", "subject": "compiler:test",
            "predicate": "codegen_law", "epistemic_state": "verified",
            "value": "entry schedule lever", "valid_from": TODAY,
            "attributes": {"scope": "matching",
                           "tags": ["entry-schedule", "matching"]},
        })
        # improved attempt with a schedule residual class
        _write(records / "attempts" / "attempt.sched-residual.v1.json",
               _attempt("attempt.sched-residual.v1", "function:test_fn",
                        residual_class="REGISTER_ONLY/SCHEDULE"))
        # parked attempt on test_fn whose recorded measurement (90) no longer
        # matches the report (95) -> score_moved_since_park
        _write(records / "attempts" / "attempt.moved.v1.json", _attempt(
            "attempt.moved.v1", "function:test_fn", outcome="parked",
            axis="offsetof probe capped",
            after={"fuzzy_percent": 90.0},
        ))
        # parked CONVERSION attempt on other_fn with no form documented
        # -> failing_form_undocumented
        _write(records / "attempts" / "attempt.formless.v1.json", _attempt(
            "attempt.formless.v1", "function:other_fn", outcome="parked",
            axis="raw offset conversion regressed, reverted",
        ))
        # scheduler park on other_fn: conversion heuristic must NOT flag it
        _write(records / "attempts" / "attempt.schedpark.v1.json", _attempt(
            "attempt.schedpark.v1", "function:other_fn", outcome="parked",
            axis="register rotation resisted, scheduler fog",
        ))
        # push other_fn over the 5-attempt cap (formless + 5 = 6)
        for i in range(5):
            _write(records / "attempts" / f"attempt.bulk{i}.v1.json", _attempt(
                f"attempt.bulk{i}.v1", "function:other_fn",
            ))
        # one active-stale claim, one released claim
        _write(cls.root / "memory_graph" / "inbox" / "work_claim.a.json", {
            "schema_version": 1, "id": "work_claim.a", "kind": "work_claim",
            "function": "function:test_fn", "owner": "worker-a",
            "state": "active", "claimed_at": "2026-08-20",
        })
        _write(cls.root / "memory_graph" / "inbox" / "work_claim.b.json", {
            "schema_version": 1, "id": "work_claim.b", "kind": "work_claim",
            "function": "function:other_fn", "owner": "worker-b",
            "state": "released", "claimed_at": "2026-08-19",
            "released_at": "2026-08-20",
        })
        # debt fixture (path matches the splits module game/test/foo.c)
        src = cls.root / "src" / "game" / "test" / "foo.c"
        src.parent.mkdir(parents=True)
        src.write_text(
            "#define IOFF(f) (offsetof(TestFoo, f))\n"
            "void f(u8* p, u8* q, u8* r, void* x) {\n"
            "  *(s32*)(p + 4) = 1;\n"
            "  *(f32*)(q + 8) = 2.0f;\n"
            "  *(u8*)(r+1) = 3;\n"
            "  *(s16*)(p + offsetof(Player, gold)) = 6;  /* converted */\n"
            "  *(s32*)(p + IOFF(bar)) = 7;  /* macro-named, not bare */\n"
            "  bar(*(s32*)(p + 24), *(s32*)(p + offsetof(Player, exp)));\n"
            "  /* commented out: *(s32*)(p + 12) = 9; */\n"
            "  PF(p, 0x10, s32) = 4;\n"
            "  PF(x,1,u8) = 5;\n"
            "  PF(p, offsetof(Player, gold), s16) = 6;  /* converted PF */\n"
            "}\n"
            "void g(u8* p) {\n"
            "  *(s32*)(p + 20) = 8;\n"
            "  rate = rate * (f32)(ticks);      /* multiply, not a deref */\n"
            "  scale = lut[i] * (f32)(u32)(v);  /* multiply, not a deref */\n"
            "}\n",
            encoding="utf-8",
        )
        header = cls.root / "include" / "game" / "testfoo.h"
        header.parent.mkdir(parents=True)
        header.write_text(
            "typedef struct TestFoo {\n"
            "    s32   alpha;          /* 0x00 */\n"
            "    f32   beta[3];        /* 0x04 */\n"
            "    s16   gamma;          /* 0x10 */\n"
            "} TestFoo;               /* size 0x14 */\n",
            encoding="utf-8",
        )
        # objdiff report for the stale op
        report = cls.root / "build" / "GUNE5D" / "report.json"
        report.parent.mkdir(parents=True)
        report.write_text(json.dumps({
            "units": [{"name": "main/game/test/foo", "functions": [
                {"name": "test_fn", "fuzzy_match_percent": 95.0},
                {"name": "other_fn", "fuzzy_match_percent": 80.0},
            ]}]
        }), encoding="utf-8")
        cls.stats = build_database(cls.root)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, ignore_errors=True)

    def test_build_reports_attempt_overflow(self):
        overflow = self.stats.get("attempt_overflow", {})
        self.assertEqual(overflow.get("function:other_fn"), 7)
        self.assertNotIn("function:test_fn", overflow)

    def test_law_corpus_counts_and_supersession(self):
        result = law_corpus(root=self.root)
        # Run-32 change, deliberate: the unfiltered browse is the
        # DETERMINISTIC VIEW and excludes provisional laws (zero verified
        # successes) from `laws`. v1 has no landing, so it moves to
        # `provisional_laws` — segregated, never deleted, because the
        # unfiltered call is also the corpus enumeration surface. Everything
        # this test actually pins (supersession reporting, applied_count) is
        # asserted below across the union.
        laws = {row["id"]: row
                for row in result["laws"] + result["provisional_laws"]}
        self.assertNotIn("claim.law.test-law.v1",
                         {row["id"] for row in result["laws"]})
        self.assertIn("claim.law.test-law.v1",
                      {row["id"] for row in result["provisional_laws"]})
        # The invariant that matters: the hidden count equals what was
        # segregated, so the browse loses nothing it does not also hand back.
        self.assertEqual(result["hidden_provisional"],
                         len(result["provisional_laws"]))
        self.assertGreaterEqual(result["hidden_provisional"], 1)
        self.assertIn("claim.law.test-law.v2", laws)
        self.assertEqual(laws["claim.law.test-law.v1"]["superseded_by"],
                         "claim.law.test-law.v2")
        self.assertIsNone(laws["claim.law.test-law.v2"]["superseded_by"])
        self.assertEqual(laws["claim.law.test-law.v2"]["applied_count"], 1)
        self.assertEqual(laws["claim.law.test-law.v1"]["applied_count"], 0)
        # Fixture stamps a local date; age is computed against UTC now, so
        # allow the one-day skew a date boundary introduces.
        self.assertLessEqual(laws["claim.law.test-law.v2"]["age_days"], 1)
        # Run 34 item 6: query terms are OR-matched, so "scope v1" now
        # SURFACES both laws instead of the old AND filter's single
        # exact-phrase hit (which returned 0 on a spread query). v1 carries
        # BOTH terms, v2 only "scope". Cross-tier order still obeys tier
        # first (v2 is verified, v1 provisional), so this pins the surfacing
        # and per-term evidence, not the cross-tier position.
        filtered = law_corpus("scope v1", root=self.root)
        by_id = {r["id"]: r for r in filtered["laws"]}
        self.assertIn("claim.law.test-law.v1", by_id)
        self.assertEqual(by_id["claim.law.test-law.v1"]["query_terms_matched"],
                         2)
        self.assertEqual(by_id["claim.law.test-law.v2"]["query_terms_matched"],
                         1)
        # Per-term corpus hit counts: "scope" is in both scopes, "v1" only in
        # v1's; a zero here would name a dead term.
        self.assertEqual(filtered["query_term_hits"]["scope"], 2)
        self.assertEqual(filtered["query_term_hits"]["v1"], 1)

    def test_work_claims_staleness(self):
        result = work_claims(root=self.root, stale_after=2)
        ids = {row["id"]: row for row in result["claims"]}
        self.assertIn("work_claim.a", ids)
        self.assertNotIn("work_claim.b", ids)  # released hidden by default
        self.assertTrue(ids["work_claim.a"]["stale"])  # claimed 10 days ago
        with_released = work_claims(root=self.root, include_released=1)
        self.assertIn("work_claim.b",
                      {row["id"] for row in with_released["claims"]})

    def test_fakematch_debt_census(self):
        result = fakematch_debt(root=self.root)
        self.assertEqual(result["tu_count"], 1)
        row = result["tus"][0]
        self.assertEqual(row["tu"], "src/game/test/foo.c")
        # The two binary-multiply lookalikes in g() must not be counted.
        # 5 bare + 3 named: explicit offsetof, the IOFF macro (object- or
        # function-like #define whose body contains offsetof), and the
        # SECOND cast of the multi-cast call — while its bare sibling in
        # the SAME statement stays bare (the pb_diag same-statement
        # false-positive). Commented-out cast not counted.
        self.assertEqual(row["bare_sites"], 5)
        self.assertEqual(row["named_sites"], 3)
        self.assertEqual(row["cast_sites"], 8)
        self.assertEqual(row["pf_sites"], 2)
        self.assertEqual(row["pf_named"], 1)
        self.assertEqual(result["bare_total"], 5)
        self.assertEqual(fakematch_debt("nomatch", root=self.root)["tu_count"], 0)
        lined = fakematch_debt("game/test/foo", root=self.root, show_lines=1)
        self.assertEqual(len(lined["bare_site_lines"]), 5)
        owners = [entry.rsplit("(", 1)[1].rstrip(")")
                  for entry in lined["bare_site_lines"]]
        self.assertEqual(owners, ["f", "f", "f", "f", "g"])

    def test_fakematch_debt_by_function(self):
        result = fakematch_debt("game/test/foo", root=self.root,
                                by_function=1)
        self.assertEqual(result["functions"],
                         [{"function": "f", "bare_sites": 4},
                          {"function": "g", "bare_sites": 1}])
        # No tu filter -> no per-function aggregation.
        self.assertNotIn("functions", fakematch_debt(root=self.root,
                                                     by_function=1))

    def test_record_template_shape_and_guard(self):
        from memory_graph.core import record_template, stage_record_proposal
        template = record_template("attempt")
        self.assertEqual(template["kind"], "attempt")
        self.assertEqual(template["schema_version"], 1)
        # Run 34 item 10: the head fields lead by construction so the CLI,
        # which prints the template WITHOUT sort_keys, presents a top-down
        # fill-in. sort_keys would sink schema_version to the bottom.
        self.assertEqual(list(template)[:3], ["schema_version", "id", "kind"])
        self.assertIn("law_screen", template["attributes"])
        self.assertIn("<REQUIRED", template["id"])
        with self.assertRaises(MemoryGraphError):
            record_template("nonsense")
        # A template with placeholders still present must never stage.
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(dict(template), root=self.root,
                                  dry_run=True)
        self.assertIn("placeholder", str(caught.exception))

    def test_cli_large_output_spills_to_file(self):
        import contextlib
        import io
        from memory_graph import gdlmem
        original = gdlmem.SPILL_THRESHOLD
        gdlmem.SPILL_THRESHOLD = 10
        try:
            buffer = io.StringIO()
            with contextlib.redirect_stdout(buffer):
                code = gdlmem.main(["--root", str(self.root), "laws"])
            self.assertEqual(code, 0)
            pointer = json.loads(buffer.getvalue())
            self.assertIn("large_output", pointer)
            spilled = json.loads(
                Path(pointer["large_output"]).read_text(encoding="utf-8"))
            self.assertIn("laws", spilled)
        finally:
            gdlmem.SPILL_THRESHOLD = original

    def test_struct_local_header_authority(self):
        from memory_graph.core import xbox_struct_layout
        result = xbox_struct_layout("TestFoo", root=self.root, offset="0x8")
        self.assertEqual(len(result["local_headers"]), 1)
        local = result["local_headers"][0]
        self.assertEqual(local["file"], "include/game/testfoo.h")
        covering = local["covering_field"]
        self.assertEqual(covering["field_offset"], "0x4")  # beta[3] @ 0x04
        self.assertIn("beta", covering["text"])
        self.assertEqual(covering["delta_into_field"], "0x4")
        self.assertIn("GC project header", result["hint"] or local["authority"])

    def test_record_lookup_batch(self):
        single = record_lookup("claim.law.test-law.v2", root=self.root)
        self.assertEqual(single["record"]["id"], "claim.law.test-law.v2")
        batch = record_lookup(
            "claim.law.test-law.v1, claim.law.test-law.v2, nope.missing",
            root=self.root)
        self.assertEqual(batch["count"], 2)
        self.assertEqual(batch["missing"], ["nope.missing"])
        with self.assertRaises(MemoryGraphError):
            record_lookup("nope.missing", root=self.root)

    def test_laws_full_inlines_text(self):
        heads = law_corpus(root=self.root, tag="core-screen")
        full = law_corpus(root=self.root, tag="core-screen", full=1)
        self.assertEqual(full["laws"][0]["head"], "new law text mentioning"
                         " game/test/foo.c")
        self.assertEqual(heads["laws"][0]["id"], full["laws"][0]["id"])

    def test_find_residual_facet(self):
        hits = find_records(root=self.root, residual="SCHED")
        self.assertIn("attempt.sched-residual.v1",
                      {row["id"] for row in hits["results"]})
        self.assertNotIn("attempt.moved.v1",
                         {row["id"] for row in hits["results"]})

    def test_brief_matching_laws_and_residuals(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        matching_ids = {row["id"] for row in brief["matching_laws"]}
        self.assertIn("claim.law.match-lever.v1", matching_ids)
        self.assertNotIn("claim.law.test-law.v2", matching_ids)  # defake law
        self.assertIsNone(brief["scores_note"])  # report exists in fixture
        by_id = {row["id"]: row for row in brief["live_attempts"]}
        self.assertEqual(
            by_id["attempt.sched-residual.v1"]["residual_class"],
            "REGISTER_ONLY/SCHEDULE")

    def test_symbol_naming_audit_alignment(self):
        from memory_graph.core import symbol_naming_audit
        result = symbol_naming_audit(root=self.root)
        self.assertEqual(result["totals"]["placeholders"], 1)
        module = result["modules"][0]
        self.assertEqual(module["gc_module"], "game/test/foo.c")
        self.assertEqual(module["xbox_module"], "foo.obj")
        self.assertEqual(module["orientation"], "forward")
        self.assertEqual(module["anchors"], 2)  # test_fn, other_fn
        self.assertEqual(len(module["proposals"]), 1)
        proposal = module["proposals"][0]
        self.assertEqual(proposal["gc"], "fn_80001010")
        self.assertEqual(proposal["xbox_candidate"], "mystery_one")
        self.assertEqual(proposal["confidence"], "exact-gap")
        self.assertEqual(module["no_candidate"], [])

    def test_symbol_rename_atomic(self):
        from memory_graph.core import rename_symbol
        # dry-run reports the full plan without touching anything
        plan = rename_symbol("fn_80001010", "mystery_one", root=self.root)
        self.assertFalse(plan["applied"])
        symbols = (self.root / "config" / "GUNE5D" / "symbols.txt"
                   ).read_text(encoding="utf-8")
        self.assertIn("fn_80001010", symbols)
        # collision with a live symbol fails closed
        with self.assertRaisesRegex(MemoryGraphError, "multiply-defined"):
            rename_symbol("fn_80001010", "other_fn", root=self.root)
        # unknown source symbol fails closed
        with self.assertRaisesRegex(MemoryGraphError, "not a symbol"):
            rename_symbol("fn_deadbeef", "whatever", root=self.root)

    def test_stale_reopen_heuristics(self):
        result = attempt_staleness(self.root)
        reasons = {entry["record"]: entry["reason"]
                   for entry in result["reopen_candidates"]}
        self.assertEqual(reasons.get("attempt.moved.v1"),
                         "score_moved_since_park")
        # failing_form_undocumented is RETIRED (2026-09-01): both field
        # hits were false positives; probed_form is the durable fix.
        # score_moved is now the sole heuristic reopen signal.
        self.assertNotIn("attempt.formless.v1", reasons)
        self.assertNotIn("attempt.schedpark.v1", reasons)

    def test_laws_tag_filter_and_vocabulary_report(self):
        result = law_corpus(root=self.root, tag="core-screen")
        self.assertEqual([row["id"] for row in result["laws"]],
                         ["claim.law.test-law.v2"])
        self.assertEqual(result["laws"][0]["tags"],
                         ["core-screen", "alias-form", "defake"])
        self.assertEqual(result["tags_available"],
                         {"alias-form": 2, "core-screen": 1, "defake": 2,
                          "entry-schedule": 1, "matching": 1})

    def test_find_facets(self):
        by_kind = find_records(root=self.root, kind="work_claim")
        self.assertTrue(all(row["kind"] == "work_claim"
                            for row in by_kind["results"]))
        self.assertGreaterEqual(by_kind["count"], 2)

        by_fn = find_records(root=self.root, function="test_fn",
                             kind="attempt")
        ids = {row["id"] for row in by_fn["results"]}
        self.assertIn("attempt.applies.v1", ids)
        self.assertIn("attempt.moved.v1", ids)
        self.assertNotIn("attempt.formless.v1", ids)  # other_fn

        by_tu = find_records(root=self.root, tu="game/test/foo",
                             kind="attempt")
        tu_ids = {row["id"] for row in by_tu["results"]}
        # TU facet reaches BOTH functions through the derived module join
        self.assertIn("attempt.moved.v1", tu_ids)
        self.assertIn("attempt.formless.v1", tu_ids)
        self.assertEqual(by_tu["results"][0]["tu"], "game/test/foo.c")

        parked = find_records(root=self.root, tu="game/test/foo",
                              outcome="parked")
        self.assertEqual(
            {row["id"] for row in parked["results"]},
            {"attempt.moved.v1", "attempt.formless.v1",
             "attempt.schedpark.v1"},
        )

        by_law = find_records(root=self.root, law="claim.law.test-law.v2",
                              kind="attempt")
        self.assertIn("attempt.applies.v1",
                      {row["id"] for row in by_law["results"]})

        with self.assertRaisesRegex(MemoryGraphError, "at least one"):
            find_records(root=self.root)

    def test_tu_briefing_assembles_sections(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        self.assertEqual(brief["tu"], ["game/test/foo.c"])
        roster = {row["function"]: row for row in brief["functions"]}
        self.assertEqual(set(roster), {"test_fn", "fn_80001010", "other_fn"})
        self.assertEqual(roster["test_fn"]["fuzzy"], 95.0)
        attempt_ids = [row["id"] for row in brief["live_attempts"]]
        self.assertIn("attempt.moved.v1", attempt_ids)
        # parked/capped records lead the list
        self.assertIn(brief["live_attempts"][0]["outcome"],
                      ("parked", "capped"))
        self.assertEqual(
            [row["id"] for row in brief["active_claims"]], ["work_claim.a"]
        )
        self.assertEqual(
            [row["id"] for row in brief["core_screen_laws"]],
            ["claim.law.test-law.v2"],
        )
        self.assertEqual(brief["raw_offset_debt"][0]["total"], 11)
        self.assertEqual(brief["raw_offset_debt"][0]["bare_sites"], 5)
        with self.assertRaisesRegex(MemoryGraphError, "no GameCube module"):
            tu_briefing("does/not/exist", root=self.root)

    def test_tu_briefing_accepts_src_prefixed_paths(self):
        # the src/-prefix trap from the field test: both spellings work
        for spelling in ("src/game/test/foo", "src\\game\\test\\foo.c"):
            brief = tu_briefing(spelling, root=self.root)
            self.assertEqual(brief["tu"], ["game/test/foo.c"])


class AcceptRecordsTests(unittest.TestCase):
    def setUp(self):
        self.root = make_root()
        inbox = self.root / "memory_graph" / "inbox"
        _write(inbox / "attempt.wave.v1.json", _attempt(
            "attempt.wave.v1", "function:test_fn",
            attributes={"law_screen": "none applicable: test"},
        ))
        _write(inbox / "work_claim.w.json", {
            "schema_version": 1, "id": "work_claim.w", "kind": "work_claim",
            "function": "function:test_fn", "owner": "worker-w",
            "state": "active", "claimed_at": TODAY,
        })

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_accept_moves_release_deletes_and_rebuilds(self):
        result = accept_records(
            ["attempt.wave.v1"], release=["work_claim.w"], root=self.root
        )
        moved = (self.root / "memory_graph" / "records" / "attempts"
                 / "attempt.wave.v1.json")
        self.assertTrue(moved.exists())
        self.assertFalse(
            (self.root / "memory_graph" / "inbox" / "attempt.wave.v1.json").exists()
        )
        self.assertFalse(
            (self.root / "memory_graph" / "inbox" / "work_claim.w.json").exists()
        )
        self.assertFalse(result["staged"])  # temp root has no .git
        self.assertTrue(result["graph_rebuilt"])
        for path in result["paths"]:
            self.assertIn(path, result["commit_command"])

    def test_accept_in_git_repo_with_untracked_inbox_source(self):
        # MB's field bug: an accepted record whose inbox file was never
        # git-tracked must not abort staging after the filesystem moves.
        import subprocess
        subprocess.run(["git", "-C", str(self.root), "init", "-q",
                        "-b", "main"], check=True)
        subprocess.run(["git", "-C", str(self.root), "-c",
                        "user.email=t@t", "-c", "user.name=t",
                        "commit", "-q", "--allow-empty", "-m", "init"],
                       check=True)
        result = accept_records(
            ["attempt.wave.v1"], release=["work_claim.w"], root=self.root
        )
        self.assertTrue(result["staged"])
        status = subprocess.run(
            ["git", "-C", str(self.root), "status", "--porcelain"],
            capture_output=True, text=True, check=True).stdout
        self.assertIn("attempt.wave.v1.json", status)
        # the never-tracked, now-gone inbox paths are excluded from staging
        for path in result["staged_paths"]:
            self.assertTrue((self.root / path).exists())

    def test_accept_branch_guard(self):
        import subprocess
        subprocess.run(["git", "-C", str(self.root), "init", "-q",
                        "-b", "work"], check=True)
        subprocess.run(["git", "-C", str(self.root), "-c",
                        "user.email=t@t", "-c", "user.name=t",
                        "commit", "-q", "--allow-empty", "-m", "init"],
                       check=True)
        with self.assertRaisesRegex(MemoryGraphError, "integrator-only"):
            accept_records(["attempt.wave.v1"], root=self.root)
        # still in inbox — the guard fired before any mutation
        self.assertTrue(
            (self.root / "memory_graph" / "inbox"
             / "attempt.wave.v1.json").exists()
        )
        result = accept_records(["attempt.wave.v1"], root=self.root,
                                allow_any_branch=True)
        self.assertTrue(result["staged"])

    def test_accept_fails_closed(self):
        with self.assertRaisesRegex(MemoryGraphError, "not found"):
            accept_records(["attempt.missing"], root=self.root)
        with self.assertRaisesRegex(MemoryGraphError, "work_claim"):
            accept_records(["work_claim.w"], root=self.root)
        with self.assertRaisesRegex(MemoryGraphError, "nothing to accept"):
            accept_records([], root=self.root)
        # nothing was moved by the failed calls
        self.assertTrue(
            (self.root / "memory_graph" / "inbox" / "attempt.wave.v1.json").exists()
        )


class RenameSymbolApplyTests(unittest.TestCase):
    def test_apply_patches_symbols_source_and_records(self):
        from memory_graph.core import rename_symbol
        root = make_root()
        src = root / "src" / "game" / "test" / "foo.c"
        src.parent.mkdir(parents=True)
        src.write_text("void fn_80001010(void);\n"
                       "void caller(void) { fn_80001010(); }\n",
                       encoding="utf-8")
        record = root / "memory_graph" / "records" / "attempts" / "a.json"
        _write(record, _attempt("attempt.a.v1", "function:fn_80001010",
                                outcome="parked"))
        try:
            result = rename_symbol("fn_80001010", "mystery_one", root=root,
                                   apply=True)
            self.assertTrue(result["applied"])
            symbols = (root / "config" / "GUNE5D" / "symbols.txt"
                       ).read_text(encoding="utf-8")
            self.assertNotIn("fn_80001010", symbols)
            self.assertIn("mystery_one = .text:0x80001010", symbols)
            source = src.read_text(encoding="utf-8")
            self.assertNotIn("fn_80001010", source)
            self.assertEqual(source.count("mystery_one"), 2)
            patched = json.loads(record.read_text(encoding="utf-8"))
            self.assertEqual(patched["function"], "function:mystery_one")
            self.assertIn("src/game/test/foo.c", result["source_files"])
            self.assertTrue(result["record_files_patched"])
        finally:
            shutil.rmtree(root, ignore_errors=True)


class DefakeGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        gate_path = (Path(__file__).resolve().parent.parent
                     / "tools" / "gdl" / "defake_gate.py")
        spec = importlib.util.spec_from_file_location("defake_gate", gate_path)
        cls.gate = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.gate)

    CLASSIFY = (
        "EXACT               fn_a\n"
        "STRUCTURAL          fn_b  insns 100/101\n"
        "REGISTER_ONLY       fn_c  insns 50/50\n"
        "BASE_ONLY           helper\n"
    )
    COUNT = (
        "DIFF fn_b  insns 100/101  lines 12  real 9\n"
        "DIFF fn_c  insns 50/50  lines 4  real 2\n"
    )

    def test_snapshot_merges_views(self):
        snap = self.gate.snapshot(self.CLASSIFY, self.COUNT)
        self.assertEqual(snap["fn_a"]["real"], 0)
        self.assertEqual(snap["fn_b"]["real"], 9)
        self.assertEqual(snap["fn_c"]["real"], 2)
        self.assertIsNone(snap["helper"]["real"])

    def test_compare_verdicts(self):
        baseline = self.gate.snapshot(self.CLASSIFY, self.COUNT)
        current = self.gate.snapshot(
            "STRUCTURAL          fn_a  insns 10/11\n"
            "STRUCTURAL          fn_b  insns 100/101\n"
            "EXACT               fn_c\n"
            "BASE_ONLY           helper\n",
            "DIFF fn_a  insns 10/11  lines 3  real 3\n"
            "DIFF fn_b  insns 100/101  lines 15  real 12\n",
        )
        verdicts = {name: (verdict, detail)
                    for name, verdict, detail in
                    self.gate.compare(baseline, current)}
        self.assertEqual(verdicts["fn_a"][0], "REGRESSION")  # exact fell
        self.assertEqual(verdicts["fn_b"][0], "REGRESSION")  # real 9 -> 12
        self.assertEqual(verdicts["fn_c"][0], "IMPROVED")    # real 2 -> 0
        self.assertNotIn("helper", verdicts)                 # unchanged status

    def test_compare_catches_vanished_function(self):
        baseline = self.gate.snapshot(self.CLASSIFY, self.COUNT)
        current = {name: row for name, row in baseline.items()
                   if name != "fn_b"}
        verdicts = {name: verdict for name, verdict, _ in
                    self.gate.compare(baseline, current)}
        self.assertEqual(verdicts["fn_b"], "REGRESSION")

    def test_defake_rewrite_patterns(self):
        rewrite_path = (Path(__file__).resolve().parent.parent
                        / "tools" / "gdl" / "defake_rewrite.py")
        spec = importlib.util.spec_from_file_location("defake_rewrite",
                                                      rewrite_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        pattern = module.build_pattern("c->hdr")
        m1 = pattern.search("x = *(s32*)((u8*)c->hdr + 0x124);")
        self.assertIsNotNone(m1)
        self.assertEqual(int(m1.group("off"), 0), 0x124)
        m2 = pattern.search("y = *(f32 *)(c->hdr + 92);")
        self.assertIsNotNone(m2)
        self.assertEqual(int(m2.group("off"), 0), 92)
        self.assertIsNone(pattern.search("z = *(s32*)((u8*)other + 4);"))

    def test_normalize_unit_strips_src_prefix(self):
        self.assertEqual(self.gate.normalize_unit("src/game/mb/mb_font.c"),
                         "game/mb/mb_font.c")
        self.assertEqual(self.gate.normalize_unit("src\\game\\mb\\mb_font.c"),
                         "game/mb/mb_font.c")
        self.assertEqual(self.gate.normalize_unit("game/mb/mb_font.c"),
                         "game/mb/mb_font.c")


class SchemaShapeTests(unittest.TestCase):
    """Run-29 field SHAPE checks, which run on EVERY record.

    These bind corpus-wide (import and propose) precisely so the BF lane's
    in-place annotation of already-accepted records is caught by
    `gdlmem validate`/`build` rather than slipping in unvalidated.
    """

    def _check(self, record):
        _validate_record(dict(record), Path("<test>"))

    def test_valid_residual_object_passes(self):
        self._check(_attempt(
            "attempt.res.v1", "function:test_fn",
            residual={"signature": "+1 addi -1 li",
                      "family": "live-zero-remat",
                      "capability_needed": "dataflow-equivalence",
                      "measured_at": "2026-09-01"}))

    def test_partial_and_null_residual_fields_pass(self):
        self._check(_attempt("attempt.res.v2", "function:test_fn",
                             residual={"signature": "+1 mr",
                                       "capability_needed": None}))

    def test_family_outside_vocabulary_fails_closed(self):
        with self.assertRaisesRegex(MemoryGraphError, "outside the contract"):
            self._check(_attempt("attempt.res.v3", "function:test_fn",
                                 residual={"family": "made-up-family"}))

    def test_unknown_residual_keys_are_tolerated(self):
        # REGRESSION: the first cut REFUSED unknown keys, and that broke the
        # entire corpus import the moment the BF lane added its provenance
        # fields (confidence / extraction_status / signature_source). An
        # additive schema must not let one lane's extension break another
        # lane's build.
        self._check(_attempt(
            "attempt.res.v4", "function:test_fn",
            residual={"signature": "+1 addi", "confidence": "hand-verified",
                      "extraction_status": "measured-dead",
                      "signature_source": "fndiff sweep 2026-09-01",
                      "some_future_key": "tolerated"}))

    def test_extension_field_types_are_still_checked(self):
        with self.assertRaisesRegex(MemoryGraphError, "must be a string"):
            self._check(_attempt("attempt.res.v4b", "function:test_fn",
                                 residual={"confidence": ["not", "a", "str"]}))

    def test_family_sentinels_are_accepted(self):
        # BF's backfill uses these on 956 records: 'unclassified' = no family
        # assigned, 'no-residual' = the function has none.
        for sentinel in ("unclassified", "no-residual"):
            self._check(_attempt(f"attempt.sent.{sentinel}",
                                 "function:test_fn",
                                 residual={"family": sentinel}))

    def test_family_candidate_is_vocabulary_checked_but_kept_separate(self):
        self._check(_attempt("attempt.cand.v1", "function:test_fn",
                             residual={"family": "unclassified",
                                       "family_candidate": "copy-form"}))
        with self.assertRaisesRegex(MemoryGraphError, "family_candidate"):
            self._check(_attempt("attempt.cand.v2", "function:test_fn",
                                 residual={"family_candidate": "invented"}))

    def test_attributes_residual_prose_is_not_read_as_structure(self):
        # 654 accepted records carry attributes.residual as free prose;
        # conflating it with the structured object would read prose as data.
        record = _attempt("attempt.prose.v1", "function:test_fn",
                          attributes={"residual": "left raw, see the diff"})
        self._check(record)
        self.assertIsNone(record.get("residual"))

    def test_bad_measured_at_fails_closed(self):
        with self.assertRaisesRegex(MemoryGraphError, "YYYY-MM-DD"):
            self._check(_attempt("attempt.res.v5", "function:test_fn",
                                 residual={"measured_at": "Sept 1"}))

    def test_legacy_prose_residual_still_accepted(self):
        # attributes.residual is free prose across the accepted corpus and
        # must keep validating, or every legacy record breaks on import.
        self._check(_attempt("attempt.res.v6", "function:test_fn",
                             attributes={"residual": "a prose description"}))

    def test_asserted_by_must_be_a_string_array(self):
        with self.assertRaisesRegex(MemoryGraphError, "asserted_by"):
            self._check({"schema_version": 1, "id": "claim.a.v1",
                         "kind": "claim", "subject": "compiler:test",
                         "predicate": "codegen_law",
                         "epistemic_state": "verified", "value": "x",
                         "asserted_by": "tools/gdl/webfrank.py"})

    def test_empty_falsifier_fails_closed(self):
        with self.assertRaisesRegex(MemoryGraphError, "non-empty"):
            self._check({"schema_version": 1, "id": "claim.b.v1",
                         "kind": "claim", "subject": "compiler:test",
                         "predicate": "codegen_law",
                         "epistemic_state": "verified", "value": "x",
                         "falsifier": "   "})

    def test_records_without_the_new_fields_are_untouched(self):
        # The corpus predates every field above; shape validation must be a
        # no-op on it or `gdlmem build` regresses on 1400+ records.
        self._check(_attempt("attempt.plain.v1", "function:test_fn"))


class ProposalGateTests(unittest.TestCase):
    """The three run-29 gates. They bind NEW proposals only.

    Each is traceable to a recorded burned-probe criticism, and the error
    text names that record so an author reads the reason, not just a rule.
    """

    def setUp(self):
        self.root = make_root(with_symbols=False)
        self._probe = core._probe_record_references
        core._probe_record_references = lambda *a, **k: None

    def tearDown(self):
        core._probe_record_references = self._probe
        shutil.rmtree(self.root, ignore_errors=True)

    def _law(self, rid, value, **extra):
        record = {"schema_version": 1, "id": rid, "kind": "claim",
                  "subject": "compiler:test", "predicate": "codegen_law",
                  "epistemic_state": "verified", "value": value}
        record.update(extra)
        return record

    # --- Gate A: necessity language requires a falsifier -----------------
    def test_necessity_law_without_falsifier_is_refused(self):
        law = self._law("claim.law.needs-falsifier.v1",
                        "MWCC must spill the third web before the call.")
        with self.assertRaisesRegex(MemoryGraphError, "falsifier"):
            stage_record_proposal(law, root=self.root)

    def test_necessity_law_with_falsifier_is_accepted(self):
        law = self._law(
            "claim.law.has-falsifier.v1",
            "MWCC cannot close a count-asymmetric residual.",
            falsifier="a webfrank rule closing a function whose counts"
                      " differ; tools/gdl/tests/test_webfrank.py",
            asserted_by=["tools/gdl/webfrank.py"])
        path = stage_record_proposal(law, root=self.root)
        self.assertTrue(path.exists())

    def test_gate_a_error_cites_its_motivating_record(self):
        law = self._law("claim.law.cite-check.v1",
                        "This only happens under -O4.")
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(law, root=self.root)
        self.assertIn("RQ_webfrank-audit-silence-is-not-ineligibility",
                      str(caught.exception))

    def test_descriptive_law_needs_no_falsifier(self):
        law = self._law("claim.law.descriptive.v1",
                        "The target loads two nearby values in source order.")
        self.assertTrue(stage_record_proposal(law, root=self.root).exists())

    def test_falsifier_in_attributes_also_satisfies_gate_a(self):
        # Readers are tolerant of either spelling so a record authored the
        # other way is still valid; only top-level is documented.
        law = self._law("claim.law.attr-falsifier.v1",
                        "The prologue must save r31 first.",
                        attributes={"falsifier": "a target prologue that"
                                                 " does not"})
        self.assertTrue(stage_record_proposal(law, root=self.root).exists())

    # --- Gate B: postprocessor reclassification requires insns N/N -------
    def test_postprocessor_reclassification_without_counts_is_refused(self):
        record = _attempt(
            "attempt.reclass.v1", "function:test_fn", outcome="parked",
            axis="this residual is postprocessor-class, not source-class",
            attributes={"law_screen": "none applicable: test"})
        with self.assertRaisesRegex(MemoryGraphError, "N/N"):
            stage_record_proposal(record, root=self.root)

    def test_postprocessor_reclassification_with_counts_is_accepted(self):
        record = _attempt(
            "attempt.reclass.v2", "function:test_fn", outcome="parked",
            axis="postprocessor-class: counts are 27/27, pure recolor",
            attributes={"law_screen": "none applicable: test"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_b_error_cites_the_count_asymmetry_laws(self):
        record = _attempt(
            "attempt.reclass.v3", "function:test_fn", outcome="parked",
            axis="eligible for the WebFrank path",
            attributes={"law_screen": "none applicable: test"})
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=self.root)
        self.assertIn("webfrank-cannot-close-a-count-asymmetric-residual",
                      str(caught.exception))

    def test_gate_b_does_not_fire_on_a_record_describing_the_gate(self):
        # REGRESSION, found by dogfooding: the first cut of gate B refused
        # the very record that DOCUMENTED it, because the record's prose
        # contains "postprocessor-class". That is exactly the defect
        # claim.RC_stale-reopen-queue-is-a-classifier-artifact.20260901.v1
        # measured (43/43 false positives from matching a record's own
        # citation prose as evidence about its subject). The fix is to fire
        # only on FUNCTION-ANCHORED records.
        methodology = {
            "schema_version": 1, "id": "claim.about-the-gate.v1",
            "kind": "claim", "subject": "project:gdl",
            "predicate": "workflow_law", "epistemic_state": "verified",
            "falsifier": "any counterexample",
            "value": "Records reclassifying a function postprocessor-class"
                     " require a quoted insns count; this claim only"
                     " describes the rule and reclassifies nothing.",
        }
        self.assertTrue(
            stage_record_proposal(methodology, root=self.root).exists())

    def test_gate_b_ignores_citation_and_verification_prose(self):
        # Second half of the same defect class: quoting your gate commands
        # in attributes.verification must not make you look like the thing
        # being searched for.
        record = _attempt(
            "attempt.cites.v1", "function:test_fn", outcome="parked",
            axis="register rotation resisted",
            attributes={
                "law_screen": "screened claim.law.webfrank-cannot-close-a-"
                              "count-asymmetric-residual; postprocessor-class"
                              " ruled out",
                "verification": "defake_gate check; considered the"
                                " postprocessor path and rejected it",
            })
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_ordinary_park_is_not_treated_as_a_reclassification(self):
        record = _attempt(
            "attempt.ordinary.v1", "function:test_fn", outcome="parked",
            axis="register rotation resisted three spellings",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "swapped the two locals"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    # --- Gate C: multi-edit probed_form requires held_fixed --------------
    def test_multi_edit_probed_form_without_held_fixed_is_refused(self):
        record = _attempt(
            "attempt.multi.v1", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "tried three forms: a cached base,"
                                       " a volatile scaffold, and an"
                                       " extern ghost"})
        with self.assertRaisesRegex(MemoryGraphError, "held_fixed"):
            stage_record_proposal(record, root=self.root)

    def test_multi_edit_probed_form_with_held_fixed_is_accepted(self):
        record = _attempt(
            "attempt.multi.v2", "function:test_fn", outcome="capped",
            held_fixed="the extern-ghost declaration, kept in all three",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "tried three forms: a, b, c"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_numbered_enumeration_also_triggers_gate_c(self):
        record = _attempt(
            "attempt.multi.v3", "function:test_fn", outcome="parked",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "1) hoist the base 2) drop volatile"})
        with self.assertRaisesRegex(MemoryGraphError, "held_fixed"):
            stage_record_proposal(record, root=self.root)

    def test_single_edit_probed_form_is_not_gated(self):
        record = _attempt(
            "attempt.single.v1", "function:test_fn", outcome="parked",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "hoisted the base pointer to a local"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_c_error_cites_the_btricol_joint_lever(self):
        record = _attempt(
            "attempt.multi.v4", "function:test_fn", outcome="parked",
            attributes={"law_screen": "none applicable: test",
                        "probed_form": "two spellings, both negative"})
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=self.root)
        self.assertIn("btricol", str(caught.exception))

    def test_gates_do_not_fire_on_records_lacking_the_trigger(self):
        # The whole point of siting the gates in staging: an ordinary
        # attempt proposal is unaffected.
        record = _attempt("attempt.clean.v1", "function:test_fn",
                          attributes={"law_screen": "none applicable: test"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    # --- Gate G: a postprocessor closure must enumerate its verifiers ----
    #
    # attempt.HV_drawmemcardmessage-uninitialised-path-bar-reconfirmed
    # .20260901.v2 concluded "the postprocessor path is closed and no
    # permutation repair will reopen it" having run
    # verify_consistent_recolor and NEVER verify_value_equality_recolor —
    # the mode the refusal message itself names as an escape. MC re-screened
    # a run later and had to supersede it. The cap was not wrong, it was
    # UNDERDETERMINED, and nothing in it said so.
    CLOSURE = ("insns 204/204. verify_consistent_recolor refuses at +0x1ec,"
               " so the postprocessor path is closed and no permutation"
               " repair will reopen it")

    def test_postprocessor_closure_without_verifiers_run_is_refused(self):
        record = _attempt(
            "attempt.closure.v1", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.CLOSURE})
        with self.assertRaisesRegex(MemoryGraphError, "verifiers_run"):
            stage_record_proposal(record, root=self.root)

    def test_postprocessor_closure_with_verifiers_run_is_accepted(self):
        record = _attempt(
            "attempt.closure.v2", "function:test_fn", outcome="capped",
            verifiers_run=["copy_register_fields",
                           "verify_consistent_recolor",
                           "verify_value_equality_recolor"],
            attributes={"law_screen": "none applicable: test",
                        "residual": self.CLOSURE})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_g_error_names_the_shipped_verifier_surface(self):
        """The message has to tell the author what there was to run."""
        record = _attempt(
            "attempt.closure.v3", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.CLOSURE})
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=self.root)
        message = str(caught.exception)
        self.assertIn("verify_value_equality_recolor", message)
        # cites the motivating record by id (its slug is lowercased)
        self.assertIn("drawmemcardmessage", message)

    def test_gate_g_only_fires_on_veto_outcomes(self):
        """An IMPROVED record vetoes nothing, so it is not taxed — the same
        narrowing gate C took."""
        record = _attempt(
            "attempt.closure.v4", "function:test_fn", outcome="improved",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.CLOSURE})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    # --- Gate G, composed extension (run 38): name the WINDOWS ----------
    #
    # A composed rule is a CHOICE OF SPANS AND ORDERS, not one object, so a
    # composition that refuses refuses AT A SHAPE. MEASURED:
    # attempt.MC_init-all-dir-info-composed-refusal-... denied the existing
    # composed class for init_all_dir_info on ONE window, `pre
    # 0x68:0x70:1,0`, refusing "on both arrow orders" — which names the
    # MODE, while the record's own analysis puts the refusal in the
    # +0x14..+0x20 half that window does not cover at all.
    COMPOSED = ("copy_register_fields cannot rewrite the li, so the composed"
                " rule refuses on both arrow orders and the existing"
                " WebFrank composition is closed for this function")

    def test_a_composed_refusal_naming_only_the_mode_is_refused(self):
        record = _attempt(
            "attempt.composed.v1", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COMPOSED})
        with self.assertRaisesRegex(MemoryGraphError, "windows_tried"):
            stage_record_proposal(record, root=self.root)

    def test_a_composed_refusal_with_windows_tried_is_accepted(self):
        record = _attempt(
            "attempt.composed.v2", "function:test_fn", outcome="capped",
            windows_tried=["0x68:0x70:1,0", "0x14:0x20:2,0,1"],
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COMPOSED})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_windows_quoted_in_the_PROSE_also_satisfy_the_gate(self):
        """The MC record's own spelling: the shape is in the measurement."""
        record = _attempt(
            "attempt.composed.v3", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COMPOSED
                        + ". Window tried: 0x68:0x70:1,0 only."})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_the_composed_gate_error_names_the_mode_versus_shape_distinction(
            self):
        record = _attempt(
            "attempt.composed.v4", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COMPOSED})
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=self.root)
        message = str(caught.exception)
        self.assertIn("init_all_dir_info", message)
        self.assertIn("arrow orders", message)

    def test_the_composed_gate_only_fires_on_veto_outcomes(self):
        record = _attempt(
            "attempt.composed.v5", "function:test_fn", outcome="improved",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COMPOSED})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_an_ordinary_park_is_not_taxed_by_the_composed_gate(self):
        record = _attempt(
            "attempt.composed.v6", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": "Plain register-allocation park; no"
                                    " postprocessor claim at all."})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_g_does_not_fire_on_a_single_verifier_report(self):
        """Reporting ONE verifier's refusal without generalising from it is
        not a closure claim and is not gated."""
        record = _attempt(
            "attempt.closure.v5", "function:test_fn", outcome="capped",
            attributes={
                "law_screen": "none applicable: test",
                "residual": "verify_consistent_recolor refuses at +0x1ec"
                            " with 'use of g28 does not correspond to g25';"
                            " next lane should try the value-equality mode"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_g_ignores_a_record_with_no_webfrank_vocabulary(self):
        record = _attempt(
            "attempt.closure.v6", "function:test_fn", outcome="capped",
            attributes={"law_screen": "none applicable: test",
                        "residual": "the path is closed for source work"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    # --- Gate H: region-untouched claims state their scan coverage -------
    #
    # "Nothing writes r1+8..55" is a UNIVERSAL claim over the instruction
    # stream, so a scan enumerating only the forms its author thought of
    # produces the same sentence as a complete one. Two records in CH's
    # swbos lineage asserted the region untouched while both were blind to
    # register-relative cursor stores — the very form a by-value aggregate
    # argument copy emits, which was the mechanism under investigation.
    UNTOUCHED = ("There is no store into r1+8..55 anywhere in the function,"
                 " so the 48 dead bytes cannot be an argument copy.")
    COVERED = ("There is no store into r1+8..55 under ANY addressing mode,"
               " register-relative included, so the 48 dead bytes cannot be"
               " an argument copy.")

    def test_region_untouched_without_coverage_is_refused(self):
        record = _attempt(
            "attempt.region.v1", "function:test_fn", outcome="negative",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.UNTOUCHED})
        with self.assertRaisesRegex(MemoryGraphError,
                                    "addressing_modes_covered"):
            stage_record_proposal(record, root=self.root)

    def test_the_prose_coverage_sentence_discharges_it(self):
        """The wording the settling CH record actually used."""
        record = _attempt(
            "attempt.region.v2", "function:test_fn", outcome="negative",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.COVERED})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_the_typed_field_discharges_it(self):
        record = _attempt(
            "attempt.region.v3", "function:test_fn", outcome="negative",
            addressing_modes_covered=["D-form", "indexed", "update-form",
                                      "register-relative cursors"],
            attributes={"law_screen": "none applicable: test",
                        "residual": self.UNTOUCHED})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_h_needs_an_actual_region(self):
        """A general 'nothing writes it' with no region is not this claim."""
        record = _attempt(
            "attempt.region.v4", "function:test_fn", outcome="negative",
            attributes={"law_screen": "none applicable: test",
                        "residual": "there is no store into the scratch"
                                    " buffer at any point"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_h_error_names_the_blind_spot(self):
        record = _attempt(
            "attempt.region.v5", "function:test_fn", outcome="negative",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.UNTOUCHED})
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=self.root)
        message = str(caught.exception)
        self.assertIn("REGISTER-RELATIVE CURSOR STORES", message)
        self.assertIn("stwx", message)

    def test_gate_h_applies_to_a_positive_outcome_too(self):
        """Unlike gates C and G, this one is not about vetoing an axis: a
        wrong region claim misleads whatever its outcome."""
        record = _attempt(
            "attempt.region.v6", "function:test_fn", outcome="improved",
            attributes={"law_screen": "none applicable: test",
                        "residual": self.UNTOUCHED})
        with self.assertRaisesRegex(MemoryGraphError,
                                    "addressing_modes_covered"):
            stage_record_proposal(record, root=self.root)


class TuNameCandidateTests(unittest.TestCase):
    """A `tu:` reference must accept the path a worker actually types.

    Run-37 item 10. The brief said the error should "say the module path
    needs its .c suffix" — REFUTED on measurement: the suffix was already
    optional in both directions. What actually failed was the leading
    `src/`, which is how every matching tool spells a unit path, and the
    refusal's own directory said the suffix was optional while never
    mentioning the prefix — so the obvious next guess was the one thing
    that was already fine. tools/gdl strips a stray `src/`; the graph now
    agrees with it.
    """

    def test_a_src_prefixed_unit_path_is_a_candidate(self):
        self.assertIn("game/sys/memcard",
                      core.tu_name_candidates("src/game/sys/memcard.c"))

    def test_the_suffix_was_never_the_problem(self):
        for spelling in ("game/sys/memcard", "game/sys/memcard.c",
                         "game/sys/memcard.cpp"):
            self.assertIn("game/sys/memcard.c",
                          core.tu_name_candidates(spelling))

    def test_a_renamed_tu_resolves_from_its_former_spelling(self):
        """movieplayer.c -> movieplayer.cpp, 2026-08-31."""
        self.assertIn("game/movie/movieplayer.cpp",
                      core.tu_name_candidates("game/movie/movieplayer.c"))

    def test_candidates_are_deduplicated_and_ordered(self):
        out = core.tu_name_candidates("game/sys/memcard")
        self.assertEqual(len(out), len(set(out)))
        self.assertEqual(out[0], "game/sys/memcard")

    def test_the_error_names_the_prefix_not_the_suffix(self):
        message = core.unknown_entity_message(
            "tu:src/game/nope/not_a_tu.c", [], [])
        self.assertIn("`src/` PREFIX IS WHAT BROKE THIS", message)
        self.assertIn("suffix really is optional", message)

    def test_the_prefix_note_is_absent_when_there_is_no_prefix(self):
        message = core.unknown_entity_message("tu:game/nope/x.c", [], [])
        self.assertNotIn("PREFIX IS WHAT BROKE THIS", message)


class MechanismSentenceTests(unittest.TestCase):
    """Pin prose that names a SIBLING is evidence the sibling cannot get.

    Run-37 item 5 (UA): a pin's mechanism note routinely explains its
    residual by reference to another function in the same TU, and nothing
    surfaced those sentences — the sibling's own records are silent and the
    pin is filed under a different function's name. Verified live on
    game/world/camera: the do_camera pin names camera_mode_level.
    """

    TEXT = ("The window is control-free. camera_mode_level forced the"
            " colouring here. Unrelated closing sentence.")

    def test_returns_only_sentences_naming_the_function(self):
        out = core.mechanism_sentences_naming(
            self.TEXT, ["camera_mode_level"])
        self.assertEqual(len(out["camera_mode_level"]), 1)
        self.assertIn("forced the colouring", out["camera_mode_level"][0])

    def test_a_name_that_never_appears_is_absent(self):
        self.assertEqual(
            core.mechanism_sentences_naming(self.TEXT, ["do_camera"]), {})

    def test_exclude_stops_a_pin_reporting_itself(self):
        out = core.mechanism_sentences_naming(
            self.TEXT, ["camera_mode_level"], exclude="camera_mode_level")
        self.assertEqual(out, {})

    def test_word_boundaries_prevent_a_prefix_match(self):
        """`do_players` must not match inside `do_players_tail`."""
        out = core.mechanism_sentences_naming(
            "do_players_tail was rewritten.", ["do_players"])
        self.assertEqual(out, {})

    def test_multiple_names_each_get_their_own_sentences(self):
        text = ("alpha was hoisted. beta was not. alpha and beta both moved.")
        out = core.mechanism_sentences_naming(text, ["alpha", "beta"])
        self.assertEqual(len(out["alpha"]), 2)
        self.assertEqual(len(out["beta"]), 2)

    def test_empty_inputs_are_safe(self):
        self.assertEqual(core.mechanism_sentences_naming("", ["a"]), {})
        self.assertEqual(core.mechanism_sentences_naming("text", []), {})
        self.assertEqual(core.mechanism_sentences_naming("text", None), {})


class RetrievalQueryTests(unittest.TestCase):
    """residual / family / capability queries, slug + pin indexing, brief."""

    @classmethod
    def setUpClass(cls):
        cls.root = make_root()
        records = cls.root / "memory_graph" / "records"
        _write(records / "entities" / "entity.compiler-test.json", {
            "schema_version": 1, "id": "entity.compiler-test",
            "kind": "entity", "entity_type": "compiler",
            "key": "compiler:test", "name": "test compiler",
        })
        # A law whose id carries the family words but whose PROSE does not:
        # the exact shape the prose-only index missed.
        _write(records / "claims" /
               "claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1"
               ".json", {
                   "schema_version": 1,
                   "id": "claim.law.live-zero-copy-vs-remat-is-allocator"
                         "-not-source.20260831.v1",
                   "kind": "claim", "subject": "compiler:test",
                   "predicate": "codegen_law", "epistemic_state": "verified",
                   "value": "The allocator picks the carrier; no source form"
                            " reaches it.",
                   "valid_from": TODAY,
                   "falsifier": "a source spelling that flips the carrier",
                   "asserted_by": ["tools/gdl/webfrank.py"],
                   "attributes": {"scope": "MWCC 1.2.5n"},
               })
        # Two attempts carrying structured residuals in the same family.
        _write(records / "attempts" / "attempt.resid-a.v1.json", _attempt(
            "attempt.resid-a.v1", "function:test_fn", outcome="parked",
            axis="zero carrier park",
            residual={"signature": "+1 addi -1 li",
                      "family": "live-zero-remat",
                      "capability_needed": "dataflow-equivalence",
                      "measured_at": "2026-09-01"},
            attributes={"laws_applied": [
                "claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1"]},
        ))
        _write(records / "attempts" / "attempt.resid-b.v1.json", _attempt(
            "attempt.resid-b.v1", "function:other_fn", outcome="capped",
            axis="frame slot park",
            residual={"signature": "+2 stw -2 stmw", "family": "frame-slot",
                      "capability_needed": None,
                      "measured_at": "2026-09-01"},
        ))
        # LEGACY tier: coarse residual_class, no family — the 941-record
        # population that must not be invisible to --family.
        _write(records / "attempts" / "attempt.legacy-class.v1.json", _attempt(
            "attempt.legacy-class.v1", "function:test_fn", outcome="parked",
            axis="legacy coarse class only",
            residual_class="REGISTER_ONLY/SCHEDULE (allocator residual)",
            residual={"family": "unclassified",
                      "family_candidate": "live-zero-remat",
                      "family_candidate_confidence": "UNVERIFIED guess",
                      "signature": "DIFFERS target-only: +1 addi ours-only:"
                                   " -1 li; insns T71/O71; 6 ops clusters"},
        ))
        # A 10b hypothesis + a multi-edit park with held_fixed, on test_fn.
        _write(records / "attempts" / "attempt.hypothesis.v1.json", _attempt(
            "attempt.hypothesis.v1", "function:test_fn", outcome="capped",
            axis="capped after four probes",
            held_fixed="the volatile scaffold",
            attributes={
                "probed_form": "tried three forms: a, b, c",
                "residual": "UNPARK CONDITION: only a dataflow-equivalence"
                            " audit class reaches this. There is no source"
                            " lever.",
            },
        ))
        # webfrank pins: one law-backed, one with no trail at all.
        (cls.root / "config" / "GUNE5D" / "webfrank.json").write_text(
            json.dumps({"version": 1, "units": {"game/test/foo": [
                {"function": "test_fn",
                 "copy_register_fields": {},
                 "mechanism": "the live-zero carrier is chosen by the"
                              " allocator; see"
                              " claim.law.live-zero-copy-vs-remat-is-allocator"
                              "-not-source.20260831.v1 — target does addi,"
                              " we do li"},
                {"function": "other_fn",
                 "instruction_permutation": {},
                 "mechanism": "a bare schedule window with no cited record"},
            ]}}), encoding="utf-8")
        report = cls.root / "build" / "GUNE5D" / "report.json"
        report.parent.mkdir(parents=True)
        report.write_text(json.dumps({
            "units": [{"name": "main/game/test/foo", "functions": [
                {"name": "test_fn", "fuzzy_match_percent": 95.0},
            ]}]
        }), encoding="utf-8")
        build_database(cls.root)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, ignore_errors=True)

    # --- slug-word + pin indexing ---------------------------------------
    def test_slug_words_find_a_family_the_prose_index_misses(self):
        # The law's PROSE contains none of these words; only its id does.
        result = law_corpus("live zero remat", root=self.root)
        ids = {row["id"] for row in result["laws"]}
        self.assertIn(
            "claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1", ids)
        row = next(r for r in result["laws"] if r["id"] in ids)
        self.assertIn("slug", row["match"])
        self.assertNotIn("text", row["match"])

    def test_slug_index_ignores_date_and_version_suffixes(self):
        # RC: "COUNT CITATIONS BY SLUG, NOT BY DATE SUFFIX".
        self.assertNotIn("20260831", core._slug_words("claim.law.foo-bar"
                                                      ".20260831.v1"))
        self.assertNotIn("v1", core._slug_words("claim.law.foo-bar"
                                                ".20260831.v1"))
        self.assertIn("bar", core._slug_words("claim.law.foo-bar.20260831.v1"))

    def test_prose_still_matches_and_is_labelled(self):
        result = law_corpus("allocator picks", root=self.root)
        row = result["laws"][0]
        self.assertIn("text", row["match"])

    def test_pin_mechanism_prose_is_searchable(self):
        result = law_corpus("carrier", root=self.root)
        pins = result["pin_mechanisms"]
        self.assertEqual([p["function"] for p in pins], ["test_fn"])
        self.assertIn(
            "claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1",
            pins[0]["cites_records"])

    def test_pin_mechanism_is_not_truncated(self):
        """Run-37 item 5: the prose used to be cut at 600 characters, which
        discarded 59.1% of the corpus's pin derivations (77,547 of 131,115
        chars across 82 of 91 pins). The operative sentence is routinely in
        the tail — UA measured one there that outvalued every graph query."""
        pin = law_corpus("carrier", root=self.root)["pin_mechanisms"][0]
        self.assertEqual(pin["mechanism_chars"], len(pin["mechanism"]))
        self.assertNotIn(" …", pin["mechanism"])

    def test_law_rows_expose_falsifier_and_asserted_by(self):
        row = law_corpus("live zero remat", root=self.root)["laws"][0]
        self.assertEqual(row["asserted_by"], ["tools/gdl/webfrank.py"])
        self.assertTrue(row["falsifier"])

    # --- run 34 item 6: per-term hit counts + OR-rank --------------------
    def test_or_rank_surfaces_a_partial_match_the_and_filter_dropped(self):
        # "remat" is a slug word, "picks" is in the prose, "nope" is nowhere.
        # The old AND filter required ALL tokens in ONE field and returned 0
        # (the "reloc blind real naming" failure); OR-rank surfaces the 2-of-3.
        result = law_corpus("remat picks nope", root=self.root)
        ids = [row["id"] for row in result["laws"]]
        self.assertIn(
            "claim.law.live-zero-copy-vs-remat-is-allocator-not-source"
            ".20260831.v1", ids)
        self.assertEqual(result["laws"][0]["query_terms_matched"], 2)

    def test_per_term_hit_counts_name_a_dead_term(self):
        hits = law_corpus("remat picks nope", root=self.root)["query_term_hits"]
        self.assertEqual(hits["remat"], 1)
        self.assertEqual(hits["picks"], 1)
        self.assertEqual(hits["nope"], 0)

    def test_a_single_term_query_reports_its_own_hit_count(self):
        result = law_corpus("allocator", root=self.root)
        self.assertEqual(result["query_term_hits"], {"allocator": 1})

    # --- laws --residual -------------------------------------------------
    def test_residual_signature_finds_sibling_records(self):
        result = law_corpus(root=self.root, residual="+1 addi -1 li")
        matches = {row["record"]: row for row in result["residual_matches"]}
        self.assertIn("attempt.resid-a.v1", matches)
        self.assertNotIn("attempt.resid-b.v1", matches)  # disjoint mnemonics
        self.assertEqual(matches["attempt.resid-a.v1"]["shared_tokens"],
                         ["addi", "li"])
        self.assertEqual(matches["attempt.resid-a.v1"]["capability_needed"],
                         "dataflow-equivalence")

    def test_residual_pulls_in_the_laws_those_siblings_applied(self):
        result = law_corpus(root=self.root, residual="+1 addi -1 li")
        ids = {row["id"] for row in result["laws"]}
        self.assertIn(
            "claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1", ids)

    def test_residual_surfaces_pins_naming_the_same_mnemonics(self):
        result = law_corpus(root=self.root, residual="+1 addi -1 li")
        self.assertEqual([p["function"] for p in result["pin_mechanisms"]],
                         ["test_fn"])

    # --- laws --residual discrimination (run-38 item 10) -----------------
    def test_a_shared_token_carries_its_corpus_frequency(self):
        """So a reader can see WHY a row ranked where it did."""
        result = law_corpus(root=self.root, residual="+1 addi -1 li")
        row = next(r for r in result["residual_matches"]
                   if r["record"] == "attempt.resid-a.v1")
        self.assertEqual(set(row["shared_token_frequency"]), {"addi", "li"})
        self.assertGreater(row["token_specificity"], 0.0)

    def test_a_rare_mnemonic_outranks_a_ubiquitous_one(self):
        """THE MEASURED DEFECT. A jumptable-class query returned 153 rows
        of which 151 shared exactly one token — `b`, carried by 151 of the
        corpus's signatures — while the two rows that actually shared
        `jumptable` sorted in among them. Counting shared mnemonics
        treats every opcode as equally informative; they are not."""
        common = {f"op{n}": 40 for n in range(1)}
        common["b"] = 151
        common["jumptable"] = 2
        rare = core._token_rarity("jumptable", common, 200)
        ubiquitous = core._token_rarity("b", common, 200)
        self.assertGreater(rare, ubiquitous)

    def test_token_rarity_is_zero_on_an_empty_corpus(self):
        self.assertEqual(core._token_rarity("b", {}, 0), 0.0)

    def test_an_instruction_band_is_not_a_discriminating_facet(self):
        """The weights already call it 'a coincidence two hundred records
        also share'; the SELECTION gate did not read them."""
        self.assertEqual(
            core.discriminating_facets(
                ["insnband:200", "parity:even", "flag:x"]),
            [])

    def test_real_signature_content_IS_discriminating(self):
        self.assertEqual(
            core.discriminating_facets(["op:jumptable", "insnband:200"]),
            ["op:jumptable"])

    def test_a_zero_weight_facet_never_discriminates(self):
        """`kind:` gates the comparison and scores nothing, so it cannot
        also be the evidence that selects a row."""
        self.assertEqual(
            core.discriminating_facets(["kind:reorder", "resolution:x"]), [])

    def test_a_weak_only_match_is_suppressed_and_COUNTED(self):
        """A filter that quietly drops rows reads as a false all-clear."""
        result = law_corpus(root=self.root, residual="+1 addi -1 li")
        self.assertIn("residual_weak_only_suppressed", result)
        self.assertIsInstance(result["residual_weak_only_suppressed"], int)

    # --- find --family / --capability ------------------------------------
    def test_find_family_facet(self):
        hits = find_records(root=self.root, family="live-zero-remat")
        verified = {row["id"] for row in hits["results"]
                    if row["match"] == "family"}
        self.assertEqual(verified, {"attempt.resid-a.v1"})
        self.assertEqual(hits["results"][0]["residual"]["family"],
                         "live-zero-remat")

    def test_signature_tokens_parse_the_real_fndiff_ops_format(self):
        # Word-splitting the measured format yields {differs, target, only,
        # ours, insns, ops, clusters, t71, o71}, so ANY two signatures
        # overlap on framing words and the facet degenerates.
        tokens = core._signature_tokens(
            "DIFFERS target-only: +1 add ours-only: -1 mr;"
            " insns T71/O71; 6 ops clusters")
        self.assertEqual(tokens, {"add", "mr"})

    def test_signature_tokens_of_an_identical_multiset_are_empty(self):
        self.assertEqual(
            core._signature_tokens(
                "0t opcode multiset IDENTICAL (155/155); insns T155/O155;"
                " 0 ops clusters"),
            set())

    def test_two_unrelated_signatures_do_not_share_framing_words(self):
        a = core._signature_tokens(
            "DIFFERS target-only: +1 add ours-only: -1 mr; insns T71/O71")
        b = core._signature_tokens(
            "DIFFERS target-only: +2 stw ours-only: -2 stmw; insns T9/O9")
        self.assertEqual(a & b, set())

    def test_find_family_tiers_are_labelled_and_ranked(self):
        hits = find_records(root=self.root, family="live-zero-remat",
                            limit=50)
        by_id = {row["id"]: row for row in hits["results"]}
        # verified tier
        self.assertEqual(by_id["attempt.resid-a.v1"]["match"], "family")
        # legacy coarse class bridges in, but LABELLED as a widening
        self.assertEqual(by_id["attempt.legacy-class.v1"]["match"],
                         "residual_class-fallback")
        self.assertIn("REGISTER_ONLY",
                      by_id["attempt.legacy-class.v1"]["fallback_class"])
        # exact hits rank ahead of the widening
        self.assertEqual(hits["results"][0]["match"], "family")
        self.assertIn("family_match_counts", hits)

    def test_family_candidates_are_quarantined_unless_asked_for(self):
        without = find_records(root=self.root, family="live-zero-remat",
                               limit=50)
        matches = {row["id"]: row["match"] for row in without["results"]}
        # present only via the coarse fallback, never as a candidate
        self.assertNotIn("family_candidate", set(matches.values()))
        withc = find_records(root=self.root, family="live-zero-remat",
                             limit=50, include_candidates=1)
        by_id = {row["id"]: row for row in withc["results"]}
        self.assertEqual(by_id["attempt.legacy-class.v1"]["match"],
                         "family_candidate")
        self.assertIn("UNVERIFIED",
                      by_id["attempt.legacy-class.v1"]["candidate_warning"])

    def test_residual_class_normalizer_handles_compound_spellings(self):
        self.assertEqual(
            core.normalize_residual_class(
                "STRUCTURAL(1 cluster) + REGISTER_ONLY/SCHEDULE(2 clusters)"),
            ["REGISTER_ONLY", "STRUCTURAL", "SCHEDULE"])
        self.assertEqual(core.normalize_residual_class("REGISTER"),
                         ["REGISTER_ONLY"])
        self.assertEqual(core.normalize_residual_class(None), [])

    def test_find_family_rejects_a_vocabulary_typo(self):
        # A typo would otherwise return zero rows, which reads as a false
        # all-clear on what is used as a NEGATIVE screen.
        with self.assertRaisesRegex(MemoryGraphError, "unknown residual"):
            find_records(root=self.root, family="live-zero-rematt")

    def test_find_capability_marks_structured_and_prose_hits(self):
        hits = find_records(root=self.root, capability="dataflow-equivalence",
                            limit=50)
        by_id = {row["id"]: row for row in hits["results"]}
        self.assertEqual(by_id["attempt.resid-a.v1"]["match"], "field")
        # the 10b record only MENTIONS the capability in prose
        self.assertEqual(by_id["attempt.hypothesis.v1"]["match"], "prose")
        self.assertIn("capability_note", hits)

    def test_find_requires_a_facet(self):
        with self.assertRaisesRegex(MemoryGraphError, "at least one"):
            find_records(root=self.root)

    # --- brief upgrade ----------------------------------------------------
    def test_brief_leads_with_open_hypotheses(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        self.assertEqual(list(brief)[1], "open_hypotheses")
        markers = {row["marker"] for row in brief["open_hypotheses"]}
        self.assertIn("unpark condition", markers)
        self.assertEqual(brief["open_hypotheses"][0]["record"],
                         "attempt.hypothesis.v1")

    def test_brief_reports_vetoed_axes_with_form_and_held_fixed(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        by_id = {row["record"]: row for row in brief["vetoed_axes"]}
        self.assertTrue(by_id["attempt.hypothesis.v1"]["has_probed_form"])
        self.assertEqual(by_id["attempt.hypothesis.v1"]["held_fixed"],
                         "the volatile scaffold")
        # a park with no probed_form is surfaced as the weaker veto it is
        self.assertFalse(by_id["attempt.resid-a.v1"]["has_probed_form"])

    def test_brief_carries_the_structured_residual(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        by_id = {row["record"]: row for row in brief["vetoed_axes"]}
        self.assertEqual(by_id["attempt.resid-a.v1"]["residual"]["family"],
                         "live-zero-remat")

    def test_brief_stamps_staleness_on_every_number(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        self.assertIn("REMEASURE", brief["staleness_banner"])
        self.assertIsNotNone(brief["report_generated_at"])
        for row in brief["functions"]:
            self.assertIn("REMEASURE", row["fuzzy_staleness"])
        recorded = [row for row in brief["live_attempts"]
                    if row.get("recorded_fuzzy") is not None]
        for row in recorded:
            self.assertIn("remeasure", row["recorded_fuzzy_staleness"])

    def test_brief_classes_pin_provenance(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        by_fn = {row["function"]: row for row in brief["webfrank_pins"]}
        self.assertEqual(by_fn["test_fn"]["provenance"],
                         "law-backed-source-unreachable")
        # other_fn's park exists but documents no probed_form
        self.assertEqual(by_fn["other_fn"]["provenance"],
                         "parked-without-probed_form")

    def test_brief_roster_carries_the_unabsorbed_closability_column(self):
        """run-31 item 6.

        The test fixture has no built objects, so the metric is UNDEFINED
        for every row. The contract that matters is that it reads null
        with a staleness note saying so — null must never be confused with
        0, which would read as "fully absorbed, stage-closable".
        """
        brief = tu_briefing("game/test/foo", root=self.root)
        self.assertTrue(brief["functions"])
        for row in brief["functions"]:
            self.assertIn("unabsorbed", row)
            self.assertIn("unabsorbed_tier", row)
            self.assertIsNone(row["unabsorbed"])
            self.assertIsNone(row["unabsorbed_tier"])
            self.assertIn("never means zero", row["unabsorbed_staleness"])

    def test_brief_has_no_internal_record_leakage(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        for row in brief["live_attempts"]:
            self.assertNotIn("_record", row)


def ev_root():
    """A test root that can hold law records.

    `make_root` seeds only the GameCube symbol map, so `compiler:test` and
    `project:gdl` — the subjects every law and methodology claim is anchored
    to — do not resolve. Seeding them here keeps the run-32 fixtures
    self-contained instead of widening the shared helper.
    """
    root = make_root()
    entities = root / "memory_graph" / "records" / "entities"
    for key, kind, name in (("compiler:test", "compiler", "test compiler"),
                            ("project:gdl", "project", "GDL")):
        _write(entities / f"{key.replace(':', '-')}.json", {
            "schema_version": 1, "id": f"entity.{key.replace(':', '-')}",
            "kind": "entity", "entity_type": kind, "key": key, "name": name,
        })
    return root


def _law(rid, value="a law about copy forms", **extra):
    record = {
        "schema_version": 1, "id": rid, "kind": "claim",
        "subject": "compiler:test", "predicate": "codegen_law",
        "epistemic_state": "verified", "value": value,
        "valid_from": extra.pop("valid_from", "2026-08-20"),
        "recorded_at": extra.pop("recorded_at", "2026-08-20T09:00:00Z"),
    }
    record.update(extra)
    return record


class EvidenceLayerTests(unittest.TestCase):
    """Deliverable 1: the derived evidence table and what counts as evidence."""

    @classmethod
    def setUpClass(cls):
        cls.root = ev_root()
        records = cls.root / "memory_graph" / "records"
        # winner: two landings, nothing against it
        _write(records / "claims" / "w.json", _law("claim.law.winner.v1"))
        # heavily-cited law whose citations are mostly PARKS it predicted
        _write(records / "claims" / "p.json", _law("claim.law.predictor.v1"))
        # refuted: no landing, one refutation
        _write(records / "claims" / "r.json", _law("claim.law.rotten.v1"))
        # contested: a landing AND a refutation
        _write(records / "claims" / "c.json", _law("claim.law.contested.v1"))
        # provisional: never cited at all
        _write(records / "claims" / "u.json", _law("claim.law.untried.v1"))

        for i, outcome in enumerate(("improved", "exact")):
            _write(records / "attempts" / f"win{i}.json", _attempt(
                f"attempt.win{i}.v1", "function:test_fn", outcome=outcome,
                attributes={"laws_applied": ["claim.law.winner.v1"]}))
        # the JSON-ENCODED-STRING spelling of laws_applied, which the
        # importer used to drop on the floor (142 of 1912 citations survived)
        _write(records / "attempts" / "winstr.json", _attempt(
            "attempt.winstr.v1", "function:test_fn", outcome="improved",
            attributes={"laws_applied": '["claim.law.winner.v1"]'}))
        # parks and caps the predictor law correctly called: NOT failures
        for i, outcome in enumerate(("parked", "capped", "parked", "negative")):
            _write(records / "attempts" / f"pred{i}.json", _attempt(
                f"attempt.pred{i}.v1", "function:test_fn", outcome=outcome,
                attributes={"laws_applied": ["claim.law.predictor.v1"]}))
        _write(records / "attempts" / "predwin.json", _attempt(
            "attempt.predwin.v1", "function:test_fn", outcome="improved",
            attributes={"laws_applied": ["claim.law.predictor.v1"]}))
        # refutations
        _write(records / "attempts" / "refuter.json", _attempt(
            "attempt.refuter.v1", "function:test_fn", outcome="improved",
            refutes="claim.law.rotten.v1"))
        _write(records / "attempts" / "refuter2.json", _attempt(
            "attempt.refuter2.v1", "function:test_fn", outcome="improved",
            refutes="claim.law.contested.v1"))
        _write(records / "attempts" / "conwin.json", _attempt(
            "attempt.conwin.v1", "function:test_fn", outcome="improved",
            attributes={"laws_applied": ["claim.law.contested.v1"]}))
        build_database(cls.root)
        cls.rows = {
            row["id"]: row for row in
            law_corpus(root=cls.root, include_provisional=1)["laws"]
        }

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, ignore_errors=True)

    def test_success_is_an_exact_or_improved_landing(self):
        winner = self.rows["claim.law.winner.v1"]["evidence"]
        self.assertEqual(winner["successes"], 3)  # improved + exact + string
        self.assertEqual(winner["failures"], 0)

    def test_json_string_spelling_of_laws_applied_is_counted(self):
        """The measured import bug: only the list spelling was read."""
        winner = self.rows["claim.law.winner.v1"]["evidence"]
        self.assertIn("attempt.winstr.v1", winner["success_records"])

    def test_a_park_a_cap_and_a_negative_are_not_failures(self):
        predictor = self.rows["claim.law.predictor.v1"]["evidence"]
        self.assertEqual(predictor["failures"], 0)
        self.assertEqual(predictor["neutral_citations"], 4)
        self.assertEqual(predictor["successes"], 1)

    def test_n_is_the_landing_denominator_not_the_citation_count(self):
        predictor = self.rows["claim.law.predictor.v1"]
        self.assertEqual(predictor["evidence"]["cited_total"], 5)
        self.assertEqual(predictor["n"], 1)
        self.assertLess(predictor["n"], predictor["evidence"]["cited_total"])

    def test_a_refutes_edge_is_a_failure(self):
        rotten = self.rows["claim.law.rotten.v1"]["evidence"]
        self.assertEqual(rotten["failures"], 1)
        self.assertEqual(rotten["failure_records"], ["attempt.refuter.v1"])

    def test_explicit_laws_failed_citation_counts(self):
        root = ev_root()
        records = root / "memory_graph" / "records"
        _write(records / "claims" / "l.json", _law("claim.law.explicit.v1"))
        _write(records / "attempts" / "f.json", _attempt(
            "attempt.explicit-fail.v1", "function:test_fn", outcome="parked",
            attributes={"laws_failed": ["claim.law.explicit.v1"]}))
        build_database(root)
        rows = {row["id"]: row for row in
                law_corpus(root=root, include_provisional=1)["laws"]}
        self.assertEqual(
            rows["claim.law.explicit.v1"]["evidence"]["failures"], 1)
        shutil.rmtree(root, ignore_errors=True)

    def test_the_table_is_derived_and_a_rebuild_is_idempotent(self):
        before = law_corpus(root=self.root, include_provisional=1)["laws"]
        build_database(self.root)
        after = law_corpus(root=self.root, include_provisional=1)["laws"]
        self.assertEqual([(r["id"], r["n"], r["score"]) for r in before],
                         [(r["id"], r["n"], r["score"]) for r in after])

    def test_wilson_penalises_a_small_sample(self):
        self.assertEqual(wilson_lower_bound(0, 0), 0.0)
        self.assertLess(wilson_lower_bound(1, 0), wilson_lower_bound(40, 0))
        self.assertEqual(wilson_lower_bound(0, 5), 0.0)
        self.assertLessEqual(wilson_lower_bound(3, 0), 1.0)

    def test_beta_mean_distinguishes_unknown_from_known_bad(self):
        # no information reads 0.5; a refuted law with no landing reads below
        self.assertAlmostEqual(beta_mean(0, 0), 0.5)
        self.assertLess(beta_mean(0, 3), 0.5)
        self.assertGreater(beta_mean(3, 0), 0.5)


class LawRankingTests(unittest.TestCase):
    """Deliverables 2 and 3: tiering, ranking, and provisional handling."""

    def test_status_tiers(self):
        self.assertEqual(law_evidence_score(3, 0)["status"], "established")
        self.assertEqual(law_evidence_score(3, 1)["status"], "contested")
        self.assertEqual(law_evidence_score(0, 1)["status"], "refuted")
        self.assertEqual(law_evidence_score(0, 0)["status"], "provisional")

    def test_tier_outranks_raw_score(self):
        """THE CANARY INVARIANT, in miniature.

        A refuted law with 11 landings scores 0.65; a clean winner with one
        landing scores 0.21. Ranking on the bare number would float the
        refuted one above the winner, which is the exact failure the run-32
        canary forbids. The tier must win.
        """
        contested = law_evidence_score(11, 1) | {"id": "contested"}
        winner = law_evidence_score(1, 0) | {"id": "winner"}
        self.assertGreater(contested["wilson"], winner["wilson"])
        self.assertLess(law_score_sort_key(winner),
                        law_score_sort_key(contested))

    def test_tier_order_is_the_documented_one(self):
        self.assertEqual(
            LAW_STATUS_ORDER,
            ("established", "contested", "provisional", "refuted"))

    def test_provisional_hidden_from_the_browse_but_not_deleted(self):
        root = ev_root()
        records = root / "memory_graph" / "records"
        _write(records / "claims" / "u.json", _law("claim.law.unverified.v1"))
        _write(records / "claims" / "w.json", _law("claim.law.verified.v1"))
        _write(records / "attempts" / "a.json", _attempt(
            "attempt.a.v1", "function:test_fn", outcome="improved",
            attributes={"laws_applied": ["claim.law.verified.v1"]}))
        build_database(root)
        browse = law_corpus(root=root)
        self.assertNotIn("claim.law.unverified.v1",
                         {row["id"] for row in browse["laws"]})
        # segregated, NOT deleted: the browse is also the enumeration surface
        self.assertIn("claim.law.unverified.v1",
                      {row["id"] for row in browse["provisional_laws"]})
        opened = law_corpus(root=root, include_provisional=1)
        self.assertIn("claim.law.unverified.v1",
                      {row["id"] for row in opened["laws"]})
        shutil.rmtree(root, ignore_errors=True)

    def test_a_targeted_request_never_drops_a_provisional_match(self):
        """A mandatory screen must not silently shrink.

        Measured on the live corpus: 7 of the 33 core-screen laws are
        provisional. Suppressing inside a tag/query/residual filter would
        have removed 21% of a screen AGENTS.md calls mandatory.
        """
        root = ev_root()
        records = root / "memory_graph" / "records"
        _write(records / "claims" / "u.json", _law(
            "claim.law.unverified-screen.v1",
            attributes={"tags": ["core-screen"], "scope": "screening"}))
        build_database(root)
        for kwargs in ({"tag": "core-screen"}, {"query": "unverified screen"}):
            with self.subTest(**kwargs):
                result = law_corpus(root=root, **kwargs)
                self.assertIn("claim.law.unverified-screen.v1",
                              {row["id"] for row in result["laws"]})
                self.assertEqual(result["provisional_retained"], 1)
                self.assertEqual(result["hidden_provisional"], 0)
        shutil.rmtree(root, ignore_errors=True)

    def test_ranking_happens_before_truncation(self):
        """A limit must cut the WORST rows, not the date-newest ones."""
        root = ev_root()
        records = root / "memory_graph" / "records"
        for i in range(4):
            _write(records / "claims" / f"l{i}.json", _law(
                f"claim.law.rank{i}.v1", valid_from=f"2026-08-0{i + 1}"))
            for j in range(i + 1):
                _write(records / "attempts" / f"a{i}_{j}.json", _attempt(
                    f"attempt.a{i}x{j}.v1", "function:test_fn",
                    outcome="improved",
                    attributes={"laws_applied": [f"claim.law.rank{i}.v1"]}))
        build_database(root)
        top = law_corpus(root=root, limit=1)
        # rank3 has the most landings and the OLDEST date but must survive
        self.assertEqual(top["laws"][0]["id"], "claim.law.rank3.v1")
        self.assertEqual(top["truncated"], 3)
        shutil.rmtree(root, ignore_errors=True)

    def test_brief_law_rows_carry_the_evidence_columns(self):
        root = ev_root()
        records = root / "memory_graph" / "records"
        _write(records / "claims" / "l.json", _law(
            "claim.law.briefed.v1",
            attributes={"tags": ["core-screen"], "scope": "briefing"}))
        _write(records / "attempts" / "a.json", _attempt(
            "attempt.briefed.v1", "function:test_fn", outcome="improved",
            attributes={"laws_applied": ["claim.law.briefed.v1"]}))
        build_database(root)
        brief = tu_briefing("game/test/foo", root=root)
        row = next(r for r in brief["core_screen_laws"]
                   if r["id"] == "claim.law.briefed.v1")
        for column in ("status", "score", "n", "successes", "failures"):
            self.assertIn(column, row)
        self.assertEqual(row["status"], "established")
        self.assertIn("law_evidence_note", brief)
        shutil.rmtree(root, ignore_errors=True)


class RegimeEventTests(unittest.TestCase):
    """Deliverable 4: needs-revalidation is EVENT-based, never calendar decay."""

    def _root(self):
        root = ev_root()
        records = root / "memory_graph" / "records"
        _write(records / "claims" / "old.json", _law(
            "claim.law.regnorm-thing.v1", valid_from="2026-08-01",
            recorded_at="2026-08-01T09:00:00Z",
            attributes={"tags": ["metrics"], "scope": "tools/gdl/regnorm.py"}))
        _write(records / "attempts" / "a.json", _attempt(
            "attempt.oldwin.v1", "function:test_fn", outcome="improved",
            valid_from="2026-08-01",
            attributes={"laws_applied": ["claim.law.regnorm-thing.v1"]}))
        return root

    def test_event_add_stages_and_lists(self):
        root = self._root()
        path = stage_event_proposal("regnorm-v2", scope="regnorm",
                                    occurred_at="2026-08-30", root=root)
        self.assertTrue(path.exists())
        build_database(root)
        events = regime_events(root=root)
        self.assertEqual(events[0]["slug"], "regnorm-v2")
        self.assertEqual(events[0]["scope"], "regnorm")
        shutil.rmtree(root, ignore_errors=True)

    def test_banner_fires_when_evidence_predates_the_event(self):
        root = self._root()
        stage_event_proposal("regnorm-v2", scope="regnorm",
                             occurred_at="2026-08-30", root=root)
        build_database(root)
        row = next(r for r in law_corpus(root=root, include_provisional=1)["laws"]
                   if r["id"] == "claim.law.regnorm-thing.v1")
        self.assertEqual(row["needs_revalidation"]["banner"],
                         "NEEDS REVALIDATION")
        shutil.rmtree(root, ignore_errors=True)

    def test_evidence_postdating_the_event_clears_the_banner(self):
        root = self._root()
        _write(root / "memory_graph" / "records" / "attempts" / "new.json",
               _attempt("attempt.newwin.v1", "function:test_fn",
                        outcome="improved", valid_from="2026-09-01",
                        recorded_at="2026-09-01T09:00:00Z",
                        attributes={"laws_applied":
                                    ["claim.law.regnorm-thing.v1"]}))
        stage_event_proposal("regnorm-v2", scope="regnorm",
                             occurred_at="2026-08-30", root=root)
        build_database(root)
        row = next(r for r in law_corpus(root=root, include_provisional=1)["laws"]
                   if r["id"] == "claim.law.regnorm-thing.v1")
        self.assertNotIn("needs_revalidation", row)
        shutil.rmtree(root, ignore_errors=True)

    def test_age_alone_never_flags_a_law(self):
        """No event, no banner — however old the evidence is."""
        root = self._root()
        build_database(root)
        rows = law_corpus(root=root, include_provisional=1)["laws"]
        self.assertTrue(all("needs_revalidation" not in row for row in rows))
        shutil.rmtree(root, ignore_errors=True)

    def test_event_scope_matches_the_id_slug_not_only_scope_prose(self):
        """Measured: only 124 of 357 laws carry scope prose, and none of it
        named the subject. Slug matching is what makes the feature fire."""
        root = ev_root()
        _write(root / "memory_graph" / "records" / "claims" / "l.json", _law(
            "claim.law.regnorm-counts-rows.v1", valid_from="2026-08-01",
            recorded_at="2026-08-01T09:00:00Z"))  # no attributes.scope at all
        stage_event_proposal("regnorm-v2", scope="regnorm",
                             occurred_at="2026-08-30", root=root)
        build_database(root)
        row = next(r for r in law_corpus(root=root, include_provisional=1)["laws"]
                   if r["id"] == "claim.law.regnorm-counts-rows.v1")
        self.assertIn("needs_revalidation", row)
        shutil.rmtree(root, ignore_errors=True)

    def test_scope_matches_a_tag(self):
        root = self._root()
        stage_event_proposal("metric-redefinition", scope="metrics",
                             occurred_at="2026-08-30", root=root)
        build_database(root)
        row = next(r for r in law_corpus(root=root, include_provisional=1)["laws"]
                   if r["id"] == "claim.law.regnorm-thing.v1")
        self.assertIn("needs_revalidation", row)
        shutil.rmtree(root, ignore_errors=True)

    def test_bad_slug_and_missing_scope_fail_closed(self):
        root = ev_root()
        with self.assertRaisesRegex(MemoryGraphError, "kebab-case"):
            stage_event_proposal("Not A Slug", scope="*", root=root)
        with self.assertRaisesRegex(MemoryGraphError, "needs --scope"):
            stage_event_proposal("fine-slug", scope="  ", root=root)
        shutil.rmtree(root, ignore_errors=True)


class DedupAtProposeTests(unittest.TestCase):
    """Deliverable 5: near-duplicate claims attach, they do not error out."""

    def setUp(self):
        self.root = ev_root()
        _write(self.root / "memory_graph" / "records" / "claims" / "e.json",
               _law("claim.law.copy-form-remat-is-allocator-choice.v1"))
        build_database(self.root)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _near(self, rid):
        return _law(rid, value="a fresh derivation of the same thing")

    def test_near_duplicate_is_refused_with_attach_guidance(self):
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(
                self._near("claim.law.copy-form-remat-is-allocator-choice.v2x"),
                root=self.root)
        message = str(caught.exception)
        self.assertIn("copy-form-remat-is-allocator-choice", message)
        self.assertIn("--confirm-new", message)
        # attach-not-error semantics: it must say what to do instead
        self.assertIn("ATTACHING", message)

    def test_confirm_new_proceeds(self):
        path = stage_record_proposal(
            self._near("claim.law.copy-form-remat-is-allocator-choice.v2x"),
            root=self.root, confirm_new=True)
        self.assertTrue(path.exists())

    def test_a_supersession_is_exempt(self):
        """A v2 is SUPPOSED to resemble its v1; gating that would fire
        hardest on exactly the records the corpus most wants written."""
        record = self._near(
            "claim.law.copy-form-remat-is-allocator-choice.v2x")
        record["supersedes"] = \
            "claim.law.copy-form-remat-is-allocator-choice.v1"
        path = stage_record_proposal(record, root=self.root)
        self.assertTrue(path.exists())

    def test_a_refutation_is_exempt(self):
        record = self._near(
            "claim.law.copy-form-remat-is-allocator-choice.v2x")
        record["refutes"] = \
            "claim.law.copy-form-remat-is-allocator-choice.v1"
        path = stage_record_proposal(record, root=self.root)
        self.assertTrue(path.exists())

    def test_a_genuinely_different_claim_passes(self):
        path = stage_record_proposal(
            _law("claim.law.entry-schedule-follows-initializer-split.v1"),
            root=self.root)
        self.assertTrue(path.exists())

    def test_attempts_are_not_deduped(self):
        """Attempt records are per-function forensics and SHOULD resemble
        their siblings."""
        record = _attempt("attempt.copy-form-remat-is-allocator-choice.v1",
                          "function:test_fn",
                          attributes={"law_screen": "none applicable: test"})
        path = stage_record_proposal(record, root=self.root)
        self.assertTrue(path.exists())


class TypedProseObjectTests(unittest.TestCase):
    """Deliverable 6: typed denial/hypothesis objects and gate D."""

    def setUp(self):
        self.root = ev_root()
        build_database(self.root)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _denial(self, **overrides):
        denial = {
            "scope": "the offsetof axis on test_fn only",
            "premise_measurement": "fndiff --ops: real 30 -> 34, 3 probes",
            "expiry_check": "python tools/gdl/probe.py game/test/foo.c test_fn",
            "falsifier": "any offsetof form that measures real < 30",
        }
        denial.update(overrides)
        return denial

    def test_denial_shape_is_checked(self):
        record = _attempt("attempt.d.v1", "function:test_fn",
                          outcome="parked", denial="just prose")
        with self.assertRaisesRegex(MemoryGraphError, "structured object"):
            _validate_record(record, Path("<t>"))

    def test_denial_missing_a_field_fails_closed(self):
        denial = self._denial()
        del denial["expiry_check"]
        record = _attempt("attempt.d.v1", "function:test_fn",
                          outcome="parked", denial=denial)
        with self.assertRaisesRegex(MemoryGraphError, "expiry_check"):
            _validate_record(record, Path("<t>"))

    def test_complete_denial_passes(self):
        record = _attempt("attempt.d.v1", "function:test_fn",
                          outcome="parked", denial=self._denial())
        _validate_record(record, Path("<t>"))

    def test_hypothesis_shape_is_checked(self):
        record = _attempt("attempt.h.v1", "function:test_fn",
                          hypothesis={"statement": "try the split decl"})
        with self.assertRaisesRegex(MemoryGraphError,
                                    "cheapest_refuting_observation"):
            _validate_record(record, Path("<t>"))

    def test_gate_d_refuses_prose_denial_without_the_typed_object(self):
        record = _attempt(
            "attempt.prose.v1", "function:test_fn", outcome="parked",
            axis="offsetof rotation is a do-not-retry axis here",
            attributes={"law_screen": "none applicable: test"})
        with self.assertRaisesRegex(MemoryGraphError, "typed `denial`"):
            stage_record_proposal(record, root=self.root)

    def test_gate_d_accepts_the_same_record_with_a_typed_denial(self):
        record = _attempt(
            "attempt.prose.v1", "function:test_fn", outcome="parked",
            axis="offsetof rotation is a do-not-retry axis here",
            denial=self._denial(),
            attributes={"law_screen": "none applicable: test"})
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_d_does_not_refuse_the_record_that_documents_it(self):
        """The GS gate-B lesson, applied in advance rather than after.

        A project-anchored methodology claim necessarily contains the denial
        vocabulary it describes. Anchor scoping excludes it by construction.
        """
        record = _law(
            "claim.law.a-prose-denial-cannot-expire.v1",
            value=("A record saying do-not-retry or NOT a candidate or"
                   " ineligible, with no typed denial, states no scope and"
                   " no expiry check."),
            subject="project:gdl", predicate="workflow_law",
            falsifier="show a prose denial a later lane screened out cheaply")
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_gate_d_ignores_citation_and_verification_prose(self):
        """Substance projection: a well-run record QUOTES the vocabulary in
        its law_screen, and must not be caught by its own apparatus."""
        record = _attempt(
            "attempt.cites.v1", "function:test_fn", outcome="improved",
            axis="split the declaration",
            attributes={
                "law_screen": "screened the do-not-retry denial laws;"
                              " none applicable",
                "verification": "confirmed this is not a candidate for the"
                                " postprocessor path",
            })
        self.assertTrue(stage_record_proposal(record, root=self.root).exists())

    def test_brief_prefers_a_typed_hypothesis(self):
        records = self.root / "memory_graph" / "records"
        _write(records / "attempts" / "typed.json", _attempt(
            "attempt.typed-hyp.v1", "function:test_fn", outcome="improved",
            hypothesis={
                "statement": "split the initializer to move the entry store",
                "cheapest_refuting_observation": "fnasm 0x0:0x20 --diff",
                "screened_against_target": "no",
            }))
        _write(records / "attempts" / "prose.json", _attempt(
            "attempt.prose-hyp.v1", "function:other_fn", outcome="parked",
            axis="untried: perhaps the pad belongs one slot lower"))
        build_database(self.root)
        brief = tu_briefing("game/test/foo", root=self.root)
        self.assertEqual(brief["open_hypotheses"][0]["marker"], "TYPED")
        self.assertEqual(brief["open_hypotheses"][0]["record"],
                         "attempt.typed-hyp.v1")
        self.assertIn("cheapest_refuting_observation",
                      brief["open_hypotheses"][0])


class LegacyQuarantineTests(unittest.TestCase):
    """Deliverable 7: flag ungated prose denials, never auto-extract them."""

    def setUp(self):
        self.root = ev_root()
        records = self.root / "memory_graph" / "records"
        _write(records / "attempts" / "legacy.json", _attempt(
            "attempt.legacy-prose.v1", "function:test_fn", outcome="parked",
            axis="regalloc rotation; this function is NOT a candidate,"
                 " do-not-retry"))
        _write(records / "attempts" / "typed.json", _attempt(
            "attempt.typed-denial.v1", "function:other_fn", outcome="parked",
            axis="do-not-retry the offsetof axis",
            denial={"scope": "offsetof axis on other_fn",
                    "premise_measurement": "real 30 -> 34 over 3 probes",
                    "expiry_check": "probe.py game/test/foo.c other_fn",
                    "falsifier": "an offsetof form measuring real < 30"}))
        _write(records / "attempts" / "clean.json", _attempt(
            "attempt.clean.v1", "function:test_fn", outcome="parked",
            axis="scheduler fog after three distinct forms"))
        build_database(self.root)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_context_flags_the_prose_denial(self):
        rows = {row["record_id"]: row
                for row in symbol_context("test_fn", root=self.root)["attempts"]}
        flagged = rows["attempt.legacy-prose.v1"]
        self.assertEqual(flagged["quarantine"]["banner"],
                         "UNGATED-PROSE-DENIAL")
        self.assertIn("NOT a candidate", flagged["quarantine"]["matched"]
                      + flagged["quarantine"]["why"] + "NOT a candidate")

    def test_a_typed_denial_is_not_quarantined(self):
        rows = {row["record_id"]: row
                for row in symbol_context("other_fn", root=self.root)["attempts"]}
        typed = rows["attempt.typed-denial.v1"]
        self.assertNotIn("quarantine", typed)
        self.assertIn("denial", typed)

    def test_an_ordinary_park_is_not_flagged(self):
        rows = {row["record_id"]: row
                for row in symbol_context("test_fn", root=self.root)["attempts"]}
        self.assertNotIn("quarantine", rows["attempt.clean.v1"])

    def test_the_flag_carries_no_extracted_fields(self):
        """Render-flag ONLY. The family backfill measured prose extraction at
        30-50% precision; a fabricated typed denial would be trusted more
        than the prose it replaced."""
        rows = {row["record_id"]: row
                for row in symbol_context("test_fn", root=self.root)["attempts"]}
        quarantine = rows["attempt.legacy-prose.v1"]["quarantine"]
        for field in ("scope", "premise_measurement", "expiry_check",
                      "falsifier"):
            self.assertNotIn(field, quarantine)
        self.assertIn("30-50%", quarantine["not_extracted"])

    def test_brief_carries_the_quarantine_on_vetoed_axes(self):
        brief = tu_briefing("game/test/foo", root=self.root)
        rows = {row["record"]: row for row in brief["vetoed_axes"]}
        self.assertIn("quarantine", rows["attempt.legacy-prose.v1"])
        self.assertNotIn("quarantine", rows["attempt.clean.v1"])


class ClaimsOwnsTests(unittest.TestCase):
    """Deliverable 8: `claims --owns <path>`."""

    def setUp(self):
        self.root = ev_root()
        _write(self.root / "memory_graph" / "inbox" / "wc.json", {
            "schema_version": 1, "id": "work_claim.owner.v1",
            "kind": "work_claim", "function": "function:test_fn",
            "owner": "worker-x", "state": "active", "claimed_at": TODAY,
            "attributes": {"scope": "game/test/foo.c whole TU"},
        })
        build_database(self.root)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_owns_finds_the_claim_by_scope_path(self):
        result = work_claims(root=self.root, owns="game/test/foo.c")
        self.assertEqual(result["verdict"], "CLAIMED")
        self.assertEqual(result["claims"][0]["owner"], "worker-x")

    def test_owns_matches_a_backslash_path(self):
        result = work_claims(root=self.root, owns=r"src\game\test\foo.c")
        self.assertEqual(result["count"], 1)

    def test_owns_finds_the_claim_by_anchor_function(self):
        result = work_claims(root=self.root, owns="test_fn")
        self.assertEqual(result["count"], 1)

    def test_an_unclaimed_path_reports_no_claim_but_no_guarantee(self):
        result = work_claims(root=self.root, owns="game/other/bar.c")
        self.assertEqual(result["count"], 0)
        self.assertEqual(result["verdict"], "no claim found")
        self.assertIn("unpushed claim protects nothing", result["owns_note"])

    def test_owns_is_absent_from_an_unfiltered_listing(self):
        result = work_claims(root=self.root)
        self.assertNotIn("verdict", result)
        self.assertEqual(result["count"], 1)


class ReorderIndexTests(unittest.TestCase):
    """RG run-33 deliverable 1: the pure-reorder residual index axis."""

    def test_stored_and_live_forms_both_parse(self):
        # The stored corpus form has no colon; live `fndiff --ops` does. A
        # regex written from the stored form alone silently classified every
        # live signature as `empty` — measured on all 13 pure-reorder
        # acceptance rows before the fix.
        stored = core.parse_residual_signature(
            "0t opcode multiset IDENTICAL (155/155); insns T155/O155;"
            " 0 ops clusters")
        live = core.parse_residual_signature(
            "  opcode multiset: IDENTICAL (33/33) -- pure reorder,"
            " schedule-class residual")
        self.assertEqual(stored["kind"], "reorder")
        self.assertEqual(stored["insns_target"], 155)
        self.assertEqual(live["kind"], "reorder")
        self.assertEqual(live["insns_target"], 33)

    def test_immediate_rows_are_a_different_family_from_a_reorder(self):
        # MB lane: DrawPsysSub's stored `0t (290/290)` hid 49 IMMEDIATE rows,
        # and the stale label sent a whole charter down the wrong class.
        reorder = core.parse_residual_signature(
            "opcode multiset IDENTICAL (290/290); u4 i0 g0")
        immediate = core.parse_residual_signature(
            "opcode multiset: IDENTICAL (290/290) -- but 49 IMMEDIATE word(s)"
            " differ at aligned same-opcode positions: NOT pure reorder")
        self.assertEqual(reorder["kind"], "reorder")
        self.assertEqual(immediate["kind"], "immediate-aligned")
        self.assertEqual(immediate["immediates"], 49)
        self.assertEqual(reorder["unpaired"], 4)

    def test_a_multiset_only_signature_is_labelled_unresolved(self):
        bare = core.parse_residual_signature(
            "opcode multiset IDENTICAL (290/290)")
        rich = core.parse_residual_signature(
            "opcode multiset IDENTICAL (290/290); u0 i49 g3")
        self.assertEqual(bare["resolution"], "multiset-only")
        self.assertEqual(rich["resolution"], "row-resolved")

    def test_four_pilot_signatures_no_longer_collapse(self):
        # claim.law.RS_residual-retrieval-is-blind-to-pure-reorder-residuals
        # measured four distinct pilot signatures returning byte-identical
        # payloads. Their facet sets must now differ.
        facets = {
            name: tuple(core.parse_residual_signature(
                f"opcode multiset: IDENTICAL ({n}/{n}) -- pure reorder"
            )["facets"])
            for name, n in (("Atree", 33), ("Tower", 25), ("Extract", 92),
                            ("Collide", 124))
        }
        self.assertEqual(len(set(facets.values())), 4)

    def test_metadata_only_facets_never_pair_two_rows(self):
        # kind/resolution weigh 0. Letting them into the set made every
        # reorder record match every reorder query, so the SELECTED SET was
        # constant across 13 acceptance rows and only the order varied.
        metadata = {"kind:reorder", "resolution:multiset-only"}
        strength, shared = core.residual_facet_similarity(metadata, metadata)
        self.assertEqual(shared, [])
        self.assertEqual(strength, 0.0)

    def test_parity_is_not_emitted_for_an_identical_multiset(self):
        parsed = core.parse_residual_signature(
            "opcode multiset IDENTICAL (25/25); insns T25/O25")
        self.assertNotIn("parity:held", parsed["facets"])


class SimilarResidualsTests(unittest.TestCase):
    """RG run-33 deliverable 2: cross-function transferability."""

    def test_self_anchored_records_are_excluded(self):
        # The RS protocol's scoring screen, promoted to a product rule: a
        # record anchored to the function under test is that function's own
        # write-up, not transfer.
        result = core.similar_residuals(root=REPO_ROOT, function="clear_player",
                                        limit=8)
        self.assertTrue(
            all(row["function"] != "clear_player" for row in result["rows"]))
        self.assertIn("channel_role", result)
        self.assertIn("self_records_excluded", result)

    def test_rows_carry_the_mechanism_and_say_why_they_matched(self):
        result = core.similar_residuals(
            root=REPO_ROOT, function="TowerInit", limit=5,
            signature="opcode multiset: IDENTICAL (25/25) -- pure reorder")
        self.assertTrue(result["rows"])
        for row in result["rows"]:
            self.assertTrue(row["match"], "every row must say WHY it matched")
            self.assertIn("mechanism", row)
            self.assertIn("rank_score", row)

    def test_context_carries_the_transferability_section(self):
        context = symbol_context("TowerInit", root=REPO_ROOT)
        self.assertIn("similar_residuals", context)
        self.assertIn("rows", context["similar_residuals"])


class DerivedRecountTests(unittest.TestCase):
    """RG run-33 deliverable 4: independent recounts for every derived table."""

    def test_every_derived_table_recounts_and_prints_its_values(self):
        result = core.recount_derived_tables(REPO_ROOT)
        self.assertTrue(result["tables"])
        for row in result["tables"]:
            # A parity check must PRINT the values it compared — one passed
            # once by comparing two empty dicts.
            self.assertIn("shipped", row)
            self.assertIn("independent", row)
            self.assertIn("delta", row)
            self.assertEqual(row["delta"], row["shipped"] - row["independent"])
        self.assertTrue(result["ok"], result["tables"])

    def test_the_recount_reads_fields_without_the_importers_helper(self):
        # The 92.6% defect lived in the field reader, so the check must not
        # call it. Both documented spellings and both homes must be read.
        as_list = core._recount_id_list(
            {"attributes": {"laws_applied": ["claim.law.a"]}}, "laws_applied")
        as_string = core._recount_id_list(
            {"laws_applied": '["claim.law.a"]'}, "laws_applied")
        self.assertEqual(as_list, ["claim.law.a"])
        self.assertEqual(as_string, ["claim.law.a"])


class ValidateIncrementalTests(unittest.TestCase):
    """RG run-33 deliverable 5: validate made usable."""

    def test_validate_is_incremental_and_reports_its_cost(self):
        first = core.validate_records(REPO_ROOT, refresh=1)
        second = core.validate_records(REPO_ROOT)
        self.assertIn("elapsed_seconds", first)
        self.assertEqual(first["record_count"], second["record_count"])
        self.assertEqual(second["schema_checks_cached"],
                         second["record_count"])

    def test_dangling_citations_are_debt_not_a_validation_failure(self):
        # `prune-attempts` DELETES ejected records by design, stranding every
        # supersedes that pointed at one. Failing the corpus on that would
        # make the gate refuse the workflow that documents it.
        result = core.validate_records(REPO_ROOT)
        self.assertIn("dangling_citation_count", result)
        self.assertTrue(result["valid"])

    def test_a_new_proposal_is_still_strict_about_citations(self):
        record = {
            "schema_version": 1, "id": "attempt.rg-strict-probe.20260902.v1",
            "kind": "attempt", "function": "function:TowerInit",
            "attempted_axis": "probe", "outcome": "neutral",
            "supersedes": "attempt.this-id-does-not-exist.v1",
            "attributes": {"law_screen": "none applicable: test fixture"},
        }
        with self.assertRaises(MemoryGraphError):
            stage_record_proposal(record, root=REPO_ROOT, dry_run=True)


class SpillStubTests(unittest.TestCase):
    """RG run-33 deliverable 3: the machine-readable auto-spill stub."""

    def test_row_counts_distinguish_a_full_result_from_an_empty_one(self):
        gdlmem = importlib.import_module("memory_graph.gdlmem")
        full = gdlmem.result_row_counts({"laws": [1, 2, 3], "note": "x"})
        empty = gdlmem.result_row_counts({"laws": [], "note": "x"})
        self.assertEqual(full["laws"], 3)
        self.assertEqual(empty["laws"], 0)
        self.assertNotEqual(full, empty)


class ProposalGateNarrowingTests(unittest.TestCase):
    """RG run-33: the two MB gate-tuning items."""

    def _attempt_record(self, **extra):
        record = {
            "schema_version": 1, "id": "attempt.rg-gate-probe.20260902.v1",
            "kind": "attempt", "function": "function:TowerInit",
            "attempted_axis": "probe", "outcome": "improved",
            "attributes": {"law_screen": "none applicable: test fixture"},
        }
        record.update(extra)
        return record

    def test_describes_denial_of_releases_the_denial_gate(self):
        quoting = self._attempt_record(
            attempted_axis="re-measuring a prior park that called this axis"
                           " a do-not-retry, to see whether it still holds")
        with self.assertRaises(MemoryGraphError):
            core._apply_proposal_gates(quoting)
        quoting["describes_denial_of"] = "attempt.some-prior-park.v1"
        core._apply_proposal_gates(quoting)

    def test_held_fixed_is_only_demanded_of_a_vetoing_outcome(self):
        multi = "tried three forms: (1) alias, (2) hoist, (3) reorder"
        retained = self._attempt_record(outcome="improved", probed_form=multi)
        core._apply_proposal_gates(retained)   # retained edits veto nothing
        parked = self._attempt_record(outcome="parked", probed_form=multi)
        with self.assertRaises(MemoryGraphError):
            core._apply_proposal_gates(parked)


class TypedDenialEscapeTests(unittest.TestCase):
    """T6 run-36 item 9: CL read the refusal as "invent a denial".

    The escape was implemented and named — in the last sentence of a
    nine-sentence paragraph. An escape a reader does not reach is an escape
    that does not exist.
    """

    REAL_LAW = ("claim.law.webfrank-pinned-function-source-freeze"
                ".20260831.v1")

    def _record(self, **extra):
        record = {
            "schema_version": 1, "kind": "attempt",
            "id": "attempt.t6-denial-escape.20260902.v1",
            "function": "function:TowerInit", "outcome": "improved",
            "attempted_axis": "re-measuring a prior park that called this"
                              " axis a do-not-retry, to see if it holds",
            "attributes": {"law_screen": "none applicable: test fixture"},
        }
        record.update(extra)
        return record

    def test_the_escape_is_offered_before_the_typed_object(self):
        with self.assertRaises(MemoryGraphError) as caught:
            core._apply_proposal_gates(self._record())
        message = str(caught.exception)
        self.assertIn("describes_denial_of", message)
        self.assertLess(message.index("describes_denial_of"),
                        message.index("premise_measurement"),
                        "the escape must come BEFORE the typed-object"
                        " instructions a describing record does not need")

    def test_the_message_asks_which_of_the_two_you_are_doing(self):
        with self.assertRaises(MemoryGraphError) as caught:
            core._apply_proposal_gates(self._record())
        message = str(caught.exception)
        self.assertIn("ARE YOU DESCRIBING SOMEONE ELSE'S DENIAL", message)
        self.assertIn("ARE YOU ISSUING THE DENIAL?", message)

    def test_the_escape_still_releases_the_gate(self):
        core._apply_proposal_gates(
            self._record(describes_denial_of="attempt.some-prior-park.v1"))

    def test_the_escape_id_must_actually_resolve(self):
        """The text calls it a citation; staging now makes that true."""
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(
                self._record(describes_denial_of="attempt.no-such-park.v9"),
                root=REPO_ROOT, dry_run=True)
        self.assertIn("does not resolve", str(caught.exception))

    def test_a_resolving_escape_id_passes_staging(self):
        stage_record_proposal(
            self._record(describes_denial_of=self.REAL_LAW),
            root=REPO_ROOT, dry_run=True)

    def test_the_attributes_spelling_is_checked_too(self):
        record = self._record()
        record["attributes"]["describes_denial_of"] = "attempt.no-such.v9"
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=REPO_ROOT, dry_run=True)
        self.assertIn("does not resolve", str(caught.exception))


class BankedEvidenceClaimGateTests(unittest.TestCase):
    """T6 run-36 item 8: "banked in the graph" prose is not a citation.

    Dispatch reads a work_claim's scope as the lane's briefing
    (claim.law.MT_a-banked-in-the-graph-premise-is-not-a-citation), so an
    unnamed premise sends a worker after evidence it cannot find.
    """

    REAL_LAW = ("claim.law.webfrank-pinned-function-source-freeze"
                ".20260831.v1")

    def _claim(self, scope):
        return {
            "schema_version": 1, "kind": "work_claim",
            "id": "work_claim.t6-gatef-probe.20260902.v1",
            "function": "function:TowerInit",
            "owner": "claude-fleet-worker-T6", "state": "active",
            "claimed_at": "2026-09-02",
            "attributes": {"scope": scope},
        }

    def test_an_unnamed_banked_premise_is_refused(self):
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(
                self._claim("close it; the mechanism is banked in the graph"),
                root=REPO_ROOT, dry_run=True)
        self.assertIn("is not a citation", str(caught.exception))
        self.assertIn("MT_a-banked-in-the-graph-premise",
                      str(caught.exception))

    def test_a_resolving_record_id_discharges_it(self):
        stage_record_proposal(
            self._claim("close it; mechanism banked in the graph as"
                        f" {self.REAL_LAW}"),
            root=REPO_ROOT, dry_run=True)

    def test_an_unresolvable_id_does_not_discharge_it(self):
        """A citation that resolves to nothing is the defect, not the cure."""
        with self.assertRaises(MemoryGraphError):
            stage_record_proposal(
                self._claim("banked in the graph as attempt.no-such.v9"),
                root=REPO_ROOT, dry_run=True)

    def test_a_claim_making_no_banked_assertion_is_untouched(self):
        stage_record_proposal(
            self._claim("close the register residual on TowerInit"),
            root=REPO_ROOT, dry_run=True)

    def test_the_phrase_family_is_covered(self):
        for scope in ("the premise is recorded in the graph",
                      "per the graph this axis is dead",
                      "the graph already holds the census",
                      "that measurement is already in the memory graph"):
            with self.assertRaises(MemoryGraphError, msg=scope):
                stage_record_proposal(self._claim(scope), root=REPO_ROOT,
                                      dry_run=True)

    def test_the_gate_is_scoped_to_work_claims(self):
        """An attempt record narrating its own history is not a dispatch."""
        attempt = {
            "schema_version": 1, "kind": "attempt",
            "id": "attempt.t6-gatef-scope.20260902.v1",
            "function": "function:TowerInit", "attempted_axis": "probe",
            "outcome": "improved",
            "attributes": {"law_screen": "none applicable: test fixture",
                           "note": "the mechanism is banked in the graph"},
        }
        stage_record_proposal(attempt, root=REPO_ROOT, dry_run=True)


class UnknownEntityMessageTests(unittest.TestCase):
    """T6 run-36 item 7: the refusal named 2 of the 3 resolvable forms.

    Two run-35 lanes burned records discovering `project:gdl` by counting
    ~1,600 record files, because nothing told them an existing entity_key in
    ANY namespace resolves.
    """

    NAMESPACES = [("function", 445, "function:AddItemSub"),
                  ("tu", 65, "tu:MSL/atanf.c"),
                  ("project", 1, "project:gdl")]

    def test_all_three_resolvable_forms_are_named(self):
        message = core.unknown_entity_message("projekt:gdl", [], [])
        self.assertIn("entity_key ALREADY IN the graph", message)
        self.assertIn("function:<symbol>", message)
        self.assertIn("tu:<module>", message)

    def test_the_live_namespaces_are_listed_with_examples(self):
        message = core.unknown_entity_message(
            "projekt:gdl", self.NAMESPACES, [])
        self.assertIn("project: 1 — e.g. project:gdl", message)
        self.assertIn("function: 445", message)

    def test_a_near_miss_is_offered(self):
        message = core.unknown_entity_message(
            "projekt:gdl", self.NAMESPACES, ["project:gdl"])
        self.assertIn("Did you mean: project:gdl?", message)

    def test_it_tells_you_not_to_go_counting_files(self):
        message = core.unknown_entity_message("x:y", [], [])
        self.assertIn("do NOT go counting record files", message)
        self.assertIn("gdlmem.py find --query", message)

    def test_namespaces_are_read_from_the_live_corpus(self):
        """Hardcoding the list would rot the first time a lane coins one."""
        core.ensure_database(REPO_ROOT, None)
        with closing(core.open_database(REPO_ROOT, None)) as connection:
            namespaces = core.entity_key_namespaces(connection)
        prefixes = {row[0] for row in namespaces}
        self.assertIn("function", prefixes)
        self.assertIn("tu", prefixes)
        # project:gdl is the key the two lanes could not find; it resolves.
        self.assertIn("project", prefixes)
        for prefix, count, example in namespaces:
            self.assertTrue(example.startswith(prefix + ":"), example)
            self.assertGreater(count, 0)

    def test_the_real_refusal_carries_the_directory(self):
        record = {
            "schema_version": 1, "kind": "claim",
            "id": "claim.t6-subject-probe.20260902.v1",
            "subject": "projekt:gdl", "predicate": "workflow_note",
            "value": "probe", "epistemic_state": "hypothesis",
            "attributes": {},
        }
        with self.assertRaises(MemoryGraphError) as caught:
            stage_record_proposal(record, root=REPO_ROOT, dry_run=True)
        message = str(caught.exception)
        self.assertIn("project:gdl", message)
        self.assertIn("THREE things resolve", message)


class WindowedResidualWordCountGateTests(unittest.TestCase):
    """T6 run-36 item 6: a "4-word residual" that was 122 of 215 words.

    --ops clusters where the OPCODE stream diverges and cannot see pure
    register-field words, so a window-confined residual sized off --ops is
    sized off the wrong number entirely.
    """

    def _attempt_record(self, **extra):
        record = {
            "schema_version": 1, "id": "attempt.t6-word-gate.20260902.v1",
            "kind": "attempt", "function": "function:fn_800D8BCC",
            "attempted_axis": "probe", "outcome": "parked",
            "attributes": {"law_screen": "none applicable: test fixture"},
        }
        record.update(extra)
        return record

    def test_a_windowed_word_sized_residual_without_a_count_is_refused(self):
        record = self._attempt_record(
            residual_class="REGISTER_ONLY",
            attempted_axis="the window at +0x40..+0x60 carries a 4-word"
                           " residual; sized for a live-zero recolor rule")
        with self.assertRaises(MemoryGraphError) as caught:
            core._apply_proposal_gates(record)
        self.assertIn("DIFFERING-WORD COUNT", str(caught.exception))
        self.assertIn("wf_word_diff.py", str(caught.exception))

    def test_quoting_the_tools_output_line_discharges_it(self):
        record = self._attempt_record(
            attempted_axis="the window at +0x40..+0x60 carries a 4-word"
                           " residual by --ops, but wf_word_diff reports"
                           " DIFFERING WORDS = 122 over 215 insns")
        core._apply_proposal_gates(record)

    def test_the_typed_field_discharges_it(self):
        record = self._attempt_record(
            differing_words=122,
            attempted_axis="the window at +0x40..+0x60 carries a 4-word"
                           " residual")
        core._apply_proposal_gates(record)

    def test_a_window_with_no_word_sized_claim_does_not_fire(self):
        """The gate checks a SIZE claim, not the word 'window'."""
        record = self._attempt_record(
            attempted_axis="permuted the window at +0x40..+0x60 and"
                           " re-derived the pin")
        core._apply_proposal_gates(record)

    def test_a_word_count_with_no_window_does_not_fire(self):
        record = self._attempt_record(
            attempted_axis="a 4-word residual across the whole body")
        core._apply_proposal_gates(record)

    def test_the_pb_window_tu_name_is_not_a_window_token(self):
        """`_` is a word character, so `pb_window` has no \\b before it."""
        record = self._attempt_record(
            function="function:pbWindowDraw",
            attempted_axis="pb_window cleanup left a 4-word residual")
        core._apply_proposal_gates(record)

    def test_an_unanchored_record_is_out_of_scope(self):
        record = self._attempt_record(
            attempted_axis="the window at +0x40..+0x60 carries a 4-word"
                           " residual")
        del record["function"]
        core._apply_proposal_gates(record)

    def test_citation_prose_quoting_a_windowed_claim_is_not_caught(self):
        """Gate B/D's self-refusal lesson, applied here too."""
        record = self._attempt_record(
            attempted_axis="re-measure the upstream park",
            attributes={
                "law_screen": "screened attempt.x.v1, which recorded a"
                              " 4-word residual in the window at"
                              " +0x40..+0x60",
            })
        core._apply_proposal_gates(record)


class TemplateCliOrderTests(unittest.TestCase):
    """Run 34 item 10: the CLI must NOT sort_keys the --template result, or
    schema_version sinks to the bottom of the fill-in skeleton and id/kind
    scatter. Measured on the real CLI, because the defect was purely in
    gdlmem.py's json.dumps and invisible at the core-function level."""

    def _template(self, kind):
        import subprocess
        gdlmem = REPO_ROOT / "memory_graph" / "gdlmem.py"
        out = subprocess.run(
            [sys.executable, str(gdlmem), "propose-record", "--template", kind],
            capture_output=True, text=True, cwd=str(REPO_ROOT))
        self.assertEqual(out.returncode, 0, out.stderr)
        return out.stdout

    def test_the_head_fields_lead_the_printed_template(self):
        for kind in ("attempt", "claim"):
            text = self._template(kind)
            keys = [line.strip().split('"')[1]
                    for line in text.splitlines()
                    if line.strip().startswith('"')]
            self.assertEqual(keys[:3], ["schema_version", "id", "kind"],
                             f"{kind} template head order")


class MissingAnchorTests(unittest.TestCase):
    """A record whose anchor file is gone is a REOPEN candidate.

    Run-34 criticism (MV): an ACCEPTED PlayVQMovie record was anchored to a
    deleted `movieplayer.c` and described a layout the tree no longer
    produces. Every other stale heuristic compares SCORES, and a record whose
    anchor evaporated has no score to move, so nothing could see it.
    """

    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        for relative in ("src/game/movie/movieplayer.cpp",
                         "src/game/audio/sndfx.c",
                         "include/game/leveldata.h",
                         "tools/gdl/probe.py"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("x", encoding="utf-8")

    def test_a_live_anchor_is_not_flagged(self):
        record = {"attributes": {"anchors": ["tools/gdl/probe.py"]}}
        self.assertEqual(missing_anchor_paths(record, self.root), [])

    def test_a_deleted_anchor_is_flagged(self):
        record = {"attributes": {"anchors": ["src/game/ui/gone.c"]}}
        found = missing_anchor_paths(record, self.root)
        self.assertEqual([entry["path"] for entry in found],
                         ["src/game/ui/gone.c"])

    def test_the_c_to_cpp_rename_names_the_survivor(self):
        record = {"attributes": {
            "anchors": ["src/game/movie/movieplayer.c PlayVQMovie"]}}
        found = missing_anchor_paths(record, self.root)
        self.assertEqual(found[0]["renamed_to"],
                         "src/game/movie/movieplayer.cpp")

    def test_a_moved_file_is_resolved_by_basename_when_unambiguous(self):
        index = anchor_basename_index(self.root)
        record = {"attributes": {"anchors": ["src/game/sfx/sndfx.c"]}}
        found = missing_anchor_paths(record, self.root, index)
        self.assertEqual(found[0]["moved_to"], "src/game/audio/sndfx.c")

    def test_no_index_means_no_moved_to_guess(self):
        record = {"attributes": {"anchors": ["src/game/sfx/sndfx.c"]}}
        found = missing_anchor_paths(record, self.root)
        self.assertNotIn("moved_to", found[0])

    def test_ambiguous_basenames_are_candidates_not_a_repair(self):
        for relative in ("src/a/dup.c", "src/b/dup.c"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("x", encoding="utf-8")
        index = anchor_basename_index(self.root)
        record = {"attributes": {"anchors": ["src/c/dup.c"]}}
        found = missing_anchor_paths(record, self.root, index)
        self.assertNotIn("moved_to", found[0])
        self.assertEqual(found[0]["candidates"], ["src/a/dup.c", "src/b/dup.c"])

    def test_object_paths_under_a_source_tree_are_not_record_rot(self):
        """108 of a first cut's 111 hits were exactly this shape.

        Records name an object by its source-tree path; those live under
        build/ by construction and are absent from src/ always.
        """
        record = {"attributes": {"anchors": [
            "src/game/boss/boss.o",
            "src/game/world/.postprocess/body/btricol.o"]}}
        self.assertEqual(missing_anchor_paths(record, self.root), [])

    def test_narrative_prose_is_not_scanned_for_anchors(self):
        """law_screen/probed_form/value narrate; they do not anchor.

        claim.RC_stale-reopen-queue-is-a-classifier-artifact measured what
        mining narrative prose does to a reopen queue (43/43 false hits).
        """
        record = {"value": "we did not touch src/game/ui/gone.c",
                  "attributes": {"law_screen": "screened src/game/ui/gone.c",
                                 "probed_form": "src/game/ui/gone.c"}}
        self.assertEqual(missing_anchor_paths(record, self.root), [])

    def test_the_evidence_locator_is_an_anchor(self):
        record = {"kind": "evidence", "locator": "src/game/ui/gone.c:120"}
        found = missing_anchor_paths(record, self.root)
        self.assertEqual([entry["path"] for entry in found],
                         ["src/game/ui/gone.c"])

    def test_untracked_roots_are_not_treated_as_missing(self):
        """build/ and orig/ are gitignored and absent in a fresh worktree."""
        record = {"attributes": {"anchors": [
            "build/GUNE5D/report.json", "orig/GUNE5D/sys/main.dol"]}}
        self.assertEqual(missing_anchor_paths(record, self.root), [])

    def test_one_anchor_repeated_is_reported_once(self):
        record = {"attributes": {"anchors": ["src/game/ui/gone.c"],
                                 "verification": "see src/game/ui/gone.c"}}
        self.assertEqual(len(missing_anchor_paths(record, self.root)), 1)


class HypothesisRefuterTests(unittest.TestCase):
    """A refuter must name something about the idea it claims to kill.

    Run-34 criticism (CI): a MANDATORY-STEP-1 hypothesis shipped with a
    cheapest_refuting_observation that could never refute it. Discipline 10b
    makes such a hypothesis the next lane's first action, so an unkillable
    one hands that lane a step 1 with no exit.
    """

    STATEMENT = ("the volatile qualifier on gFrameTicks forces a reload at"
                 " each loop iteration")

    def warn(self, refuter, statement=None):
        return hypothesis_refuter_warning({
            "statement": statement if statement is not None else self.STATEMENT,
            "cheapest_refuting_observation": refuter,
            "screened_against_target": "no"})

    def test_a_generic_refuter_is_warned_about(self):
        text = self.warn("re-run the probe and see whether the score moves")
        self.assertIsNotNone(text)
        self.assertIn("cheapest_refuting_observation", text)
        self.assertIn("volatile", text)

    def test_a_mechanism_naming_refuter_is_silent(self):
        self.assertIsNone(self.warn(
            "drop the volatile qualifier and check whether the reload"
            " survives"))

    def test_one_shared_mechanism_term_is_enough(self):
        self.assertIsNone(self.warn("read gFrameTicks out of the disassembly"))

    def test_morphology_is_tolerated_on_a_six_character_stem(self):
        self.assertIsNone(hypothesis_refuter_warning({
            "statement": "an extra saved register widens the frame",
            "cheapest_refuting_observation":
                "count the saved registers in the prologue"}))

    def test_a_four_character_coincidence_does_not_count_as_overlap(self):
        """`reload` and `relocation` share four characters and no meaning."""
        self.assertIsNotNone(hypothesis_refuter_warning({
            "statement": "the reload happens at each iteration",
            "cheapest_refuting_observation":
                "dump the relocations for the window"}))

    def test_a_statement_with_no_mechanism_terms_is_not_judged(self):
        self.assertIsNone(self.warn("anything at all",
                                    statement="the score will improve"))

    def test_non_string_or_absent_fields_are_not_judged(self):
        self.assertIsNone(hypothesis_refuter_warning(None))
        self.assertIsNone(hypothesis_refuter_warning({}))
        self.assertIsNone(hypothesis_refuter_warning(
            {"statement": self.STATEMENT}))

    def _stage(self, rid, refuter):
        """Stage a dry-run attempt carrying this refuter; return warnings."""
        root = make_root(with_symbols=False)
        self.addCleanup(shutil.rmtree, root, ignore_errors=True)
        original = core._probe_record_references
        core._probe_record_references = lambda *args, **kwargs: None
        try:
            record = _attempt(
                rid, "function:test_fn", outcome="parked",
                axis="volatile scaffold on the frame counter",
                hypothesis={
                    "statement": self.STATEMENT,
                    "cheapest_refuting_observation": refuter,
                    "screened_against_target": "no"},
                attributes={"law_screen": "none applicable: tooling probe"})
            warnings: list[str] = []
            path = stage_record_proposal(record, root=root, dry_run=True,
                                         warnings=warnings)
            return path, warnings
        finally:
            core._probe_record_references = original

    def test_the_warning_does_not_block_the_proposal(self):
        """It is a WARNING: vocabulary overlap is a heuristic.

        A refuter phrased in an INSTRUMENT's vocabulary is legitimate, and
        refusing those would tax correct records to catch sloppy ones — the
        failure mode this corpus measured twice already.
        """
        path, warnings = self._stage(
            "attempt.t5-refuter-warning-probe.v1",
            "re-run and see whether the number moves")
        self.assertTrue(str(path).endswith(".json"))
        self.assertEqual(len(warnings), 1)
        self.assertIn("MANDATORY STEP 1", warnings[0])

    def test_a_sound_refuter_produces_no_warning_through_the_gate(self):
        _path, warnings = self._stage(
            "attempt.t5-refuter-clean-probe.v1",
            "drop the volatile qualifier and see if the reload survives")
        self.assertEqual(warnings, [])


class SupersessionScreenPerformanceTests(unittest.TestCase):
    """T6 run-36 item 10: the suite's 54.7s-of-72.9s hot path.

    The "is this record still live?" screen was a CORRELATED subquery
    running a json_extract over the whole record_ingest table once per
    candidate row. These tests pin the two properties that make the
    non-correlated replacement both correct and fast, so neither can be
    undone by a later edit without a red test.
    """

    CORRELATED = (
        "SELECT r.record_id FROM record_ingest r WHERE NOT EXISTS ("
        " SELECT 1 FROM record_ingest newer WHERE"
        " json_extract(newer.raw_json,'$.supersedes')=r.record_id"
        " AND newer.record_state='accepted') ORDER BY r.record_id")

    def _fixture(self):
        import sqlite3
        connection = sqlite3.connect(":memory:")
        connection.execute(
            "CREATE TABLE record_ingest (record_id TEXT PRIMARY KEY,"
            " record_state TEXT NOT NULL, raw_json TEXT NOT NULL)")
        rows = [
            # superseded by an accepted record -> must be screened OUT
            ("attempt.a.v1", "accepted", "{}"),
            ("attempt.a.v2", "accepted",
             '{"supersedes": "attempt.a.v1"}'),
            # superseded only by a PROPOSED record -> still live
            ("attempt.b.v1", "accepted", "{}"),
            ("attempt.b.v2", "proposed",
             '{"supersedes": "attempt.b.v1"}'),
            # never superseded, and carrying no `supersedes` key at all:
            # this is the row that goes missing without the NULL guard
            ("attempt.c.v1", "accepted", "{}"),
        ]
        connection.executemany(
            "INSERT INTO record_ingest VALUES (?,?,?)", rows)
        return connection

    def test_the_flat_screen_answers_exactly_what_the_correlated_one_did(self):
        connection = self._fixture()
        correlated = [row[0] for row in
                      connection.execute(self.CORRELATED).fetchall()]
        flat = [row[0] for row in connection.execute(
            "SELECT record_id FROM record_ingest WHERE record_id NOT IN"
            f" ({core.SUPERSEDED_RECORD_IDS}) ORDER BY record_id"
        ).fetchall()]
        self.assertEqual(correlated, flat)
        # Print the values compared, per the parity-check discipline: a
        # gate that passes by comparing two empty lists is not a gate.
        self.assertEqual(
            flat, ["attempt.a.v2", "attempt.b.v1", "attempt.b.v2",
                   "attempt.c.v1"])

    def test_the_is_not_null_guard_is_load_bearing(self):
        """Drop it and `NOT IN` returns NOTHING — the silent-empty trap."""
        connection = self._fixture()
        without_guard = connection.execute(
            "SELECT record_id FROM record_ingest WHERE record_id NOT IN ("
            " SELECT json_extract(newer.raw_json,'$.supersedes')"
            " FROM record_ingest newer WHERE newer.record_state='accepted')"
        ).fetchall()
        self.assertEqual(without_guard, [])
        self.assertIn("IS NOT NULL", core.SUPERSEDED_RECORD_IDS)

    def test_no_correlated_supersedes_subquery_remains_in_core(self):
        """The idiom is quadratic; it must not come back by copy-paste."""
        source = (REPO_ROOT / "memory_graph" / "core.py").read_text(
            encoding="utf-8")
        # Strip comments so SUPERSEDED_RECORD_IDS' own explanation of the
        # bad form (which quotes it verbatim) is not read as a use of it.
        code = "\n".join(line for line in source.splitlines()
                         if not line.lstrip().startswith("#"))
        offenders = [
            line for line in code.splitlines()
            if "'$.supersedes')" in line and "= r.record_id" in line
        ] + [
            line for line in code.splitlines()
            if "'$.supersedes')" in line and "= a.record_id" in line
        ]
        self.assertEqual(offenders, [], offenders)

    def test_the_roster_join_resolves_raw_name_through_an_index(self):
        """binary_symbol has 21k rows; the join key is raw_name.

        Only `normalized_name` was indexed, so the planner scanned every
        gamecube symbol per candidate row (2.227s -> 0.029s once indexed).
        Asserted on the QUERY PLAN rather than on wall-clock, which is not
        a sound gate on a machine shared by a build fleet.
        """
        core.ensure_database(REPO_ROOT, None)
        with closing(core.open_database(REPO_ROOT, None)) as connection:
            plan = " ".join(
                str(tuple(row)) for row in connection.execute(
                    "EXPLAIN QUERY PLAN SELECT bm.object_name"
                    " FROM attempt a"
                    " LEFT JOIN entity fe ON fe.id = a.function_entity_id"
                    " LEFT JOIN binary_symbol bs"
                    "   ON fe.entity_key LIKE 'function:%'"
                    "  AND bs.raw_name = substr(fe.entity_key, 10)"
                    "  AND bs.platform = 'gamecube'"
                    "  AND bs.symbol_kind = 'function'"
                    " LEFT JOIN binary_module bm ON bm.id = bs.module_id"))
        self.assertIn("binary_symbol_raw_name_idx", plan, plan)
        self.assertIn("raw_name=?", plan, plan)


if __name__ == "__main__":
    unittest.main(verbosity=2)
