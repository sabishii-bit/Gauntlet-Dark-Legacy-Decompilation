"""The pragma scaffold re-audit, and the measurement trap it closes.

`AGENTS.md` requires every historical stop claim to carry a current measured
premise, and `probe.py` prints a standing "N pragma/volatile scaffold row(s) in
this TU -- re-audit each" that nothing automated. scaffold_audit deletes each
region, rebuilds, and compares `real` for EVERY function the region spans.

THE TRAP, measured. Scoring a region on the one NON-EXACT function it guards
found four "HARMFUL" regions at e28d467f. A TU-wide objdiff fuzzy then showed
three of them REGRESSING their TU -- screensaver -0.1846, mb_particle -0.0144,
pb_diag -0.0032 -- because removal helped the measured function and hurt its
neighbours in the same span. Only bosscam survived (+0.0034, landed as
ccddd4a2). Spanning every function catches that without a full report build:
the same bosscam file now shows a region whose removal takes an already-EXACT
function BossCameraUpdate from real 0 to 31.

TWO-SIDED. Positive: an all-improving region is HARMFUL, an all-unchanged one
is DEAD, and both pragma and function-definition lines are recognized.
Negative: a MIXED region is LOAD-BEARING and never HARMFUL (the trap); a
prototype ending in `;` is not a definition; a `#pragma ... reset` with no
opener is not a region; and an empty delta set never reports a verdict that
would license a keep.
"""
import importlib.util
import unittest
from pathlib import Path

SPEC = (Path(__file__).resolve().parent.parent
        / "composed_census" / "scaffold_audit.py")


def load_module():
    spec = importlib.util.spec_from_file_location("scaffold_audit", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class ScaffoldAuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load_module()

    # ---------- positive ----------

    def test_all_improving_is_harmful(self):
        v, worse, better = self.mod.verdict_for_deltas(
            {"a": (16, 6), "b": (4, 4)})
        self.assertEqual(v, "HARMFUL")
        self.assertEqual(better, ["a"])
        self.assertEqual(worse, [])

    def test_all_unchanged_is_dead(self):
        v, worse, better = self.mod.verdict_for_deltas(
            {"a": (6, 6), "b": (0, 0)})
        self.assertEqual(v, "DEAD")
        self.assertEqual((worse, better), ([], []))

    def test_pragma_and_function_lines_are_recognized(self):
        self.assertTrue(self.mod.PRAGMA.match("#pragma opt_propagation off"))
        self.assertTrue(self.mod.PRAGMA.match("#pragma opt_lifetimes reset"))
        self.assertTrue(self.mod.PRAGMA.match("#pragma optimization_level 4"))
        m = self.mod.FNDEF.match("void AudioSetupBossStreams(register int idx,")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "AudioSetupBossStreams")

    # ---------- negative ----------

    def test_a_mixed_region_is_load_bearing_never_harmful(self):
        """The trap: one function improving does not license removal."""
        v, worse, better = self.mod.verdict_for_deltas(
            {"guarded": (543, 527), "neighbour": (0, 31)})
        self.assertEqual(v, "LOAD-BEARING")
        self.assertEqual(worse, ["neighbour"])
        # the improvement is still reported, but it does not decide the verdict
        self.assertEqual(better, ["guarded"])

    def test_a_single_worsening_function_is_load_bearing(self):
        v, _w, _b = self.mod.verdict_for_deltas({"a": (8, 12)})
        self.assertEqual(v, "LOAD-BEARING")

    def test_an_empty_delta_set_is_not_harmful(self):
        v, _w, _b = self.mod.verdict_for_deltas({})
        self.assertEqual(v, "DEAD")
        self.assertNotEqual(v, "HARMFUL")

    def test_a_prototype_is_not_a_definition(self):
        self.assertIsNone(self.mod.FNDEF.match("void msgPost(int idx);"))
        self.assertIsNone(self.mod.FNDEF.match("extern int helptab_num(void);"))

    def test_an_exact_function_reads_as_real_zero_not_unmeasured(self):
        """fndiff prints `OK <fn>` with no `real` field for an exact function.

        Reading only the `real` token returned None for every exact function
        and reported the region UNMEASURED -- 55 of 139 at a9c09f62, and
        exactly the ones that matter: an EXACT function going non-zero is the
        regression signal. Verified after the fix on game/audio/adstream,
        whose four regions went from UNMEASURED to LOAD-BEARING, one of them
        protecting adsMoveRawToCooked from real 0 -> 97.
        """
        import subprocess
        from types import SimpleNamespace
        calls = {}

        def fake_run(cmd, *a, **kw):
            return SimpleNamespace(stdout=calls["out"], stderr="",
                                   returncode=0)
        real_sub = self.mod.subprocess
        self.mod.subprocess = SimpleNamespace(run=fake_run,
                                              PIPE=subprocess.PIPE)
        self.addCleanup(setattr, self.mod, "subprocess", real_sub)

        calls["out"] = "(rebuilt adstream.o)\nOK   adsMoveRawToCooked\n"
        self.assertEqual(self.mod.real_of("game/audio/adstream",
                                          "adsMoveRawToCooked"), 0)
        calls["out"] = "POOL msgWidth  (0 real diff lines after pool-name)\n"
        self.assertEqual(self.mod.real_of("game/ui/message", "msgWidth"), 0)
        calls["out"] = "DIFF msgPost  insns 387/387  lines 700  real 8\n"
        self.assertEqual(self.mod.real_of("game/ui/message", "msgPost"), 8)
        # a name that appears in NO verdict line stays unmeasured, not 0
        calls["out"] = "OK   someOtherFunction\n"
        self.assertIsNone(self.mod.real_of("game/ui/message", "msgPost"))

    def test_a_non_pragma_line_is_not_matched(self):
        for line in ("/* #pragma opt_propagation off */",
                     "#include <stdio.h>",
                     "#pragma opt_propagation"):
            self.assertIsNone(self.mod.PRAGMA.match(line), line)


if __name__ == "__main__":
    unittest.main()
