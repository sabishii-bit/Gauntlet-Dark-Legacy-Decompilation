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
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from memory_graph import core
from memory_graph.core import (
    ATTEMPT_BYTE_CAP,
    MemoryGraphError,
    _record_age_days,
    _validate_record,
    accept_records,
    attempt_staleness,
    build_database,
    fakematch_debt,
    find_records,
    law_corpus,
    prune_attempts,
    record_lookup,
    stage_record_proposal,
    tu_briefing,
    work_claims,
)

TODAY = "2026-08-30"


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
        laws = {row["id"]: row for row in result["laws"]}
        self.assertIn("claim.law.test-law.v1", laws)
        self.assertIn("claim.law.test-law.v2", laws)
        self.assertEqual(laws["claim.law.test-law.v1"]["superseded_by"],
                         "claim.law.test-law.v2")
        self.assertIsNone(laws["claim.law.test-law.v2"]["superseded_by"])
        self.assertEqual(laws["claim.law.test-law.v2"]["applied_count"], 1)
        self.assertEqual(laws["claim.law.test-law.v1"]["applied_count"], 0)
        # Fixture stamps a local date; age is computed against UTC now, so
        # allow the one-day skew a date boundary introduces.
        self.assertLessEqual(laws["claim.law.test-law.v2"]["age_days"], 1)
        filtered = law_corpus("scope v1", root=self.root)
        self.assertEqual([row["id"] for row in filtered["laws"]],
                         ["claim.law.test-law.v1"])

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
        self.assertEqual(reasons.get("attempt.formless.v1"),
                         "failing_form_undocumented")
        # scheduler/regalloc parks are NOT conversion re-try candidates
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
        self.assertEqual(brief["raw_offset_debt"][0]["total"], 10)
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
