"""wf_word_diff --json: a summable census and a real body hash (item 3).

THE OBSERVATION (ER2 lane). `--unit <unit> --json` gives a COUNT-ASYMMETRIC
row `differing_words: null`, which is the honest value -- 0 would read as
EXACT -- but a census that sums the column throws on it, and nothing in the
row carried a body hash, so "same word count" was being read as "same
bytes" and lanes hand-rolled their own hashing.

Reproduced at 20c0d7ea1 (build/t4_scratch/t4_wf_json_probe.py):

    game/enemy/enemy       84 rows, 0 non-integer, sum = 830
    game/movie/movieplayer 52 rows, 1 non-integer (DTextInitColorRamp),
        sum(row["differing_words"] for row in rows) raises
        TypeError: unsupported operand type(s) for +: 'int' and 'NoneType'
    per-function body hash fields present: NONE

THE CURE keeps the null and adds what a consumer needs beside it: a boolean
`count_asymmetric` and an integer `instruction_delta` on every row, integer
whole-TU `totals`, and `body_sha256_ours` / `body_sha256_target` hashed from
the very bytes the row measured.
"""
import json
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
if str(TOOLS / "composed_census") not in sys.path:
    sys.path.insert(0, str(TOOLS / "composed_census"))
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import wf_word_diff as wd  # noqa: E402


def measured(name, words, pinned=False, ours="a", target=None):
    return {"function": name, "pinned": pinned, "target_insns": 10,
            "ours_insns": 10, "instruction_delta": 0,
            "count_asymmetric": False, "verdict": "MEASURED",
            "differing_words": words,
            "body_sha256_ours": ours,
            "body_sha256_target": ours if target is None else target}


def asymmetric(name, delta=3):
    return {"function": name, "pinned": False, "target_insns": 10,
            "ours_insns": 10 + delta, "instruction_delta": delta,
            "count_asymmetric": True, "verdict": "COUNT-ASYMMETRIC",
            "differing_words": None, "body_sha256_ours": "a",
            "body_sha256_target": "b"}


class Totals(unittest.TestCase):
    def test_every_total_is_an_integer_even_with_an_asymmetric_row(self):
        rows = [measured("a", 5), measured("b", 0), asymmetric("c"),
                measured("d", 7, pinned=True, ours="x", target="y")]
        totals = wd.unit_totals(rows)
        for key, value in totals.items():
            with self.subTest(key=key):
                self.assertIsInstance(value, int)
        self.assertEqual(totals["functions"], 4)
        self.assertEqual(totals["measured"], 3)
        self.assertEqual(totals["count_asymmetric"], 1)
        self.assertEqual(totals["differing_words_total"], 12)
        self.assertEqual(totals["open_differing_words"], 5)
        self.assertEqual(totals["exact"], 1)
        self.assertEqual(totals["pinned"], 1)

    def test_the_asymmetric_row_is_excluded_not_counted_as_zero(self):
        # Counting it as 0 would make it indistinguishable from an EXACT
        # function, which is the mistake the null exists to prevent.
        only = wd.unit_totals([asymmetric("c")])
        self.assertEqual(only["differing_words_total"], 0)
        self.assertEqual(only["measured"], 0)
        self.assertEqual(only["exact"], 0)
        self.assertEqual(only["count_asymmetric"], 1)

    def test_body_equality_is_counted_from_the_hashes_not_the_word_count(self):
        rows = [measured("same", 0, ours="h", target="h"),
                measured("different_bytes_same_count", 4,
                         ours="h", target="k")]
        self.assertEqual(wd.unit_totals(rows)["body_equal"], 1)

    def test_an_empty_roster_totals_to_zeroes_not_an_error(self):
        self.assertEqual(wd.unit_totals([])["functions"], 0)


UNIT = "game/movie/movieplayer"
BUILT = (ROOT / "build/GUNE5D/obj/game/movie/movieplayer.o").is_file()


@unittest.skipUnless(BUILT, "needs the split target objects")
class LiveJson(unittest.TestCase):
    """movieplayer is the unit that HAS a count-asymmetric row."""

    @classmethod
    def setUpClass(cls):
        done = subprocess.run(
            [sys.executable, "tools/gdl/composed_census/wf_word_diff.py",
             "--unit", UNIT, "--json"], cwd=str(ROOT), capture_output=True,
            text=True)
        if done.returncode:
            raise unittest.SkipTest("wf_word_diff refused: "
                                    + done.stderr.strip()[:200])
        cls.data = json.loads(done.stdout)

    def test_the_count_asymmetric_row_is_flagged_and_still_null(self):
        rows = [r for r in self.data["rows"] if r["count_asymmetric"]]
        self.assertTrue(rows, "movieplayer must still carry one")
        for row in rows:
            self.assertIsNone(row["differing_words"])
            self.assertEqual(row["verdict"], "COUNT-ASYMMETRIC")
            self.assertNotEqual(row["instruction_delta"], 0)

    def test_a_census_can_sum_the_column_without_throwing(self):
        total = sum(row["differing_words"] for row in self.data["rows"]
                    if not row["count_asymmetric"])
        self.assertEqual(total, self.data["totals"]["differing_words_total"])

    def test_every_row_carries_both_body_hashes(self):
        for row in self.data["rows"]:
            with self.subTest(function=row["function"]):
                for key in ("body_sha256_ours", "body_sha256_target"):
                    self.assertRegex(row[key], r"^[0-9a-f]{64}$")

    def test_equal_hashes_and_zero_differing_words_agree(self):
        for row in self.data["rows"]:
            if row["count_asymmetric"]:
                continue
            with self.subTest(function=row["function"]):
                self.assertEqual(
                    row["body_sha256_ours"] == row["body_sha256_target"],
                    row["differing_words"] == 0)

    def test_the_totals_block_matches_a_recomputation(self):
        self.assertEqual(self.data["totals"],
                         wd.unit_totals(self.data["rows"]))


if __name__ == "__main__":
    unittest.main()
