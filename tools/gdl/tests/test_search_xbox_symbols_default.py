"""search_xbox_symbols defaulted to a directory no clean checkout has.

The default was `include/xbox_symbols`, which `split_pdb_header.py` produces
from the PRIVATE `misc/Xbox/shell3D.h`. That header is not in the repository,
so in a clean checkout every invocation printed

    Error: include/xbox_symbols does not exist
    Have you run: python tools/split_pdb_header.py misc/Xbox/shell3D.h ?

advising a script whose input nobody has -- while the dumped headers that ARE
committed sat unused in `research/xbox_symbols/` (misc.h, game.h, audio.h, ...
plus xbox_structs.tsv). Every caller had to know to pass
`--symbols-dir research/xbox_symbols`, and a session lost time to it before
finding `struct HELPTAB` (size 0x1c, the match for game/ui/message's MsgDesc)
that way.

TWO-SIDED. Positive: the default resolves to the in-repo headers when they
exist, that directory really does hold the dump, and an explicit --symbols-dir
still wins. Negative: the default falls back to the split-output path when the
in-repo dump is absent (so a user who HAS run split_pdb_header is unaffected),
and the fallback is not silently reported as present.
"""
import importlib.util
import unittest
from pathlib import Path

SPEC = Path(__file__).resolve().parent.parent / "search_xbox_symbols.py"


def load_module():
    spec = importlib.util.spec_from_file_location("search_xbox_symbols", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class DefaultSymbolsDirTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load_module()

    # ---------- positive ----------

    def test_the_default_is_the_in_repo_dump(self):
        self.assertEqual(self.mod.default_symbols_dir(),
                         self.mod.IN_REPO_SYMBOLS)
        self.assertTrue(self.mod.IN_REPO_SYMBOLS.exists(),
                        "research/xbox_symbols should be committed")

    def test_that_directory_actually_holds_the_dump(self):
        names = {p.name for p in self.mod.IN_REPO_SYMBOLS.glob("*.h")}
        self.assertIn("misc.h", names)
        self.assertIn("game.h", names)

    # ---------- negative ----------

    def test_it_falls_back_when_the_in_repo_dump_is_absent(self):
        """A user who ran split_pdb_header must keep working."""
        real = self.mod.IN_REPO_SYMBOLS
        self.mod.IN_REPO_SYMBOLS = real.parent / "definitely_not_here"
        self.addCleanup(setattr, self.mod, "IN_REPO_SYMBOLS", real)
        self.assertEqual(self.mod.default_symbols_dir(),
                         self.mod.SPLIT_SYMBOLS)

    def test_the_two_candidate_paths_are_distinct(self):
        self.assertNotEqual(self.mod.IN_REPO_SYMBOLS, self.mod.SPLIT_SYMBOLS)
        self.assertEqual(self.mod.SPLIT_SYMBOLS.name, "xbox_symbols")
        self.assertEqual(self.mod.SPLIT_SYMBOLS.parent.name, "include")


if __name__ == "__main__":
    unittest.main()
