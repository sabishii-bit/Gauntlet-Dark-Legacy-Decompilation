#!/usr/bin/env python3
"""T16 run-46 item 1: machine-readable claim scopes.

Two-sided throughout. The measured motivation is in the module docstring of
tools/gdl/claimscope.py: the prose screen it replaces fires on 20 of 250 src
units against run-46's six claims and 17 of those (85%) are units no scope
names, while this screen -- fixtured on the same six claims with every list
declared -- fires 4 to 6 times per lane with ZERO false positives and zero
misses.
"""

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
         "owned_units": ["tools/gdl", "memory_graph"]}
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

    def test_undecidable_warns_but_does_not_refuse(self):
        import io
        buf = io.StringIO()
        with tempfile.TemporaryDirectory() as tmp:
            rc = claimscope.warn_or_refuse("game/x/y.c", "test", repo=tmp,
                                           stream=buf)
        self.assertEqual(rc, 0)


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


class RefusalPath(unittest.TestCase):
    def _tree(self, tmp, claim, lane):
        root = Path(tmp)
        (root / "memory_graph" / "records").mkdir(parents=True)
        (root / "memory_graph" / "records" / "work_claim.a.json").write_text(
            json.dumps(claim), encoding="utf-8")
        (root / "LANE_LOCK").write_text(lane, encoding="utf-8")
        return root

    def test_foreign_unit_refuses_with_exit_three(self):
        import io
        claim = {"kind": "work_claim", "id": "wc.a", "owner": "worker-MF",
                 "state": "active",
                 "attributes": {"owned_units": ["game/ps2/ml_fmath.c"]}}
        with tempfile.TemporaryDirectory() as tmp:
            root = self._tree(tmp, claim, "worker-NM")
            buf = io.StringIO()
            rc = claimscope.warn_or_refuse("game/ps2/ml_fmath.c", "probe",
                                           repo=root, stream=buf)
            self.assertEqual(rc, claimscope.FOREIGN_EXIT)
            self.assertIn("CLAIM CONFLICT", buf.getvalue())

    def test_override_downgrades_the_refusal(self):
        import io
        claim = {"kind": "work_claim", "id": "wc.a", "owner": "worker-MF",
                 "state": "active",
                 "attributes": {"owned_units": ["game/ps2/ml_fmath.c"]}}
        with tempfile.TemporaryDirectory() as tmp:
            root = self._tree(tmp, claim, "worker-NM")
            buf = io.StringIO()
            rc = claimscope.warn_or_refuse("game/ps2/ml_fmath.c", "probe",
                                           repo=root, enforce=False,
                                           stream=buf)
            self.assertEqual(rc, 0)

    def test_released_claims_do_not_protect(self):
        import io
        claim = {"kind": "work_claim", "id": "wc.a", "owner": "worker-MF",
                 "state": "released",
                 "attributes": {"owned_units": ["game/ps2/ml_fmath.c"]}}
        with tempfile.TemporaryDirectory() as tmp:
            root = self._tree(tmp, claim, "worker-NM")
            buf = io.StringIO()
            rc = claimscope.warn_or_refuse("game/ps2/ml_fmath.c", "probe",
                                           repo=root, stream=buf)
            self.assertEqual(rc, 0)


class WiredIntoTheEditLoop(unittest.TestCase):
    def test_probe_and_defake_gate_both_call_the_screen(self):
        for name in ("probe.py", "defake_gate.py"):
            text = (REPO / "tools" / "gdl" / name).read_text(encoding="utf-8")
            self.assertIn("claimscope", text, name)
            self.assertIn("--ignore-claim", text, name)

    def test_the_cli_runs(self):
        r = subprocess.run([sys.executable, "tools/gdl/claimscope.py",
                            "--index"], cwd=str(REPO), capture_output=True,
                           text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        payload = json.loads(r.stdout)
        self.assertIn("owned_units_index", payload)
        self.assertIn("claims_without_owned_units", payload)


if __name__ == "__main__":
    unittest.main()
