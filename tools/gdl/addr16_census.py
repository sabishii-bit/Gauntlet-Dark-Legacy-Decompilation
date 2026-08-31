"""addr16_census.py -- image-wide census of the ADDR16_LO callee-saved "home" shapes.

Joins the TARGET split assembly (build/GUNE5D/asm) with OUR built objects
(build/GUNE5D/src) function-by-function and classifies, per (function, symbol),
how the address of a file-scope object is materialised into its callee-saved home:

  shape A  lis rT,SYM@ha ; addi rHOME,rT,SYM@lo        (coalesced, direct)
  shape B  lis rT,SYM@ha ; addi r0,rT,SYM@lo ; mr rHOME,r0   (home copy paid)

Populations reported:
  P  target=A ours=B  -- the divergence (our allocator declined to coalesce)
  Q  target=B ours=B  -- both pay the copy (agreed)
  R  target=A ours=A  -- both direct (agreed)
  S  target=B ours=A  -- inverse divergence

Also reports the prologue-shadow size histogram on both sides, which is the
control for "does our build schedule the frame shadow differently".

Usage:  python tools/gdl/addr16_census.py      (writes build/AD_paired.json)
"""
import os, re, json, subprocess, collections

OBJDUMP = os.path.abspath(os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")
CS = {f"r{i}" for i in range(14, 32)}
VOL = {"r0"} | {f"r{i}" for i in range(3, 13)}

# ---------------- target ----------------
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
                    insns.append(m.group(2))


# ---------------- ours ----------------
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
                    insns.append((m.group(2), None))
            if cur:
                yield unit, cur, insns


def analyse(texts, syms):
    """texts: list of insn strings. syms: parallel list of ADDR16_LO symbol or None.
    Returns dict with shadow info and home-shape records."""
    stwu_i = None
    for i, t in enumerate(texts[:14]):
        if re.match(r"stwu\s+r1,\s*-", t.replace(", ", ",")):
            stwu_i = i
            break
    hoisted = []
    if stwu_i is not None:
        for i in range(stwu_i):
            t = texts[i]
            op = t.split()[0] if t.split() else ""
            if op in ("mflr", "mfspr"):
                continue
            if re.match(r"stw\s+r0,\s*(0x)?[48]\(r1\)", t.replace(", ", ",")):
                continue
            if re.match(r"stw\s+r0,0x?[48]\(r1\)", t):
                continue
            hoisted.append(t)
    # saved-reg style
    save = "none"
    for t in texts[:20]:
        if t.startswith("stmw "):
            save = "stmw"
            break
        if re.match(r"stw\s+r3[01],", t):
            save = "stw"
    is_leaf = not any(t.startswith("bl ") or t.startswith("bl\t") for t in texts)
    frame = None
    m = None
    for t in texts[:14]:
        m = re.match(r"stwu\s+r1,\s*-(?:0x)?([0-9a-fA-F]+)\(r1\)", t.replace(", ", ","))
        if m:
            frame = int(m.group(1), 16) if t.count("0x") else int(m.group(1))
            break

    homes = []
    for i, t in enumerate(texts):
        sym = syms[i]
        mm = re.match(r"addi\s+(r\d+),\s*(r\d+),", t.replace(", ", ","))
        if not mm or sym is None:
            continue
        dst = mm.group(1)
        if dst in CS:
            homes.append(dict(sym=sym, shape="A", reg=dst, idx=i,
                              in_shadow=(stwu_i is not None and i < stwu_i)))
        elif dst in VOL:
            for j in range(i + 1, min(i + 9, len(texts))):
                m2 = re.match(r"mr\s+(r\d+),\s*r?" + dst[1:] + r"\s*$",
                              texts[j].replace(", ", ","))
                m2 = re.match(r"mr\s+(r\d+)," + dst + r"$", texts[j].replace(", ", ","))
                if m2 and m2.group(1) in CS:
                    homes.append(dict(sym=sym, shape="B", reg=m2.group(1), via=dst, idx=i,
                                      gap=j - i,
                                      in_shadow=(stwu_i is not None and i < stwu_i)))
                    break
                if re.match(r"^(b|bl|blr|bc|bdnz)", texts[j]):
                    break
    return dict(hoisted=hoisted, n_hoist=len(hoisted), save=save, leaf=is_leaf,
                frame=frame, homes=homes, n=len(texts))


def main():
    # target: resolve @l symbol names
    tgt = {}
    thist = collections.Counter()
    for unit, fn, texts in target_fns():
        syms = []
        for t in texts:
            m = re.match(r"addi\s+r\d+,\s*r\d+,\s*(\S+)@l\b", t)
            syms.append(m.group(1) if m else None)
        a = analyse([t.replace(", ", ",") for t in texts], syms)
        tgt[(unit, fn)] = a
        if a["frame"] is not None:
            thist[a["n_hoist"]] += 1

    ours = {}
    ohist = collections.Counter()
    for unit, fn, pairs in our_fns():
        texts = [p[0].replace(", ", ",") for p in pairs]
        syms = [p[1] for p in pairs]
        a = analyse(texts, syms)
        ours[(unit, fn)] = a
        if a["frame"] is not None:
            ohist[a["n_hoist"]] += 1

    print("=" * 78)
    print("PROLOGUE-SHADOW SIZE HISTOGRAM (framed functions)")
    print(f"  {'hoisted':>8} {'TARGET':>8} {'OURS':>8}")
    for k in sorted(set(thist) | set(ohist)):
        print(f"  {k:>8} {thist.get(k,0):>8} {ohist.get(k,0):>8}")
    print(f"  {'MAX':>8} {max(thist):>8} {max(ohist):>8}")

    # paired
    common = [k for k in ours if k in tgt]
    print()
    print(f"paired functions: {len(common)}")
    dis = [k for k in common if ours[k]["n_hoist"] != tgt[k]["n_hoist"]]
    over = [k for k in dis if ours[k]["n_hoist"] > tgt[k]["n_hoist"]]
    under = [k for k in dis if ours[k]["n_hoist"] < tgt[k]["n_hoist"]]
    print(f"shadow-size disagreements: {len(dis)}  (ours LARGER {len(over)}, ours SMALLER {len(under)})")

    # home-shape populations, keyed per symbol
    P, Q, R, S = [], [], [], []
    for k in common:
        tmap = {h["sym"]: h for h in tgt[k]["homes"]}
        omap = {h["sym"]: h for h in ours[k]["homes"]}
        for sym in set(tmap) & set(omap):
            ts, os_ = tmap[sym]["shape"], omap[sym]["shape"]
            rec = dict(unit=k[0], fn=k[1], sym=sym,
                       t=tmap[sym], o=omap[sym],
                       t_hoist=tgt[k]["n_hoist"], o_hoist=ours[k]["n_hoist"],
                       t_save=tgt[k]["save"], o_save=ours[k]["save"],
                       t_frame=tgt[k]["frame"], o_frame=ours[k]["frame"],
                       leaf=ours[k]["leaf"], n=ours[k]["n"])
            if ts == "A" and os_ == "B":
                P.append(rec)
            elif ts == "B" and os_ == "B":
                Q.append(rec)
            elif ts == "A" and os_ == "A":
                R.append(rec)
            else:
                S.append(rec)

    print()
    print("HOME-SHAPE POPULATIONS (same function, same symbol, both sides)")
    print(f"  P  target=A ours=B  (THE BUG)      : {len(P)}")
    print(f"  Q  target=B ours=B  (agreed copy)  : {len(Q)}")
    print(f"  R  target=A ours=A  (agreed direct): {len(R)}")
    print(f"  S  target=B ours=A  (inverse)      : {len(S)}")

    def tab(name, pop):
        print()
        print(f"--- {name} (n={len(pop)}) ---")
        print(f"  {'unit':30} {'fn':30} {'sym':20} {'Treg':5} {'Oreg':5} "
              f"{'Thoist':6} {'Ohoist':6} {'Tshdw':5} {'Oshdw':5} {'Tsave':5} {'Osave':5}")
        for r in pop:
            print(f"  {r['unit'][:30]:30} {r['fn'][:30]:30} {r['sym'][:20]:20} "
                  f"{r['t'].get('reg',''):5} {r['o'].get('reg',''):5} "
                  f"{r['t_hoist']:6} {r['o_hoist']:6} "
                  f"{str(r['t'].get('in_shadow'))[:5]:5} {str(r['o'].get('in_shadow'))[:5]:5} "
                  f"{r['t_save']:5} {r['o_save']:5}")

    tab("POPULATION P: target direct, ours homed via scratch", P)
    tab("POPULATION S: target homed, ours direct", S)

    # discriminant tables P vs R
    print()
    print("=" * 78)
    print("CANDIDATE DISCRIMINANTS  (P = bug, R = agreed-direct control)")
    def dist(pop, key, fn):
        c = collections.Counter(fn(r) for r in pop)
        return dict(sorted(c.items(), key=lambda kv: str(kv[0])))
    for label, fn in [
        ("target addi in prologue shadow", lambda r: r["t"].get("in_shadow")),
        ("OURS addi in prologue shadow", lambda r: r["o"].get("in_shadow")),
        ("target hoist count", lambda r: r["t_hoist"]),
        ("OURS hoist count", lambda r: r["o_hoist"]),
        ("save style (target)", lambda r: r["t_save"]),
        ("home register", lambda r: r["t"].get("reg")),
        ("leaf", lambda r: r["leaf"]),
    ]:
        print(f"\n  {label}:")
        print(f"     P: {dist(P, label, fn)}")
        print(f"     R: {dist(R, label, fn)}")

    json.dump(dict(P=[{kk: vv for kk, vv in r.items()} for r in P],
                   Q=[{kk: vv for kk, vv in r.items()} for r in Q],
                   R=[{kk: vv for kk, vv in r.items()} for r in R],
                   S=[{kk: vv for kk, vv in r.items()} for r in S],
                   thist=dict(thist), ohist=dict(ohist),
                   shadow_over=[list(k) for k in over],
                   shadow_under=[list(k) for k in under]),
              open(os.path.join("build", "AD_paired.json"), "w"), indent=1)
    print("\nwrote build/AD_paired.json")


main()
