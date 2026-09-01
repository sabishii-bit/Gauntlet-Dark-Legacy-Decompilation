"""HA lane run-27: closability against the SHIPPED combined form+recolor stage.

WHY THIS EXISTS.  claim.CH_combined-stage-closability-roster... concluded the
combined stage is "sufficient for none" of its 25 carriers.  That verdict was
measured with ch_closable.py, whose simulate() does:

    if classify(a, b) in FORMS: cur[off:off+4] = tgt[off:off+4]

i.e. it COPIES THE TARGET WORD at every form site.  The shipped stage does the
opposite, and the law says so in one sentence: it "NEVER COPIES THE TARGET
WORD", it re-encodes the target's ENCODING around OUR registers
(webfrank.encode_copy_like).  Copying the target word imports the target's
register colours at that site, so the recolor that follows is asked to prove a
renaming that the simulation itself fabricated.  CH's refusals are therefore
measurements of a design the project did not build.

Two further divergences from CH's simulation, both shipped:
  * a permutation MAY run AFTER the recolor (post_recolor_permutation), so a
    displacement CAUSED BY the recolor is reachable; CH only permuted first.
  * the combined stage has three named modes with distinct obligations.

So this does not simulate at all.  It DERIVES a rule and proves it through
webfrank.apply_patch against the real object, counting residual words.  Ground
truth, shipped code, no simulation-fidelity risk.
"""
import copy
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))   # tools/gdl/composed_census
sys.path.insert(0, os.path.dirname(HERE))           # tools/gdl (webfrank)
sys.path.insert(0, HERE)                            # sibling lane scaffolding
import webfrank as wf  # noqa: E402
from cn_analyze import our_object, target_object, load  # noqa: E402

PURE_FWD = ["dominating_def", "dominating_def_across_calls"]
PURE_INV = ["dominating_def_inverse", "dominating_def_inverse_across_calls"]


def sha(b):
    return hashlib.sha256(b).hexdigest()


def regfield_only(ow, tw):
    try:
        return not ((ow ^ tw) & ~wf.register_slot_mask(ow))
    except ValueError:
        return False


def classify(ow, tw, form_first=False):
    """('same'|'regfield'|'other', None) or ('form', (family, modes)).

    ARROW ORDER.  `regfield_only` is a LEXICAL test on the encoding, and PPC
    mnemonics ALIAS: `mr rD,rS` IS `or rD,rS,rS` and `li rD,K` IS
    `addi rD,r0,K`.  So a copy->copy arrow whose SOURCE register differs
    (`mr r4,r5` -> `mr r4,r6`) is *also* regfield-only, because the two
    encodings differ in register slots and nothing else.  Testing regfield
    FIRST therefore SHADOWS the copy-form stage: the word is folded into the
    global renaming, where verify_consistent_recolor must then prove r5->r6
    consistently across the WHOLE function, instead of being closed locally
    by an `unconditional_recolor` arrow that costs the renaming nothing.  A
    refusal produced that way is a measurement of the ORDER OF TWO TESTS, not
    of the function (claim.law.HV_mnemonic-aliasing-manufactures-structural-
    rows-and-count-asymmetric-signatures.20260901.v1; discipline 14).

    Neither order dominates: when a renaming is needed anyway, folding a word
    into it is CHEAPER than spending an arrow, and arrows have their own
    per-site proof obligations.  So this takes the order as a parameter and
    the caller ENUMERATES both, with the shipped apply_patch adjudicating
    every candidate.  Note the fallbacks make the 'other' (unabsorbed) set
    IDENTICAL under both orders, so the order can never turn a closable
    function unclosable -- it can only add candidates.
    """
    if ow == tw:
        return "same", None
    rf = regfield_only(ow, tw)
    if not form_first and rf:
        return "regfield", None
    what, info = _copy_form_arrow(ow, tw)
    if what == "form" and not (rf and _same_copy_form(ow, tw)):
        return what, info
    if rf:
        return "regfield", None
    return "other", None


def _same_copy_form(ow, tw):
    """Do both words use the SAME copy form (both `li`, or both `copy`)?

    This is the boundary of the shadowing.  An arrow re-encodes the TARGET's
    form around OUR registers, so when both sides already share a form and
    differ only in register slots, the re-encoding reproduces our own word
    and apply_patch rejects the site: "re-encoding is a no-op, so this site
    is a pure recolor and belongs to the recolor stage" (measured on
    memcard::drawMemCardMessage +0x14, ours `mr r22,r3` vs target
    `mr r28,r3` -- a DEST-only difference, which is exactly what a renaming
    is for).

    So only the li<->copy direction was ever shadowed, and it is shadowed
    only in the `addi` spelling, where both words carry primary opcode 14 and
    differ in the rA slot alone: ours `addi rD,r0,0` (= `li rD,0`) against
    the target's `addi rD,rS,0` (= `mr rD,rS`).  That pair is regfield-only
    LEXICALLY while being a genuine form arrow SEMANTICALLY -- the
    live-zero-remat shape, and the one gamemain::fn_80054E78 carries at
    +0x154.  Restricting form-first to it keeps the enumeration honest
    instead of offering apply_patch sites it must reject.
    """
    o, t = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
    return o is not None and t is not None and o[0] == t[0]


def _copy_form_arrow(ow, tw):
    """The copy-form decode, independent of the regfield test."""
    ours, theirs = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
    if ours is None or theirs is None:
        return "other", None
    same_dest = ours[1] == theirs[1]
    if ours[0] == "copy" and theirs[0] == "copy":
        if ours[2] == 0:
            return "other", None                       # r0 refusal
        if same_dest and ours[2] == theirs[2]:
            return "form", ("uncond", ["unconditional"])
        return "form", ("uncond_rc", ["unconditional_recolor"])
    if ours[0] == "li" and theirs[0] == "copy":
        if theirs[2] == 0:
            return "other", None                       # r0 refusal
        if same_dest:
            return "form", ("fwd", PURE_FWD + ["constant_dataflow_recolor"])
        return "form", ("fwd_rc", ["constant_dataflow_recolor"])
    if ours[0] == "copy" and theirs[0] == "li":
        if ours[2] == 0:
            return "other", None                       # r0 refusal
        if same_dest:
            return "form", ("inv",
                            PURE_INV + ["constant_dataflow_inverse_recolor"])
        return "form", ("inv_rc", ["constant_dataflow_inverse_recolor"])
    return "other", None


def renaming_from_regfields(cur, tgt):
    """Candidate our->target GPR map read off the pure-regfield words only.

    Used ONLY to pick a starting `our_source` candidate for
    constant_dataflow_recolor.  It proves nothing: apply_patch's
    verify_consistent_recolor is what adjudicates the renaming.
    """
    fwd = {}
    for off in range(0, len(cur), 4):
        ow, tw = wf._u32(cur, off), wf._u32(tgt, off)
        if ow == tw or not regfield_only(ow, tw):
            continue
        for kind, shift, _role, _zero in wf.instruction_operands(ow):
            if kind != "g":
                continue
            a, b = (ow >> shift) & 31, (tw >> shift) & 31
            fwd.setdefault(a, {}).setdefault(b, 0)
            fwd[a][b] += 1
    return {a: max(v, key=v.get) for a, v in fwd.items()}


def context(ours_body, orel, ojt):
    """The CFG/relocation context apply_patch builds for the proof modes."""
    words = [wf._u32(ours_body, o) for o in range(0, len(ours_body), 4)]
    rel_idx = {o // 4 for o in orel}
    successors, calls = wf._successors(words, rel_idx, {o // 4 for o in ojt})
    types = {o // 4: t for o, (t, _n) in orel.items()}
    ctargets = {o: n for o, (t, n) in orel.items() if t == 10}
    return words, successors, calls, rel_idx, types, ctargets


def our_source_candidates(cur, off, theirs_source, orel, ojt, constant):
    """Registers that provably hold `constant` at `off`, best guess first."""
    words, succ, calls, rel_idx, types, ctargets = context(cur, orel, ojt)
    good = []
    for reg in range(1, 32):
        try:
            wf.prove_constant_dataflow(
                words, off // 4, reg, constant, succ, calls, rel_idx,
                relocation_types=types, call_targets=ctargets)
        except ValueError:
            continue
        good.append(reg)
    return good


def build_rule(unit, fn, pre=None, post=None, verbose=False):
    """Derive + PROVE a rule.  Returns (rule, residual, note)."""
    op, kind = our_object(unit)
    tp = target_object(unit)
    if not (os.path.exists(op) and os.path.exists(tp)):
        return None, None, "no object"
    _od, _os, _oy, ours, orel, ojt = load(op, fn)
    _td, _ts, _ty, tgt, trel, _tj = load(tp, fn)
    if len(ours) != len(tgt):
        return None, None, f"size mismatch ({len(ours)} vs {len(tgt)})"

    rule = {"function": fn, "before_sha256": sha(ours), "after_sha256": sha(tgt)}

    # --- stage 1: pre-recolor permutation (audited in OUR colouring) --------
    cur = bytearray(ours)
    orel_raw = dict(orel)
    if pre:
        for win in pre:
            lo, hi = win["lo"], win["hi"]
            order = win["order"]
            region = bytes(cur[lo:hi])
            cur[lo:hi] = b"".join(region[s * 4:s * 4 + 4] for s in order)
            # RELOCATIONS RIDE THEIR ATOM.  apply_patch permutes the real
            # relocation table with the words, so any proof run against the
            # permuted stream must see the permuted relocation set too.
            # Passing the un-permuted set marks whichever word LANDS on a
            # formerly-relocated offset as relocated, and
            # prove_constant_dataflow refuses a relocated word as a
            # definition -- a false refusal that reads exactly like a
            # property of the function.  (Measured on dcs::update_chinfo.)
            dest_by_src = {s: d for d, s in enumerate(order)}
            moved = {}
            for off, ident in orel.items():
                insn = off & ~3
                if lo <= insn < hi:
                    src = (insn - lo) // 4
                    moved[lo + dest_by_src[src] * 4 + (off - insn)] = ident
                else:
                    moved[off] = ident
            orel = moved
        rule["instruction_permutation"] = [
            _perm_entry(op, fn, ours, w) for w in pre]

    # --- the recolor aims at the intermediate when a post-perm is present ---
    if post:
        windows = [{"start": hex(w["lo"]), "end": hex(w["hi"]),
                    "order": w["order"]} for w in post]
        _w, ranges = wf.permutation_windows(windows, len(ours))
        recolor_tgt = wf.unpermute_target_windows(tgt, windows, ranges)
        rule["post_recolor_permutation"] = windows
    else:
        recolor_tgt = tgt

    # --- stages 2+3, ONCE PER SHAPE-CONSISTENT ARROW ORDER -----------------
    # regfield-first is the historical order and stays FIRST (it spends no
    # arrows when a renaming is needed anyway); form-first is the order the
    # mnemonic-aliasing law says was being shadowed.  Both are proven through
    # the shipped apply_patch, so enumerating adds candidates, never licence.
    # Per-order refusals, both reported: a single `last` string hides the more
    # informative order's failure behind whichever order happened to run last
    # (discipline 14 -- a refusal is a measurement of the guard too).
    notes = {}
    for form_first in (False, True):
        last = "no combination"
        sites, others, families = [], [], {}
        need_recolor = False
        for off in range(0, len(cur), 4):
            ow, tw = wf._u32(cur, off), wf._u32(recolor_tgt, off)
            what, info = classify(ow, tw, form_first=form_first)
            if what == "same":
                continue
            key = info[0] if what == "form" else what
            families[key] = families.get(key, 0) + 1
            if what == "other":
                others.append(off)
            elif what == "form":
                sites.append((off, info[0], info[1]))
            elif what == "regfield":
                need_recolor = True
        if others:
            # Identical under both orders (classify falls back), so this is a
            # property of the function: report it once and stop.
            return None, None, ("unabsorbed words at "
                                + ",".join(f"+0x{o:x}" for o in others[:8])
                                + (f" (+{len(others)-8} more)"
                                   if len(others) > 8 else ""))

        rmap = renaming_from_regfields(cur, recolor_tgt)

        choices = []
        for off, family, modes in sites:
            ours_d = wf.decode_copy_form(wf._u32(cur, off))
            theirs_d = wf.decode_copy_form(wf._u32(recolor_tgt, off))
            per_site = []
            for mode in modes:
                if mode == "constant_dataflow_recolor":
                    inv = {b: a for a, b in rmap.items()}
                    cands = our_source_candidates(
                        bytes(cur), off, theirs_d[2], orel, ojt, ours_d[2])
                    preferred = inv.get(theirs_d[2])
                    if preferred in cands:
                        cands = [preferred] + [c for c in cands
                                               if c != preferred]
                    for reg in cands[:6]:
                        per_site.append({"at": hex(off), "proof": mode,
                                         "our_source": reg})
                else:
                    per_site.append({"at": hex(off), "proof": mode})
            if not per_site:
                last = f"+0x{off:x}: no proof mode offered"
                choices = None
                break
            choices.append(per_site)
        if choices is None:
            notes["form-first" if form_first else "regfield-first"] = last
            continue

        combos = [[]]
        for per_site in choices:
            combos = [c + [e] for c in combos for e in per_site]
            if len(combos) > 400:
                combos = combos[:400]

        order_name = "form-first" if form_first else "regfield-first"
        for combo in combos:
            candidate = copy.deepcopy(rule)
            if combo:
                candidate["equivalent_copy_form"] = combo
            combined = any(e["proof"] in wf._COMBINED_PROOFS for e in combo)
            if need_recolor or combined:
                candidate["copy_register_fields"] = True
            try:
                resid = prove(op, tp, fn, candidate, tgt)
            except ValueError as e:
                last = str(e)
                continue
            if resid == 0:
                return candidate, 0, (f"CLOSES ({kind}); arrow order "
                                      f"{order_name}; families={families}")
            last = f"residual {resid}"
        notes[order_name] = last
    if len(set(notes.values())) == 1:
        return None, None, next(iter(notes.values()))
    return None, None, "; ".join(f"[{k}] {v}" for k, v in notes.items())


def _perm_entry(op, fn, ours, win):
    """Build the permutation entry, reading the REAL (offset,type,addend)
    relocation triples out of the object's RELA section.

    Fabricating the addend as 0 makes _relocation_sha256 disagree with the one
    apply_patch computes and the rule dies as "relocation input hash changed",
    which looks like a permutation defect and is really an authoring defect.
    """
    lo, hi, order = win["lo"], win["hi"], win["order"]
    region = ours[lo:hi]
    permuted = b"".join(region[s * 4:s * 4 + 4] for s in order)
    from ch_derive import raw_region_records, region_symbols, moved_symbols
    odata, osec, osym, _b, orel, _j = load(op, fn)
    recs = raw_region_records(odata, osec, osym, lo, hi) or []
    dest_by_src = {s: d for d, s in enumerate(order)}
    prec = sorted(((dest_by_src[(o // 4)] * 4 + (o % 4), t, a)
                   for o, t, a in recs), key=lambda x: x[0])
    # Name-bound relocation hashing (run-28 migration): passing the triples
    # alone raises "relocation hash needs the symbol name" on every window
    # that carries a relocation.  See ch_derive.region_symbols.
    win_syms = region_symbols(orel, lo, hi)
    return {"start": hex(lo), "end": hex(hi), "order": order,
            "before_sha256": sha(region), "after_sha256": sha(permuted),
            "before_relocations_sha256": wf._relocation_sha256(
                recs, win_syms),
            "after_relocations_sha256": wf._relocation_sha256(
                prec, moved_symbols(win_syms, order))}


def prove(op, tp, fn, rule, tgt):
    """Run the rule through the SHIPPED apply_patch; return residual words."""
    data = bytearray(open(op, "rb").read())
    wf.apply_patch(data, copy.deepcopy(rule), open(tp, "rb").read())
    sec = wf._sections(data)
    sym = wf._find_symbol(data, sec, fn)
    text = sec[sym.section_index]
    got = bytes(data[text.offset + sym.value:
                     text.offset + sym.value + sym.size])
    return sum(1 for o in range(0, len(got), 4)
               if wf._u32(got, o) != wf._u32(tgt, o))


if __name__ == "__main__":
    unit, fn = sys.argv[1], sys.argv[2]
    rule, resid, note = build_rule(unit, fn, verbose=True)
    print(f"{unit}::{fn}: {'CLOSES' if rule else 'refuses'} -- {note}")
    if rule:
        import json
        print(json.dumps(rule, indent=1))
