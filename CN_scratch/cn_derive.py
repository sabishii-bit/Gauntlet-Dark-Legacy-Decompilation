"""CN lane: derive + machine-prove a composed permute+copy-form webfrank rule.

Order derivation binds each relocated atom to its target slot by
(relocation type, symbol) rather than by instruction word, per
claim.law.HV_permute-payload-check-does-not-bind-a-relocation-to-its-atom
.20260901.v1 -- webfrank's own payload check proves conservation, not binding,
so the binding is this script's obligation and is asserted explicitly below.

Every proof runs through webfrank.apply_patch against the REAL raw object,
never a hand-rolled replay of the stages, per
claim.C2_permutation-moves-relocated-words-into-later-stage-proof-spans
.20260901.v1.
"""
import copy
import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def our_object(unit):
    d, base = unit.rsplit("/", 1)
    body = os.path.join(ROOT, "build", "GUNE5D", "src", d, ".postprocess",
                        "body", base + ".o")
    if os.path.exists(body):
        return body
    return os.path.join(ROOT, "build", "GUNE5D", "src", unit + ".o")


def load(path, name):
    data = bytearray(open(path, "rb").read())
    sections = wf._sections(data)
    sym = wf._find_symbol(data, sections, name)
    text = sections[sym.section_index]
    start = text.offset + sym.value
    body = bytes(data[start:start + sym.size])
    relocs = wf._function_text_relocations(
        data, sections, sym.section_index, sym.value, sym.value + sym.size)
    return data, sections, sym, body, relocs


def sha(b):
    return hashlib.sha256(b).hexdigest()


def region_relocs(relocs, lo, hi):
    """(offset-relative-to-region, info, addend) is what webfrank hashes; but we
    only have (type,name) here, so return the raw records from the object."""
    return {off: v for off, v in relocs.items() if lo <= off < hi}


def raw_region_records(data, sections, sym, lo, hi):
    """Exact (offset, info, addend) triples webfrank will feed the permuter."""
    rel = [s for s in sections
           if s.section_type == wf.SHT_RELA and s.info == sym.section_index]
    assert len(rel) == 1, rel
    s = rel[0]
    out = []
    for off in range(s.offset, s.offset + s.size, 12):
        r = struct.unpack_from(">IIi", data, off)
        if sym.value + lo <= r[0] < sym.value + hi:
            out.append((r[0] - (sym.value + lo), r[1], r[2]))
    return out


def derive(unit, fn, start, end, order, forms):
    our_path = our_object(unit)
    tgt_path = os.path.join(ROOT, "build", "GUNE5D", "obj", unit + ".o")
    odata, osec, osym, ours, orel = load(our_path, fn)
    _tdata, _tsec, _tsym, tgt, trel = load(tgt_path, fn)
    assert len(ours) == len(tgt), "size mismatch"

    print(f"### {unit}::{fn}  ({len(ours)//4} insns)")
    diffs_before = [o for o in range(0, len(ours), 4)
                    if wf._u32(ours, o) != wf._u32(tgt, o)]
    print(f"  baseline differing words: {len(diffs_before)} "
          f"{[hex(d) for d in diffs_before]}")

    region = ours[start:end]
    n = len(region) // 4

    # ---- STEP 0 (C1's law): is the permutation legal in OUR colouring alone?
    try:
        wf.check_permutation_dependences(region, order, None)
        print(f"  STEP 0: check_permutation_dependences PASS (strict, "
              f"exit_dead=None) order={order}")
    except ValueError as e:
        print(f"  STEP 0: REFUSED -> {e}")
        return None

    # ---- relocation binding obligation, discharged by (type, symbol)
    ourr = region_relocs(orel, start, end)
    tgtr = region_relocs(trel, start, end)
    dest_by_src = {s: d for d, s in enumerate(order)}
    print(f"  region relocations: ours {sorted(ourr.items())} "
          f"target {sorted(tgtr.items())}")
    bind_ok = True
    for off, ident in sorted(ourr.items()):
        src_atom = (off - start) // 4
        within = (off - start) % 4
        dst_off = start + dest_by_src[src_atom] * 4 + within
        got = trel.get(dst_off)
        status = "OK" if got == ident else f"MISMATCH (target has {got})"
        if got != ident:
            bind_ok = False
        print(f"    bind +0x{off:x} {ident} -> +0x{dst_off:x}: {status}")
    if not ourr and not tgtr:
        print("    (region carries no relocations on either side)")
    if not bind_ok:
        print("  RELOCATION BINDING FAILED -- do not author this rule")
        return None

    records = raw_region_records(odata, osec, osym, start, end)
    permuted_records = []
    for off, info, addend in records:
        permuted_records.append(
            (dest_by_src[off // 4] * 4 + off % 4, info, addend))
    permuted_records.sort(key=lambda i: i[0])
    atoms = [region[i * 4:i * 4 + 4] for i in range(n)]
    permuted = b"".join(atoms[s] for s in order)

    rule = {
        "function": fn,
        "before_sha256": sha(ours),
        "after_sha256": sha(tgt),
        "instruction_permutation": {
            "start": hex(start), "end": hex(end),
            "order": list(order),
            "before_sha256": sha(region),
            "after_sha256": sha(permuted),
            "before_relocations_sha256": wf._relocation_sha256(records),
            "after_relocations_sha256": wf._relocation_sha256(permuted_records),
        },
    }
    if forms:
        rule["equivalent_copy_form"] = [dict(f) for f in forms]

    # ---- PROVE through apply_patch against the real object
    probe = copy.deepcopy(rule)
    data = bytearray(open(our_path, "rb").read())
    tgt_bytes = open(tgt_path, "rb").read()
    try:
        _b, _a, changed = wf.apply_patch(data, probe, tgt_bytes)
    except ValueError as e:
        print(f"  apply_patch REFUSED: {e}")
        return None
    sections = wf._sections(data)
    sym = wf._find_symbol(data, sections, fn)
    text = sections[sym.section_index]
    got = bytes(data[text.offset + sym.value:
                     text.offset + sym.value + sym.size])
    residual = [o for o in range(0, len(got), 4)
                if wf._u32(got, o) != wf._u32(tgt, o)]
    print(f"  apply_patch OK: adjusted {changed} atoms/fields")
    for f in probe.get("equivalent_copy_form", []):
        if "_proved_at" in f:
            print(f"    copy-form {f['at']} proof={f['proof']} "
                  f"discharged by the def at +0x{f['_proved_at']:x}")
    print(f"  RESIDUAL AFTER RULE: {len(residual)} differing words "
          f"{[hex(r) for r in residual]}")
    print(f"  FUNCTION CLOSES: {'YES (real 0)' if not residual else 'NO'}")
    print("  RULE JSON:")
    print(json.dumps(rule, indent=2))
    return rule if not residual else None


CANDIDATES = {
    "dcs": ("game/audio/dcs", "dcsSampleAllocUpload", 0x10, 0x2c,
            [4, 0, 1, 5, 6, 2, 3],
            [{"at": "0x1c", "proof": "dominating_def"},
             {"at": "0x20", "proof": "dominating_def"}]),
    "memcard": ("game/sys/memcard", "MemCardCreateGaunt", 0x24, 0x38,
                [3, 0, 1, 4, 2],
                [{"at": "0x30", "proof": "dominating_def"}]),
    "camera": ("game/world/camera", "do_camera", 0x1d8, 0x1e4,
               [1, 2, 0],
               [{"at": "0x1dc", "proof": "dominating_def"}]),
}

if __name__ == "__main__":
    keys = sys.argv[1:] or list(CANDIDATES)
    for k in keys:
        derive(*CANDIDATES[k])
        print()
