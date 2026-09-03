"""WP lane (run 45): which guard does each refusal test actually pin?

AGENTS.md two-sided rule: "refusal tests shown to FAIL without their guard".
An assertion that a test guards something is a claim to CHECK, not to state:
this installs one transfer function per deleted guard in place of the shipped
one and prints what each body does under each, so a guard that is load-bearing
and a guard that is merely present look different.

  SHIPPED   = webfrank._value_equality_transfer as it now stands
  NO_ZERO   = the per-field RA|0 presence test deleted
  NO_GATE   = the zero_involved gate deleted (the exchange is taken whenever
              the crossed pairs are in the relation)
  DEST_KEY  = the run-44 position-keyed behaviour restored for the remapped
              flag only (remap_zero_none dropped)

MEASURED at 444cd82f2, and the reason this tool exists rather than a sentence
in a docstring: only TWO of the four bodies flip when a guard is removed.
The two `RA=0 on one side` bodies read REFUSED under every variant, because
with the RA|0 presence test deleted the value-equality relation check refuses
them anyway -- they pin a refusal and its message, not a single guard. A first
draft of CommutativeExchangeZeroFieldTests' docstring claimed "delete that
test and both bodies prove"; running this table showed that was false.

    python tools/gdl/composed_census/wp_guard_two_sided.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import webfrank as wf                                          # noqa: E402
from tools.gdl.tests import test_webfrank as T                 # noqa: E402


def _variant(*, keep_gate, keep_travel, keep_zero_test=True):
    def transfer(index, cur, tgt, relation, renaming, our_copy, target_copy,
                 substitutions, exchanges):
        opcode = cur >> 26
        if opcode in (46, 47):
            renaming = wf._recolor_transfer(index, cur, tgt, renaming)
            if opcode == 46:
                for number in range((cur >> 21) & 0x1F, 32):
                    relation = wf._relation_define(relation, "g", number,
                                                   number)
            return relation, renaming
        operands = wf.instruction_operands(cur)
        allowed = 0
        for _, shift, _, _ in operands:
            allowed |= 0x1F << shift
        if (cur ^ tgt) & ~allowed:
            raise ValueError(f"+0x{index * 4:x}: non-register bits differ")
        fields = {shift: (bank, role, zero_none,
                          (cur >> shift) & 0x1F, (tgt >> shift) & 0x1F)
                  for bank, shift, role, zero_none in operands}
        compare_field = wf._compare_result_field(cur)
        pair = wf._commutative_shifts(cur)
        if pair is None and compare_field is not None:
            pair = (16, 11)
        remap, remap_zero_none = {}, {}
        if pair is not None and all(shift in fields for shift in pair):
            first, second = pair
            bank, _, zero_1, cur_1, tgt_1 = fields[first]
            _, _, zero_2, cur_2, tgt_2 = fields[second]
            zero_involved = ((zero_1 and 0 in (cur_1, tgt_1))
                             or (zero_2 and 0 in (cur_2, tgt_2)))
            if not keep_gate:
                zero_involved = False
            straight = ((bank, cur_1, tgt_1) in relation
                        and (bank, cur_2, tgt_2) in relation)
            if not straight and not zero_involved \
                    and (bank, cur_1, tgt_2) in relation \
                    and (bank, cur_2, tgt_1) in relation:
                remap = {first: tgt_2, second: tgt_1}
                if keep_travel:
                    remap_zero_none = {first: zero_2, second: zero_1}
                if compare_field is not None:
                    exchanges.add((index, bank, cur_1, cur_2, tgt_1, tgt_2))
        for shift, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
            expected = remap.get(shift, tgt_r)
            zero_none = remap_zero_none.get(shift, zero_none)
            if keep_zero_test and zero_none and (cur_r == 0 or expected == 0):
                if cur_r != expected:
                    raise ValueError(
                        f"+0x{index * 4:x}: base register presence differs "
                        f"({bank}{cur_r} vs {bank}{expected})")
                continue
            if role not in ("u", "b"):
                continue
            if (bank, cur_r, expected) not in relation:
                raise ValueError(
                    f"+0x{index * 4:x}: use of {bank}{cur_r} is not "
                    f"value-equal to {bank}{expected}")
            if renaming.get((bank, cur_r)) != expected:
                substitutions.add((index, bank, cur_r, expected))
        for _, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
            if zero_none and cur_r == 0:
                continue
            if role not in ("d", "b"):
                continue
            relation = wf._relation_define(relation, bank, cur_r, tgt_r)
            wf._map_define(renaming, (bank, cur_r), tgt_r)
        return relation, renaming
    return transfer


C = T.CommutativeExchangeZeroFieldTests
W = T._words

BODIES = {
    "PAYOFF   crossing with GPR0 in the target RB": (C.OURS, C.TARGET),
    "REFUSAL  target RA=0 (no base register)": (
        C.OURS, W(T.LI_R28_0, T.ADDI_R28_R6_4, T.LWZX_R7_0_R28, T.BLR)),
    "REFUSAL  our RA=0 (no base register)": (
        W(T.LI_R30_0, T.ADDI_R7_R6_4, T.LWZX_R7_0_R30, T.BLR), C.TARGET),
    "REFUSAL  gate: our RB=GPR0 vs target with no base": (
        W(T.ADDI_R7_R6_4, T.LWZX_R7_R7_R0, T.BLR),
        W(T.ADDI_R28_R6_4, T.LWZX_R7_0_R28, T.BLR)),
    "REFUSAL  flag travels: our RB=0 after the crossing": (
        W(T.ADDI_R0_R6_4, T.ADDI_R7_R6_8, T.LWZX_R7_R7_R0, T.BLR),
        W(T.ADDI_R28_R6_4, T.ADDI_R5_R6_8, T.LWZX_R7_R28_R5, T.BLR)),
}

VARIANTS = (
    ("SHIPPED ", None),
    ("NO_ZERO ", _variant(keep_gate=True, keep_travel=True,
                          keep_zero_test=False)),
    ("NO_GATE ", _variant(keep_gate=False, keep_travel=True)),
    ("DEST_KEY", _variant(keep_gate=True, keep_travel=False)),
)


def main():
    real = wf._value_equality_transfer
    print(f"{'body':52} " + "  ".join(name for name, _ in VARIANTS))
    for label, (ours, target) in BODIES.items():
        cells = []
        for _name, variant in VARIANTS:
            wf._value_equality_transfer = real if variant is None else variant
            try:
                wf.verify_value_equality_recolor(ours, target)
                cells.append("PROVED  ")
            except ValueError:
                cells.append("REFUSED ")
            finally:
                wf._value_equality_transfer = real
        print(f"{label:52} " + "  ".join(cells))
    print()
    print("Reading the table. The PAYOFF row must read PROVED under SHIPPED")
    print("and REFUSED under DEST_KEY: that is the run-44 behaviour the")
    print("refinement replaces. A REFUSAL row that reads REFUSED under")
    print("SHIPPED and PROVED under one variant is a test whose guard is")
    print("LOAD-BEARING -- deleting the guard breaks the test. A REFUSAL row")
    print("that reads REFUSED under EVERY column is refused REDUNDANTLY (a")
    print("later check catches it too); it pins the refusal and its message")
    print("and is a negative control, but it does not by itself demonstrate")
    print("that any one guard is needed. Both kinds are here on purpose.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
