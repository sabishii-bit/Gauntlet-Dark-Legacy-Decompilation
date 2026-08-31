"""AD: does the ADDR16_LO home-copy mechanism generalise to ours-only copies
that carry NO relocation?  Census every `mr rCS,r0` / `addi rCS,r0,0` homing on
both sides, classify what DEFINED r0, and diff ours vs target per function."""
import os, re, json, subprocess, collections

OBJDUMP = os.path.abspath(os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")
CS = {f"r{i}" for i in range(14, 32)}

T_INSN = re.compile(r"^/\*\s+[0-9A-F]{8}\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s+\*/\s+(.*?)\s*$")
T_FN = re.compile(r"^\.fn\s+([^,]+),")
T_END = re.compile(r"^\.endfn\s+(\S+)")
O_FN = re.compile(r"^[0-9a-f]{8} <(.+)>:\s*$")
O_INSN = re.compile(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")
O_REL = re.compile(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)")

FOCUS = {"LoadItems", "StartWorldLoad", "init_player_change", "pbDrawVerts",
         "ProcessCritterList", "LoadWorldDone", "LoadWorldData",
         "MapMusicStart", "fn_80088714", "PlayerCollideFloor", "LoadPlyrData"}


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
                    insns.append((m.group(1).replace(", ", ","), None))


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
                    insns[-1] = (insns[-1][0], m.group(1))
                    continue
                m = O_INSN.match(line)
                if m and cur is not None:
                    insns.append((m.group(1).replace(", ", ","), None))
            if cur:
                yield unit, cur, insns


def homings(insns):
    """Find `mr rCS,r0` / `addi rCS,r0,0` and classify the producer of r0."""
    out = []
    texts = [t for t, _r in insns]
    for j, t in enumerate(texts):
        m = re.match(r"(?:mr\s+(r\d+),r0|addi\s+(r\d+),r0,0)$", t)
        if not m:
            continue
        home = m.group(1) or m.group(2)
        if home not in CS:
            continue
        # walk back for the def of r0
        producer, reloc = None, None
        for i in range(j - 1, max(-1, j - 10), -1):
            mm = re.match(r"(\w+\.?)\s+r0\b", texts[i])
            if mm:
                producer = mm.group(1)
                reloc = insns[i][1]
                break
        out.append(dict(idx=j, home=home, producer=producer, reloc=reloc))
    return out


def main():
    tgt, ours = {}, {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = homings(insns)
    for unit, fn, insns in our_fns():
        ours[(unit, fn)] = homings(insns)

    rows = []
    for k in ours:
        if k not in tgt:
            continue
        extra = len(ours[k]) - len(tgt[k])
        if extra > 0:
            rows.append((extra, k, ours[k], tgt[k]))

    print(f"functions where OURS pays MORE r0->callee-saved homings than target: {len(rows)}")
    byproducer = collections.Counter()
    for extra, k, o, t in rows:
        for h in o[len(t):] if len(o) > len(t) else []:
            byproducer[(h["producer"], h["reloc"])] += 1
    print()
    print("producer of r0 for the SURPLUS homings (opcode, relocation):")
    for (prod, rel), n in byproducer.most_common(20):
        print(f"   {str(prod):10} {str(rel):14} x{n}")

    print()
    print("FOCUS functions named by the control-flow lane:")
    print(f"  {'unit':26} {'function':24} {'T':>3} {'O':>3}  our homing producers")
    for extra, k, o, t in sorted(rows, key=lambda r: r[1]):
        if k[1] not in FOCUS:
            continue
        prods = ", ".join(f"{h['producer']}/{h['reloc']}" for h in o)
        print(f"  {k[0][:26]:26} {k[1][:24]:24} {len(t):>3} {len(o):>3}  {prods}")
    print()
    print("  (functions from the FOCUS list with NO surplus r0 homing:)")
    for k in sorted(ours):
        if k[1] in FOCUS and (k in tgt) and len(ours[k]) <= len(tgt[k]):
            print(f"    {k[0]:26} {k[1]:24} T={len(tgt[k])} O={len(ours[k])}")


main()
