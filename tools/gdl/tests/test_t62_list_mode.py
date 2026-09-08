"""`fndiff -l` scores a function the way `--clean` does (run 62 item 3b).

THE OBSERVATION. `-l` decided on RAW line equality, so recovering a literal
-- our anonymous `@37` becoming the target's `lbl_80346490` -- flipped a
function from `OK` to `DIFF` while `--clean` still printed
`MATCH (pool-name noise only), 0 real diff lines` for the same two objects.
Five such rows per literal recovery read as five regressions.

CENSUS at f08e8640d, 255 configured units (build/c62_listmode_census.py):
3001 paired functions -- 2051 identical, 442 with a real residual, and 508
(17%, across 98 units) that `-l` called DIFF and `--clean` scored at zero.
Zero of the 508 carried a confirmed POOL-DEFECT row.

THE NEGATIVE SIDE is that "pool-name noise" must never swallow a pool row
that reads a DIFFERENT datum: those keep their own verdict word.
"""
import io
import subprocess
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import fndiff  # noqa: E402
from tools.gdl.tests.test_pool_rows import (ADDR, OURS, OURS_VALUES,  # noqa: E402
                                            TARGET, VALUES)

# One address, two spellings: the run-34 case relocation_signature exists
# for. Nothing about the codegen differs.
ALIAS_TARGET = ["lwz     r3,0(0)", "    R_PPC_EMB_SDA21 sFlags"]
ALIAS_OURS = ["lwz     r3,0(0)", "    R_PPC_EMB_SDA21 gControllerButtons+0x4"]

REAL_TARGET = ["addi    r3,r3,1", "blr"]
REAL_OURS = ["addi    r3,r3,2", "blr"]


class ListRow(unittest.TestCase):
    def test_identical_lines_are_ok(self):
        self.assertEqual(fndiff.list_row("f", REAL_TARGET, REAL_TARGET),
                         "OK   f")

    def test_a_real_residual_is_still_diff(self):
        self.assertEqual(fndiff.list_row("f", REAL_TARGET, REAL_OURS),
                         "DIFF f")

    def test_a_pool_name_only_change_is_not_a_regression(self):
        addresses = dict(fndiff._SYMBOL_ADDRESSES or {})
        addresses.update({"sFlags": 0x803445CC,
                          "gControllerButtons": 0x803445C8})
        saved = fndiff._SYMBOL_ADDRESSES
        fndiff._SYMBOL_ADDRESSES = addresses
        try:
            row = fndiff.list_row("f", ALIAS_TARGET, ALIAS_OURS)
        finally:
            fndiff._SYMBOL_ADDRESSES = saved
        self.assertTrue(row.startswith("POOL f"), row)
        self.assertNotIn("DIFF", row)
        self.assertIn("0 real diff lines", row)

    def test_the_row_agrees_with_what_clean_scores(self):
        # The property the item is about: one function, two views, one
        # verdict. `--clean` prints `MATCH (pool-name noise only)` here.
        addresses = dict(fndiff._SYMBOL_ADDRESSES or {})
        addresses.update({"sFlags": 0x803445CC,
                          "gControllerButtons": 0x803445C8})
        saved = fndiff._SYMBOL_ADDRESSES
        fndiff._SYMBOL_ADDRESSES = addresses
        buffer = io.StringIO()
        try:
            with redirect_stdout(buffer):
                fndiff.clean_diff("f", ALIAS_TARGET, ALIAS_OURS)
            row = fndiff.list_row("f", ALIAS_TARGET, ALIAS_OURS)
        finally:
            fndiff._SYMBOL_ADDRESSES = saved
        self.assertIn("MATCH (pool-name noise only)", buffer.getvalue())
        self.assertTrue(row.startswith("POOL "), row)


class PoolDefectKeepsItsOwnWord(unittest.TestCase):
    """A wrong DATUM must never be listed as pool-name noise."""

    def setUp(self):
        self._addr = fndiff._SYMBOL_ADDRESSES
        self._pool = fndiff._POOL_SYMBOLS
        self._target_bytes = fndiff.target_datum_bytes
        self._ours_bytes = fndiff.ours_datum_bytes
        fndiff._SYMBOL_ADDRESSES = dict(ADDR)
        fndiff._POOL_SYMBOLS = frozenset(VALUES)
        fndiff.target_datum_bytes = lambda sym: VALUES.get(sym.strip())
        fndiff.ours_datum_bytes = (
            lambda sym, obj: OURS_VALUES.get(sym.strip()))

    def tearDown(self):
        fndiff._SYMBOL_ADDRESSES = self._addr
        fndiff._POOL_SYMBOLS = self._pool
        fndiff.target_datum_bytes = self._target_bytes
        fndiff.ours_datum_bytes = self._ours_bytes

    def test_the_recorded_defect_prologue_lists_as_a_defect(self):
        row = fndiff.list_row("adsInitFromHeader", TARGET, OURS)
        self.assertTrue(row.startswith("POOL-DEFECT adsInitFromHeader"), row)
        self.assertIn("DIFFERENT datum", row)
        self.assertIn("--clean", row)

    def test_an_unpaired_instruction_is_a_real_residual_not_a_pool_row(self):
        # Item 3a's CANDIDATE class cannot reach this view: a matcher-chosen
        # pairing needs an unpaired block, and an unpaired block is real
        # diff lines, which is DIFF. Pinned so the branch is not re-added.
        from tools.gdl.tests.test_t62_pool_alignment import (ASYM_OURS,
                                                             ASYM_TARGET)
        self.assertEqual(fndiff.list_row("f", ASYM_TARGET, ASYM_OURS),
                         "DIFF f")
        self.assertTrue(any(fndiff.pool_row_reliability(ASYM_TARGET,
                                                        ASYM_OURS)))


LIVE = (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").is_file()


@unittest.skipUnless(LIVE, "needs the split target objects and a built tree")
class LiveList(unittest.TestCase):
    def test_the_enemy_list_reports_the_censused_pool_population(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/fndiff.py", "game/enemy/enemy", "-l",
             "--no-build"], cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        lines = done.stdout.splitlines()
        pool = [line for line in lines if line.startswith("POOL ")]
        diff = [line for line in lines if line.startswith("DIFF ")]
        # 38 pool-noise functions were censused in this unit; the count is
        # allowed to move with the source, the CLASS is what is pinned.
        self.assertTrue(pool, "enemy had 38 pool-noise functions at f08e8640d")
        self.assertTrue(diff, "enemy still has real residuals")
        verdicts = [line.split()[0] for line in lines]
        names = [line.split()[1] for line in lines if len(line.split()) > 1]
        self.assertEqual(len(names), len(set(names)),
                         "each function is listed exactly once")
        self.assertNotIn("POOL-DEFECT", verdicts)


if __name__ == "__main__":
    unittest.main()
