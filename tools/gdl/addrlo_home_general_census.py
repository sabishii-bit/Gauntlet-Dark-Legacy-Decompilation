"""addrlo_home_general_census.py -- census of ADDR16_LO homing via a GENERAL temp.

addr16_census.py censused the home-copy shape only when the intermediate was r0
(`addi r0,rT,SYM@lo ; mr rHOME,r0`).  Its FINDING 6 explicitly reported T=0/O=0
for the five "extra-address-copy" parks, i.e. r0 is NOT their intermediate --
but it never censused the general case, where the intermediate is any volatile
temp and the copy is spelled `addi rHOME,rX,0`.  That gap is this tool.

Shapes, per (function, callee-saved register that receives a file-scope address):

  A  lis rT,SYM@ha ; ... ; addi rHOME,rT,SYM@lo                 direct home
  B  lis rT,SYM@ha ; ... ; addi rX,rT,SYM@lo (rX volatile) ;
                            ... ; addi rHOME,rX,0 | mr rHOME,rX  copy paid

Populations paired per function on the count of B:
  P ours MORE B than target   Q equal nonzero   R both zero   S ours FEWER B

Prints the prologue-shadow association for every B instance on both sides
(the causal question: is the copy forced by the @lo landing before the stmw?)
and a matched-control table for the named lane functions.

Usage: python tools/gdl/addrlo_home_general_census.py  (writes build/LC_home.json)
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
    """Normalise an instruction so target-.s and objdump text compare equal.

    CRITICAL: the target split spells a register copy `addi rD,rS,0x0` (hex)
    while objdump spells it `addi rD,rS,0` (decimal).  A `,0$` matcher sees
    only OUR copies and silently undercounts the target's -- which manufactures
    a one-directional divergence out of nothing.  559 `addi rX,rY,0x0` in
    game/world alone.  Normalise immediates to decimal on both sides.
    """
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
CPY_ADDI = re.compile(r"^addi\s+(r\d+),(r\d+),0$")
CPY_MR = re.compile(r"^mr\s+(r\d+),(r\d+)$")
STWU = re.compile(r"^stwu\s+r1,-")
BR = re.compile(r"^(b|ba|bl|blr|bc|bdnz|bne|beq|bge|blt|bgt|ble|bctr)")
WRITE = re.compile(r"^(?:addi|addis|add|sub[a-z]*|or|ori|and[a-z]*|xor[a-z]*|"
                   r"lwz|lbz|lhz|lha|lwzu|li|lis|mr|mflr|mfspr|rlwinm|rlwimi|"
                   r"srawi|slwi|srwi|neg|extsb|extsh|mulli|mullw|divw|cntlzw|"
                   r"nor|nand|eqv|lwzx|lbzx|lhzx|subf)[a-z.]*\s+(r\d+),")


def analyse(insns):
    stwu_i = None
    for i, (t, _s) in enumerate(insns[:16]):
        if STWU.match(t):
            stwu_i = i
            break
    homes = []
    for i, (t, sym) in enumerate(insns):
        m = ADDI.match(t)
        if not m or sym is None:
            continue
        dst, src = m.group(1), m.group(2)
        shadow = (stwu_i is not None and i < stwu_i)
        if dst in CS:
            homes.append(dict(sym=sym, shape="A", home=dst, via=None, idx=i,
                              gap=0, shadow=shadow))
            continue
        # volatile destination: look forward for a copy into a callee-saved reg
        for j in range(i + 1, min(i + 12, len(insns))):
            tj = insns[j][0]
            m2 = CPY_ADDI.match(tj) or CPY_MR.match(tj)
            if m2 and m2.group(2) == dst and m2.group(1) in CS:
                homes.append(dict(sym=sym, shape="B", home=m2.group(1), via=dst,
                                  idx=i, gap=j - i, shadow=shadow))
                break
            mw = WRITE.match(tj)
            if mw and mw.group(1) == dst:
                break
            if BR.match(tj):
                break
    return dict(homes=homes, n=len(insns), stwu=stwu_i,
                A=sum(1 for h in homes if h["shape"] == "A"),
                B=sum(1 for h in homes if h["shape"] == "B"))


def main():
    tgt, ours = {}, {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = analyse(insns)
    for unit, fn, insns in our_fns():
        ours[(unit, fn)] = analyse(insns)
    common = sorted(k for k in ours if k in tgt)
    print(f"paired functions: {len(common)}")

    tA = sum(tgt[k]["A"] for k in common); tB = sum(tgt[k]["B"] for k in common)
    oA = sum(ours[k]["A"] for k in common); oB = sum(ours[k]["B"] for k in common)
    print()
    print("=" * 78)
    print("ADDR16_LO -> CALLEE-SAVED HOME, general intermediate (r0 or any volatile)")
    print(f"  shape A (direct home)  target {tA:5}   ours {oA:5}")
    print(f"  shape B (copy paid)    target {tB:5}   ours {oB:5}")

    P, Q, R, S = [], [], [], []
    for k in common:
        t, o = tgt[k], ours[k]
        rec = dict(unit=k[0], fn=k[1], tA=t["A"], oA=o["A"], tB=t["B"], oB=o["B"],
                   t_n=t["n"], o_n=o["n"], t_homes=t["homes"], o_homes=o["homes"])
        if o["B"] > t["B"]:
            P.append(rec)
        elif o["B"] < t["B"]:
            S.append(rec)
        elif o["B"] > 0:
            Q.append(rec)
        else:
            R.append(rec)
    print()
    print("PER-FUNCTION POPULATIONS (paired on shape-B count)")
    print(f"  P  ours MORE copies (divergence) : {len(P)}")
    print(f"  Q  equal, nonzero                : {len(Q)}")
    print(f"  R  both zero                     : {len(R)}")
    print(f"  S  ours FEWER copies (inverse)   : {len(S)}")

    def tab(name, pop, lim=300):
        print()
        print(f"--- {name} (n={len(pop)}) ---")
        print(f"  {'unit':26} {'fn':30} {'tA':>3} {'oA':>3} {'tB':>3} {'oB':>3} "
              f"{'Tn':>5} {'On':>5} {'d':>4}")
        for r in sorted(pop, key=lambda r: abs(r['t_n'] - r['o_n']))[:lim]:
            print(f"  {r['unit'][:26]:26} {r['fn'][:30]:30} {r['tA']:>3} {r['oA']:>3} "
                  f"{r['tB']:>3} {r['oB']:>3} {r['t_n']:>5} {r['o_n']:>5} "
                  f"{r['o_n']-r['t_n']:>+4}")
    tab("POPULATION P (sorted by |insn delta|; small delta = clean instance)", P)
    tab("POPULATION S (inverse)", S)

    # shadow association for every B instance
    print()
    print("=" * 78)
    print("SHADOW ASSOCIATION OF SHAPE-B INSTANCES  (is the copy forced by the")
    print("@lo landing before the stmw/frame save?)")
    for side, store in (("TARGET", tgt), ("OURS", ours)):
        c = collections.Counter()
        for k in common:
            for h in store[k]["homes"]:
                if h["shape"] == "B":
                    c[h["shadow"]] += 1
        print(f"  {side:7} shape-B in shadow: {c[True]}   not in shadow: {c[False]}")
    for side, store in (("TARGET", tgt), ("OURS", ours)):
        c = collections.Counter()
        for k in common:
            for h in store[k]["homes"]:
                if h["shape"] == "A":
                    c[h["shadow"]] += 1
        print(f"  {side:7} shape-A in shadow: {c[True]}   not in shadow: {c[False]}")

    LANE = [("game/world/items", "LoadItems"), ("game/world/world", "StartWorldLoad"),
            ("game/ui/select", "init_player_change"), ("game/pb/pb_objregs", "pbDrawVerts"),
            ("game/enemy/critter", "ProcessCritterList")]
    print()
    print("=" * 78)
    print("LANE FUNCTIONS")
    for k in LANE:
        if k not in ours:
            print(f"  {k} : NOT PAIRED")
            continue
        print(f"  {k[0]}::{k[1]}  tA={tgt[k]['A']} oA={ours[k]['A']} "
              f"tB={tgt[k]['B']} oB={ours[k]['B']}  Tn={tgt[k]['n']} On={ours[k]['n']}")
        for side, store in (("T", tgt), ("O", ours)):
            for h in store[k]["homes"]:
                print(f"      {side} {h['shape']} sym={h['sym'][:28]:28} home={h['home']:4} "
                      f"via={str(h['via']):4} idx={h['idx']:4} shadow={h['shadow']}")

    json.dump(dict(P=P, Q=Q, S=S, R_count=len(R),
                   totals=dict(tA=tA, tB=tB, oA=oA, oB=oB)),
              open(os.path.join("build", "LC_home.json"), "w"), indent=1)
    print("\nwrote build/LC_home.json")


main()
