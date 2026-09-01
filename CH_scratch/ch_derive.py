"""CH lane: derive a composed permute(+form)+recolor order by ATOM MATCHING.

CN's cn_search.py implements HV census step 4 as `itertools.permutations`,
which forces MAX_ATOMS = 8 and left towerRuneNearAudio (9 atoms),
CTriListCollide (20) and fn_8005FDA8 (20) undecided.  HV step 4 does not
actually ask for an exhaustive search -- it says to DERIVE the order by
matching atoms on register-erased form AND relocation identity.  That is a
bipartite matching, not an enumeration, and it is polynomial in the common
case because the compatibility matrix is extremely sparse: a source atom can
only serve a destination slot whose target word has the same opcode and
immediate payload.

Compatibility of our source atom s with destination slot d is exactly the
condition cn_search.classify_remainder applies AFTER permuting, evaluated
per-slot BEFORE permuting:
  - identical words, or
  - differ only in register fields (plain recolor material), or
  - a served copy-form pair (target word decodes as a register copy).
plus strict relocation binding: our atom's (type, symbol) at s must equal the
target's relocation at d, and an unrelocated atom may not land on a relocated
slot or vice versa.

Every complete candidate order is then put through the SAME proofs CN used:
C1 step-0 (check_permutation_dependences in OUR colouring, standing alone),
then webfrank.apply_patch against the real object, and it is accepted only at
residual 0.  The identity-order trap is handled explicitly: an identity order
is reported as DROP-PERMUTATION, never shipped as a permutation rule.
"""
import copy
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import our_object, target_object, load  # noqa: E402

MAX_ORDERS = 400000


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


def norm_relocs(rel, base=0):
    """Key relocations by the OWNING INSTRUCTION, not the raw r_offset.

    MEASURED image-wide (CH_scratch/ch_reloc_probe.py): R_PPC_EMB_SDA21
    (type 109) is recorded at instruction+2 in OUR objects (14259 of 14259)
    and at instruction+0 in the extracted TARGET objects (14265 of 14265),
    while @lo/@ha (types 4/5/6) sit at instruction+2 on BOTH sides and REL24
    (type 10) at instruction+0 on both.  Comparing raw offsets therefore
    false-negatives on every window holding an SDA21 relocation.  Flooring to
    the instruction boundary is the encoding-neutral identity.
    """
    out = {}
    for off, ident in rel.items():
        out[(off - base) & ~3] = ident
    return out


def word_compatible(ow, tw):
    """Can our word ow legally occupy a slot whose target word is tw?

    Returns (ok, form_kind) where form_kind is None for identical/recolor
    material or the decoded kind of OUR word for a served copy-form site.
    """
    if ow == tw:
        return True, None
    try:
        if not ((ow ^ tw) & ~wf.register_slot_mask(ow)):
            return True, None                  # plain recolor material
    except ValueError:
        return False, None
    ours, theirs = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
    if ours is None or theirs is None or theirs[0] != "copy":
        return False, None                     # inverse/unserved direction
    if theirs[2] == 0 or ours[1] != theirs[1]:
        return False, None                     # r0 source, or disguised recolor
    return True, ours[0]


def derive_orders(ours, tgt, orel, trel, lo, hi, budget=MAX_ORDERS):
    """Yield every order (order[d] = s) whose every slot is compatible and
    whose relocation identities bind.  Sparse backtracking, no enumeration."""
    n = (hi - lo) // 4
    ow = [wf._u32(ours, lo + i * 4) for i in range(n)]
    tw = [wf._u32(tgt, lo + i * 4) for i in range(n)]
    norel, ntrel = norm_relocs(orel), norm_relocs(trel)
    orl = [norel.get(lo + i * 4) for i in range(n)]
    trl = [ntrel.get(lo + i * 4) for i in range(n)]

    cand = []
    for d in range(n):
        row = []
        for s in range(n):
            ok, _k = word_compatible(ow[s], tw[d])
            if ok and orl[s] == trl[d]:
                row.append(s)
        if not row:
            return                              # slot unreachable: no order
        cand.append(row)
    # most-constrained-slot ordering keeps the tree tiny on sparse matrices
    slots = sorted(range(n), key=lambda d: len(cand[d]))
    order = [None] * n
    used = [False] * n
    produced = 0

    def rec(i):
        nonlocal produced
        if produced >= budget:
            return
        if i == len(slots):
            produced += 1
            yield list(order)
            return
        d = slots[i]
        for s in cand[d]:
            if used[s]:
                continue
            used[s] = True
            order[d] = s
            for r in rec(i + 1):
                yield r
            used[s] = False
            order[d] = None

    for r in rec(0):
        yield r


def classify_remainder(cur, tgt):
    forms = []
    for off in range(0, len(cur), 4):
        a, b = wf._u32(cur, off), wf._u32(tgt, off)
        if a == b:
            continue
        ok, kind = word_compatible(a, b)
        if not ok:
            return None, False
        if kind is not None:
            forms.append((off, kind))
    return forms, True


def build(unit, fn, lo, hi, order, verbose=True):
    op, _ = our_object(unit)
    tp = target_object(unit)
    odata, osec, osym, ours, orel, _ = load(op, fn)
    _d, _s, _y, tgt, _tr, _j = load(tp, fn)
    region = ours[lo:hi]
    permuted = b"".join(region[s * 4:s * 4 + 4] for s in order)
    cur = ours[:lo] + permuted + ours[hi:]
    forms, ok = classify_remainder(cur, tgt)
    if not ok:
        return None
    records = raw_region_records(odata, osec, osym, lo, hi)
    dest_by_src = {s: d for d, s in enumerate(order)}
    prec = sorted(((dest_by_src[o // 4] * 4 + o % 4, i, a) for o, i, a in records),
                  key=lambda x: x[0])
    identity = order == list(range(len(order)))
    rule = {"function": fn,
            "before_sha256": sha(ours),
            "after_sha256": sha(tgt)}
    if not identity:
        rule["instruction_permutation"] = {
            "start": hex(lo), "end": hex(hi), "order": order,
            "before_sha256": sha(region), "after_sha256": sha(permuted),
            "before_relocations_sha256": wf._relocation_sha256(records),
            "after_relocations_sha256": wf._relocation_sha256(prec),
        }
    if forms:
        rule["equivalent_copy_form"] = [
            {"at": hex(o), "proof": "dominating_def" if k == "li"
             else "unconditional"} for o, k in forms]
    probe_cur = bytearray(cur)
    for o, _k in forms:
        probe_cur[o:o + 4] = tgt[o:o + 4]
    if bytes(probe_cur) != tgt:
        rule["copy_register_fields"] = True

    data = bytearray(open(op, "rb").read())
    try:
        _b, _a, changed = wf.apply_patch(data, copy.deepcopy(rule),
                                         open(tp, "rb").read())
    except ValueError as e:
        if verbose:
            print(f"    apply_patch REFUSED for order {order}: {e}")
        return None
    sec = wf._sections(data)
    sym = wf._find_symbol(data, sec, fn)
    text = sec[sym.section_index]
    got = bytes(data[text.offset + sym.value:
                     text.offset + sym.value + sym.size])
    resid = sum(1 for o in range(0, len(got), 4)
                if wf._u32(got, o) != wf._u32(tgt, o))
    if resid:
        if verbose:
            print(f"    order {order} leaves residual {resid}")
        return None
    tag = "  [IDENTITY ORDER -> permutation stage DROPPED]" if identity else ""
    print(f"    CLOSES: order {order}, {changed} atoms/fields, residual 0{tag}")
    return rule


def run(unit, fn, lo, hi, verbose=True):
    op, kind = our_object(unit)
    tp = target_object(unit)
    _od, _os, _oy, ours, orel, _oj = load(op, fn)
    _td, _ts, _ty, tgt, trel, _tj = load(tp, fn)
    if len(ours) != len(tgt):
        print(f"  {fn}: SIZE MISMATCH -- ineligible")
        return None
    lo, hi = max(0, lo), min(len(ours), hi)
    n = (hi - lo) // 4
    region = ours[lo:hi]
    if any(wf._is_control_instruction(wf._u32(region, i * 4)) for i in range(n)):
        print(f"  {fn}: window holds a control op -- refused (screen 3)")
        return None
    print(f"  {fn}: window [0x{lo:x},0x{hi:x}) = {n} atoms ({kind})")
    seen = 0
    step0 = None
    for order in derive_orders(ours, tgt, orel, trel, lo, hi):
        seen += 1
        try:
            wf.check_permutation_dependences(region, order, None)
        except ValueError as e:
            if step0 is None:
                step0 = str(e)
            continue
        r = build(unit, fn, lo, hi, order, verbose)
        if r:
            print(f"  {fn}: derived from {seen} candidate order(s)")
            return r
    print(f"  {fn}: no closing order ({seen} slot-compatible candidates)"
          + (f"; first step-0 refusal: {step0}" if step0 else ""))
    return None


if __name__ == "__main__":
    u, f = sys.argv[1], sys.argv[2]
    lo, hi = int(sys.argv[3], 0), int(sys.argv[4], 0)
    run(u, f, lo, hi)
