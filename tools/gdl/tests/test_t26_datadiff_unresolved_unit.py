"""Run-56 item 3: datadiff read a mistyped unit as a clean gate.

REPRODUCED VERBATIM at 105dbd1ea, with the reporter's own command:

    $ python tools/gdl/datadiff.py --sections game/does/not/exist
    [game/does/not/exist] no splits entry
    EXIT=0
    $ python tools/gdl/textorder.py game/does/not/exist
    NO-PAIR      game/does/not/exist  (missing: ...)
    EXIT=2

Same input, same class of question, opposite verdicts -- and datadiff's is
the one a scripted flip gate reads as "zero blockers". This is the third
recurrence of run-50 item 3's rule inside this same tool: EMPTY OUTPUT CAN
NEVER MEAN SUCCESS.

HALF THE OBSERVATION WAS NOT A TYPO. `src/game/enemy/critter` is a spelling
AGENTS.md says the core tools accept (`fndiff.unit_key` is named as THE
normalizer); datadiff did not normalize, so a correct unit spelled the
documented way took the same silent branch. The fix normalizes first and
refuses second, which is why a design that only added an exit code would
have made the tool WORSE for that spelling.

TWO-SIDED CALIBRATION at 105dbd1ea, configure.py's 255 `Object()` rows
against splits.txt's 257 units:
  NEGATIVE (rows a refusal newly fails): 1 -- and it is
      `TRK_MINNOW_DOLPHIN/ppc/Generic/exception.s`, an assembly file, not a
      C unit. 0 rows have a splits entry with an unbuilt object.
  POSITIVE (spellings that took the silent branch): `game/does/not/exist`,
      `game/enemy/critter.c.c`, `game/enemy/Critter` -- and
      `src/game/enemy/critter`, which is a REAL unit.
"""
import re
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS))

import datadiff                                                  # noqa: E402


def run_datadiff(*args):
    proc = subprocess.run(
        [sys.executable, str(TOOLS / "datadiff.py"), *args],
        cwd=REPO, capture_output=True, text=True)
    return proc.returncode, proc.stdout


class UnresolvedUnitIsARefusal(unittest.TestCase):

    def test_a_mistyped_unit_exits_nonzero_and_says_nothing_was_compared(self):
        code, out = run_datadiff("--sections", "game/does/not/exist")
        self.assertEqual(code, 2, out)
        self.assertIn("UNRESOLVED UNIT", out)
        self.assertIn("NO comparison was made", out)

    def test_the_refusal_code_is_distinct_from_the_blocker_code(self):
        """2 = I could not measure; 1 = I measured and found blockers.

        Collapsing them would trade one unreadable verdict for another.
        """
        code, _ = run_datadiff("--sections", "game/does/not/exist")
        self.assertEqual(code, 2)
        self.assertNotEqual(code, 1)


class DocumentedSpellingsResolve(unittest.TestCase):
    """`fndiff.unit_key` is THE normalizer; datadiff must agree with it."""

    SPELLINGS = ("game/enemy/critter", "game/enemy/critter.c",
                 "src/game/enemy/critter", "src/game/enemy/critter.c",
                 r"src\game\enemy\critter.c")

    def test_every_documented_spelling_maps_to_one_key(self):
        keys = {datadiff._unit_key(s) for s in self.SPELLINGS}
        self.assertEqual(keys, {"game/enemy/critter"})

    def test_datadiffs_normalizer_matches_fndiffs(self):
        import fndiff
        for spelling in self.SPELLINGS:
            self.assertEqual(datadiff._unit_key(spelling),
                             fndiff.unit_key(spelling), spelling)

    def test_the_src_spelling_resolves_instead_of_reading_as_absent(self):
        code, out = run_datadiff("--sections", "src/game/enemy/critter")
        self.assertNotEqual(code, 2, out)
        self.assertNotIn("UNRESOLVED UNIT", out)
        self.assertIn("game/enemy/critter.c", out)


class NegativeSideStaysQuiet(unittest.TestCase):
    """The calibration's negative side, asserted rather than remembered."""

    def test_configure_units_resolve_except_the_one_assembly_row(self):
        units = datadiff.parse_splits()
        cfg = (REPO / "configure.py").read_text(encoding="utf-8")
        rows = [m.group(2) for m in
                re.finditer(r'Object\((\w+),\s*"([^"]+)"', cfg)]
        self.assertGreater(len(rows), 200, "configure.py parse changed shape")
        missing = []
        for path in rows:
            key = datadiff._unit_key(path)
            if not any(k.rsplit(".", 1)[0] == key or k == key for k in units):
                missing.append(path)
        self.assertEqual(missing,
                         ["TRK_MINNOW_DOLPHIN/ppc/Generic/exception.s"],
                         "the refusal's cost changed: re-run the calibration")


if __name__ == "__main__":
    unittest.main()
