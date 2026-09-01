"""Screen every shipped rule still resting on `unproven_recolor_audit`.

An `unproven_recolor_audit` is the one escape in the WebFrank corpus that is
accepted by human inspection rather than by proof, so the corpus should shed
them whenever a proof mode grows to cover one.  This reports, per escape,
either that `verify_value_equality_recolor` now proves it (the audit can be
replaced by a declared proof, byte-neutral) or the instrumented reason it
still cannot -- which is the roster the next capability should be aimed at.

    python tools/gdl/composed_census/wf_unproven_audit_screen.py

Requires a completed `ninja`.  Reads the RAW `.postprocess/body` output, so
the escapes are screened against the residual they actually stand on.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, HERE)
import webfrank as wf  # noqa: E402
from wf_census import our_path, functions, OBJ  # noqa: E402
from wf_value_equality_census import prove, load_function  # noqa: E402

OTHER_STAGES = ("instruction_permutation", "equivalent_copy_form",
                "post_recolor_permutation", "recolors", "register_fields")


def main():
    config = json.load(open(os.path.join(ROOT, "config", "GUNE5D",
                                         "webfrank.json"), encoding="utf-8"))
    units = config.get("units", config)
    escapes = provable = 0
    for unit, rules in units.items():
        for rule in rules:
            if not rule.get("unproven_recolor_audit"):
                continue
            escapes += 1
            name = rule["function"]
            path, _raw = our_path(unit)
            our_data = bytearray(open(path, "rb").read())
            target_data = bytearray(
                open(os.path.join(OBJ, unit + ".o"), "rb").read())
            our_sections = wf._sections(our_data)
            target_sections = wf._sections(target_data)
            try:
                our_symbol = {s.name: s
                              for s in functions(our_data, our_sections)}[name]
                target_symbol = {
                    s.name: s
                    for s in functions(target_data, target_sections)}[name]
            except KeyError:
                print(f"{unit}::{name}\n   symbol not found in both objects")
                continue
            stages = [key for key in OTHER_STAGES if rule.get(key)]
            header = f"{unit}::{name} ({our_symbol.size // 4} insns)"
            if stages:
                header += f"  other stages: {stages}"
            if our_symbol.size != target_symbol.size:
                print(f"{header}\n   size differs; not a pure recolor")
                continue
            ours = load_function(our_data, our_sections, our_symbol)
            target = load_function(target_data, target_sections, target_symbol)
            try:
                recolored, _n = wf.copy_register_fields(ours, target)
            except ValueError as error:
                print(f"{header}\n   copy_register_fields refuses: {error}")
                continue
            if recolored != target:
                print(f"{header}\n   register fields alone do not reach the "
                      f"target (this rule needs its other stages first)")
                continue
            our_relocations = wf._function_text_relocations(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            target_relocations = wf._function_text_relocations(
                target_data, target_sections, target_symbol.section_index,
                target_symbol.value, target_symbol.value + target_symbol.size)
            jumptables = wf._jumptable_targets(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            calls = {offset: symbol
                     for offset, (kind, symbol) in our_relocations.items()
                     if kind == 10}
            try:
                subs, exch, error = prove(
                    ours, target, jumptables, set(our_relocations),
                    set(target_relocations), calls)
            except Exception as failure:
                subs, exch = [], []
                error = f"{type(failure).__name__}: {failure}"
            if error is None:
                provable += 1
                print(f"{header}\n   *** PROVABLE: {len(subs)} "
                      f"substitution(s), {len(exch)} exchange(s) -- replace "
                      f"the audit with a declared value_equality_recolor")
            else:
                print(f"{header}\n   still unproven: {error}")
    print(f"\nunproven audit escapes: {escapes}; now provable: {provable}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
