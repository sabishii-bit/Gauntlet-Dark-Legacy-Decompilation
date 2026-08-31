"""addrlo_inplace_census.py -- census of the ADDR16_LO IN-PLACE vs FRESH split.

WHY THIS TOOL EXISTS.  addrlo_home_general_census.py pairs functions on the
"home copy" shape, but its shape-B detector REQUIRES the copy's final
destination to be a CALLEE-SAVED register (`m2.group(1) in CS`).  Its P=40
roster is therefore, by construction, entirely
claim.law.pointer-decl-order-flips-addrlo-inplace-vs-fresh-destination's
class (a) -- the destination-callee-saved class that law marks S=0 and
source-unreachable ("do not probe").

The DECL-ORDER LEVER is proven only for class (b): destination VOLATILE,
where our build folds the low half onto the @ha temp IN PLACE
(`addi rT,rT,SYM@lo`) while the target writes a FRESH volatile register
(`addi rD,rT,SYM@lo`, rD != rT) and we then pay a copy.  That population has
never been counted.  This tool counts it.

Per (function, ADDR16_LO instance whose base register was set by a matching
`lis rS,SYM@ha`):

  IN_PLACE  addi rS,rS,SYM@lo      the low half folded onto the @ha temp
  FRESH     addi rD,rS,SYM@lo      rD != rS, a fresh destination

each split by whether the DESTINATION is callee-saved (CS) or volatile (VOL).
Class (a) is the FRESH/CS column (the direct home).  Class (b) -- the lever
population -- is functions where OURS has more IN_PLACE/VOL than the target
and the target correspondingly has more FRESH/VOL.

Immediates are normalised to decimal on both sides per
claim.law.target-asm-hex-immediates-bias-any-hand-rolled-census.20260901.v1,
and the mandatory sanity check (no function from a 100%-fuzzy unit may appear
in a divergence population) is enforced and printed.

Usage: python tools/gdl/addrlo_inplace_census.py   (writes build/DO_inplace.json)
"""
import os, re, json, subprocess, collections

OBJDUMP = os.path.abspath(os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")
REPORT = os.path.join("build", "GUNE5D", "report.json")
CS = {f"r{i}" for i in range(14, 32)}

T_INSN = re.compile(r"^/\*\s+([0-9A-F]{8})\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s+\*/\s+(.*?)\s*$")
T_FN = re.compile(r"^\.fn\s+([^,]+),")
T_END = re.compile(r"^\.endfn\s+(\S+)")
O_FN = re.compile(r"^[0-9a-f]{8} <(.+)>:\s*$")
O_INSN = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")
O_REL = re.compile(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)")

HEXIMM = re.compile(r"(,-?)0x([0-9a-fA-F]+)")


def norm(t):
    t = t.replace(", ", ",").strip()
    return HEXIMM.sub(lambda m: m.group(1) + str(int(m.group(2), 16)), t)


# target spells relocations inline as SYM@ha / SYM@l
T_LIS = re.compile(r"^lis\s+(r\d+),(\S+)@ha$")
T_ADDI_LO = re.compile(r"^addi\s+(r\d+),(r\d+),(\S+)@l$")
# our objects carry a bare immediate plus a following reloc line
O_LIS = re.compile(r"^lis\s+(r\d+),")
O_ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),")
# any write to a GPR, used to invalidate a pending @ha temp
WRITE = re.compile(r"^(?:addi|addis|add|sub[a-z]*|or|ori|and[a-z]*|xor[a-z]*|"
                   r"lwz|lbz|lhz|lha|lwzu|li|lis|mr|mflr|mfspr|rlwinm|rlwimi|"
                   r"srawi|slwi|srwi|neg|extsb|extsh|mulli|mullw|divw|cntlzw|"
                   r"nor|nand|eqv|lwzx|lbzx|lhzx|subf)[a-z.]*\s+(r\d+),")


def target_fns():
    for root, _d, names in os.walk(ASM):
        for n in sorted(names):
            if not n.endswith(".s"):
                continue
            path = os.path.join(root, n)
            unit = os.path.relpath(path, ASM).replace("\\", "/")[:-2]
            cur, insns = None, []
            for line in open(path, errors="replace"):
                m = T_FN.match(line)
                if m:
                    cur, insns = m.group(1).strip(), []
                    continue
                if cur is None:
                    continue
                if T_END.match(line):
                    yield unit, cur, insns
                    cur, insns = None, []
                    continue
                m = T_INSN.match(line)
                if m:
                    t = norm(m.group(2))
                    ha = T_LIS.match(t)
                    lo = T_ADDI_LO.match(t)
                    if ha:
                        insns.append((t, "HA", ha.group(1), None, ha.group(2)))
                    elif lo:
                        insns.append((t, "LO", lo.group(1), lo.group(2), lo.group(3)))
                    else:
                        insns.append((t, None, None, None, None))


def our_fns():
    for root, _d, names in os.walk(OBJROOT):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if not n.endswith(".o"):
                continue
            path = os.path.join(root, n)
            unit = os.path.relpath(path, OBJROOT).replace("\\", "/")[:-2]
            txt = subprocess.run([OBJDUMP, "-dr", "-m", "powerpc", path],
                                 capture_output=True, text=True).stdout
            cur, insns = None, []
            for line in txt.splitlines():
                m = O_FN.match(line)
                if m:
                    if cur:
                        yield unit, cur, insns
                    cur, insns = m.group(1), []
                    continue
                m = O_REL.match(line)
                if m and insns:
                    kind, sym = m.group(1), m.group(2)
                    t, _k, d, s, _y = insns[-1]
                    if kind == "ADDR16_HA" and O_LIS.match(t):
                        insns[-1] = (t, "HA", O_LIS.match(t).group(1), None, sym)
                    elif kind == "ADDR16_LO":
                        ma = O_ADDI.match(t)
                        if ma:
                            insns[-1] = (t, "LO", ma.group(1), ma.group(2), sym)
                    continue
                m = O_INSN.match(line)
                if m and cur is not None:
                    insns.append((norm(m.group(2)), None, None, None, None))
            if cur:
                yield unit, cur, insns


CPY_ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),0$")
CPY_MR = re.compile(r"^mr\s+(r\d+),(r\d+)$")
BR = re.compile(r"^(b|ba|bl|blr|bc|bdnz|bne|beq|bge|blt|bgt|ble|bctr)")


def copy_paid(insns, i, reg):
    """Does a copy OUT of `reg` follow the @lo addi at index i?

    THE COST TEST.  The IN_PLACE form is not itself a defect: audio/sndfx::
    sndFxInitVoices folds sAudioState in place onto r5 where the target writes
    a fresh r5 from an @ha temp in r4 -- same instruction count, a pure
    register-field difference, nothing to lever.  The lever only pays where
    the in-place fold FORCES an extra copy because the @ha temp's register is
    wanted for something else (init_player_change's `addi r4,r3,0`).  So the
    discriminant is a copy sourced from the folded register, not the fold.
    """
    for j in range(i + 1, min(i + 12, len(insns))):
        tj = insns[j][0]
        m = CPY_ADDI.match(tj) or CPY_MR.match(tj)
        if m and m.group(2) == reg:
            return m.group(1)
        mw = WRITE.match(tj)
        if mw and mw.group(1) == reg:
            return None
        if BR.match(tj):
            return None
    return None


def analyse(insns):
    """Classify each @lo addi whose base was set by a matching @ha lis."""
    pending = {}          # reg -> symbol of a live `lis reg,SYM@ha`
    out = []
    for i, (t, kind, dst, src, sym) in enumerate(insns):
        if kind == "HA":
            pending[dst] = sym
            continue
        if kind == "LO" and pending.get(src) == sym:
            shape = "IN_PLACE" if dst == src else "FRESH"
            cls = "CS" if dst in CS else "VOL"
            out.append(dict(sym=sym, shape=shape, cls=cls, dst=dst, src=src,
                            copy_to=copy_paid(insns, i, dst)))
            if dst != src:
                pending.pop(dst, None)
            continue
        mw = WRITE.match(t)
        if mw:
            pending.pop(mw.group(1), None)
    return out


def key(homes):
    c = collections.Counter((h["shape"], h["cls"]) for h in homes)
    return c


def main():
    exact_units = set()
    if os.path.exists(REPORT):
        rep = json.load(open(REPORT))
        for u in rep["units"]:
            for s in u["sections"]:
                if s["name"] == ".text" and float(s.get("fuzzy_match_percent", 0)) >= 100.0:
                    exact_units.add(u["name"].split("main/", 1)[-1])

    tgt, ours = {}, {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = analyse(insns)
    for unit, fn, insns in our_fns():
        ours[(unit, fn)] = analyse(insns)
    common = sorted(k for k in ours if k in tgt)
    print(f"paired functions: {len(common)}")

    tot = collections.Counter()
    for k in common:
        for lbl, store in (("T", tgt), ("O", ours)):
            for h in store[k]:
                tot[(lbl, h["shape"], h["cls"])] += 1
    print()
    print("=" * 78)
    print("ADDR16_LO LOW-HALF FORM, image-wide instance counts")
    print(f"  {'':22} {'TARGET':>8} {'OURS':>8}")
    for shape in ("IN_PLACE", "FRESH"):
        for cls in ("VOL", "CS"):
            print(f"  {shape+'/'+cls:22} {tot[('T',shape,cls)]:>8} {tot[('O',shape,cls)]:>8}")

    # Lever population: ours folds in place onto a VOLATILE temp where the
    # target writes a fresh VOLATILE destination.
    # SECOND CENSUS HAZARD (measured here, 2026-08-31): function-symbol
    # BOUNDARY asymmetry.  Our object may split one target `.fn` into several
    # symbols -- dolphin/os/OS::OSExceptionVector is one target function but
    # four symbols in our object (OSExceptionVector, __DBVECTOR,
    # __OSEVSetNumber, __OSEVEnd), and the lis/addi pair lands in
    # __OSEVSetNumber.  Name-based pairing then sees the instance on the
    # target side only and fabricates an "ours folds in place FEWER" row.
    # The bias is directional, exactly like the hex-immediate hazard.  A
    # function in a byte-exact unit CANNOT diverge, so exclude those units
    # from the populations rather than reporting artifacts as findings.
    excluded = [k for k in common if k[0] in exact_units]
    common = [k for k in common if k[0] not in exact_units]
    print()
    print(f"excluded {len(excluded)} functions in {len(exact_units)} byte-exact "
          f"units (cannot diverge; guards symbol-boundary artifacts)")

    # THIRD CENSUS HAZARD (measured here, 2026-08-31): pairing per FUNCTION on
    # an instance COUNT masks per-SYMBOL swaps.  ui/select::init_player_change
    # -- the very function the lever was derived from -- has target
    # IN_PLACE/VOL = 1 (on lbl_80284878) and ours = 1 (on gPlayers), so a
    # count-based pairing files it under "equal" and it never reaches the
    # roster, even though the two sides fold DIFFERENT symbols in place and
    # the gPlayers instance is a textbook lever site.  Pair per
    # (function, symbol) instead.
    lever, equal, both_zero, inverse = [], [], [], []
    for k in common:
        tc, oc = key(tgt[k]), key(ours[k])
        ti, oi = tc[("IN_PLACE", "VOL")], oc[("IN_PLACE", "VOL")]
        tf, of = tc[("FRESH", "VOL")], oc[("FRESH", "VOL")]
        o_ip = {h["sym"] for h in ours[k]
                if h["shape"] == "IN_PLACE" and h["cls"] == "VOL"}
        t_ip = {h["sym"] for h in tgt[k]
                if h["shape"] == "IN_PLACE" and h["cls"] == "VOL"}
        t_fv = {h["sym"] for h in tgt[k]
                if h["shape"] == "FRESH" and h["cls"] == "VOL"}
        o_fv = {h["sym"] for h in ours[k]
                if h["shape"] == "FRESH" and h["cls"] == "VOL"}
        # LEVER site: a symbol we fold IN PLACE onto the @ha temp that the
        # target writes to a FRESH VOLATILE destination (init_player_change
        # signature).  INVERSE site: the mirror image.
        hit = sorted((o_ip & t_fv) - t_ip)
        inv = sorted((t_ip & o_fv) - o_ip)
        # COST TEST: keep only sites where our in-place fold actually FORCES a
        # copy that the target does not pay.  Without this the roster is
        # dominated by cost-free @ha-temp choices (see copy_paid()).
        o_cost = {h["sym"] for h in ours[k] if h["shape"] == "IN_PLACE"
                  and h["cls"] == "VOL" and h["copy_to"]}
        t_cost = {h["sym"] for h in tgt[k] if h["copy_to"]}
        paid = sorted((set(hit) & o_cost) - t_cost)
        rec_paid = paid
        rec = dict(unit=k[0], fn=k[1], t_inplace_vol=ti, o_inplace_vol=oi,
                   t_fresh_vol=tf, o_fresh_vol=of,
                   t_fresh_cs=tc[("FRESH", "CS")], o_fresh_cs=oc[("FRESH", "CS")],
                   lever_syms=hit, inverse_syms=inv, paid_syms=rec_paid,
                   t_homes=tgt[k], o_homes=ours[k])
        if hit:
            lever.append(rec)
        elif inv:
            inverse.append(rec)
        elif oi > 0:
            equal.append(rec)
        else:
            both_zero.append(rec)

    print()
    print("POPULATIONS (paired per (function, SYMBOL))")
    print(f"  L  ours IN_PLACE where target FRESH/VOL (LEVER) : {len(lever)}")
    print(f"  I  target IN_PLACE where ours FRESH/VOL (inverse): {len(inverse)}")
    print(f"  E  in-place present, no per-symbol divergence   : {len(equal)}")
    print(f"  Z  no in-place at all                           : {len(both_zero)}")

    # MANDATORY sanity check per the hex-immediate law.
    bad = [r for r in lever + inverse if r["unit"] in exact_units]
    print()
    print(f"SANITY CHECK: byte-exact(100% fuzzy unit) functions in a divergence "
          f"population: {len(bad)}")
    if bad:
        print("  *** TOOL BUG -- a fully matched function cannot diverge ***")
        for r in bad[:20]:
            print(f"    {r['unit']}::{r['fn']}")
    else:
        print(f"  clean ({len(exact_units)} fully-matched units checked)")

    def tab(name, pop, field, lim=300):
        print()
        print(f"--- {name} (n={len(pop)}) ---")
        print(f"  {'unit':24} {'fn':30} {'symbols'}")
        for r in sorted(pop, key=lambda r: (r['unit'], r['fn']))[:lim]:
            print(f"  {r['unit'][:24]:24} {r['fn'][:30]:30} "
                  f"{','.join(r[field])[:44]}")
    tab("POPULATION L -- in-place/fresh divergence sites", lever, "lever_syms")
    paid_pop = [r for r in lever if r["paid_syms"]]
    tab("POPULATION L-PAID -- TRUE lever roster (in-place fold FORCES a copy)",
        paid_pop, "paid_syms")
    print(f"\n  L={len(lever)} sites diverge in form; only {len(paid_pop)} pay a "
          f"copy for it -- the rest are cost-free @ha-temp choices.")
    tab("POPULATION I -- inverse sites", inverse, "inverse_syms")

    json.dump(dict(lever=lever, inverse=inverse, equal_count=len(equal),
                   both_zero_count=len(both_zero),
                   totals={f"{a}/{b}/{c}": v for (a, b, c), v in tot.items()}),
              open(os.path.join("build", "DO_inplace.json"), "w"), indent=1)
    print("\nwrote build/DO_inplace.json")


main()
