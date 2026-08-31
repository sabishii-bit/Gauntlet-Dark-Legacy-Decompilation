"""addrlo_shadow_probe.py -- is the ADDR16_LO home copy a SCHEDULER divergence?

Follow-up to addrlo_home_general_census.py.  For every paired function it
measures the prologue-shadow fill (non-prologue instructions scheduled between
`mflr` and `stwu r1,-N`) on both sides, and cross-tabulates the shadow-size
disagreement against the shape-B (address-home-copy) surplus.

The causal question, which addr16_census.py could not ask because its shape-B
detector only recognised `mr rHOME,r0`:  does our build pay the copy BECAUSE its
scheduler hoisted the low-half `addi` into the prologue shadow (where the
callee-saved home is not yet saved and therefore cannot be written)?

Usage: python tools/gdl/addrlo_shadow_probe.py
"""
import os, re, json, subprocess, collections

OBJDUMP = os.path.abspath(os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")
CS = {f"r{i}" for i in range(14, 32)}

T_INSN = re.compile(r"^/\*\s+([0-9A-F]{8})\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s+\*/\s+(.*?)\s*$")
T_FN = re.compile(r"^\.fn\s+([^,]+),")
T_END = re.compile(r"^\.endfn\s+(\S+)")
O_FN = re.compile(r"^[0-9a-f]{8} <(.+)>:\s*$")
O_INSN = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")
O_REL = re.compile(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)")


HEXIMM = re.compile(r"(,-?)0x([0-9a-fA-F]+)")


def norm(t):
    """Target .s spells copies `addi rD,rS,0x0`; objdump spells them `,0`.
    Normalise immediates to decimal or the copy detector is one-sided."""
    t = t.replace(", ", ",").strip()
    return HEXIMM.sub(lambda m: m.group(1) + str(int(m.group(2), 16)), t)


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
                    mm = re.match(r"addi\s+r\d+,\s*r\d+,\s*(\S+)@l\b", t)
                    insns.append((norm(t), mm.group(1) if mm else None))


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
                    insns.append((norm(m.group(2)), None))
            if cur:
                yield unit, cur, insns


ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),")
CPY = re.compile(r"^(?:addi\s+(r\d+),(r\d+),0|mr\s+(r\d+),(r\d+))$")
STWU = re.compile(r"^stwu\s+r1,-")
BR = re.compile(r"^(b|ba|bl|blr|bc|bdnz|bne|beq|bge|blt|bgt|ble|bctr)")
WRITE = re.compile(r"^(?:addi|addis|add|sub[a-z]*|or|ori|and[a-z]*|xor[a-z]*|"
                   r"lwz|lbz|lhz|lha|lwzu|li|lis|mr|mflr|mfspr|rlwinm|rlwimi|"
                   r"srawi|slwi|srwi|neg|extsb|extsh|mulli|mullw|divw|cntlzw|"
                   r"nor|nand|eqv|lwzx|lbzx|lhzx|subf)[a-z.]*\s+(r\d+),")
PRO = re.compile(r"^(mflr|mfspr)\b|^stw\s+r0,(0x)?[48]\(r1\)$")


def analyse(insns):
    stwu_i = None
    for i, (t, _s) in enumerate(insns[:16]):
        if STWU.match(t):
            stwu_i = i
            break
    hoisted = []
    if stwu_i is not None:
        for i in range(stwu_i):
            if PRO.match(insns[i][0]):
                continue
            hoisted.append(insns[i][0])
    homes = []
    for i, (t, sym) in enumerate(insns):
        m = ADDI.match(t)
        if not m or sym is None:
            continue
        dst, src = m.group(1), m.group(2)
        shadow = (stwu_i is not None and i < stwu_i)
        if dst in CS:
            homes.append(dict(shape="A", sym=sym, home=dst, idx=i, shadow=shadow))
            continue
        for j in range(i + 1, min(i + 12, len(insns))):
            tj = insns[j][0]
            m2 = CPY.match(tj)
            if m2:
                a, b = (m2.group(1), m2.group(2)) if m2.group(1) else (m2.group(3), m2.group(4))
                if b == dst and a in CS:
                    homes.append(dict(shape="B", sym=sym, home=a, idx=i, shadow=shadow))
                    break
            mw = WRITE.match(tj)
            if mw and mw.group(1) == dst:
                break
            if BR.match(tj):
                break
    return dict(hoisted=hoisted, nh=len(hoisted), homes=homes, n=len(insns),
                framed=(stwu_i is not None),
                B=sum(1 for h in homes if h["shape"] == "B"),
                Bshadow=sum(1 for h in homes if h["shape"] == "B" and h["shadow"]))


def main():
    tgt, ours = {}, {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = analyse(insns)
    for unit, fn, insns in our_fns():
        ours[(unit, fn)] = analyse(insns)
    common = sorted(k for k in ours if k in tgt and ours[k]["framed"] and tgt[k]["framed"])
    print(f"paired framed functions: {len(common)}")

    dis = [k for k in common if ours[k]["nh"] != tgt[k]["nh"]]
    over = [k for k in dis if ours[k]["nh"] > tgt[k]["nh"]]
    under = [k for k in dis if ours[k]["nh"] < tgt[k]["nh"]]
    print(f"shadow-size disagreements: {len(dis)}  (ours LARGER {len(over)}, ours SMALLER {len(under)})")

    Pfns = [k for k in common if ours[k]["B"] > tgt[k]["B"]]
    Pshadow = [k for k in Pfns if ours[k]["Bshadow"] > tgt[k]["Bshadow"]]
    print(f"P functions (ours more shape-B): {len(Pfns)}   of which the surplus copy is IN SHADOW: {len(Pshadow)}")

    print()
    print("=" * 78)
    print("2x2: shadow-size disagreement  x  shape-B surplus")
    a = sum(1 for k in common if k in set(over) and ours[k]["B"] > tgt[k]["B"])
    b = sum(1 for k in over if not (ours[k]["B"] > tgt[k]["B"]))
    c = sum(1 for k in Pfns if k not in set(over))
    d = len(common) - a - b - c
    print(f"  ours-hoists-MORE & B-surplus : {a}")
    print(f"  ours-hoists-MORE & no surplus: {b}")
    print(f"  same/less hoist & B-surplus  : {c}")
    print(f"  neither                      : {d}")

    print()
    print("=" * 78)
    print("SHADOW-SUBGROUP ROSTER: our surplus copy lands in the prologue shadow")
    print(f"  {'unit':26} {'fn':30} {'Tnh':>3} {'Onh':>3} {'TB':>3} {'OB':>3} {'Tn':>5} {'On':>5}")
    for k in sorted(Pshadow, key=lambda k: abs(ours[k]['n'] - tgt[k]['n'])):
        print(f"  {k[0][:26]:26} {k[1][:30]:30} {tgt[k]['nh']:>3} {ours[k]['nh']:>3} "
              f"{tgt[k]['B']:>3} {ours[k]['B']:>3} {tgt[k]['n']:>5} {ours[k]['n']:>5}")
        print(f"      T shadow: {tgt[k]['hoisted']}")
        print(f"      O shadow: {ours[k]['hoisted']}")

    print()
    print("=" * 78)
    print("NON-SHADOW SUBGROUP: surplus copy NOT in the shadow")
    ns = [k for k in Pfns if k not in set(Pshadow)]
    print(f"  {'unit':26} {'fn':30} {'Tnh':>3} {'Onh':>3} {'TB':>3} {'OB':>3} {'Tn':>5} {'On':>5}")
    for k in sorted(ns, key=lambda k: abs(ours[k]['n'] - tgt[k]['n'])):
        print(f"  {k[0][:26]:26} {k[1][:30]:30} {tgt[k]['nh']:>3} {ours[k]['nh']:>3} "
              f"{tgt[k]['B']:>3} {ours[k]['B']:>3} {tgt[k]['n']:>5} {ours[k]['n']:>5}")

    # control: our shape-A instances whose function has ours-hoists-MORE
    print()
    print("=" * 78)
    print("CONTROL: functions where ours hoists MORE but no copy surplus")
    for k in sorted([x for x in over if ours[x]["B"] <= tgt[x]["B"]],
                    key=lambda k: ours[k]['n']):
        print(f"  {k[0][:26]:26} {k[1][:30]:30} Tnh={tgt[k]['nh']} Onh={ours[k]['nh']} "
              f"Tn={tgt[k]['n']} On={ours[k]['n']}")
        print(f"      T shadow: {tgt[k]['hoisted']}")
        print(f"      O shadow: {ours[k]['hoisted']}")


main()
