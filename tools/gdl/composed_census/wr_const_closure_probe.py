"""WR lane (run 42): does a CONSTANT-EQUALITY closure serve the two refusals?

READ-ONLY EXPERIMENT.  webfrank.py is not modified and nothing is shipped.
This re-runs verify_value_equality_recolor's own driver loop with ONE extra
state component -- a per-stream map register -> literal constant -- and seeds
the value-equality relation with (bank, ours, target) for every register pair
holding the SAME literal in the two streams.  It answers the question the
refusals raise: the relation is closed over value-preserving COPIES but not
over two INDEPENDENT constant loads of one literal, and both refusing sites
are exactly that shape.

    python tools/gdl/composed_census/wr_const_closure_probe.py \
        <unit> <function> [--extra FRAGMENT.json]

Prints, for the unmodified mode and for the constant-closed mode, either
PROVED (with the substitutions/exchanges the mode would have to declare) or
the guard's verbatim refusal.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                      # noqa: E402
from cn_analyze import our_object, target_object            # noqa: E402


def constants_after(word, index, relocated, previous):
    """`previous` updated for one word: {register: literal}, GPR bank only."""
    state = dict(previous)
    defined = set()
    try:
        operands = wf.instruction_operands(word)
    except ValueError:
        return {}
    for bank, shift, role, _zero in operands:
        if bank == "g" and role in ("d", "b"):
            defined.add((word >> shift) & 0x1F)
    for register in defined:
        state.pop(register, None)
    if index not in relocated:
        form = wf.decode_copy_form(word)
        if form is not None and form[0] == "li":
            state[form[1]] = form[2]
        elif form is not None and form[0] == "copy" and form[2] in previous:
            state[form[1]] = previous[form[2]]
    return state


def closure_pairs(ours, target):
    return {("g", o, t)
            for o, ko in ours.items()
            for t, kt in target.items() if ko == kt}


def prove(current, target, *, jumptable_targets, relocated_offsets,
          target_relocated_offsets, call_targets, constant_closure):
    words_cur = [wf._u32(current, o) for o in range(0, len(current), 4)]
    words_tgt = [wf._u32(target, o) for o in range(0, len(target), 4)]
    our_relocated = {o // 4 for o in relocated_offsets}
    target_relocated = {o // 4 for o in target_relocated_offsets}
    jumptable = {o // 4 for o in jumptable_targets}
    successors, calls = wf._successors(words_cur, our_relocated, jumptable)
    our_copies = [wf._value_preserving_copy(w, i, our_relocated)
                  for i, w in enumerate(words_cur)]
    target_copies = [wf._value_preserving_copy(w, i, target_relocated)
                     for i, w in enumerate(words_tgt)]

    identity_relation = {(b, n, n) for b in ("g", "f") for n in range(32)}
    identity_renaming = {("g", n): n for n in range(32)}
    identity_renaming.update({("f", n): n for n in range(32)})
    incoming = {0: (identity_relation, identity_renaming, {}, {})}
    substitutions, exchanges = set(), set()
    pending = [0]
    steps = 0
    while pending:
        index = pending.pop()
        steps += 1
        if steps > wf._VALUE_EQUALITY_STEP_LIMIT:
            return "did not converge", None, None
        relation, renaming, our_const, tgt_const = incoming[index]
        relation = set(relation)
        if constant_closure:
            relation |= closure_pairs(our_const, tgt_const)
        try:
            relation, renaming = wf._value_equality_transfer(
                index, words_cur[index], words_tgt[index],
                relation, dict(renaming),
                our_copies[index], target_copies[index],
                substitutions, exchanges)
        except ValueError as failure:
            return str(failure), None, None
        our_const = constants_after(words_cur[index], index, our_relocated,
                                    our_const)
        tgt_const = constants_after(words_tgt[index], index, target_relocated,
                                    tgt_const)
        if calls[index]:
            helper = wf._helper_call(
                call_targets.get(index * 4) if call_targets else None)
            if helper is None:
                volatile = frozenset(wf._CALL_VOLATILE)
                relation = {e for e in relation
                            if (e[0], e[1]) not in volatile
                            and (e[0], e[2]) not in volatile}
                for key in wf._CALL_VOLATILE:
                    renaming.pop(key, None)
                    if key[0] == "g":
                        our_const.pop(key[1], None)
                        tgt_const.pop(key[1], None)
                for key in wf._CALL_RETURNS:
                    relation = wf._relation_define(relation, key[0], key[1],
                                                   key[1])
                    wf._map_define(renaming, key, key[1])
            elif helper[0] == "rest":
                _, bank, first = helper
                for number in range(first, 32):
                    relation = wf._relation_define(relation, bank, number,
                                                   number)
                    wf._map_define(renaming, (bank, number), number)
        for successor in successors[index]:
            known = incoming.get(successor)
            if known is None:
                merged = (set(relation), dict(renaming), dict(our_const),
                          dict(tgt_const))
            else:
                merged = (
                    known[0] & relation,
                    {k: v for k, v in known[1].items()
                     if renaming.get(k) == v},
                    {k: v for k, v in known[2].items()
                     if our_const.get(k) == v},
                    {k: v for k, v in known[3].items()
                     if tgt_const.get(k) == v},
                )
            if known is None or merged != known:
                incoming[successor] = merged
                pending.append(successor)
    for site in sorted(exchanges):
        index = site[0]
        field = wf._compare_result_field(words_cur[index])
        if field is None or not wf._compare_exchange_is_semantics_preserving(
                words_cur, index, field, successors, calls, call_targets):
            return (f"+0x{index * 4:x}: comparison exchange is not "
                    f"equivalence-preserving"), None, None
    return None, sorted(substitutions), sorted(exchanges)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--extra", default=None)
    arguments = parser.parse_args()

    ours_path, kind = our_object(arguments.unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_object(arguments.unit), "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    osym = wf._find_symbol(odata, osec, arguments.function)
    tsym = wf._find_symbol(tdata, tsec, arguments.function)
    ostart = osec[osym.section_index].offset + osym.value
    tstart = tsec[tsym.section_index].offset + tsym.value
    target = bytes(tdata[tstart:tstart + tsym.size])

    extra = {}
    if arguments.extra:
        extra = json.load(open(arguments.extra, encoding="utf-8"))
    # Reproduce the images the register stage actually sees: our raw body with
    # any declared pre-recolor equivalent_copy_form edits applied, and
    # copy_register_fields' output on top of it.
    pre_image = bytes(odata[ostart:ostart + osym.size])
    relocations = wf._function_text_relocations(
        odata, osec, osym.section_index, osym.value, osym.value + osym.size)
    target_relocations = wf._function_text_relocations(
        tdata, tsec, tsym.section_index, tsym.value, tsym.value + tsym.size)
    jumptable = wf._jumptable_targets(
        odata, osec, osym.section_index, osym.value, osym.value + osym.size)
    call_targets = {o: n for o, (k, n) in relocations.items() if k == 10}

    if extra.get("equivalent_copy_form"):
        pre_image, _c = wf.equivalent_copy_form(
            pre_image, target, extra["equivalent_copy_form"],
            relocated_offsets=set(relocations),
            target_relocated_offsets=set(target_relocations),
            jumptable_offsets=jumptable, call_targets=call_targets,
            relocation_types={o // 4: k
                              for o, (k, _n) in relocations.items()})
    post_image, _n = wf.copy_register_fields(pre_image, target)

    print(f"{arguments.unit}::{arguments.function}  ({kind}, "
          f"{len(pre_image)//4} insns)")
    for label, closure in (("shipped value-equality  ", False),
                           ("+ constant-equality     ", True)):
        failure, subs, exch = prove(
            pre_image, post_image,
            jumptable_targets=jumptable,
            relocated_offsets=set(relocations),
            target_relocated_offsets=set(target_relocations),
            call_targets=call_targets, constant_closure=closure)
        if failure:
            print(f"  {label}: REFUSED -- {failure}")
        else:
            print(f"  {label}: PROVED; would declare "
                  f"{len(subs)} substitution(s), {len(exch)} exchange(s)")
            for site in subs[:12]:
                print(f"      sub {wf._format_substitution(site)}")
            for site in exch[:6]:
                print(f"      exch {wf._format_exchange(site)}")
            print(f"      byte-equal to target: "
                  f"{post_image == target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
