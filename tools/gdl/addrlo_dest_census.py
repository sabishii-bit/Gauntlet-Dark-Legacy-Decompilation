"""addrlo_dest_census.py -- image-wide census of the ADDR16_LO *destination* shape.

Companion to addr16_census.py (which censused the r0-homing shape and found
population P source-unreachable).  This tool censuses a DIFFERENT, disjoint
mechanism: where the low-half `addi` of an address materialisation WRITES.

For every `lis rT,SYM@ha ; ... ; addi rD,rT,SYM@lo` the destination rD is one of

  IN_PLACE  rD == rT   -- the low half is folded onto the HA temp itself
  R0        rD == r0   -- addr16_census.py's shape B (that lane's class)
  FRESH     rD != rT and rD != r0

IN_PLACE is the shape that forces a later register-to-register copy whenever the
completed address has to live somewhere other than the HA temp: our build emits
`addi rHOME,rT,0` where the target emits `addi rHOME,rT,SYM@lo` directly.

Populations, paired per function on the COUNT of IN_PLACE materialisations
(count-based rather than symbol-name-based, because the target split and our
objects spell the same file-scope object with different symbol names --
`sObjectsFile_80112AB8` vs `sObjectsFile` -- so a name join silently drops the
very instances under study):

  P  ours has MORE IN_PLACE than target  -- the divergence (we pay a copy)
  Q  both sides have the same nonzero IN_PLACE count (agreed in-place)
  R  both sides have zero IN_PLACE       (agreed fresh)
  S  ours has FEWER IN_PLACE than target -- inverse divergence

Also reports the plain-copy surplus (`addi rX,rY,0` / `mr` with no relocation),
which is the family-level signature the control-flow lane observed.

Usage:  python tools/gdl/addrlo_dest_census.py   (writes build/LC_addrlo.json)
"""
import os, re, json, subprocess, collections

OBJDUMP = os.path.abspath(os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")
CS = {f"r{i}" for i in range(14, 32)}

T_INSN = re.compile(r"^/\*\s+([0-9A-F]{8})\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s+\*/\s+(.*?)\s*$")
T_FN = re.compile(r"^\.fn\s+([^,]+),")
T_END = re.compile(r"^\.endfn\s+(\S+)")


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
                    t = m.group(2)
                    sym = None
                    mm = re.match(r"addi\s+r\d+,\s*r\d+,\s*(\S+)@l\b", t)
                    if mm:
                        sym = mm.group(1)
                    insns.append((t.replace(", ", ","), sym))


O_FN = re.compile(r"^[0-9a-f]{8} <(.+)>:\s*$")
O_INSN = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")
O_REL = re.compile(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)")


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
                    if m.group(1) == "ADDR16_LO":
                        insns[-1] = (insns[-1][0], m.group(2))
                    continue
                m = O_INSN.match(line)
                if m and cur is not None:
                    insns.append((m.group(2).replace(", ", ","), None))
            if cur:
                yield unit, cur, insns


ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),")
COPY = re.compile(r"^(?:addi\s+(r\d+),(r\d+),0|mr\s+(r\d+),(r\d+))\s*$")
STWU = re.compile(r"^stwu\s+r1,-")


def analyse(insns):
    """insns: list of (text, addr16lo_symbol_or_None)."""
    stwu_i = None
    for i, (t, _s) in enumerate(insns[:16]):
        if STWU.match(t):
            stwu_i = i
            break
    lo, copies = [], 0
    for i, (t, sym) in enumerate(insns):
        m = ADDI.match(t)
        if m and sym is not None:
            d, s = m.group(1), m.group(2)
            shape = "R0" if d == "r0" else ("IN_PLACE" if d == s else "FRESH")
            lo.append(dict(i=i, sym=sym, dst=d, src=s, shape=shape,
                           cs=(d in CS),
                           shadow=(stwu_i is not None and i < stwu_i)))
        elif sym is None and COPY.match(t):
            copies += 1
    return dict(lo=lo, copies=copies, n=len(insns), stwu=stwu_i,
                inplace=sum(1 for h in lo if h["shape"] == "IN_PLACE"),
                fresh=sum(1 for h in lo if h["shape"] == "FRESH"),
                r0=sum(1 for h in lo if h["shape"] == "R0"))


def main():
    tgt = {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = analyse(insns)
    ours = {}
    for unit, fn, insns in our_fns():
        ours[(unit, fn)] = analyse(insns)

    common = sorted(k for k in ours if k in tgt)
    print(f"paired functions: {len(common)}")

    P, Q, R, S = [], [], [], []
    for k in common:
        t, o = tgt[k], ours[k]
        rec = dict(unit=k[0], fn=k[1],
                   t_inplace=t["inplace"], o_inplace=o["inplace"],
                   t_fresh=t["fresh"], o_fresh=o["fresh"],
                   t_r0=t["r0"], o_r0=o["r0"],
                   t_copies=t["copies"], o_copies=o["copies"],
                   t_n=t["n"], o_n=o["n"],
                   t_lo=t["lo"], o_lo=o["lo"])
        if o["inplace"] > t["inplace"]:
            P.append(rec)
        elif o["inplace"] < t["inplace"]:
            S.append(rec)
        elif o["inplace"] > 0:
            Q.append(rec)
        else:
            R.append(rec)

    tot_lo_t = sum(len(tgt[k]["lo"]) for k in common)
    tot_lo_o = sum(len(ours[k]["lo"]) for k in common)
    tip = sum(tgt[k]["inplace"] for k in common)
    oip = sum(ours[k]["inplace"] for k in common)
    print()
    print("=" * 78)
    print("ADDR16_LO DESTINATION SHAPE, image-wide instruction counts")
    print(f"  total ADDR16_LO addi   target {tot_lo_t:6}   ours {tot_lo_o:6}")
    print(f"  IN_PLACE (rD == rT)    target {tip:6}   ours {oip:6}")
    print(f"  FRESH                  target {sum(tgt[k]['fresh'] for k in common):6}"
          f"   ours {sum(ours[k]['fresh'] for k in common):6}")
    print(f"  R0                     target {sum(tgt[k]['r0'] for k in common):6}"
          f"   ours {sum(ours[k]['r0'] for k in common):6}")
    print()
    print("PER-FUNCTION POPULATIONS (paired on IN_PLACE count)")
    print(f"  P  ours MORE in-place  (the divergence) : {len(P)}")
    print(f"  Q  equal, nonzero      (agreed in-place): {len(Q)}")
    print(f"  R  both zero           (agreed fresh)   : {len(R)}")
    print(f"  S  ours FEWER in-place (inverse)        : {len(S)}")

    def tab(name, pop, lim=200):
        print()
        print(f"--- {name} (n={len(pop)}) ---")
        print(f"  {'unit':28} {'fn':32} {'Tip':>3} {'Oip':>3} {'Tfr':>3} {'Ofr':>3} "
              f"{'Tcp':>3} {'Ocp':>3} {'Tn':>4} {'On':>4}")
        for r in pop[:lim]:
            print(f"  {r['unit'][:28]:28} {r['fn'][:32]:32} "
                  f"{r['t_inplace']:>3} {r['o_inplace']:>3} {r['t_fresh']:>3} {r['o_fresh']:>3} "
                  f"{r['t_copies']:>3} {r['o_copies']:>3} {r['t_n']:>4} {r['o_n']:>4}")

    tab("POPULATION P: ours folds the low half onto the HA temp, target does not", P)
    tab("POPULATION S: inverse", S)

    # ---- discriminant battery: P vs Q (both have an in-place form in OURS) ----
    print()
    print("=" * 78)
    print("DISCRIMINANT BATTERY  (per OUR in-place instance)")
    # build per-instance rosters
    def instances(pop, side):
        out = []
        for r in pop:
            for h in r[side]:
                if h["shape"] == "IN_PLACE":
                    out.append((r, h))
        return out
    Pi = instances(P, "o_lo")
    Qi = instances(Q, "o_lo")
    Ti = [(r, h) for r in (P + Q + R + S) for h in r["t_lo"] if h["shape"] == "IN_PLACE"]
    print(f"  our IN_PLACE instances in P functions: {len(Pi)}")
    print(f"  our IN_PLACE instances in Q functions: {len(Qi)}")
    print(f"  target IN_PLACE instances (all fns)  : {len(Ti)}")

    def dist(label, pop, fn):
        c = collections.Counter(fn(r, h) for r, h in pop)
        return dict(sorted(c.items(), key=lambda kv: str(kv[0])))

    for label, f in [
        ("in prologue shadow", lambda r, h: h["shadow"]),
        ("dest is callee-saved", lambda r, h: h["cs"]),
        ("formation index /8", lambda r, h: min(8, h["i"] * 8 // max(1, r["o_n"]))),
        ("src register", lambda r, h: h["src"]),
    ]:
        print(f"\n  {label}:")
        print(f"     P(ours) : {dist(label, Pi, f)}")
        print(f"     Q(ours) : {dist(label, Qi, f)}")
        print(f"     TARGET  : {dist(label, Ti, lambda r, h: f(r, h) if label != 'formation index /8' else min(8, h['i'] * 8 // max(1, r['t_n'])))}")

    json.dump(dict(P=P, Q=Q, S=S, R_count=len(R),
                   totals=dict(t_lo=tot_lo_t, o_lo=tot_lo_o, t_inplace=tip, o_inplace=oip)),
              open(os.path.join("build", "LC_addrlo.json"), "w"), indent=1)
    print("\nwrote build/LC_addrlo.json")


main()
