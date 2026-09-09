import re
import unittest
from pathlib import Path

from tools.gdl.mwcc_p6 import patch_pe


ROOT = Path(__file__).resolve().parents[3]


class CiSpecialCompilerTests(unittest.TestCase):
    def test_ci_builds_payload_and_selects_reviewed_compilers(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("compiler_payload:", workflow)
        self.assertIn("tools/gdl/mwcc_p6/build_payload.sh", workflow)
        self.assertIn("name: gdl_mwcc_p6_payload", workflow)
        self.assertIn("--gdl-special-compilers", workflow)
        self.assertIn("--experimental-p6-compiler", workflow)
        self.assertRegex(workflow, r"(?m)^    needs: compiler_payload$")
        self.assertEqual(workflow.count("pnpm install --frozen-lockfile"), 1)

    def test_ci_refreshes_the_live_objneutral_fixture(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(
            encoding="utf-8"
        )
        fixture = "ninja build/GUNE5D/src/game/enemy/critter.o"
        tests = "python -m unittest discover tools/gdl/tests -b"
        self.assertIn(fixture, workflow)
        self.assertIn(tests, workflow)
        self.assertLess(workflow.index(fixture), workflow.index(tests))
        analysis_step = workflow[workflow.index("- name: Analysis tool tests"):
                                 workflow.index("- name: Reconstruction diagnostics")]
        self.assertNotRegex(analysis_step, r"(?m)^\s+HOME:")
        self.assertRegex(
            workflow,
            r"(?ms)- name: Analysis tool tests\n"
            r"\s+env:\n"
            r"\s+WINEPREFIX: /tmp/\.wine\n"
            r"\s+run: \|.*?"
            r"ninja build/GUNE5D/src/game/enemy/critter\.o.*?"
            r"python -m unittest discover tools/gdl/tests -b",
        )

    def test_ci_removes_every_live_test_object_before_rebuilding(self):
        """A cache-restored object can be NEWER than the checked-out source
        while its content comes from an older commit; ninja keeps it. CI must
        delete the objects the suite reads live and rebuild through all_source
        before the analysis suite runs (reproduced: five live failures on a
        player.o built from 8255cd6f3, green after a real rebuild)."""
        workflow = (ROOT / ".github/workflows/build.yml").read_text(
            encoding="utf-8"
        )
        step = workflow[workflow.index("- name: Analysis tool tests"):
                        workflow.index("- name: Reconstruction diagnostics")]
        tests = "python -m unittest discover tools/gdl/tests -b"
        self.assertIn("rm -f build/GUNE5D/src/", step)
        for unit in ("game/enemy/critter", "game/game/player",
                     "game/game/controls", "game/enemy/enemy",
                     "game/audio/dcsdrv", "game/sound/sounds",
                     "game/movie/movieplayer", "game/ui/btext",
                     "game/pb/pb_diag", "game/anim/atree"):
            self.assertIn(f"build/GUNE5D/src/{unit}.o", step)
        self.assertIn("ninja all_source", step)
        self.assertLess(step.index("rm -f build/GUNE5D/src/"),
                        step.index("ninja all_source"))
        self.assertLess(step.index("ninja all_source"), step.index(tests))

    def test_ci_payload_hash_matches_the_patcher_contract(self):
        script = (ROOT / "tools/gdl/mwcc_p6/build_payload.sh").read_text(
            encoding="utf-8"
        )
        match = re.search(r"(?m)^expected=([0-9a-f]{64})$", script)
        self.assertIsNotNone(match)
        self.assertEqual(match.group(1), patch_pe.PAYLOAD_SHA256)

    def test_ci_does_not_upload_a_derived_compiler(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(
            encoding="utf-8"
        )
        artifact_paths = re.findall(r"(?m)^\s+path: (.+)$", workflow)
        self.assertNotIn("build/compilers", artifact_paths)
        self.assertFalse(any("mwcceppc" in path for path in artifact_paths))


if __name__ == "__main__":
    unittest.main()
