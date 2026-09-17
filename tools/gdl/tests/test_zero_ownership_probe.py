"""The canonical-zero A/B's role reader, and why it reads USE not position.

`claude/canonical-zero-rule.md` names one MWCC decision as the priority-1
research item: it gates game/world/world, game/g3d/gcontrolpads,
game/g3d/g3dpad and game/ui/message. volregs reads WorldSaveInitState as 111
target insns against 111 of ours with just TWO structural rows, and both are the
same disagreement -- which induction variable inherits the zero that was
materialized before the call sequence:

    target  r29 = i (stride 1) owns it; r5 = i*60 is a volatile COPY
    ours    r30 = i*60 owns it;         r11 = i is a fresh li

Whichever IV inherits it is callee-saved BY CONSTRUCTION, since its live range
crosses the calls, so this one choice sets the class of both.

THE ANSWER, for context: put the loop in its own (even `static inline`)
function with its own locals. The zero-store then lives in the caller, and the
merge falls to the loop's own counter -- the target's assignment, at the same
instruction count, frame and save set. `--controls` demonstrates it and
`--fidelity` asserts it against the real object. These tests cover the role
READER, which is what makes any of those comparisons meaningful.

THE ROLES ARE READ OFF USE, NEVER POSITION, and that is the point of these
tests. `base` is whichever register forms the +232 displacement; `memBase` is
whichever register the `subf` subtracts; an induction variable is named by its
stride. Reading them by register number instead would be circular -- the
numbering is exactly what differs between the two streams, and the probe's
whole job is to report an assignment that can be COMPARED against the target's.

TWO-SIDED. Positive: our real assignment and the target's are both recognized
from their instructions alone; a stride-1 IV holding the zero reads
`counter(zero)`; the signature tuple is ordered r29, r30, r31. Negative: the
zero marker attaches to exactly one register; a non-IV callee-saved register is
not given a stride; an address low half (`addi rX,rX,0`) does not become a
stride-0 IV; and an empty row list yields an all-None signature rather than a
spurious match against either stream.
"""
import importlib.util
import unittest
from pathlib import Path

SPEC = (Path(__file__).resolve().parent.parent
        / "composed_census" / "zero_ownership_probe.py")


def load():
    spec = importlib.util.spec_from_file_location("zop", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# The INLINE form (the old reconstruction), reduced to the rows the reader uses.
OURS_ROWS = [
    ("stwu", "r1,-24(r1)"), ("stmw", "r29,12(r1)"),
    ("li", "r30,0"), ("addi", "r31,r3,0"),
    ("stw", "r30,0(0)"), ("lwz", "r29,0(0)"),
    ("addi", "r12,r31,232"), ("li", "r11,0"), ("li", "r3,0"), ("li", "r4,0"),
    ("addi", "r11,r11,1"), ("addi", "r3,r3,12"), ("addi", "r4,r4,4"),
    ("addi", "r30,r30,60"), ("subf", "r0,r29,r0"),
]
# The target's, same reduction.
TARGET_ROWS = [
    ("stwu", "r1,-24(r1)"), ("stmw", "r29,12(r1)"),
    ("li", "r29,0"), ("addi", "r31,r3,0"),
    ("stw", "r29,0(0)"), ("lwz", "r30,0(0)"),
    ("addi", "r5,r29,0"), ("addi", "r12,r31,232"),
    ("li", "r3,0"), ("li", "r4,0"),
    ("addi", "r29,r29,1"), ("addi", "r3,r3,12"), ("addi", "r4,r4,4"),
    ("addi", "r5,r5,60"), ("subf", "r0,r30,r0"),
]


class RoleReaderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load()

    # ---------- positive ----------

    def test_the_inline_form_assignment_is_recognized(self):
        self.assertEqual(self.mod.sig(OURS_ROWS), self.mod.INLINED)
        self.assertEqual(self.mod.sig(OURS_ROWS),
                         ("memBase", "IV60(zero)", "base"))

    def test_the_target_assignment_is_recognized(self):
        self.assertEqual(self.mod.sig(TARGET_ROWS), self.mod.TARGET)
        self.assertEqual(self.mod.sig(TARGET_ROWS),
                         ("counter(zero)", "memBase", "base"))

    def test_a_stride_one_iv_holding_the_zero_reads_as_the_counter(self):
        roles, zero = self.mod.assignment(TARGET_ROWS)
        self.assertEqual(zero, "r29")
        self.assertEqual(roles["r29"], "counter(zero)")

    def test_base_is_found_by_its_displacement_not_its_number(self):
        roles, _ = self.mod.assignment(OURS_ROWS)
        self.assertEqual(roles["r31"], "base")
        moved = [(op, o.replace("r31", "r27")) for op, o in OURS_ROWS]
        self.assertEqual(self.mod.assignment(moved)[0]["r27"], "base")

    def test_the_signature_is_ordered_r29_r30_r31(self):
        roles, _ = self.mod.assignment(TARGET_ROWS)
        self.assertEqual(self.mod.sig(TARGET_ROWS),
                         (roles["r29"], roles["r30"], roles["r31"]))

    # ---------- negative ----------

    def test_only_one_register_is_marked_as_the_zero(self):
        rows = OURS_ROWS + [("li", "r28,0"), ("addi", "r28,r28,4")]
        roles, zero = self.mod.assignment(rows)
        self.assertEqual(zero, "r30")
        self.assertEqual([r for r, v in roles.items() if "(zero)" in v],
                         ["r30"])

    def test_a_callee_saved_register_that_is_not_an_iv_gets_no_stride(self):
        roles, _ = self.mod.assignment(OURS_ROWS)
        self.assertEqual(roles["r29"], "memBase")
        self.assertNotIn("IV", roles["r29"])

    def test_an_address_low_half_does_not_become_a_stride_zero_iv(self):
        rows = OURS_ROWS + [("addi", "r29,r29,0")]
        self.assertEqual(self.mod.assignment(rows)[0]["r29"], "memBase")

    def test_empty_rows_match_neither_stream(self):
        self.assertEqual(self.mod.sig([]), (None, None, None))
        self.assertNotEqual(self.mod.sig([]), self.mod.INLINED)
        self.assertNotEqual(self.mod.sig([]), self.mod.TARGET)

    def test_the_two_streams_are_not_equal(self):
        """A reader that collapsed them would report agreement forever."""
        self.assertNotEqual(self.mod.INLINED, self.mod.TARGET)
        self.assertNotEqual(self.mod.sig(OURS_ROWS),
                            self.mod.sig(TARGET_ROWS))

    def test_the_two_fixtures_really_differ_by_the_function_boundary(self):
        """The central claim is an A/B, so the two fixtures must stay distinct.

        Compiling is out of scope here (these are parser tests), but a
        "simplification" that collapsed the two forms into one would make
        --controls report agreement forever, which is the same failure mode as
        a role reader that collapsed the two signatures.
        """
        inline, helper = self.mod.INLINE, self.mod.HELPER
        self.assertIn("static inline void sHelper(void)", helper)
        self.assertNotIn("static inline void sHelper(void)", inline)
        self.assertIn("sHelper();", helper)
        # the loop is in the helper, not in the storing function
        self.assertLess(helper.index("for (i = 0;"), helper.index("probefn"))
        # and the inline form keeps its loop placeholder inside probefn
        self.assertGreater(inline.index("__LOOP__"), inline.index("probefn"))

    def test_the_target_and_inline_signatures_are_both_three_wide(self):
        """sig() reports r29/r30/r31, so a constant of the wrong arity would
        silently never match."""
        self.assertEqual(len(self.mod.TARGET), 3)
        self.assertEqual(len(self.mod.INLINED), 3)
        self.assertEqual(len(self.mod.sig([])), 3)


if __name__ == "__main__":
    unittest.main()
