"""Run-56 item 5, both halves: an unreadable name and an unreadable anchor.

(a) REPRODUCED at 2a90f8403: every unit in `build/GUNE5D/report.json` is
    named `main/<unit>` with the `.c`/`.cpp` stripped -- 333 of 333, first
    segment `main`, no exceptions. So both joins a lane writes first return
    NOTHING:

        [u for u in units if u["name"] == "game/enemy/critter.c"]         -> 0
        [u for u in units if u["name"].endswith("game/enemy/critter.c")]  -> 0

    and `progress.py` -- the file that hosts the IMPORTABLE CORE for reading
    that report -- never said so. NEIGHBOURHOOD SCREEN: of the 19 tools that
    read report.json AND a unit `name`, 15 carry a private copy of the prefix
    strip. The sixteenth is where the wasted pass happens, so the strip is
    published as `report_unit_key` / `load_report_units` rather than
    documented and re-derived. (The first screen counted 55 files by looking
    for a `"units"` key; `config/GUNE5D/webfrank.json` has one too, so that
    number measured the wrong thing and is not the one quoted.)

(b) REPRODUCED: `"measured at c0f978273"` resolved to nothing. 1,144 records
    cite a hash-shaped token, 1,194 distinct, of which 956 (80%) are commits
    here and 238 (20%) are not (source sha1s, body digests, worker-branch
    commits). `whenrun.py` resolves the first population and REPORTS the
    second rather than guessing.

    DESIGN REVERSAL: the first cut ordered by commit DATE and reported
    `c0f978273` -- which IS the run-52 staging commit -- as run 54, because
    runs 52, 53 and 54 all staged on 2026-09-03. Run membership is an
    ANCESTRY question; the tool reads `git log --topo-order` now, and this
    file pins the case that caught it.
"""
import json
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS))

import fndiff                                                    # noqa: E402
import progress                                                  # noqa: E402
import whenrun                                                    # noqa: E402


class ReportUnitNamesCarryAPrefix(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        path = REPO / "build" / "GUNE5D" / "report.json"
        if not path.exists():
            raise unittest.SkipTest("report.json not built in this tree")
        cls.names = [u.get("name", "") for u in
                     json.loads(path.read_text(encoding="utf-8"))
                     .get("units", [])]

    def test_the_prefix_is_universal_not_occasional(self):
        self.assertTrue(self.names)
        self.assertEqual([n for n in self.names if not n.startswith("main/")],
                         [])

    def test_the_naive_joins_return_nothing(self):
        """The measurement that makes the docstring note worth having."""
        probe = "game/enemy/critter.c"
        self.assertEqual([n for n in self.names if n == probe], [])
        self.assertEqual([n for n in self.names if n.endswith(probe)], [])

    def test_report_unit_key_agrees_with_fndiff_unit_key(self):
        for spelling in ("game/enemy/critter", "game/enemy/critter.c",
                         "src/game/enemy/critter.c"):
            self.assertEqual(progress.report_unit_key(spelling),
                             fndiff.unit_key(spelling), spelling)
        self.assertEqual(progress.report_unit_key("main/game/enemy/critter"),
                         "game/enemy/critter")

    def test_load_report_units_keys_by_the_unit_path(self):
        units = progress.load_report_units()
        self.assertIn("game/enemy/critter", units)
        self.assertNotIn("main/game/enemy/critter", units)


class CommitCitationsResolveToRuns(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.hist = whenrun.history()
        if not cls.hist:
            raise unittest.SkipTest("no git history reachable here")
        cls.markers = whenrun.run_markers(cls.hist)

    def test_the_staging_commit_of_a_run_resolves_to_that_run(self):
        """The exact input the date-ordered first cut got wrong.

        Every `Stage run-N work claims` commit must resolve to run N: it is
        its own nearest marker. Three runs staged on one calendar day, so
        this is the case a date comparison cannot pass.
        """
        self.assertTrue(self.markers)
        for number, sha, _date, _index in self.markers:
            row = whenrun.resolve(sha, markers=self.markers, hist=self.hist)
            self.assertEqual(row["run_floor"], number, sha)

    def test_a_run_number_is_reported_as_a_floor_because_runs_lack_markers(self):
        numbers = sorted({row[0] for row in self.markers})
        gaps = [n for n in range(numbers[0], numbers[-1] + 1)
                if n not in numbers]
        self.assertTrue(gaps, "if every run staged a marker this can be exact")

    def test_a_non_commit_is_reported_not_guessed(self):
        row = whenrun.resolve("deadbeef123", markers=self.markers,
                              hist=self.hist)
        self.assertFalse(row["is_commit"])
        self.assertIn("not a commit", row["note"])

    def test_an_age_is_a_number_of_days(self):
        sha = self.markers[0][1]
        row = whenrun.resolve(sha, markers=self.markers, hist=self.hist)
        self.assertIsInstance(row["age_days"], int)
        self.assertGreaterEqual(row["age_days"], 0)


if __name__ == "__main__":
    unittest.main()
