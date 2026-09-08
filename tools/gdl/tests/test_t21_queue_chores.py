#!/usr/bin/env python3
"""Promotion-queue schema and explicitly supplied path-ownership validation."""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import claimscope             # noqa: E402
import t15_promotion_queue    # noqa: E402


class PromotionQueueSchema(unittest.TestCase):
    def test_the_schema_names_the_row_grain_and_the_join_key(self):
        schema = t15_promotion_queue.OUT_SCHEMA
        self.assertEqual(schema["rows_key"], "rules")
        self.assertEqual(schema["join_key"], ["unit", "function"])
        self.assertIn("ONE WEBFRANK RULE", schema["row_is"])



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
                 "owned_units": ["tools/gdl", "include"],
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
                 "owned_units": [".vscode/lint/fakematch_lint.toml",
                                 "tools/gdl/webfrank.py"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual({row["status"] for row in rows}, {"file"})

    def test_the_row_list_accounts_for_every_entry(self):
        claim = {"owner": "t21", "id": "work_claim.fake.v5",
                 "owned_units": ["game/sys/ml_mem", "tools/gdl",
                                 ".vscode/lint/fakematch_lint.toml",
                                 "game/ps2/ml_mem"],
                 "declared": True}
        rows = claimscope.audit_owned_units([claim], repo=REPO)
        self.assertEqual(len(rows), len(claim["owned_units"]))
        self.assertEqual([row["status"] for row in rows],
                         ["unit", "prefix", "file", "UNRESOLVED"])



if __name__ == "__main__":
    unittest.main()
