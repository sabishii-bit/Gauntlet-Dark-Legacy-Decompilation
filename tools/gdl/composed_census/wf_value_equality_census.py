"""Image-wide screen for WebFrank's value-equality recolor mode.

Finds every function that is a PURE register-field difference -- no
permutation needed, `copy_register_fields` alone reaches the target bytes --
which the strict `verify_consistent_recolor` REFUSES but
`verify_value_equality_recolor` PROVES.  Those are the "inconsistent
renaming" refusals the mode was built for: both allocators replicated ONE
value across several registers and then read different members of that web,
which a function-valued renaming state cannot express at all.

Reads the RAW `.postprocess/body` output where present, so already-shipped
rules stay visible as canaries (game/pb/dbgtext::fn_800C031C is one).

    python tools/gdl/composed_census/wf_value_equality_census.py

Requires a completed `ninja` so both build/GUNE5D/src and build/GUNE5D/obj
are current; stale objects silently misreport.  A hit is a CANDIDATE: screen
the function's own attempt records and any foreign work_claim before writing
a rule, and derive the declarations with the real verifier rather than by
hand (the `prove` helper below is what the rule builders use).
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, HERE)
import webfrank as wf  # noqa: E402
from wf_census import units, our_path, functions, OBJ  # noqa: E402

SUBSTITUTION = re.compile(r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)")
EXCHANGE = re.compile(
    r"\+0x([0-9a-f]+) \(([gf])(\d+),[gf](\d+)\)<->\([gf](\d+),[gf](\d+)\)")


def prove(ours, target, jumptables, our_relocations, target_relocations,
          calls, rounds=60):
    """Harvest the declarations the SHIPPED verifier demands, by feeding its
    own refusals back to it until it accepts.

    Nothing here re-implements the proof, so a rule built from these
    declarations cannot drift from what the build will check.
    """
    substitutions, exchanges = [], []
    for _round in range(rounds):
        try:
            wf.verify_value_equality_recolor(
                ours, target,
                jumptable_targets=jumptables,
                relocated_offsets=our_relocations,
                target_relocated_offsets=target_relocations,
                call_targets=calls,
                substitutions=substitutions,
                compare_exchanges=exchanges)
            return substitutions, exchanges, None
        except ValueError as error:
            text = str(error)
            if "undeclared value-equality" in text:
                for at, bank, ours_r, target_r in SUBSTITUTION.findall(text):
                    substitutions.append({"at": f"0x{at}", "bank": bank,
                                          "ours": int(ours_r),
                                          "target": int(target_r)})
            elif "undeclared comparison" in text:
                for at, bank, c1, c2, t1, t2 in EXCHANGE.findall(text):
                    exchanges.append({"at": f"0x{at}", "bank": bank,
                                      "ours": [int(c1), int(c2)],
                                      "target": [int(t1), int(t2)]})
            else:
                return substitutions, exchanges, text
    return substitutions, exchanges, "did not converge"


def load_function(data, sections, symbol):
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return bytes(data[start:start + symbol.size])


def main():
    pinned = set()
    config = json.load(open(os.path.join(ROOT, "config", "GUNE5D",
                                         "webfrank.json"), encoding="utf-8"))
    for unit, rules in config.get("units", config).items():
        for rule in rules:
            pinned.add((unit, rule["function"]))

    hits, refused, scanned, candidates = [], {}, 0, 0
    for unit in units():
        path, _raw = our_path(unit)
        if not path:
            continue
        try:
            our_data = bytearray(open(path, "rb").read())
            target_data = bytearray(
                open(os.path.join(OBJ, unit + ".o"), "rb").read())
            our_sections = wf._sections(our_data)
            target_sections = wf._sections(target_data)
        except Exception:
            continue
        ours_by_name = {s.name: s for s in functions(our_data, our_sections)}
        target_by_name = {s.name: s
                          for s in functions(target_data, target_sections)}
        for name, our_symbol in ours_by_name.items():
            target_symbol = target_by_name.get(name)
            if target_symbol is None or not our_symbol.size:
                continue
            if target_symbol.size != our_symbol.size:
                continue
            scanned += 1
            ours = load_function(our_data, our_sections, our_symbol)
            target = load_function(target_data, target_sections, target_symbol)
            if ours == target:
                continue
            try:
                recolored, _n = wf.copy_register_fields(ours, target)
            except ValueError:
                continue
            if recolored != target:
                continue           # not a pure register-field difference
            candidates += 1
            our_relocations = wf._function_text_relocations(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            target_relocations = wf._function_text_relocations(
                target_data, target_sections, target_symbol.section_index,
                target_symbol.value, target_symbol.value + target_symbol.size)
            jumptables = wf._jumptable_targets(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            calls = {offset: name
                     for offset, (kind, name) in our_relocations.items()
                     if kind == 10}
            try:
                wf.verify_consistent_recolor(
                    ours, target, jumptable_targets=jumptables,
                    relocated_offsets=set(our_relocations),
                    call_targets=calls)
                continue           # the strict proof already serves it
            except ValueError:
                pass
            try:
                subs, exch, error = prove(
                    ours, target, jumptables, set(our_relocations),
                    set(target_relocations), calls)
            except Exception as failure:
                subs, exch = [], []
                error = f"{type(failure).__name__}: {failure}"
            if error is None:
                hits.append((unit, name, our_symbol.size // 4,
                             len(subs), len(exch), (unit, name) in pinned))
            else:
                reason = error.split(":")[-1].strip()[:60]
                refused[reason] = refused.get(reason, 0) + 1

    print(f"functions scanned (equal size):  {scanned}")
    print(f"pure register-field candidates:  {candidates}")
    print(f"PROVED by the value-equality mode: {len(hits)}")
    for unit, name, insns, subs, exch, is_pinned in sorted(
            hits, key=lambda row: -row[2]):
        flag = "   [already pinned]" if is_pinned else ""
        print(f"  {unit}::{name}  {insns} insns  "
              f"{subs} substitution(s) {exch} exchange(s){flag}")
    print(f"\nstill refused, by reason ({sum(refused.values())} functions):")
    for reason, count in sorted(refused.items(), key=lambda kv: -kv[1]):
        print(f"  {count:4}  {reason}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
