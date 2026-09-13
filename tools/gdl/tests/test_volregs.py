"""The volatile-register residual classifier, and the two bugs it had.

`savedregs` states that it never reads volatile registers, so three stuck
residuals sat in that blind spot. volregs classifies every differing aligned
row as COLOUR / SAVED-COLOUR / MATERIALIZE / STRUCTURAL / UNPAIRED and reports
whether one relabeling explains the volatile rows.

Measured at 54e14df4 once both bugs below were fixed:

  game/g3d/g3dpad::G3DUpdatePadStatus      250 identical, 32 COLOUR, nothing
      else at all; correspondence r7->r9, r9->r7 — the whole 72-line residual
      that a prior campaign closed with a (now retired) postprocessing rule is
      ONE transposition.
  game/g3d/gcontrolpads::G3DReadControlPadStates
      5 COLOUR (r6<->r8), 2 MATERIALIZE, 2 STRUCTURAL, 2 UNPAIRED.
  game/ui/message::msgDraw                 0 COLOUR, 5 SAVED-COLOUR,
      8 STRUCTURAL — and all 8 are `lfs f1,0(0)` against a NAMED target pool
      symbol vs our anonymous `@187`/`@236`, i.e. pool naming, not colour.

Census over all 336 non-exact functions of NonMatching TUs: CONSISTENT 95,
INCONSISTENT 155, NO-COLOUR 86, and 286 carry STRUCTURAL rows against only 16
with MATERIALIZE rows. So the bulk of remaining work is real difference, not
unreachable colour.

TWO BUGS THIS FILE PINS.

1. Registers live INSIDE operands. Comparing whole operand strings read
   `lwz r4,0(r8)` against `lwz r4,0(r6)` as STRUCTURAL — a plain colour row
   mislabelled a real difference, measured on G3DReadControlPadStates.
2. A register-for-register swap touching the callee-saved bank is savedregs'
   subject. Classing it STRUCTURAL double-counted its findings as real
   differences: msgDraw read 13 STRUCTURAL before the SAVED-COLOUR split, 5 of
   them its already-known callee-saved permutation.

TWO-SIDED. Positive: volatile swaps are COLOUR including inside memory
operands, a bijection is CONSISTENT, and constant-vs-copy is MATERIALIZE.
Negative: a differing immediate is STRUCTURAL and never colour; a callee-saved
swap is SAVED-COLOUR and never STRUCTURAL; a many-to-one map is INCONSISTENT
in either direction; an empty map is NO-COLOUR, not CONSISTENT.
"""
import importlib.util
import unittest
from collections import defaultdict
from pathlib import Path

SPEC = Path(__file__).resolve().parent.parent / "volregs.py"


def load_module():
    spec = importlib.util.spec_from_file_location("volregs", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def maps(pairs):
    fwd, back = defaultdict(set), defaultdict(set)
    for a, b in pairs:
        fwd[a].add(b)
        back[b].add(a)
    return fwd, back


class VolregsClassifyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load_module()

    # ---------- positive ----------

    def test_volatile_swap_is_colour(self):
        kind, subs = self.mod.classify("addi r8,r31,8", "addi r6,r31,8")
        self.assertEqual(kind, "COLOUR")
        self.assertEqual(subs, [("r8", "r6")])

    def test_volatile_swap_inside_a_memory_operand_is_colour(self):
        """Bug 1: `0(r8)` vs `0(r6)` differ by one register and nothing else."""
        kind, subs = self.mod.classify("lwz r4,0(r8)", "lwz r4,0(r6)")
        self.assertEqual(kind, "COLOUR")
        self.assertEqual(subs, [("r8", "r6")])

    def test_constant_versus_copy_is_materialize(self):
        for t, o in (("addi r7,r29,0", "li r7,0"), ("mr r30,r29", "li r30,0")):
            self.assertEqual(self.mod.classify(t, o)[0], "MATERIALIZE", (t, o))
        # and symmetrically, whichever stream loads the constant
        self.assertEqual(self.mod.classify("li r7,0", "addi r7,r29,0")[0],
                         "MATERIALIZE")

    def test_a_bijection_is_consistent(self):
        fwd, back = maps([("r7", "r9"), ("r9", "r7")])
        self.assertEqual(self.mod.verdict_for(fwd, back)[2], "CONSISTENT")

    def test_skeleton_pulls_registers_out_of_an_operand(self):
        skel, regs = self.mod.skeleton("0(r8)")
        self.assertEqual(regs, ["r8"])
        self.assertEqual(skel, self.mod.skeleton("0(r6)")[0])

    # ---------- negative ----------

    def test_a_differing_immediate_is_structural_not_colour(self):
        kind, subs = self.mod.classify("addi r8,r31,8", "addi r8,r31,12")
        self.assertEqual(kind, "STRUCTURAL")
        self.assertEqual(subs, [])

    def test_a_callee_saved_swap_is_saved_colour_not_structural(self):
        """Bug 2: this is savedregs' subject, not a real difference here."""
        kind, _subs = self.mod.classify("addi r29,r3,2", "addi r25,r3,2")
        self.assertEqual(kind, "SAVED-COLOUR")

    def test_a_named_versus_anonymous_pool_symbol_is_structural(self):
        """msgDraw's 8 rows: identical words, different relocation spelling."""
        kind, _ = self.mod.classify("lfs f1,0(0)  @lbl_80348618(EMB_SDA21)",
                                    "lfs f1,0(0)  @@187(EMB_SDA21)")
        self.assertEqual(kind, "STRUCTURAL")

    def test_a_register_against_a_non_register_is_structural(self):
        self.assertEqual(self.mod.classify("mr r3,r4", "mr r3,0")[0],
                         "STRUCTURAL")

    def test_many_to_one_is_inconsistent_in_either_direction(self):
        fwd, back = maps([("r7", "r9"), ("r7", "r8")])
        amb, _amb_back, verdict = self.mod.verdict_for(fwd, back)
        self.assertEqual(verdict, "INCONSISTENT")
        self.assertEqual(amb, {"r7": ["r8", "r9"]})
        # and the reverse direction must be caught too, not just the forward
        fwd, back = maps([("r7", "r9"), ("r8", "r9")])
        _amb, amb_back, verdict = self.mod.verdict_for(fwd, back)
        self.assertEqual(verdict, "INCONSISTENT")
        self.assertEqual(amb_back, {"r9": ["r7", "r8"]})

    def test_no_substitutions_is_no_colour_not_consistent(self):
        fwd, back = maps([])
        self.assertEqual(self.mod.verdict_for(fwd, back)[2], "NO-COLOUR")

    def test_abi_fixed_registers_are_not_volatile_colour(self):
        """r1/r2/r13 are ABI plumbing; a difference there is not colour."""
        kind, _ = self.mod.classify("addi r3,r1,8", "addi r3,r13,8")
        self.assertEqual(kind, "STRUCTURAL")


if __name__ == "__main__":
    unittest.main()
