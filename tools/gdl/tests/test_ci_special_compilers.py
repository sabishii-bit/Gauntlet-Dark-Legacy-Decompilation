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
