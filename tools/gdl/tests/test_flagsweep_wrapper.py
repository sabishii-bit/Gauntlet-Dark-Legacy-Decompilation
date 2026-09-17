"""flagsweep could not run on a host where ninja wraps the compiler.

`ninja_base_cmd` located the `mwcceppc.exe` token and kept only what followed
it, so two things in a non-Windows compile edge were lost or mangled:

1. Every token BEFORE the compiler was dropped. On Windows native that is
   nothing, but on Linux/macOS it is `build/tools/wibo`, the wrapper that
   actually executes the PE. Without it every variant ran the PE directly and
   died with `PermissionError: .../mwcceppc.exe`, so the whole battery reported
   "compile failed" and no flag axis could be measured off-Windows at all.

2. Ninja chains the dep transform onto the compile with `&&`, and those tokens
   were kept as mwcc flags. mwcc then aborted with
   `Usage Error: Specified file '&&' not found`.

Measured after the fix, at 5f1787d3, on game/g3d/gcontrolpads
G3DReadControlPadStates: target 49 lines, control 51 lines / diff 13, and NO
variant in the battery beats the control — `-opt noprop` included, which is the
flag form of the `#pragma opt_propagation off` already guarding that function.
That retires the flag/pragma axis for it as measured-dead rather than untried.

TWO-SIDED. Positive: a wrapped Linux-style edge yields the wrapper separately
and flags that stop at the operator. Negative: an unwrapped Windows-style edge
yields an EMPTY wrapper (the fix must not invent one), and a source with no
mwcc edge still raises SystemExit rather than silently sweeping nothing.
"""
import importlib.util
import subprocess
import unittest
from pathlib import Path
from types import SimpleNamespace

SPEC = Path(__file__).resolve().parent.parent / "flagsweep.py"

WRAPPED = (
    "build/tools/wibo build/tools/sjiswrap.exe "
    "build/compilers/GC/1.2.5n/mwcceppc.exe -nodefaults -proc gekko "
    "-fp hardware -lang=c -MMD -c src/game/g3d/gcontrolpads.c "
    "-o build/GUNE5D/src/game/g3d && \"/usr/bin/python3\" "
    "tools/transform_dep.py a.d b.d"
)
BARE = (
    "build/compilers/GC/1.2.5n/mwcceppc.exe -nodefaults -proc gekko "
    "-fp hardware -lang=c -MMD -c src/game/g3d/gcontrolpads.c "
    "-o build/GUNE5D/src/game/g3d"
)


def load_module():
    spec = importlib.util.spec_from_file_location("flagsweep", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class FlagsweepWrapperTest(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()
        self.src = self.mod.ROOT / "src/game/g3d/gcontrolpads.c"

    def fake_ninja(self, stdout):
        def run(cmd, *a, **kw):
            return SimpleNamespace(stdout=stdout, stderr="", returncode=0)
        self.mod.subprocess = SimpleNamespace(run=run, PIPE=subprocess.PIPE)
        self.addCleanup(setattr, self.mod, "subprocess", subprocess)

    def test_wrapped_edge_keeps_the_wrapper_and_stops_at_the_operator(self):
        self.fake_ninja(WRAPPED)
        flags, tag, wrapper = self.mod.ninja_base_cmd(self.src)
        self.assertEqual(wrapper, ["build/tools/wibo",
                                   "build/tools/sjiswrap.exe"])
        self.assertEqual(tag, "1.2.5n")
        # the chained dep-transform command must not appear as mwcc flags
        self.assertNotIn("&&", flags)
        self.assertFalse([f for f in flags if "transform_dep" in f], flags)
        # -MMD, -o/-c with their arguments, and -lang are stripped as before
        self.assertNotIn("-MMD", flags)
        self.assertNotIn("-o", flags)
        self.assertNotIn("-lang=c", flags)
        self.assertNotIn("src/game/g3d/gcontrolpads.c", flags)
        # the real flags survive
        self.assertIn("-nodefaults", flags)
        self.assertIn("gekko", flags)
        self.assertIn("hardware", flags)

    def test_unwrapped_edge_yields_an_empty_wrapper(self):
        """Windows native has no wrapper; the fix must not invent one."""
        self.fake_ninja(BARE)
        flags, tag, wrapper = self.mod.ninja_base_cmd(self.src)
        self.assertEqual(wrapper, [])
        self.assertEqual(tag, "1.2.5n")
        self.assertIn("-nodefaults", flags)

    def test_no_mwcc_edge_still_refuses(self):
        self.fake_ninja("python tools/somethingelse.py foo\n")
        with self.assertRaises(SystemExit):
            self.mod.ninja_base_cmd(self.src)

    def test_compile_variant_unpacks_the_three_tuple(self):
        """A stale 2-tuple caller would raise ValueError; assert the arity."""
        self.fake_ninja(WRAPPED)
        base = self.mod.ninja_base_cmd(self.src)
        self.assertEqual(len(base), 3)
        obj, err = self.mod.compile_variant(self.src, "CC=does-not-exist",
                                            "t00", base)
        self.assertIsNone(obj)
        self.assertIn("no compiler", err)


if __name__ == "__main__":
    unittest.main()
