"""The synthetic-compile harness, and the two parser bugs it had.

Every source-shape question in this project had been answered by editing a real
TU and rebuilding through ninja. synthprobe answers the prior question -- what
does MWCC DO with this shape? -- by compiling a standalone file with the exact
argv ninja uses for a chosen TU and disassembling one function. ~0.15s per
compile, nothing under src/ touched.

These tests are PARSER tests and need no compiler: they run the row analysis
over disassembly text, which is what the reasoning actually rests on.

TWO BUGS PINNED HERE, both found by running the real thing.

1. A ZERO STRIDE IS NOT AN INDUCTION VARIABLE. `addi rX,rX,0` is how MWCC
   completes an address (`lis rX,sym@ha` then `addi rX,rX,sym@l`), and objdump
   prints the low half as literal 0 with the relocation on its own line. The
   first version read it as an IV and reported `r3+=0` on the WorldSaveInitState
   model -- where r3 IS genuinely a stride-12 IV -- so a last-wins dict replaced
   the real stride with the address artifact.
2. `stmw` HIDES THE SAVE COUNT. `stmw r29,12(r1)` saves r29, r30 and r31; the
   frame-size questions this harness exists to answer all turn on HOW MANY
   callee-saved registers a shape costs, and a single opaque instruction
   reported one.

TWO-SIDED. Positive: rows parse with mnemonic and operands; frame comes off the
prologue; stmw expands; multiple distinct strides are both reported; MWCC's
`addi rD,rS,0` copy spelling is found. Negative: r1's stack pop is never an IV;
a zero stride is never an IV; a register with two strides is not an
unambiguous IV; a function name that is absent yields no rows and never the
next function's rows.
"""
import importlib.util
import unittest
from pathlib import Path

SPEC = Path(__file__).resolve().parent.parent / "synthprobe.py"

# Two functions, so an absent name cannot silently return the other's rows.
DISASM = """
build/synthprobe/x.o:     file format elf32-powerpc


Disassembly of section .text:

00000000 <probefn>:
       0:\t7c 08 02 a6 \tmflr    r0
       4:\t3c 60 00 00 \tlis     r3,0
\t\t\t4: R_PPC_ADDR16_HA\tgName
       8:\t90 01 00 04 \tstw     r0,4(r1)
       c:\t94 21 ff e8 \tstwu    r1,-24(r1)
      10:\tbf a1 00 0c \tstmw    r29,12(r1)
      14:\t3b c0 00 00 \tli      r30,0
      18:\t3b e3 00 00 \taddi    r31,r3,0
\t\t\t18: R_PPC_ADDR16_LO\tgName
      1c:\t38 a0 00 00 \tli      r5,0
      20:\t38 be 00 00 \taddi    r5,r30,0
      24:\t3b de 00 3c \taddi    r30,r30,60
      28:\t38 63 00 0c \taddi    r3,r3,12
      2c:\t38 63 00 00 \taddi    r3,r3,0
\t\t\t2c: R_PPC_ADDR16_LO\tgFmt
      30:\t38 21 00 18 \taddi    r1,r1,24
      34:\t4e 80 00 20 \tblr

00000038 <otherfn>:
      38:\t38 60 00 07 \tli      r3,7
      3c:\t4e 80 00 20 \tblr
"""


def load():
    spec = importlib.util.spec_from_file_location("synthprobe", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class SynthprobeParseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load()
        cls.rows = cls.mod.parse_rows(DISASM, "probefn")

    # ---------- positive ----------

    def test_rows_carry_mnemonic_and_operands(self):
        self.assertEqual(self.rows[0], ("mflr", "r0"))
        self.assertEqual(self.rows[4], ("stmw", "r29,12(r1)"))
        # 14 instructions in probefn; 16 would mean the parser ran on
        # into otherfn, which is the whole point of the break.
        self.assertEqual(len(self.rows), 14)

    def test_frame_comes_from_the_prologue(self):
        self.assertEqual(self.mod.frame(self.rows), 24)

    def test_stmw_expands_to_every_register_it_saves(self):
        """Bug 2: the save COUNT is what frame questions turn on."""
        self.assertEqual(self.mod.saved(self.rows), ["r29", "r30", "r31"])

    def test_distinct_strides_are_both_reported(self):
        self.assertEqual(self.mod.self_increments(self.rows),
                         {"r30": [60], "r3": [12]})

    def test_unambiguous_induction_variables(self):
        self.assertEqual(self.mod.induction_variables(self.rows),
                         {"r30": 60, "r3": 12})

    def test_zero_literals_are_reported_in_emission_order(self):
        self.assertEqual(self.mod.zero_literals(self.rows), ["r30"])
        self.assertEqual(self.mod.zero_literals(self.rows,
                                                self.mod.VOLATILE), ["r5"])

    def test_the_copy_spelling_is_found(self):
        """`addi rD,rS,0` is MWCC's 'reuse the register I know holds this'."""
        self.assertEqual(self.mod.copies_of(self.rows, "r30"), ["r5"])

    def test_ops_trims_fields(self):
        self.assertEqual(self.mod.ops("r30, r30, 60"), ["r30", "r30", "60"])

    # ---------- negative ----------

    def test_the_stack_pop_is_never_an_induction_variable(self):
        self.assertNotIn("r1", self.mod.self_increments(self.rows))

    def test_a_zero_stride_is_never_an_induction_variable(self):
        """Bug 1: `addi r3,r3,0` is an address low half, not an IV.

        r3 carries BOTH in this fixture, which is the case that made a
        last-wins dict report the real stride-12 IV as stride 0.
        """
        self.assertEqual(self.mod.self_increments(self.rows)["r3"], [12])
        self.assertNotIn(0, self.mod.self_increments(self.rows)["r3"])

    def test_a_register_with_two_strides_is_not_an_unambiguous_iv(self):
        rows = self.mod.parse_rows(DISASM, "probefn") + [
            ("addi", "r3,r3,4")]
        self.assertEqual(self.mod.self_increments(rows)["r3"], [4, 12])
        self.assertNotIn("r3", self.mod.induction_variables(rows))

    def test_an_absent_function_yields_no_rows_not_the_next_one(self):
        self.assertEqual(self.mod.parse_rows(DISASM, "missingfn"), [])
        self.assertEqual(self.mod.parse_rows(DISASM, "otherfn"),
                         [("li", "r3,7"), ("blr", "")])

    def test_frame_and_saved_are_empty_on_an_empty_function(self):
        self.assertIsNone(self.mod.frame([]))
        self.assertEqual(self.mod.saved([]), [])
        self.assertEqual(self.mod.self_increments([]), {})

    def test_a_store_to_a_non_stack_address_is_not_a_save(self):
        rows = [("stw", "r30,0(r12)"), ("stw", "r3,4(r1)")]
        self.assertEqual(self.mod.saved(rows), [])


if __name__ == "__main__":
    unittest.main()
