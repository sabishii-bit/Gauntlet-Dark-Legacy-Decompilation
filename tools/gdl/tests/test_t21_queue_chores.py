#!/usr/bin/env python3
"""T21 run-51 item 9: three chores, each reproduced before it was changed.

(9a) `t15_promotion_queue.py --out` emitted `{"rules": [...]}` and nothing
else. Reproduced at f672034a6:

    top-level keys: ['rules']
      rules: list 154
         row keys: ['atoms', 'attempt_records', 'classification', 'function',
                    'insns', 'probed_form_records', 'proof', 'provenance',
                    'stage_count', 'stages', 'unit', 'words']
    attempt_records: 3          <- an int, under a name that reads as a list
    probed_form_records: 2         of record ids. Two joins were written
                                   against them before the shape was noticed.

Renaming the fields would break every consumer, so the file now DESCRIBES
ITSELF: a `schema` block naming the row grain, the join key, and each field's
type — including what `attempt_records` is NOT.

(9b) Six accepted records anchor on `function: "project:gdl"`, and
`find --function` reached them under NEITHER spelling. Reproduced at
f672034a6:

    $ gdlmem.py find --function project:gdl   -> {"count": 0, ...}
    $ gdlmem.py find --function gdl           -> {"count": 0, ...}

MECHANISM: the facet split the argument on ':' and rebuilt it as
`function:gdl`, while the stored entity key is `project:gdl`. The six are
T9, T16, T17, T19, T20 and WF_t17-ordered-datum-rescreen — every previous
TOOL lane's own attempt record, invisible to the facet a tool lane would use
to find them. Three spellings now match; measured over all 572 entities (87
of them non-`function:` prefixed), suffix collisions with a function name
number ZERO, so the widening cannot merge two entities today.

(9c) An `owned_units` entry that names nothing protects nothing, silently: a
unit no claim covers reads as FREE. Run 50: the PR lane's seed TU was
unprotected because its claim said `game/ps2/ml_mem.c` while the file is
`src/game/sys/ml_mem.c` — wrong directory, right basename. Verified at
f672034a6: `src/game/ps2/ml_mem.c` does not exist and `src/game/sys/ml_mem.c`
does. `claimscope.py --audit` now checks every active claim's entries and
offers the basename match; a DIRECTORY PREFIX (`tools/gdl`, `memory_graph`)
is verified as a directory and reported as `prefix`, never as a miss.
Run-51's own six claims audit clean (12 entries, 0 unresolved).
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import claimscope             # noqa: E402
import t15_promotion_queue    # noqa: E402
from memory_graph import core  # noqa: E402


class PromotionQueueSchema(unittest.TestCase):
    def test_the_schema_names_the_row_grain_and_the_join_key(self):
        schema = t15_promotion_queue.OUT_SCHEMA
        self.assertEqual(schema["rows_key"], "rules")
        self.assertEqual(schema["join_key"], ["unit", "function"])
        self.assertIn("ONE WEBFRANK RULE", schema["row_is"])

    def test_the_count_fields_say_they_are_not_id_lists(self):
        fields = t15_promotion_queue.OUT_SCHEMA["fields"]
        for name in ("attempt_records", "probed_form_records"):
            self.assertIn("NOT a list", fields[name])
            self.assertTrue(fields[name].startswith("int"))

    def test_every_emitted_row_field_is_documented(self):
        # The schema cannot drift from the rows without this failing.
        rows = t15_promotion_queue.rule_rows()
        if not rows:
            self.skipTest("no rules in this tree")
        documented = set(t15_promotion_queue.OUT_SCHEMA["fields"])
        self.assertEqual(set(rows[0]) - documented, set())


class FunctionFacetSpellings(unittest.TestCase):
    def test_all_three_spellings_reach_a_project_anchored_record(self):
        for spelling in ("project:gdl", "gdl"):
            result = core.find_records(function=spelling, root=REPO,
                                       limit=50)
            self.assertGreater(result["count"], 0, spelling)

    def test_an_ordinary_function_is_unaffected(self):
        plain = core.find_records(function="do_players", root=REPO, limit=50)
        prefixed = core.find_records(function="function:do_players",
                                     root=REPO, limit=50)
        self.assertEqual(plain["count"], prefixed["count"])
        self.assertGreater(plain["count"], 0)


class CountConsequenceAdvisory(unittest.TestCase):
    """Found while authoring this lane's OWN record: the advisory promises
    that "a `count_consequence` key silences this" and never looked for the
    key — it scanned the concatenated hypothesis VALUES for count vocabulary,
    so the honest answer for a change that compiles nothing warned anyway.

    MEASURED over all 225 hypothesis blocks in records/ and inbox/: 27 carry
    the key, 2 of those warned (this lane's record, and the already-accepted
    attempt.CV_ordered-datum-defect-queue-all-twelve-rows-refuted
    .20260903.v1). The other 163 warnings carry no key and are untouched.
    """

    BASE = {"statement": "reorder the two declarations",
            "cheapest_refuting_observation": "savedregs after the swap",
            "screened_against_target": "no — not built"}

    def test_no_key_and_no_count_vocabulary_still_warns(self):
        self.assertIsNotNone(
            core.hypothesis_count_consequence_warning(dict(self.BASE)))

    def test_an_explicit_key_silences_it_as_the_message_promises(self):
        block = dict(self.BASE, count_consequence="none: nothing compiles")
        self.assertIsNone(core.hypothesis_count_consequence_warning(block))

    def test_an_empty_key_does_not_silence_it(self):
        for value in ("", "   ", None):
            block = dict(self.BASE, count_consequence=value)
            self.assertIsNotNone(
                core.hypothesis_count_consequence_warning(block), repr(value))

    def test_count_vocabulary_in_the_prose_still_silences_it(self):
        block = dict(self.BASE,
                     statement="the cure is count-neutral at T104/O104")
        self.assertIsNone(core.hypothesis_count_consequence_warning(block))


class OwnedUnitsAudit(unittest.TestCase):
    def test_the_run50_defect_is_caught_and_the_right_unit_offered(self):
        claim = {"owner": "claude-fleet-worker-PR",
                 "id": "work_claim.fake.v1",
                 "owned_units": ["game/ps2/ml_mem.c"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["status"], "UNRESOLVED")
        self.assertIn("game/sys/ml_mem", rows[0]["did_you_mean"])

    def test_a_directory_prefix_is_not_a_miss(self):
        claim = {"owner": "t21", "id": "work_claim.fake.v2",
                 "owned_units": ["tools/gdl", "memory_graph"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual({row["status"] for row in rows}, {"prefix"})

    def test_a_real_unit_gets_a_row_too(self):
        # Run-54 item 1: this used to assert NO row, which is what made the
        # --audit header count entries the row list did not. The row list is
        # total now; a resolving unit reports `unit`, not silence.
        claim = {"owner": "t21", "id": "work_claim.fake.v3",
                 "owned_units": ["game/sys/ml_mem", "src/game/enemy/enemy.c"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual([row["status"] for row in rows], ["unit", "unit"])
        self.assertEqual([row["resolves_to"] for row in rows],
                         ["game/sys/ml_mem", "game/enemy/enemy"])

    def test_an_existing_file_is_not_called_a_prefix(self):
        # 12 of 112 historical entries were files reported as `prefix`, both
        # distinct paths belonging to the postprocessor lane's scope.
        claim = {"owner": "t21", "id": "work_claim.fake.v4",
                 "owned_units": ["config/GUNE5D/webfrank.json",
                                 "tools/gdl/webfrank.py"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual({row["status"] for row in rows}, {"file"})

    def test_the_row_list_accounts_for_every_entry(self):
        claim = {"owner": "t21", "id": "work_claim.fake.v5",
                 "owned_units": ["game/sys/ml_mem", "tools/gdl",
                                 "config/GUNE5D/webfrank.json",
                                 "game/ps2/ml_mem"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual(len(rows), len(claim["owned_units"]))
        self.assertEqual([row["status"] for row in rows],
                         ["unit", "prefix", "file", "UNRESOLVED"])

    def test_the_live_claims_audit_clean_and_account(self):
        claims = claimscope.load_claims(REPO)
        rows = claimscope.audit_owned_units(claims, repo=REPO)
        bad = [row for row in rows if row["status"] == "UNRESOLVED"]
        self.assertEqual(bad, [])
        self.assertEqual(len(rows),
                         sum(len(c["owned_units"]) for c in claims))
        self.assertLessEqual({r["status"] for r in rows},
                             set(claimscope.AUDIT_STATUSES))


if __name__ == "__main__":
    unittest.main()
