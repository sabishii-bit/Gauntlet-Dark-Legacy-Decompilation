#!/usr/bin/env python3
"""Compare enemy pool ownership forms without editing source or pinned rules.

Run with one or more scalar addresses, e.g. 0x80346810 0x80346818.
Each candidate is a full TU generated under build/r60_enemy_pool_probes.
Whole-object baseline fidelity is mandatory. Every function is compared;
body equality alone does NOT establish relocation or data-section equality.
"""
import argparse
import json
import re
import struct

from cn_analyze import wf
from cv_probe import REPO, compile_with, read_edges
from pj_body_ab import disassemble_words, compare_words
from pj_pool_literalize import DECL, read_datum, spell_f32, spell_f64


def variants(source, addresses):
    symbols = {f"lbl_{address:08X}" for address in addresses}
    declarations = {m.group(2): (m.group(1), m.group(3))
                    for m in DECL.finditer(source) if m.group(2) in symbols}
    if set(declarations) != symbols:
        raise ValueError("every selected address must be a declared scalar")
    definitions = {}
    for symbol, (kind, dimension) in declarations.items():
        if kind not in ("f32", "f64") or dimension:
            raise ValueError("only scalar floating constants are supported")
        address = int(symbol[4:], 16)
        value = struct.unpack(">f" if kind == "f32" else ">d",
                              read_datum(address, 4 if kind == "f32" else 8))[0]
        definitions[symbol] = (kind, (spell_f32 if kind == "f32" else spell_f64)(value))
    stripped = DECL.sub(lambda m: "" if m.group(2) in symbols else m.group(0), source)
    literal = stripped
    for symbol, (_, value) in definitions.items():
        # Diagnostic only: a literal cannot be addressed. Report this
        # explicitly instead of treating dropped volatile as an identity.
        literal = re.sub(r"\*\(volatile\s+(?:f32|f64)\s*\*\)\s*&\s*" + symbol + r"\b", symbol, literal)
        literal = re.sub(r"\b" + symbol + r"\b", "(" + value + ")", literal)
    yield "literals_VOLATILE_LOADS_REMOVED", literal
    # Put definitions where the first selected declaration originally was,
    # so all types have been included and all uses remain declared.
    for mode in ("static_const_before", "external_const_before", "external_const_after"):
        introduced = False
        def declaration(match):
            nonlocal introduced
            if match.group(2) not in symbols:
                return match.group(0)
            if introduced:
                return ""
            introduced = True
            if mode == "external_const_after":
                return "".join(f"extern const {kind} {symbol};\n" for symbol, (kind, _) in definitions.items())
            prefix = "static " if mode.startswith("static") else ""
            return "".join(f"{prefix}const {kind} {symbol} = {value};\n" for symbol, (kind, value) in definitions.items())
        candidate = DECL.sub(declaration, source)
        if mode == "external_const_after":
            candidate += "\n" + "".join(f"const {kind} {symbol} = {value};\n" for symbol, (kind, value) in definitions.items())
        yield mode, candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("addresses", nargs="+", type=lambda v: int(v, 0))
    args = parser.parse_args()
    edge = read_edges()["game/enemy/enemy"]
    source = (REPO / edge["src"]).read_text(encoding="utf-8")
    out = REPO / "build/r60_enemy_pool_probes" / "_".join(f"{v:08x}" for v in args.addresses)
    out.mkdir(parents=True, exist_ok=True)
    baseline, error = compile_with(edge, edge["mw"], edge["cflags"], out / "baseline.o", out)
    if error or baseline.read_bytes() != (REPO / edge["body_o"]).read_bytes():
        raise RuntimeError(error or "whole-object baseline fidelity failed")
    before = disassemble_words(str(baseline))
    print("FIDELITY: raw whole-object identity", flush=True)
    results = []
    for name, candidate in variants(source, args.addresses):
        generated = out / (name + ".c")
        generated.write_text(candidate, encoding="utf-8")
        obj, error = compile_with(dict(edge, src=str(generated)), edge["mw"], edge["cflags"], out / (name + ".o"), out)
        if error:
            raise RuntimeError(error)
        moved = compare_words(before, disassemble_words(str(obj)))
        sections = {s.name: s.size for s in wf._sections(bytearray(obj.read_bytes())) if s.name in (".sdata2", ".sdata", ".data")}
        row = {"mode": name, "sections": sections,
               "moved_functions": [{"name": n, "kind": k, "before_insns": d[0], "after_insns": d[1], "differing_words": len(d[2])} for n, k, d in moved], "object": str(obj)}
        results.append(row)
        print(json.dumps(row), flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
