"""add_remat_census.py -- image-wide census of the REMATERIALIZED INDEX BASE
signature (`add rX,rBase,rIdx` repeated before each access of one record).

Founding case: game/sound/sounds_evt::AudioSetupBossStreams, which stood at a
16-instruction deficit for three sessions.  Its `fndiff --ops` line read
`target-only: +18 add` -- the target rematerialises `add rX,rBase,rIdx` before
every access into a record, folding the field offset into the load/store
displacement, while our build let address-PRE synthesise `base + idx*4` once
and park it in a callee-saved GPR for the whole function.  Respelling every
access as an array/member reference into a DECLARED record closed it
(multiset 22 tokens -> 1, fuzzy 94.29% -> 98.43%).
See claim.law.split-bss-symbols-are-one-record-and-array-form-defeats-\
synthesized-index-base.20260831.v1 and its parent
claim.law.cached-base-alias-defeats-rematerialized-index-base.20260831.v1.

This tool answers: how many OTHER functions carry the same fingerprint?

Joins the TARGET split assembly (build/GUNE5D/asm) with OUR built objects
(build/GUNE5D/src) function-by-function -- the same join addr16_census.py and
addrlo_home_general_census.py use -- and reports, per paired function:

    d_add    = target `add` count  -  our `add` count
    d_insn   = target insn count   -  our insn count

The fingerprint is d_add >= MIN_ADD together with d_insn > 0 (we are SHORT by
roughly the adds we are not emitting).  Functions already byte-identical are
excluded.  Ranked by d_add.

Usage:  python tools/gdl/add_remat_census.py    (writes build/AR_paired.json)
"""
import os
import re
import json
import subprocess
import collections

OBJDUMP = os.path.abspath(
    os.path.join("build", "binutils", "powerpc-eabi-objdump.exe"))
ASM = os.path.join("build", "GUNE5D", "asm")
OBJROOT = os.path.join("build", "GUNE5D", "src")

MIN_ADD = 3          # report functions missing at least this many adds

T_INSN = re.compile(
    r"^/\*\s+([0-9A-F]{8})\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}"
    r"\s+\*/\s+(.*?)\s*$")
T_FN = re.compile(r"^\.fn\s+([^,]+),")
T_END = re.compile(r"^\.endfn\s+(\S+)")

O_FN = re.compile(r"^[0-9a-f]{8} <(.+)>:\s*$")
O_INSN = re.compile(
    r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} ){3}[0-9a-f]{2}\s+(.*?)\s*$")


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


def our_fns():
    for root, _d, names in os.walk(OBJROOT):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if not n.endswith(".o"):
                continue
            path = os.path.join(root, n)
            unit = os.path.relpath(path, OBJROOT).replace("\\", "/")[:-2]
            txt = subprocess.run([OBJDUMP, "-d", "-m", "powerpc", path],
                                 capture_output=True, text=True).stdout
            cur, insns = None, []
            for line in txt.splitlines():
                m = O_FN.match(line)
                if m:
                    if cur:
                        yield unit, cur, insns
                    cur, insns = m.group(1), []
                    continue
                m = O_INSN.match(line)
                if m and cur is not None:
                    insns.append(m.group(2))
            if cur:
                yield unit, cur, insns


def opcode(t):
    t = t.strip()
    return t.split()[0] if t else ""


def profile(insns):
    c = collections.Counter(opcode(t) for t in insns)
    return c, len(insns)


def main():
    tgt = {}
    for unit, fn, insns in target_fns():
        tgt[(unit, fn)] = profile(insns)

    rows = []
    paired = 0
    for unit, fn, insns in our_fns():
        key = (unit, fn)
        if key not in tgt:
            continue
        paired += 1
        tc, tn = tgt[key]
        oc, on = profile(insns)
        if tc == oc and tn == on:
            continue                      # opcode-identical, nothing to see
        d_add = tc.get("add", 0) - oc.get("add", 0)
        rows.append(dict(unit=unit, fn=fn, d_add=d_add,
                         d_insn=tn - on, t_add=tc.get("add", 0),
                         o_add=oc.get("add", 0), t_insn=tn, o_insn=on))

    hits = [r for r in rows if r["d_add"] >= MIN_ADD and r["d_insn"] > 0]
    hits.sort(key=lambda r: (-r["d_add"], -r["d_insn"]))

    print(f"paired functions: {paired}   non-identical: {len(rows)}")
    print(f"FINGERPRINT (d_add >= {MIN_ADD} and ours SHORT): {len(hits)}")
    print()
    print(f"  {'d_add':>5} {'d_insn':>6} {'T add':>5} {'O add':>5} "
          f"{'T ins':>5} {'O ins':>5}  function <- unit")
    for r in hits:
        print(f"  {r['d_add']:5d} {r['d_insn']:6d} {r['t_add']:5d} "
              f"{r['o_add']:5d} {r['t_insn']:5d} {r['o_insn']:5d}  "
              f"{r['fn']} <- {r['unit']}")

    # context: the same statistic without the "ours short" requirement
    wide = [r for r in rows if r["d_add"] >= MIN_ADD]
    print(f"\nd_add >= {MIN_ADD} regardless of insn delta: {len(wide)}")
    inverse = [r for r in rows if r["d_add"] <= -MIN_ADD]
    print(f"INVERSE (ours emits >= {MIN_ADD} MORE adds): {len(inverse)}")
    for r in sorted(inverse, key=lambda r: r["d_add"])[:15]:
        print(f"  {r['d_add']:5d} {r['d_insn']:6d}  {r['fn']} <- {r['unit']}")

    out = os.path.join("build", "AR_paired.json")
    json.dump(dict(hits=hits, wide=wide, inverse=inverse, paired=paired),
              open(out, "w"), indent=1)
    print(f"\nwrote {out}")


main()
