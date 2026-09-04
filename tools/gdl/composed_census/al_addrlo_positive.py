"""al_addrlo_positive.py -- the WHOEMITS census for the ADDR16_LO home-copy class.

Both sides are read as ELF OBJECTS (build/GUNE5D/obj = dtk target split,
build/GUNE5D/src = ours) with ONE objdump parser and ONE regex set, so the
hex-vs-decimal immediate hazard of the .s-text censuses
(claim.law.target-asm-hex-immediates-bias-any-hand-rolled-census) cannot
arise: relocations are read from R_PPC_ADDR16_HA / R_PPC_ADDR16_LO lines on
both sides.

For every ADDR16_LO address materialisation it records the matching @ha `lis`,
the frame boundary (`stwu r1,-N`), and the destination class, and then it
restricts the whole census to BYTE-EXACT functions -- fndiff.parse(t)==parse(b)
-- which is the positive corpus the class has never been measured on.

Usage: python tools/gdl/composed_census/al_addrlo_positive.py [--out PATH]
       (from the repo root; --out defaults under build/GUNE5D/)
"""
import os
import re
import sys
import json
import subprocess
import collections

ROOT = os.getcwd()
OBJDUMP = os.path.abspath(os.path.join("build", "binutils",
                                       "powerpc-eabi-objdump.exe"))
TGT = os.path.join("build", "GUNE5D", "obj")
OURS = os.path.join("build", "GUNE5D", "src")

CS = {f"r{i}" for i in range(14, 32)}
VOL = {"r0"} | {f"r{i}" for i in range(3, 14)}

O_FN = re.compile(r"^[0-9a-f]+ <(.+)>:\s*$")
O_INSN = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")
O_REL = re.compile(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)")

LIS = re.compile(r"^lis\s+(r\d+),")
ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),")
CPY = re.compile(r"^(?:mr\s+(r\d+),(r\d+)|addi\s+(r\d+),(r\d+),0)$")
STWU = re.compile(r"^stwu\s+r1,-")
BR = re.compile(r"^(?:b|ba|bl|blr|bc|bdnz|bne|beq|bge|blt|bgt|ble|bctr|bctrl)\b")
WRITE = re.compile(r"^(\w+)[a-z.]*\s+(r\d+),")
NONWRITER = {"stw", "stb", "sth", "stwu", "stwx", "stbx", "sthx", "stfs",
             "stfd", "cmpw", "cmpwi", "cmplw", "cmplwi", "b", "bl", "blr",
             "bc", "mtctr", "mtlr", "mtspr", "stmw", "stfsu", "stfdu",
             "stswi", "dcbf", "dcbi", "dcbz", "icbi", "sync", "isync",
             "eieio", "twi", "tw", "stwcx.", "stwbrx", "sthbrx"}


def dump(path):
    return subprocess.run([OBJDUMP, "-dr", "-m", "powerpc", path],
                          capture_output=True, text=True).stdout


def fns_of(path):
    """yield (name, [ (text, reloctype, relocsym) ])"""
    cur, insns = None, []
    for line in dump(path).splitlines():
        m = O_FN.match(line)
        if m:
            if cur is not None:
                yield cur, insns
            cur, insns = m.group(1), []
            continue
        m = O_REL.match(line)
        if m and insns:
            t, rt, rs = insns[-1]
            insns[-1] = (t, m.group(1), m.group(2))
            continue
        m = O_INSN.match(line)
        if m and cur is not None:
            insns.append((m.group(2).replace(", ", ",").strip(), None, None))
    if cur is not None:
        yield cur, insns


def writes(text, reg):
    m = WRITE.match(text)
    if not m:
        return False
    if m.group(1) in NONWRITER:
        return False
    return m.group(2) == reg


def analyse(insns):
    stwu = None
    for i, (t, _a, _b) in enumerate(insns[:24]):
        if STWU.match(t):
            stwu = i
            break
    out = []
    for i, (t, rt, rs) in enumerate(insns):
        if rt != "ADDR16_LO":
            continue
        m = ADDI.match(t)
        if not m:
            continue
        dst, src = m.group(1), m.group(2)
        # backwards: the matching @ha lis
        lis_i = None
        for j in range(i - 1, max(-1, i - 60), -1):
            tj, rtj, rsj = insns[j]
            if LIS.match(tj) and LIS.match(tj).group(1) == src \
               and rtj == "ADDR16_HA" and rsj == rs:
                lis_i = j
                break
            if writes(tj, src):
                break
        # forward: a copy of dst into another register
        copy_i, copy_dst = None, None
        for j in range(i + 1, min(i + 14, len(insns))):
            tj = insns[j][0]
            mc = CPY.match(tj)
            if mc:
                cd = mc.group(1) or mc.group(3)
                csrc = mc.group(2) or mc.group(4)
                if csrc == dst:
                    copy_i, copy_dst = j, cd
                    break
            if writes(tj, dst) or BR.match(tj):
                break
        if dst in CS:
            shape = "A"          # direct home into a callee-saved register
            home = dst
        elif copy_dst in CS:
            shape = "B"          # home copy paid, destination callee-saved
            home = copy_dst
        elif copy_dst is not None:
            shape = "V"          # home copy paid, destination VOLATILE
            home = copy_dst
        else:
            shape = "-"          # plain volatile address, no home
            home = None
        out.append(dict(sym=rs, shape=shape, dst=dst, home=home, idx=i,
                        lis=lis_i, copy=copy_i,
                        lis_shadow=(stwu is not None and lis_i is not None
                                    and lis_i < stwu),
                        addi_shadow=(stwu is not None and i < stwu),
                        framed=(stwu is not None)))
    return dict(sites=out, n=len(insns), stwu=stwu)


def main():
    out_path = os.path.join("build", "GUNE5D", "al_addrlo_positive.json")
    if "--out" in sys.argv:
        out_path = sys.argv[sys.argv.index("--out") + 1]

    sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
    import fndiff
    from pathlib import Path

    units = []
    for root, _d, names in os.walk(OURS):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if not n.endswith(".o"):
                continue
            op = os.path.join(root, n)
            rel = os.path.relpath(op, OURS)
            tp = os.path.join(TGT, rel)
            if os.path.exists(tp):
                units.append((rel.replace("\\", "/")[:-2], tp, op))

    tgt, ours, exact = {}, {}, set()
    for unit, tp, op in units:
        tparse = fndiff.parse(Path(tp))
        oparse = fndiff.parse(Path(op))
        for name in oparse:
            if name in tparse and tparse[name] == oparse[name]:
                exact.add((unit, name))
        for name, ins in fns_of(tp):
            tgt[(unit, name)] = analyse(ins)
        for name, ins in fns_of(op):
            ours[(unit, name)] = analyse(ins)

    common = sorted(k for k in ours if k in tgt)
    print(f"units paired      : {len(units)}")
    print(f"functions paired  : {len(common)}")
    print(f"functions EXACT   : {len(exact & set(common))}")

    def xtab(store, keys, label):
        c = collections.Counter()
        for k in keys:
            for s in store[k]["sites"]:
                if not s["framed"]:
                    continue
                c[(s["shape"], s["lis_shadow"], s["addi_shadow"])] += 1
        return c

    def show(label, keys):
        print()
        print("=" * 74)
        print(label + f"   (functions: {len(keys)})")
        print(f"{'':8} {'shape':>5} {'lis@shadow':>11} {'addi@shadow':>12} "
              f"{'TARGET':>8} {'OURS':>8}")
        ct = xtab(tgt, keys, "T")
        co = xtab(ours, keys, "O")
        for key in sorted(set(ct) | set(co)):
            sh, ls, ash = key
            print(f"{'':8} {sh:>5} {str(ls):>11} {str(ash):>12} "
                  f"{ct[key]:>8} {co[key]:>8}")
        # the D13 SPLIT statistic
        for name, c in (("TARGET", ct), ("OURS", co)):
            split = sum(v for (sh, ls, ash), v in c.items()
                        if ls and not ash)
            both = sum(v for (sh, ls, ash), v in c.items() if ls and ash)
            none = sum(v for (sh, ls, ash), v in c.items() if not ls)
            splitA = sum(v for (sh, ls, ash), v in c.items()
                         if ls and not ash and sh == "A")
            print(f"   {name:7} @ha/@lo SPLIT across frame setup: {split:5}"
                  f" (of which shape A: {splitA})   both-in-shadow: {both:5}"
                  f"   lis-after-frame: {none:5}")

    show("ALL PAIRED FUNCTIONS", common)
    ex = sorted(exact & set(common))
    show("BYTE-EXACT FUNCTIONS ONLY (the positive corpus)", ex)

    # per-function populations on the generalised B|V count
    def bcount(a):
        return sum(1 for s in a["sites"] if s["shape"] in ("B", "V"))

    P, Q, R, S = [], [], [], []
    for k in common:
        tb, ob = bcount(tgt[k]), bcount(ours[k])
        rec = dict(unit=k[0], fn=k[1], tB=tb, oB=ob,
                   tn=tgt[k]["n"], on=ours[k]["n"],
                   exact=(k in exact))
        (P if ob > tb else S if ob < tb else Q if ob else R).append(rec)
    print()
    print("=" * 74)
    print("PER-FUNCTION POPULATIONS (paid-copy count, CS *and* volatile homes)")
    print(f"  P ours MORE copies : {len(P)}")
    print(f"  Q equal nonzero    : {len(Q)}   (of which byte-EXACT: "
          f"{sum(1 for r in Q if r['exact'])})")
    print(f"  R both zero        : {len(R)}")
    print(f"  S ours FEWER       : {len(S)}")
    print()
    print("--- P roster ---")
    for r in sorted(P, key=lambda r: (r["unit"], r["fn"])):
        print(f"  {r['unit'][:30]:30} {r['fn'][:34]:34} tB={r['tB']} oB={r['oB']}"
              f"  T{r['tn']}/O{r['on']}")
    if S:
        print("--- S roster (INVERSE) ---")
        for r in S:
            print(f"  {r['unit'][:30]:30} {r['fn'][:34]:34} tB={r['tB']} "
                  f"oB={r['oB']}  T{r['tn']}/O{r['on']}")

    json.dump(dict(P=P, Q=Q, S=S, R=len(R),
                   exact=sorted(f"{u}::{f}" for u, f in ex)),
              open(out_path, "w"), indent=1)
    print(f"\nwrote {out_path}")


main()
