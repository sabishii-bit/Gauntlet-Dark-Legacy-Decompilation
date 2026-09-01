"""CN lane: HV census filter steps 4-5, automated.

For a candidate's single impure cluster, search orders over the widened window
and accept only an order that (a) passes check_permutation_dependences IN OUR
COLOURING (C1's step 0), (b) binds every relocated atom to its target slot by
(relocation type, symbol) rather than by instruction word, and (c) leaves a
remainder that is entirely copy-form sites plus pure register fields.  Every
survivor is then proven end to end through webfrank.apply_patch against the
real object, never through a hand-rolled replay of the stages.
"""
import copy
import hashlib
import itertools
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import our_object, target_object, load  # noqa: E402

MAX_ATOMS = 8


def sha(b):
    return hashlib.sha256(b).hexdigest()


def raw_region_records(data, sections, sym, lo, hi):
    rel = [s for s in sections
           if s.section_type == wf.SHT_RELA and s.info == sym.section_index]
    if len(rel) != 1:
        return None
    s = rel[0]
    out = []
    for off in range(s.offset, s.offset + s.size, 12):
        r = struct.unpack_from(">IIi", data, off)
        if sym.value + lo <= r[0] < sym.value + hi:
            out.append((r[0] - (sym.value + lo), r[1], r[2]))
    return out


def classify_remainder(cur, tgt):
    """Return (form_sites, ok). ok is False if any word is unreachable."""
    forms = []
    for off in range(0, len(cur), 4):
        ow, tw = wf._u32(cur, off), wf._u32(tgt, off)
        if ow == tw:
            continue
        try:
            if not ((ow ^ tw) & ~wf.register_slot_mask(ow)):
                continue                      # plain recolor material
        except ValueError:
            return None, False
        ours, theirs = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
        if ours is None or theirs is None or theirs[0] != "copy":
            return None, False
        if theirs[2] == 0 or ours[1] != theirs[1]:
            return None, False                # r0 source, or a disguised recolor
        forms.append((off, ours[0]))
    return forms, True


def try_candidate(unit, fn, lo, hi, verbose=True):
    op, _ = our_object(unit)
    tp = target_object(unit)
    odata, osec, osym, ours, orel, ojt = load(op, fn)
    _td, _ts, _tsym, tgt, trel, _tj = load(tp, fn)
    if len(ours) != len(tgt):
        return None
    lo, hi = max(0, lo), min(len(ours), hi)
    n = (hi - lo) // 4
    if n < 2 or n > MAX_ATOMS:
        if verbose:
            print(f"  {fn}: window {n} atoms -- outside search range")
        return None
    region = ours[lo:hi]
    if any(wf._is_control_instruction(wf._u32(region, i * 4)) for i in range(n)):
        if verbose:
            print(f"  {fn}: window holds a control op -- refused (screen 3)")
        return None

    records = raw_region_records(odata, osec, osym, lo, hi)
    if records is None:
        return None
    dest_ok_cache = {}
    for order in itertools.permutations(range(n)):
        order = list(order)
        permuted = b"".join(region[s * 4:s * 4 + 4] for s in order)
        cur = ours[:lo] + permuted + ours[hi:]
        forms, ok = classify_remainder(cur, tgt)
        if not ok:
            continue
        try:
            wf.check_permutation_dependences(region, order, None)
        except ValueError as e:
            dest_ok_cache.setdefault("step0", str(e))
            continue
        # relocation binding obligation
        dest_by_src = {s: d for d, s in enumerate(order)}
        bound = True
        for off, ident in orel.items():
            if not lo <= off < hi:
                continue
            d = dest_by_src[(off - lo) // 4]
            if trel.get(lo + d * 4 + (off - lo) % 4) != ident:
                bound = False
                break
        if not bound:
            continue
        return build(unit, fn, op, tp, ours, tgt, odata, osec, osym,
                     lo, hi, order, records, forms, verbose)
    if verbose:
        msg = dest_ok_cache.get("step0")
        print(f"  {fn}: NO admissible order over [0x{lo:x},0x{hi:x})"
              + (f"  (last step-0 refusal: {msg})" if msg else ""))
    return None


def build(unit, fn, op, tp, ours, tgt, odata, osec, osym,
          lo, hi, order, records, forms, verbose):
    region = ours[lo:hi]
    permuted = b"".join(region[s * 4:s * 4 + 4] for s in order)
    dest_by_src = {s: d for d, s in enumerate(order)}
    prec = sorted(((dest_by_src[o // 4] * 4 + o % 4, i, a) for o, i, a in records),
                  key=lambda x: x[0])
    rule = {
        "function": fn,
        "before_sha256": sha(ours),
        "after_sha256": sha(tgt),
        "instruction_permutation": {
            "start": hex(lo), "end": hex(hi), "order": order,
            "before_sha256": sha(region), "after_sha256": sha(permuted),
            "before_relocations_sha256": wf._relocation_sha256(records),
            "after_relocations_sha256": wf._relocation_sha256(prec),
        },
    }
    if forms:
        rule["equivalent_copy_form"] = [
            {"at": hex(o), "proof": "dominating_def" if k == "li"
             else "unconditional"} for o, k in forms]
    cur = ours[:lo] + permuted + ours[hi:]
    if cur != tgt and not forms:
        rule["copy_register_fields"] = True
    elif cur != tgt:
        probe_cur = bytearray(cur)
        for o, _k in forms:
            probe_cur[o:o + 4] = tgt[o:o + 4]
        if bytes(probe_cur) != tgt:
            rule["copy_register_fields"] = True

    probe = copy.deepcopy(rule)
    data = bytearray(open(op, "rb").read())
    try:
        _b, _a, changed = wf.apply_patch(data, probe, open(tp, "rb").read())
    except ValueError as e:
        if verbose:
            print(f"  {fn}: order {order} apply_patch REFUSED: {e}")
        return None
    sec = wf._sections(data)
    sym = wf._find_symbol(data, sec, fn)
    text = sec[sym.section_index]
    got = bytes(data[text.offset + sym.value:
                     text.offset + sym.value + sym.size])
    resid = sum(1 for o in range(0, len(got), 4)
                if wf._u32(got, o) != wf._u32(tgt, o))
    print(f"  {fn}: CLOSES (order {order}, {changed} atoms/fields, "
          f"residual {resid})" if not resid else
          f"  {fn}: order {order} leaves residual {resid}")
    return rule if not resid else None


CANDIDATES = [
    ("game/ui/btext", "FontInit", 0x24, 0x34),
    ("game/anim/action", "DoEnemyAction", 0x24, 0x34),
    ("game/enemy/critter", "ProcessCritter", 0xb0, 0xbc),
    ("game/pb/pb_objects", "fn_800C37C4", 0x4c, 0x6c),
    ("game/world/camera", "debug_camera_pos", 0x90, 0xa8),
    ("game/game/gamemain", "fn_80051164", 0x40, 0x54),
    ("game/world/tower", "towerRuneNearAudio", 0x50, 0x74),
    ("game/sys/memcard", "init_all_dir_info", 0x64, 0x74),
    ("game/ui/auxscreen", "calc_wizard_pos", 0x58, 0x6c),
    ("game/ui/options", "next_rune_hint", 0x100, 0x118),
    ("game/ui/options", "next_boss_hint", 0x10c, 0x128),
    ("game/ui/options", "next_legend_hint", 0x10c, 0x128),
    ("game/world/gauntworld", "fn_8005FDA8", 0xec, 0x13c),
    ("game/world/worldcol", "CTriListCollide", 0x1c, 0x6c),
]

if __name__ == "__main__":
    import json
    out = {}
    for unit, fn, lo, hi in CANDIDATES:
        print(f"== {unit}::{fn}")
        try:
            r = try_candidate(unit, fn, lo, hi)
        except Exception as e:
            print(f"  {fn}: ERROR {type(e).__name__}: {e}")
            continue
        if r:
            out[f"{unit}::{fn}"] = r
    print(f"\nCLOSABLE: {list(out)}")
    open(os.path.join(os.path.dirname(__file__), "cn_found.json"), "w").write(
        json.dumps(out, indent=2))
