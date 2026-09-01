#!/usr/bin/env python3
"""Pre-link data-section byte comparator: our compiled object vs the DOL.

For each data-class section (.rodata .data .sdata .sdata2) claimed by a unit
in splits.txt, compare our object's emitted bytes against the original DOL
bytes at the claimed addresses. Words that carry a relocation in our object
are skipped (their final value exists only after the link) but their count
and placement are reported, which still exposes jumptable ORDER mismatches.

This catches what fndiff structurally normalizes away:
  - wrong constant VALUES (mathfunc's 1e-5 vs 1e-14 epsilon, atan tables)
  - wrong pool/table EMISSION ORDER (sincos duplicate 0.5f pairs)
  - initializer typos that only sha1 would catch after a full link

It also runs an ADVISORY dead-strip screen (see --deadstrip below).

Usage (from repo root):
  python tools/gdl/datadiff.py game/mathfunc
  python tools/gdl/datadiff.py MSL/sincos MSL/trigf_data
  python tools/gdl/datadiff.py --matching        # all Matching units
  python tools/gdl/datadiff.py --deadstrip game/ui/message   # screen only
  python tools/gdl/datadiff.py --no-deadstrip game/mathfunc  # byte diff only

Exit 1 if any byte mismatch found. Dead-strip warnings NEVER set exit 1:
they are advisory, because a zero-reference static can be legitimate
(claimed data a later flip will wire up, or a symbol another instrument
already accounts for).

DEAD-STRIP SCREEN (claim.law.base-cast-reconstruction-deadstrips-sibling-statics)
--------------------------------------------------------------------------------
mwld dead-strips local data that nothing relocates against. A one-symbol
base-cast reconstruction -- several `static` arrays tiling one target blob,
all reached through ONE of the symbols by casting -- therefore compiles,
links, and passes every OBJECT-level instrument (fndiff, claimcheck, and
datadiff's own byte comparison) while its sibling statics silently vanish
from the linked image, taking any string literal only they referenced with
them. That signature cost a full DOL-forensics session on game/ui/message.c.

This screen flags the exact precondition: a LOCAL (static) data symbol
defined in this object that NO relocation in the same object points at.
Reference resolution is by ADDRESS, not by name, so a static reached
through a section alias plus addend (`...rodata.0+0x28`) or through a
neighbouring symbol's addend counts as referenced -- name-only matching
false-positives on exactly the string literals this law cares about.
"""

import re
import struct
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OBJDUMP = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
SPLITS = REPO / "config" / VERSION / "splits.txt"
DOL = REPO / "orig" / VERSION / "sys" / "main.dol"

DATA_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2")


def dol_read(va, size):
    data = DOL.read_bytes()
    text_off = struct.unpack(">7I", data[0x00:0x1C])
    data_off = struct.unpack(">11I", data[0x1C:0x48])
    text_addr = struct.unpack(">7I", data[0x48:0x64])
    data_addr = struct.unpack(">11I", data[0x64:0x90])
    text_size = struct.unpack(">7I", data[0x90:0xAC])
    data_size = struct.unpack(">11I", data[0xAC:0xD8])
    for off, addr, sz in list(zip(text_off, text_addr, text_size)) + \
                         list(zip(data_off, data_addr, data_size)):
        if addr and addr <= va and va + size <= addr + sz:
            fo = off + (va - addr)
            return data[fo:fo + size]
    return None


def parse_splits():
    units = {}
    cur = None
    for line in SPLITS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(\S.+):$", line)
        if m:
            cur = m.group(1)
            units[cur] = {}
            continue
        m = re.match(r"^\t(\S+)\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", line)
        if m and cur:
            units[cur][m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return units


def obj_sections(obj):
    """name -> bytes, from objdump -s."""
    out = subprocess.run([str(OBJDUMP), "-s", str(obj)], capture_output=True,
                         text=True).stdout
    secs = {}
    name = None
    for line in out.splitlines():
        m = re.match(r"^Contents of section (\S+):", line)
        if m:
            name = m.group(1)
            secs[name] = bytearray()
            continue
        if name:
            m = re.match(r"^ ([0-9a-f]+) ((?:[0-9a-f ]{8,9}){1,4})", line)
            if m:
                hexpart = m.group(2).replace(" ", "")
                secs[name] += bytes.fromhex(hexpart)
    return secs


def obj_relocs(obj):
    """section -> set of relocated word offsets."""
    out = subprocess.run([str(OBJDUMP), "-r", str(obj)], capture_output=True,
                         text=True).stdout
    relocs = {}
    sec = None
    for line in out.splitlines():
        m = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if m:
            sec = m.group(1)
            relocs[sec] = set()
            continue
        m = re.match(r"^([0-9a-f]+)\s+R_PPC_", line)
        if m and sec:
            relocs[sec].add(int(m.group(1), 16) & ~3)
    return relocs


# Sections whose local symbols mwld can dead-strip. BSS-class included:
# an unreferenced static array there vanishes just as silently.
STRIPPABLE_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2",
                       ".bss", ".sbss", ".sbss2")

# objdump -t: "00000000 l     O .data\t00000040 lbl_80124E58"
SYM_RE = re.compile(
    r"^([0-9a-f]{8})\s(.{7})\s+(\S+)\s+([0-9a-f]{8})\s+(\S.*?)\s*$")

# objdump -r: "000001de R_PPC_ADDR16_HA   lbl_80124E58[+0x00000004]"
REL_RE = re.compile(
    r"^[0-9a-f]+\s+R_PPC_\S+\s+(\S+?)(?:\+0x([0-9a-f]+))?\s*$")


def obj_symbols(obj):
    """Parse objdump -t into symbol dicts.

    flags column is 7 chars: [lg] [w] [C] [W] [Ii] [dD] [FfO]
      flags[0] == 'l'  -> local (a C `static`, our dead-strip candidate)
      'O' in flags     -> an object (data), as opposed to a function/section
      'd' in flags     -> a section symbol
    """
    out = subprocess.run([str(OBJDUMP), "-t", str(obj)],
                         capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        m = SYM_RE.match(line.replace("\t", " "))
        if not m:
            continue
        value, flags, sec, size, name = m.groups()
        syms.append({
            "value": int(value, 16),
            "size": int(size, 16),
            "section": sec,
            "name": name,
            "local": flags[0] == "l",
            "object": "O" in flags,
            "section_sym": "d" in flags,
        })
    return syms


def obj_reloc_refs(obj):
    """Every relocation target in the object as (symbol_name, addend)."""
    out = subprocess.run([str(OBJDUMP), "-r", str(obj)],
                         capture_output=True, text=True).stdout
    refs = []
    for line in out.splitlines():
        m = REL_RE.match(line)
        if m:
            refs.append((m.group(1), int(m.group(2), 16) if m.group(2) else 0))
    return refs


def deadstrip_check(unit, obj, quiet_ok=True):
    """Advisory: local data symbols with NO incoming relocation.

    Returns the list of unreferenced symbol dicts (never affects exit code).
    """
    syms = obj_symbols(obj)
    refs = obj_reloc_refs(obj)

    by_name = {}
    for s in syms:
        by_name.setdefault(s["name"], s)

    # A "section anchor" is a section symbol (.data) or a zero-size local
    # alias for one (...data.0). SOUNDNESS LIMIT: when a relocation names an
    # anchor, the displacement that selects which datum it reaches lives in
    # the ADDR16_HA/LO INSTRUCTION IMMEDIATES, not in the relocation addend,
    # so nothing in the relocation table says which symbols of that section
    # are live. Any symbol-level liveness claim there would be a guess, so we
    # declare the whole section opaque and report nothing in it. That costs
    # false negatives (a TU addressing its data off one section base is not
    # screenable) but keeps every warning we DO emit sound -- the right
    # trade for an advisory that must not cry wolf.
    def is_anchor(sym):
        return sym["section_sym"] or (
            sym["local"] and not sym["object"] and sym["size"] == 0)

    opaque = set()
    hit_addrs = {}          # section -> set of referenced addresses
    named = set()
    for name, addend in refs:
        named.add(name)
        base = by_name.get(name)
        if base is None:
            continue
        if is_anchor(base):
            opaque.add(base["section"])
            continue
        hit_addrs.setdefault(base["section"], set()).add(base["value"] + addend)

    unreferenced = []
    for s in syms:
        if not (s["local"] and s["object"]):
            continue
        if s["section"] not in STRIPPABLE_SECTIONS or s["size"] == 0:
            continue
        if s["section"] in opaque:
            continue
        if s["name"] in named:
            continue
        lo, hi = s["value"], s["value"] + s["size"]
        if any(lo <= a < hi for a in hit_addrs.get(s["section"], ())):
            continue
        unreferenced.append(s)

    if unreferenced:
        print(f"[{unit}] DEAD-STRIP WARNING: "
              f"{len(unreferenced)} local data symbol(s) with no incoming "
              f"relocation in this object -- mwld will strip them from the "
              f"linked image (advisory, not a failure):")
        for s in unreferenced:
            print(f"[{unit}]   {s['section']}+0x{s['value']:X} "
                  f"size 0x{s['size']:X}  {s['name']}")
        print(f"[{unit}]   see claim.law.base-cast-reconstruction-"
              f"deadstrips-sibling-statics: if these tile one target blob "
              f"reached through a sibling symbol, merge them into ONE "
              f"aggregate so a single live reference keeps the whole blob.")
    elif not quiet_ok:
        print(f"[{unit}] dead-strip screen: OK, every local data symbol "
              f"is relocated against")
    return unreferenced


def check_unit(unit, claims, run_deadstrip=True):
    obj = REPO / "build" / VERSION / "src" / f"{unit.rsplit('.', 1)[0]}.o"
    if not obj.exists():
        print(f"[{unit}] SKIP: object not built ({obj})")
        return 0
    secs = obj_sections(obj)
    relocs = obj_relocs(obj)
    if run_deadstrip:
        deadstrip_check(unit, obj)
    bad = 0
    for sec in DATA_SECTIONS:
        ours = secs.get(sec)
        claim = claims.get(sec)
        if ours is None and claim is None:
            continue
        if ours is None:
            print(f"[{unit}] {sec}: claimed 0x{claim[1]-claim[0]:X} but object emits nothing")
            continue
        if claim is None:
            print(f"[{unit}] {sec}: object emits 0x{len(ours):X} but nothing claimed")
            bad += 1
            continue
        lo, hi = claim
        orig = dol_read(lo, hi - lo)
        if orig is None:
            print(f"[{unit}] {sec}: claim 0x{lo:08X} not inside DOL")
            bad += 1
            continue
        if len(ours) > hi - lo:
            print(f"[{unit}] {sec}: object 0x{len(ours):X} bytes > claim 0x{hi-lo:X}")
            bad += 1
        n = min(len(ours), len(orig))
        rel = relocs.get(sec, set())
        skipped = 0
        shown = 0
        secbad = 0
        for i in range(0, n, 4):
            if i in rel:
                skipped += 1
                continue
            a = bytes(ours[i:i + 4])
            b = orig[i:i + len(a)]
            if a != b:
                secbad += 1
                if shown < 8:
                    va = lo + i
                    fa = struct.unpack(">f", a.ljust(4, b"\0"))[0]
                    fb = struct.unpack(">f", b.ljust(4, b"\0"))[0]
                    print(f"[{unit}] {sec}+0x{i:X} (VA 0x{va:08X}): "
                          f"ours {a.hex()} ({fa!r}) != dol {b.hex()} ({fb!r})")
                    shown += 1
        if secbad and shown >= 8:
            print(f"[{unit}] {sec}: ... more mismatches suppressed")
        tail = orig[n:]
        tailpad = all(c == 0 for c in tail)
        if not tailpad:
            secbad += 1
        elif tail:
            # Zero claim slack is a FAILURE, not an advisory: objdiff credits
            # matched_data only for sections at 100%, so a claim that swallows
            # even 4 bytes of link alignment padding zeroes the whole
            # section's credit while every byte "compares OK" (two TUs sat
            # flip-blocked on exactly this — claim.law.splits-claim-can-
            # swallow-link-alignment-padding). Shrink the claim to the
            # object's true extent.
            secbad += 1
        status = "OK" if not secbad else "MISMATCH"
        extra = f", {skipped} reloc words skipped" if skipped else ""
        slack = (f", 0x{len(orig)-n:X} claim slack"
                 f"{'(zero — SHRINK THE CLAIM, this zeroes matched_data)' if tailpad else '(NONZERO!)'}"
                 if len(orig) > n else "")
        print(f"[{unit}] {sec}: {status} 0x{n:X} bytes compared{extra}{slack}")
        bad += secbad
    return bad


def section_table(unit_key):
    """--sections: ours-vs-target per-section size + match table.

    Compares our built object's data-class sections directly against the
    TARGET split object — the comparison the flip gate actually runs.
    Catches what the DOL-range byte check is structurally blind to: our
    object emitting MORE bytes than the target object (the DOL range
    compares fine; the objects differ), and surfaces the per-section
    match state that objdiff's all-or-nothing matched_data hides.
    """
    base = unit_key.rsplit(".", 1)[0]
    ours_o = REPO / "build" / VERSION / "src" / f"{base}.o"
    tgt_o = REPO / "build" / VERSION / "obj" / f"{base}.o"
    missing = [str(p) for p in (ours_o, tgt_o) if not p.exists()]
    if missing:
        print(f"[{unit_key}] SKIP --sections: missing {missing}")
        return 1

    def sections(obj):
        out = subprocess.run([str(OBJDUMP), "-h", str(obj)],
                             capture_output=True, text=True).stdout
        table = {}
        for m in re.finditer(
                r"^\s*\d+\s+(\.\w[\w.]*)\s+([0-9a-f]{8})", out, re.M):
            table[m.group(1)] = int(m.group(2), 16)
        return table

    def content(obj, sec):
        out = subprocess.run([str(OBJDUMP), "-s", "-j", sec, str(obj)],
                             capture_output=True, text=True).stdout
        data = bytearray()
        for line in out.splitlines():
            m = re.match(r"^ [0-9a-f]+ ((?:[0-9a-f]{2,8} ?){1,4}) ", line)
            if m:
                data += bytes.fromhex(m.group(1).replace(" ", ""))
        return bytes(data)

    ts, os_ = sections(tgt_o), sections(ours_o)
    bad = 0
    for sec in DATA_SECTIONS + ("extab", "extabindex",
                                ".bss", ".sbss", ".sbss2"):
        tlen, olen = ts.get(sec), os_.get(sec)
        if tlen is None and olen is None:
            continue
        if tlen != olen:
            print(f"[{unit_key}] {sec}: SIZE target 0x{tlen or 0:X} vs"
                  f" ours 0x{olen or 0:X}  <- FLIP BLOCKER")
            bad += 1
            continue
        if sec.startswith((".bss", ".sbss")):
            print(f"[{unit_key}] {sec}: size 0x{tlen:X} equal (bss)")
            continue
        tb, ob = content(tgt_o, sec), content(ours_o, sec)
        same = sum(1 for a, b in zip(tb, ob) if a == b)
        pct = 100.0 * same / len(tb) if tb else 100.0
        mark = "" if pct == 100.0 else "  <- FLIP BLOCKER (reloc words may"\
            " account for some — cross-check the byte mode)"
        print(f"[{unit_key}] {sec}: size 0x{tlen:X}, {pct:.1f}% bytes"
              f" equal{mark}")
        if pct != 100.0:
            bad += 1
            # A percentage hides a finish-line residual: "98.2% equal"
            # was actually a fully-characterized 2-byte transposition a
            # worker had to hand-derive. Name the differing bytes when
            # they are few; summarize when they are not.
            diffs = [i for i, (a, b) in enumerate(zip(tb, ob)) if a != b]
            head = ", ".join(
                f"+0x{i:X} (T {tb[i]:02x} vs O {ob[i]:02x})"
                for i in diffs[:16])
            tail = f" … and {len(diffs) - 16} more" if len(diffs) > 16 else ""
            print(f"[{unit_key}] {sec}: {len(diffs)} byte(s) differ:"
                  f" {head}{tail}")
    return bad


def main():
    args = sys.argv[1:]
    only_deadstrip = "--deadstrip" in args
    run_deadstrip = "--no-deadstrip" not in args
    only_sections = "--sections" in args
    args = [a for a in args
            if a not in ("--deadstrip", "--no-deadstrip", "--sections")]
    if not args:
        print(__doc__)
        return 1
    # --deadstrip also accepts a direct path to any .o (synthetic tests,
    # objects outside the splits map).
    if only_deadstrip:
        direct = [a for a in args if a.endswith(".o")]
        for p in direct:
            obj = Path(p)
            if not obj.exists():
                print(f"[{p}] SKIP: no such object")
                continue
            deadstrip_check(obj.name, obj, quiet_ok=False)
        args = [a for a in args if not a.endswith(".o")]
        if not args:
            return 0

    units = parse_splits()
    targets = []
    if args[0] == "--matching":
        cfg = (REPO / "configure.py").read_text(encoding="utf-8")
        targets = [m.group(1).rsplit(".", 1)[0] for m in
                   re.finditer(r'Object\(Matching, "([^"]+)"', cfg)]
    else:
        targets = [a.replace("\\", "/") for a in args]
    bad = 0
    for t in targets:
        key = next((k for k in units if k.rsplit(".", 1)[0] == t or k == t), None)
        if key is None:
            print(f"[{t}] no splits entry")
            continue
        if only_deadstrip:
            obj = REPO / "build" / VERSION / "src" / f"{key.rsplit('.', 1)[0]}.o"
            if not obj.exists():
                print(f"[{key}] SKIP: object not built ({obj})")
                continue
            deadstrip_check(key, obj, quiet_ok=False)
            continue
        if only_sections:
            bad += section_table(key)
            continue
        bad += check_unit(key, units[key], run_deadstrip=run_deadstrip)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
