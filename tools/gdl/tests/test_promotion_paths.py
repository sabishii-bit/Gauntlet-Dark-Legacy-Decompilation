"""Rule-17 promotion damage: a sys.path line that points nowhere.

MEASURED, run 43 (item 9 named ONE of these; the class is six). A tool
written in a lane's scratch directory at the repository root computes

    HERE = os.path.dirname(os.path.abspath(__file__))
    ROOT = os.path.abspath(os.path.join(HERE, ".."))
    sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

which is correct THERE and wrong the moment the file is promoted INTO
tools/gdl/composed_census: ROOT becomes tools/gdl and the inserted path
becomes tools/gdl/tools/gdl. Every such tool died on `import webfrank`
with ModuleNotFoundError, from any directory, and stayed dead because
nobody ran it after the move — exactly what AGENTS.md rule 17 requires
("a promoted script must be RUN ONCE from the repo root before the
promoting commit lands"). The six: wf_detail, wf_dump, wf_dcs_screen,
wf_recolor_probe, wf_sim, wf_web.

This test computes where each file's sys.path insert actually lands and
fails if the directory does not exist. It is static — no module is
imported, because several of these scripts read sys.argv at import time.
"""

import re
import unittest
from pathlib import Path

CENSUS = Path(__file__).resolve().parents[1] / "composed_census"
ROOT_ASSIGN = re.compile(
    r"^\s*ROOT\s*=\s*os\.path\.abspath\(os\.path\.join\(HERE\s*,\s*"
    r"([^)]*)\)\)", re.M)
PATH_INSERT = re.compile(
    r"sys\.path\.insert\(\s*0\s*,\s*os\.path\.join\(ROOT\s*,\s*([^)]*)\)\)")
PARTS = re.compile(r"[\"']([^\"']+)[\"']")


class PromotedPathTests(unittest.TestCase):
    def test_every_root_relative_sys_path_insert_exists(self):
        broken = []
        for path in sorted(CENSUS.glob("*.py")):
            text = path.read_text(encoding="utf-8", errors="replace")
            root_match = ROOT_ASSIGN.search(text)
            insert_match = PATH_INSERT.search(text)
            if not root_match or not insert_match:
                continue
            target = path.parent
            for part in PARTS.findall(root_match.group(1)):
                target = target / part
            for part in PARTS.findall(insert_match.group(1)):
                target = target / part
            resolved = target.resolve()
            if not resolved.is_dir():
                broken.append(f"{path.name} -> {resolved}")
        self.assertEqual(broken, [], "sys.path.insert lands on a directory"
                         " that does not exist (rule-17 promotion damage)")

    def test_the_six_repaired_tools_can_reach_tools_gdl(self):
        """Named explicitly so a re-break is attributed, not just counted."""
        for name in ("wf_detail.py", "wf_dump.py", "wf_dcs_screen.py",
                     "wf_recolor_probe.py", "wf_sim.py", "wf_web.py"):
            text = (CENSUS / name).read_text(encoding="utf-8")
            self.assertIn("sys.path.insert(0, os.path.dirname(HERE))", text,
                          name)

    def test_the_checker_itself_can_fail(self):
        """A gate that cannot fire reads as an all-clear."""
        target = CENSUS.parent / "tools" / "gdl"      # the broken join
        self.assertFalse(target.is_dir())


if __name__ == "__main__":
    unittest.main()
