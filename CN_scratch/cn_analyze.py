"""CN lane: aligned raw-object analysis for the census near-miss composition class.

Loads OUR raw compiler output (.postprocess/body when the TU has a webfrank
unit, else the plain object) and the extracted TARGET object, extracts one
function from each, and reports the differing words with a decoded view plus
the per-word relocation identity (type, symbol) on BOTH sides.

Relocation identity is printed because claim.law.HV_permute-payload-check-does-
not-bind-a-relocation-to-its-atom.20260901.v1 makes binding relocated atoms to
their target slots an obligation of whoever derives the order.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def our_object(unit):
    d, base = unit.rsplit("/", 1)
    body = os.path.join(ROOT, "build", "GUNE5D", "src", d, ".postprocess",
                        "body", base + ".o")
    if os.path.exists(body):
        return body, "raw postprocess body"
    plain = os.path.join(ROOT, "build", "GUNE5D", "src", unit + ".o")
    return plain, "plain object (no webfrank unit)"


def target_object(unit):
    return os.path.join(ROOT, "build", "GUNE5D", "obj", unit + ".o")


def load(path, name):
    data = bytearray(open(path, "rb").read())
    sections = wf._sections(data)
    sym = wf._find_symbol(data, sections, name)
    text = sections[sym.section_index]
    start = text.offset + sym.value
    body = bytes(data[start:start + sym.size])
    relocs = wf._function_text_relocations(
        data, sections, sym.section_index, sym.value, sym.value + sym.size)
    jt = wf._jumptable_targets(
        data, sections, sym.section_index, sym.value, sym.value + sym.size)
    return data, sections, sym, body, relocs, jt


def decode(word):
    """Short human description used only for reading; never for proofs."""
    op = word >> 26
    d = (word >> 21) & 31
    a = (word >> 16) & 31
    simm = wf._sign_extend(word & 0xFFFF, 16)
    if op == 14:
        return f"li r{d},{simm}" if a == 0 else f"addi r{d},r{a},{simm}"
    if op == 15:
        return f"lis r{d},{word & 0xFFFF}" if a == 0 else f"addis r{d},r{a},{simm}"
    if op == 24:
        return f"ori r{a},r{d},{word & 0xFFFF}"
    if op == 31 and ((word >> 1) & 0x3FF) == 444:
        s, b = d, (word >> 11) & 31
        return f"mr r{a},r{s}" if s == b else f"or r{a},r{s},r{b}"
    if op in (32, 36, 48, 52, 33, 37):
        mnem = {32: "lwz", 36: "stw", 48: "lfs", 52: "stfs",
                33: "lwzu", 37: "stwu"}[op]
        return f"{mnem} r{d},{simm}(r{a})"
    if op == 18:
        return "b/bl"
    if op in (16, 17, 19):
        return "<control>"
    return f"op{op} rD={d} rA={a} 0x{word:08x}"


def report(unit, fn):
    our_path, kind = our_object(unit)
    tgt_path = target_object(unit)
    _od, _os, osym, ours, orel, ojt = load(our_path, fn)
    _td, _ts, tsym, tgt, trel, tjt = load(tgt_path, fn)
    print(f"=== {unit}::{fn}")
    print(f"    ours   : {kind} ({len(ours)//4} insns)")
    print(f"    target : {len(tgt)//4} insns")
    if len(ours) != len(tgt):
        print("    SIZE MISMATCH -- ineligible")
        return
    diffs = [o for o in range(0, len(ours), 4)
             if wf._u32(ours, o) != wf._u32(tgt, o)]
    print(f"    differing words: {len(diffs)}")
    print(f"    our relocs   : {sorted(orel.items())}")
    print(f"    target relocs: {sorted(trel.items())}")
    print(f"    jumptable    : ours {sorted(ojt)} target {sorted(tjt)}")
    lo = max(0, (min(diffs) - 12)) if diffs else 0
    hi = min(len(ours), (max(diffs) + 16)) if diffs else 0
    print("    ---- aligned window (both from raw objects) ----")
    for off in range(lo, hi, 4):
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        mark = "  " if ow == tw else "<>"
        orr = orel.get(off)
        trr = trel.get(off)
        rr = ""
        if orr or trr:
            rr = f"   [our reloc {orr} | tgt reloc {trr}]"
        print(f"    {mark} +0x{off:03x}  ours {ow:08x} {decode(ow):<28}"
              f" tgt {tw:08x} {decode(tw)}{rr}")
    print("    ---- differing words only ----")
    for off in diffs:
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        try:
            mask = wf.register_slot_mask(ow)
            pure = "REGFIELD-ONLY" if not ((ow ^ tw) & ~mask) else "NON-REGISTER"
        except ValueError as e:
            pure = f"UNDECODABLE({e})"
        print(f"    +0x{off:03x} {pure}: ours {decode(ow)} | tgt {decode(tw)}")


if __name__ == "__main__":
    report(sys.argv[1], sys.argv[2])
