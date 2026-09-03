"""WC lane (run 44): does the commutative-exchange ZERO-FIELD test refuse a
provable site?  READ-ONLY EXPERIMENT -- webfrank.py is not modified.

AGENTS.md discipline 14: a guard's refusal is a measurement of the guard, not
only of the function.  game/pb/pb_texture::fn_800C72DC refuses at +0x9c with
`use of g7 is not value-equal to g28`, on

    ours   lwzx r7,r7,r30      (RA=7  RB=30)
    target lwzx r7,r28,r0      (RA=28 RB=0)

`lwzx` is in _COMMUTATIVE_31 with shifts (16,11), and the CROSSED pairing is
the right one: our r7 and the target's r0 are both defined at +0x90 by
`addi rD,rBase,4`, and our r30 and the target's r28 are the same loop counter,
zeroed at +0x24.  Two position-keyed uses of the RA|0 flag refuse it anyway:

  1. `zero_involved` bails whenever a 0 appears in ANY of the four operand
     values while EITHER field carries the RA|0 flag.  Here the 0 is the
     target's RB, where 0 is an ordinary GPR, and neither RA is 0.
  2. after a crossed remap the `zero_none` test is applied at the DESTINATION
     position, so an expected value that came from the RB slot is read as
     "the target has no base register here".

Both are position-keyed where the encoding rule is field-keyed.  This probe
re-runs the shipped driver with the flag following the field the value came
FROM, and reports (a) whether the site proves and (b) whether any rule that
proves today changes its verdict or its declaration counts.

    python tools/gdl/composed_census/wc_zerofield_probe.py
"""
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from cn_analyze import our_object, target_object                # noqa: E402


def refined_transfer(index, cur, tgt, relation, renaming, our_copy,
                     target_copy, substitutions, exchanges):
    """webfrank._value_equality_transfer with the two position-keyed uses of
    the RA|0 flag made field-keyed.  Everything else is copied verbatim."""
    opcode = cur >> 26
    if opcode in (46, 47):
        renaming = wf._recolor_transfer(index, cur, tgt, renaming)
        if opcode == 46:
            for number in range((cur >> 21) & 0x1F, 32):
                relation = wf._relation_define(relation, "g", number, number)
        return relation, renaming
    try:
        operands = wf.instruction_operands(cur)
    except ValueError as error:
        raise ValueError(f"+0x{index * 4:x}: {error}") from None
    allowed = 0
    for _, shift, _, _ in operands:
        allowed |= 0x1F << shift
    if (cur ^ tgt) & ~allowed:
        raise ValueError(
            f"+0x{index * 4:x}: non-register bits differ "
            f"(0x{cur:08x} vs 0x{tgt:08x})")
    fields = {
        shift: (bank, role, zero_none,
                (cur >> shift) & 0x1F, (tgt >> shift) & 0x1F)
        for bank, shift, role, zero_none in operands
    }
    compare_field = wf._compare_result_field(cur)
    pair = wf._commutative_shifts(cur)
    if pair is None and compare_field is not None:
        pair = (16, 11)
    remap: dict = {}
    remap_zero_none: dict = {}
    if pair is not None and all(shift in fields for shift in pair):
        first, second = pair
        bank, _, zero_1, cur_1, tgt_1 = fields[first]
        _, _, zero_2, cur_2, tgt_2 = fields[second]
        # CHANGE 1: the RA|0 hazard is that a field whose OWN encoding gives 0
        # the "no base register" meaning holds a 0 on one side and a register
        # on the other.  A 0 sitting in a field with no such meaning (an RB
        # slot) is an ordinary GPR and blocks nothing.
        zero_involved = ((zero_1 and 0 in (cur_1, tgt_1))
                         or (zero_2 and 0 in (cur_2, tgt_2)))
        straight = ((bank, cur_1, tgt_1) in relation
                    and (bank, cur_2, tgt_2) in relation)
        if not straight and not zero_involved \
                and (bank, cur_1, tgt_2) in relation \
                and (bank, cur_2, tgt_1) in relation:
            remap = {first: tgt_2, second: tgt_1}
            # CHANGE 2: the expected value moved between fields, so the flag
            # that governs it moves with it.
            remap_zero_none = {first: zero_2, second: zero_1}
            if compare_field is not None:
                exchanges.add((index, bank, cur_1, cur_2, tgt_1, tgt_2))
    for shift, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        expected = remap.get(shift, tgt_r)
        zero_none = remap_zero_none.get(shift, zero_none)
        if zero_none and (cur_r == 0 or expected == 0):
            if cur_r != expected:
                raise ValueError(
                    f"+0x{index * 4:x}: base register presence differs "
                    f"({bank}{cur_r} vs {bank}{expected})")
            continue
        if role not in ("u", "b"):
            continue
        if (bank, cur_r, expected) not in relation:
            raise ValueError(
                f"+0x{index * 4:x}: use of {bank}{cur_r} is not value-equal "
                f"to {bank}{expected}")
        if renaming.get((bank, cur_r)) != expected:
            substitutions.add((index, bank, cur_r, expected))
    for _, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        if zero_none and cur_r == 0:
            continue
        if role not in ("d", "b"):
            continue
        if our_copy is not None and target_copy is not None \
                and our_copy[1] == our_copy[2] \
                and target_copy[1] == target_copy[2]:
            continue
        relation = wf._relation_define(relation, bank, cur_r, tgt_r)
        wf._map_define(renaming, (bank, cur_r), tgt_r)
        if our_copy is not None and our_copy[0] == bank \
                and our_copy[1] == cur_r and our_copy[2] != cur_r:
            source = our_copy[2]
            relation |= {
                (bank, cur_r, other) for kind, one, other in tuple(relation)
                if kind == bank and one == source}
        if target_copy is not None and target_copy[0] == bank \
                and target_copy[1] == tgt_r and target_copy[2] != tgt_r:
            source = target_copy[2]
            relation |= {
                (bank, one, tgt_r) for kind, one, other in tuple(relation)
                if kind == bank and other == source}
    return relation, renaming


REAL_STRICT = wf.verify_consistent_recolor
CAPTURED = []


def recorder(current, target, **kwargs):
    CAPTURED.append((current, target, dict(kwargs)))
    return REAL_STRICT(current, target, **kwargs)


def verdict(pre, post, kwargs, *, closure, refined):
    """PROVED (with the substitutions it demands) or the verbatim refusal."""
    real = wf._value_equality_transfer
    if refined:
        wf._value_equality_transfer = refined_transfer
    demanded: list = []
    try:
        for _round in range(60):
            arguments = dict(kwargs)
            arguments["substitutions"] = list(demanded)
            arguments["compare_exchanges"] = []
            arguments["constant_equality"] = closure
            try:
                wf.verify_value_equality_recolor(pre, post, **arguments)
            except ValueError as failure:
                text = str(failure)
                grew = False
                import re
                for at, bank, o, t in re.findall(
                        r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)", text):
                    entry = {"at": "0x" + at, "bank": bank,
                             "ours": int(o), "target": int(t)}
                    if entry not in demanded:
                        demanded.append(entry)
                        grew = True
                if not grew:
                    return f"REFUSED -- {text}", demanded
                continue
            return "PROVED", demanded
        return "REFUSED -- did not converge", demanded
    finally:
        wf._value_equality_transfer = real


def main():
    config = json.load(open(os.path.join(ROOT, "config", "GUNE5D",
                                         "webfrank.json"), encoding="utf-8"))
    wf.verify_consistent_recolor = recorder
    targets, controls = [], []
    for unit, rules in config.get("units", config).items():
        for rule in rules:
            if rule.get("unproven_recolor_audit"):
                targets.append((unit, rule))
            elif rule.get("value_equality_recolor"):
                controls.append((unit, rule))

    print("PAYOFF CANDIDATES (rules still resting on unproven_recolor_audit)")
    payoff = 0
    for unit, rule in targets:
        odata = bytearray(open(our_object(unit)[0], "rb").read())
        tdata = bytes(open(target_object(unit), "rb").read())
        CAPTURED.clear()
        wf.apply_patch(odata, json.loads(json.dumps(rule)), tdata)
        pre, post, kwargs = CAPTURED[0]
        proof = {"jumptable_targets": kwargs.get("jumptable_targets", ()),
                 "relocated_offsets": kwargs.get("relocated_offsets", ()),
                 "target_relocated_offsets": (),
                 "call_targets": kwargs.get("call_targets")}
        shipped, _d = verdict(pre, post, proof, closure=True, refined=False)
        refined, demanded = verdict(pre, post, proof, closure=True,
                                    refined=True)
        changed = "  <== PAYOFF" if (refined == "PROVED"
                                     and shipped != "PROVED") else ""
        if changed:
            payoff += 1
        print(f"  {unit}::{rule['function']}{changed}")
        print(f"      shipped guards : {shipped}")
        print(f"      refined guards : {refined}"
              + (f"; would declare {len(demanded)} substitution(s): "
                 + ", ".join(f"+{d['at']} {d['bank']}{d['ours']}->"
                             f"{d['bank']}{d['target']}" for d in demanded)
                 if refined == "PROVED" else ""))

    print()
    print("CONTROLS (rules that prove today; neither verdict nor declaration "
          "count may move)")
    failed = 0
    for unit, rule in controls:
        odata = bytearray(open(our_object(unit)[0], "rb").read())
        tdata = bytes(open(target_object(unit), "rb").read())
        CAPTURED.clear()
        wf.apply_patch(odata, json.loads(json.dumps(rule)), tdata)
        pre, post, kwargs = CAPTURED[0]
        declared = rule["value_equality_recolor"]
        proof = {
            "jumptable_targets": kwargs.get("jumptable_targets", ()),
            "relocated_offsets": kwargs.get("relocated_offsets", ()),
            "target_relocated_offsets": kwargs.get(
                "target_relocated_offsets", ()),
            "call_targets": kwargs.get("call_targets"),
            "substitutions": declared.get("substitutions", ()),
            "compare_exchanges": declared.get("compare_exchanges", ()),
            "constant_equality": bool(declared.get("constant_equality")),
        }
        results = {}
        for label, refined in (("shipped", False), ("refined", True)):
            real = wf._value_equality_transfer
            if refined:
                wf._value_equality_transfer = refined_transfer
            try:
                wf.verify_value_equality_recolor(pre, post, **proof)
                results[label] = "PROVED"
            except ValueError as failure:
                results[label] = f"REFUSED -- {failure}"
            finally:
                wf._value_equality_transfer = real
        held = results["shipped"] == results["refined"] == "PROVED"
        if not held:
            failed += 1
        print(f"  {'held  ' if held else '!! MOVED'} "
              f"{unit}::{rule['function']}  shipped={results['shipped']}  "
              f"refined={results['refined']}")
    print()
    print(f"payoff rows: {payoff}; controls moved: {failed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
