#!/usr/bin/env python3
"""Pure explicit scope resolution and retired-registry safety checks."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import claimscope  # noqa: E402

MF = {"id": "wc.mf", "owner": "worker-MF", "declared": True,
      "owned_units": ["game/ps2/ml_fmath", "game/anim/atree"]}
NM = {"id": "wc.nm", "owner": "worker-NM", "declared": True,
      "owned_units": ["game/camera/newcam"]}
TOOLS = {"id": "wc.t16", "owner": "worker-T16", "declared": True,
         "owned_units": ["tools/gdl", "config"]}
BLIND = {"id": "wc.bp", "owner": "worker-BP", "declared": False,
         "owned_units": []}


class Normalization(unittest.TestCase):
    def test_every_accepted_spelling_collapses_to_one(self):
        for spelling in ("game/ps2/ml_fmath", "game/ps2/ml_fmath.c",
                         "src/game/ps2/ml_fmath.c", r"src\game\ps2\ml_fmath.c",
                         "./src/game/ps2/ml_fmath.o"):
            self.assertEqual(claimscope.normalize(spelling),
                             "game/ps2/ml_fmath", spelling)

    def test_directory_prefix_covers_files_under_it(self):
        self.assertTrue(claimscope.covers("tools/gdl", "tools/gdl/probe.py"))
        self.assertTrue(claimscope.covers("tools/gdl", "tools/gdl/a/b.py"))

    def test_prefix_matching_is_path_segment_wise(self):
        # the classic bug: "tools/gd" must not cover "tools/gdl/probe.py"
        self.assertFalse(claimscope.covers("tools/gd", "tools/gdl/probe.py"))
        self.assertFalse(claimscope.covers("game/camera/newcam",
                                           "game/camera/newcam2"))


class ForeignEditsAreCaught(unittest.TestCase):
    """POSITIVE side."""

    def test_another_lanes_listed_unit_is_foreign(self):
        v = claimscope.check_unit("game/ps2/ml_fmath.c", lane="worker-NM",
                                  claims=[MF, NM])
        self.assertEqual(v["status"], "foreign")
        self.assertEqual([o["owner"] for o in v["owners"]], ["worker-MF"])

    def test_every_spelling_of_the_unit_is_caught(self):
        for spelling in ("game/anim/atree", "src/game/anim/atree.c",
                         r"game\anim\atree.c"):
            v = claimscope.check_unit(spelling, lane="worker-NM",
                                      claims=[MF, NM])
            self.assertEqual(v["status"], "foreign", spelling)

    def test_directory_claims_protect_their_files(self):
        v = claimscope.check_unit("tools/gdl/probe.py", lane="worker-MF",
                                  claims=[TOOLS])
        self.assertEqual(v["status"], "foreign")


CARVEOUT = {"id": "wc.wv", "owner": "worker-WV", "declared": True,
            "owned_units": ["tools/gdl/webfrank.py"]}
TWIN = {"id": "wc.t17", "owner": "worker-T17", "declared": True,
        "owned_units": ["tools/gdl", "config"]}


class CarveOutsResolveMostSpecificFirst(unittest.TestCase):
    """Run-54 item 8: a file carved out of another lane's directory prefix.

    Both entries `cover` the path, so comparing them only with `covers` made
    BOTH lanes foreign — the screen refused the very owner the carve-out
    exists to name. The longer entry is the narrower grant and decides.
    """

    def test_the_carve_out_owner_may_edit_its_own_file(self):
        v = claimscope.check_unit("tools/gdl/webfrank.py", lane="worker-WV",
                                  claims=[TOOLS, CARVEOUT])
        self.assertEqual(v["status"], "ok")
        self.assertEqual(v["owners"], [])

    def test_the_prefix_owner_is_still_refused_on_the_carved_out_file(self):
        v = claimscope.check_unit("tools/gdl/webfrank.py", lane="worker-T16",
                                  claims=[TOOLS, CARVEOUT])
        self.assertEqual(v["status"], "foreign")
        self.assertEqual([o["owner"] for o in v["owners"]], ["worker-WV"])

    def test_the_prefix_owner_keeps_the_rest_of_the_directory(self):
        v = claimscope.check_unit("tools/gdl/probe.py", lane="worker-T16",
                                  claims=[TOOLS, CARVEOUT])
        self.assertEqual(v["status"], "ok")

    def test_an_exact_tie_is_still_a_collision_for_both(self):
        # NEGATIVE side: two lanes listing the SAME entry are not resolvable
        # by specificity and must both keep reading as foreign.
        for lane in ("worker-T16", "worker-T17"):
            v = claimscope.check_unit("tools/gdl/probe.py", lane=lane,
                                      claims=[TOOLS, TWIN])
            self.assertEqual(v["status"], "foreign", lane)


class OverlapReporting(unittest.TestCase):
    def test_a_nesting_is_reported_as_a_carve_out_not_a_conflict(self):
        out = claimscope.owned_unit_overlaps([TOOLS, CARVEOUT])
        self.assertEqual(out["duplicate"], {})
        self.assertEqual(len(out["nested"]), 1)
        row = out["nested"][0]
        self.assertEqual((row["outer"], row["outer_owner"]),
                         ("tools/gdl", "worker-T16"))
        self.assertEqual((row["inner"], row["inner_owner"]),
                         ("tools/gdl/webfrank.py", "worker-WV"))

    def test_an_exact_duplicate_is_the_conflict(self):
        out = claimscope.owned_unit_overlaps([TOOLS, TWIN])
        self.assertEqual(sorted(out["duplicate"]), ["config",
                                                    "tools/gdl"])
        self.assertEqual(out["nested"], [])

    def test_one_owner_nesting_its_own_entries_is_silent(self):
        self_nested = {"id": "wc.s", "owner": "worker-T16", "declared": True,
                       "owned_units": ["tools/gdl", "tools/gdl/probe.py"]}
        out = claimscope.owned_unit_overlaps([self_nested])
        self.assertEqual(out["duplicate"], {})
        self.assertEqual(out["nested"], [])

    def test_unrelated_entries_produce_nothing(self):
        out = claimscope.owned_unit_overlaps([MF, NM])
        self.assertEqual(out["duplicate"], {})
        self.assertEqual(out["nested"], [])


class OwnAndUnownedEditsAreNotCaught(unittest.TestCase):
    """NEGATIVE side: the half that decides whether this can refuse at all."""

    def test_a_lane_may_edit_its_own_listed_unit(self):
        v = claimscope.check_unit("game/ps2/ml_fmath.c", lane="worker-MF",
                                  claims=[MF, NM])
        self.assertEqual(v["status"], "ok")

    def test_an_unlisted_unit_is_ok_when_every_claim_declares(self):
        v = claimscope.check_unit("game/world/world.c", lane="worker-MF",
                                  claims=[MF, NM])
        self.assertEqual(v["status"], "ok")

    def test_own_listing_beats_another_claims_missing_list(self):
        v = claimscope.check_unit("game/ps2/ml_fmath.c", lane="worker-MF",
                                  claims=[MF, BLIND])
        self.assertEqual(v["status"], "ok")

    def test_scope_prose_never_triggers_this_screen(self):
        # the prose-screen failure this replaces: a scope that NAMES another
        # lane's TUs in order to exclude them was reported as their co-owner
        prose_only = {"id": "wc.x", "owner": "worker-BP", "declared": True,
                      "owned_units": ["game/audio/sndfx"],
                      "scope": "MF owns game/ps2/ml_fmath.c, keep off"}
        v = claimscope.check_unit("game/ps2/ml_fmath.c", lane="worker-MF",
                                  claims=[prose_only])
        self.assertEqual(v["status"], "ok")


class UndecidableIsNotAllClear(unittest.TestCase):
    def test_a_claim_without_a_list_leaves_the_question_open(self):
        v = claimscope.check_unit("game/world/world.c", lane="worker-MF",
                                  claims=[MF, BLIND])
        self.assertEqual(v["status"], "undecidable")
        self.assertEqual(v["claims_without_owned_units"], 1)


class LaneIdentity(unittest.TestCase):
    def test_lane_lock_first_line_wins(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "LANE_LOCK").write_text(
                "worker-ZZ\nnonce=1\n", encoding="utf-8")
            self.assertEqual(claimscope.lane_identity(tmp),
                             ("worker-ZZ", "LANE_LOCK"))

    def test_env_var_is_the_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.environ["GDL_LANE"] = "worker-ENV"
            try:
                self.assertEqual(claimscope.lane_identity(tmp),
                                 ("worker-ENV", "$GDL_LANE"))
            finally:
                del os.environ["GDL_LANE"]


class NoImplicitRegistry(unittest.TestCase):
    def test_missing_registry_is_not_treated_as_unowned(self):
        with tempfile.TemporaryDirectory() as tmp:
            for operation in (
                    lambda: claimscope.load_claims(tmp),
                    lambda: claimscope.check_unit("game/x/y", repo=tmp),
                    lambda: claimscope.owned_unit_overlaps(repo=tmp),
                    lambda: claimscope.webfrank_block_owners(repo=tmp),
                    lambda: claimscope.audit_owned_units(repo=tmp)):
                with self.assertRaisesRegex(RuntimeError, "registry is retired"):
                    operation()

    def test_explicit_empty_scope_is_still_a_pure_supported_input(self):
        verdict = claimscope.check_unit("game/x/y", lane="worker", claims=[])
        self.assertEqual(verdict["status"], "ok")
        self.assertEqual(verdict["active_claims"], 0)

    def test_edit_tools_do_not_run_an_automatic_ownership_screen(self):
        for name in ("probe.py", "defake_gate.py"):
            text = (REPO / "tools" / "gdl" / name).read_text(encoding="utf-8")
            self.assertNotIn("import claimscope", text, name)
            self.assertNotIn("claimscope.warn_or_refuse", text, name)
            self.assertIn("Ownership is coordinated explicitly", text, name)

    def test_old_registry_cli_refuses_instead_of_printing_free(self):
        result = subprocess.run(
            [sys.executable, "tools/gdl/claimscope.py", "--index"],
            cwd=str(REPO), capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("registry is retired", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_local_identity_cli_remains_available(self):
        result = subprocess.run(
            [sys.executable, "tools/gdl/claimscope.py", "--self"],
            cwd=str(REPO), capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertIn("lane", payload)
        self.assertIn("source", payload)


if __name__ == "__main__":
    unittest.main()
