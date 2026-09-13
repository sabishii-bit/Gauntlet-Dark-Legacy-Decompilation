"""The liveness-shaped positive-control finder, and what it must refuse.

`shapegrep` slices the target by opcode shape, so it cannot express a
LIVENESS property: hunting game/ui/message::msgDraw's residual (2 ranges
PERMUTED r29->r25, per `savedregs --per-web`) with `shapegrep addi,srawi
--exact-only` returns ml_mem::AllocFile, whose hits come from three unrelated
`>> 10` shifts on separate variables. web_reuse_census groups DEFINITIONS by
callee-saved register instead, so a hit is a byte-exact function where one
register genuinely holds the same computation twice.

Measured at 1e91f9b1 over 1309 byte-exact functions: 487 hits at the default
gap, and the population split that reframed msgDraw — 249 (role, function)
occurrences keep a repeated role in ONE callee-saved register against 608 that
spread it over two or more, i.e. **70.9% of repeated roles in byte-exact code
are split across registers**. Splitting is therefore the NORM in correct
output, not the anomaly our msgDraw form looked like, which moves that residual
from "why did it split" to "why r25 rather than r29".

TWO-SIDED. Positive: a distant same-role pair in a callee-saved register is
reported, and the role signature pairs across differing destination registers.
Negative, each of which must report NOTHING: stores and `stmw` are not
definitions (their first operand is a source), volatile registers are out of
scope, a pair closer than --gap is adjacent redefinition rather than two live
ranges, a function below the fuzzy floor is not a positive control, and two
DIFFERENT roles in one register belong to --distinct-roles, not the default.
"""
import importlib.util
import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

SPEC = (Path(__file__).resolve().parent.parent
        / "composed_census" / "web_reuse_census.py")


def load_module():
    spec = importlib.util.spec_from_file_location("web_reuse_census", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def insn(addr: int, text: str) -> str:
    """One dtk-format disassembly line; the 4 opcode bytes are not parsed."""
    return f"/* {addr:08X} 000AAAAA  00 00 00 00 */\t{text}"


class WebReuseCensusTest(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()
        self.tmp = TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.asm = Path(self.tmp.name) / "asm"
        (self.asm / "game" / "ui").mkdir(parents=True)
        self.mod.ASM_DIR = self.asm
        self.report = Path(self.tmp.name) / "report.json"
        self.mod.REPORT = self.report

    def write_fn(self, name, lines, fuzzy=100.0, path="game/ui/probe.s"):
        p = self.asm / path
        p.parent.mkdir(parents=True, exist_ok=True)
        body = "\n".join([f".fn {name}, global", *lines, ".endfn"])
        p.write_text(body + "\n")
        self.report.write_text(json.dumps({
            "units": [{"name": "main/game/ui/probe",
                       "functions": [{"name": name,
                                      "fuzzy_match_percent": fuzzy}]}]}))

    def census(self, **kw):
        opts = dict(min_fuzzy=100.0, gap=0x20, role_filter=None,
                    distinct=False, grep=None)
        opts.update(kw)
        return self.mod.census(**opts)

    # ---------- positive ----------

    def test_distant_same_role_in_one_register_is_reported(self):
        self.write_fn("reuser", [
            insn(0x800A0000, "addi r29, r3, 0x2"),
            insn(0x800A0004, "srawi r29, r29, 0x1"),
            insn(0x800A0100, "addi r29, r3, 0x2"),
            insn(0x800A0104, "srawi r29, r29, 0x1"),
        ])
        rows = self.census()
        hit = [r for r in rows if r["role"].startswith("addi")]
        self.assertEqual(len(hit), 1, rows)
        self.assertEqual(hit[0]["register"], "r29")
        self.assertEqual(hit[0]["offsets"], [0x0, 0x100])
        self.assertEqual(hit[0]["count"], 2)

    def test_role_signature_ignores_the_destination_register(self):
        """The target's r29 and our r25 must carry the SAME role."""
        a = self.mod.role_of("addi", "r29, r3, 0x2")
        b = self.mod.role_of("addi", "r25, r3, 0x2")
        self.assertEqual(a, b)
        # a different source operand is a different role
        c = self.mod.role_of("addi", "r29, r4, 0x3")
        self.assertNotEqual(a, c)

    # ---------- negative ----------

    def test_stores_are_not_definitions(self):
        """`stw r29, ...` reads r29; counting it invents a live range."""
        self.assertIsNone(self.mod.is_definition("stw", "r29, 0x8(r1)"))
        self.assertIsNone(self.mod.is_definition("stmw", "r22, 0x8(r1)"))
        self.write_fn("storer", [
            insn(0x800A0000, "stmw r22, 0x8(r1)"),
            insn(0x800A0004, "stw r29, 0xc(r1)"),
            insn(0x800A0100, "stw r29, 0x10(r1)"),
        ])
        self.assertEqual(self.census(), [])

    def test_branches_and_compares_are_not_definitions(self):
        for op, ops in (("b", "0x800A0100"), ("beq", "0x800A0100"),
                        ("cmpwi", "r29, 0x0"), ("mtlr", "r29"),
                        ("lmw", "r22, 0x8(r1)")):
            self.assertIsNone(self.mod.is_definition(op, ops), op)

    def test_volatile_registers_are_out_of_scope(self):
        """r5 is not callee-saved; its reuse says nothing about numbering."""
        self.write_fn("volatile_only", [
            insn(0x800A0000, "addi r5, r3, 0x2"),
            insn(0x800A0100, "addi r5, r3, 0x2"),
        ])
        self.assertEqual(self.census(), [])

    def test_adjacent_redefinition_is_below_the_gap(self):
        self.write_fn("adjacent", [
            insn(0x800A0000, "addi r29, r3, 0x2"),
            insn(0x800A0004, "addi r29, r3, 0x2"),
        ])
        self.assertEqual(self.census(), [])
        # ...but it IS reported once the gap is lowered to admit it
        self.assertTrue(self.census(gap=0))

    def test_a_non_exact_function_is_not_a_positive_control(self):
        self.write_fn("inexact", [
            insn(0x800A0000, "addi r29, r3, 0x2"),
            insn(0x800A0100, "addi r29, r3, 0x2"),
        ], fuzzy=99.9)
        self.assertEqual(self.census(), [])
        self.assertTrue(self.census(min_fuzzy=None))

    def test_two_different_roles_are_not_a_repeated_role_hit(self):
        self.write_fn("two_roles", [
            insn(0x800A0000, "addi r29, r3, 0x2"),
            insn(0x800A0100, "mr r29, r4"),
        ])
        self.assertEqual(self.census(), [])
        distinct = self.census(distinct=True)
        self.assertEqual(len(distinct), 1, distinct)
        self.assertEqual(distinct[0]["kind"], "distinct")

    def test_role_filter_excludes_other_roles(self):
        self.write_fn("filtered", [
            insn(0x800A0000, "addi r29, r3, 0x2"),
            insn(0x800A0100, "addi r29, r3, 0x2"),
        ])
        self.assertTrue(self.census(role_filter="addi"))
        self.assertEqual(self.census(role_filter="srawi"), [])


if __name__ == "__main__":
    unittest.main()
