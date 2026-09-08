"""Census tools write to build/, and no tracked file, ever (run 61 item 6).

THE OBSERVATION (T3 lane): six census tools wrote their JSON output into
`tools/gdl/composed_census/` and those files were TRACKED, so running any of
them dirtied the repo.

THE CENSUS at 5cfc42acf, `git ls-files tools/gdl/composed_census` -- every
tracked non-`.py` file, and whether a tool writes it:

    ch_census26.json  <- ch_census26.py   generated
    ch_closable.json  <- ch_closable.py   generated
    ch_harvest.json   <- ch_harvest.py    generated
    ch_roster.json    <- ch_roster.py     generated
    ch_shipped.json   <- ch_shipped.py    generated
    ch_sweep26.json   <- ch_sweep26.py    generated
    cn_found.json     <- cn_search.py, rewritten by cn_final.py  generated
    ch_sweep26.log    <- nothing writes it: a captured console log
    pw_rec_attempt.json, pw_rec_law.json
                      <- nothing writes or reads them: hand-authored records

so SEVEN files are generated, not six, and THREE are inputs that stay
tracked because no tool can regenerate them.

EVIDENCE THAT THE TRACKED COPIES WERE ALSO STALE: regenerating ch_shipped.py
after the move produced a file with a different sha256 from the tracked one
(webfrank.json has gained rules since it was last committed), so the tracked
artifact was describing an earlier rule set to every reader of it.
"""
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
CENSUS = TOOLS / "composed_census"
if str(CENSUS) not in sys.path:
    sys.path.insert(0, str(CENSUS))

import cc_artifact  # noqa: E402

GENERATED = ("ch_census26.json", "ch_closable.json", "ch_harvest.json",
             "ch_roster.json", "ch_shipped.json", "ch_sweep26.json",
             "cn_found.json")


class ArtifactPaths(unittest.TestCase):
    def test_the_default_is_under_build_and_never_the_source_tree(self):
        for name in GENERATED:
            with self.subTest(name=name):
                path = Path(cc_artifact.artifact_path(name))
                self.assertTrue(path.is_relative_to(ROOT / "build"), path)
                self.assertFalse(path.is_relative_to(CENSUS), path)

    def test_an_out_override_is_taken_repo_relative(self):
        path = Path(cc_artifact.artifact_path(
            "x.json", "build/GUNE5D/t4_override/x.json"))
        self.assertEqual(path, ROOT / "build/GUNE5D/t4_override/x.json")

    def test_out_is_parsed_in_both_spellings_and_otherwise_absent(self):
        self.assertEqual(cc_artifact.out_override(["t.py", "--out", "a/b"]),
                         "a/b")
        self.assertEqual(cc_artifact.out_override(["t.py", "--out=a/b"]),
                         "a/b")
        for argv in (["t.py"], ["t.py", "--help"], ["t.py", "--out"]):
            with self.subTest(argv=argv):
                self.assertIsNone(cc_artifact.out_override(argv))

    def test_a_missing_artifact_refuses_and_names_its_producer(self):
        with self.assertRaises(SystemExit) as caught:
            cc_artifact.load_artifact("t4_no_such_artifact.json", "a_test")
        message = str(caught.exception)
        self.assertIn("a_test", message)
        self.assertIn("has not been generated", message)
        self.assertIn("build/GUNE5D/composed_census", message)

    def test_every_generated_name_has_a_producer_command(self):
        for name in GENERATED:
            with self.subTest(name=name):
                self.assertIn(name, cc_artifact.PRODUCERS)
                self.assertTrue(
                    cc_artifact.PRODUCERS[name].startswith("python tools/"))


class NoTrackedArtifacts(unittest.TestCase):
    """The negative side: none of these may come back into the source tree."""

    def tracked(self):
        done = subprocess.run(["git", "ls-files", "tools/gdl/composed_census"],
                              cwd=str(ROOT), capture_output=True, text=True)
        if done.returncode:
            raise unittest.SkipTest("git unavailable")
        return set(done.stdout.split())

    def test_no_generated_artifact_is_tracked_any_more(self):
        tracked = self.tracked()
        for name in GENERATED:
            with self.subTest(name=name):
                self.assertNotIn("tools/gdl/composed_census/" + name, tracked)

    def test_the_three_hand_authored_records_are_still_tracked(self):
        # They are INPUTS: nothing regenerates them, so untracking them would
        # destroy data rather than stop a tool dirtying a tree.
        tracked = self.tracked()
        for name in ("ch_sweep26.log", "pw_rec_attempt.json",
                     "pw_rec_law.json"):
            with self.subTest(name=name):
                self.assertIn("tools/gdl/composed_census/" + name, tracked)

    def test_no_source_file_still_writes_beside_itself(self):
        offenders = []
        for path in sorted(CENSUS.glob("*.py")):
            text = path.read_text(encoding="utf-8", errors="replace")
            for name in GENERATED:
                if f'os.path.join(HERE, "{name}"' in text or \
                        f'"{name}"), "w"' in text:
                    offenders.append(f"{path.name} -> {name}")
        self.assertEqual(offenders, [])


@unittest.skipUnless((ROOT / "config/GUNE5D/webfrank.json").is_file(),
                     "needs the rule config")
class LiveRegeneration(unittest.TestCase):
    def test_running_a_producer_writes_build_and_dirties_nothing(self):
        before = subprocess.run(["git", "status", "--porcelain"],
                                cwd=str(ROOT), capture_output=True, text=True)
        if before.returncode:
            raise unittest.SkipTest("git unavailable")
        target = ROOT / "build/GUNE5D/composed_census/t4_shipped_probe.json"
        if target.exists():
            os.remove(target)
        done = subprocess.run(
            [sys.executable, "tools/gdl/composed_census/ch_shipped.py",
             "--out", "build/GUNE5D/composed_census/t4_shipped_probe.json"],
            cwd=str(ROOT), capture_output=True, text=True, errors="replace")
        self.addCleanup(lambda: target.exists() and os.remove(target))
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertTrue(target.is_file())
        self.assertIsInstance(json.loads(target.read_text(encoding="utf-8")),
                              list)
        after = subprocess.run(["git", "status", "--porcelain"],
                               cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(before.stdout, after.stdout,
                         "running a census tool changed the worktree")


if __name__ == "__main__":
    unittest.main()
