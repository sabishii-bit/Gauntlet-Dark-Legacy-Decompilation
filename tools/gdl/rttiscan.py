#!/usr/bin/env python3
"""Image-wide CodeWarrior RTTI / vtable sweep of the retail GameCube DOL.

Recovers real C++ class names, base-class chains, destructor addresses and
vtable addresses by walking the CodeWarrior RTTI chain, per
claim.law.rtti-vtable-chain-recovers-cpp-class-names.20260901.v1:

    vtable  = { RTTI*, 0, dtor, virtual... }      (first fn slot is the dtor)
    RTTI    = { const char* name, base_descriptor* }   (base NULL => root)
    basedsc = { RTTI*, offset, flags }

The layout is NOT trusted until it reproduces a control whose real mangled
name is already known.  Two controls are baked in as a self-test and the
scan REFUSES TO RUN if either fails:

    __vt__Q23std9exception      .data  0x802383A8 -> 'std::exception' (root)
    __vt__Q23std13bad_exception .data  0x80238490 -> 'std::bad_exception'
                                                     base -> 'std::exception'

RTTI/vtable data lives in .data/.sdata ranges that are often NOT part of a
TU's split, so everything is read from the retail DOL, never from a built
object.  Our cflags carry `-RTTI off` while the original was built with RTTI
on; that is harmless precisely because this data is linked from original
bytes.

Usage:
  python tools/gdl/rttiscan.py                # self-test + full class map
  python tools/gdl/rttiscan.py --selftest     # control validation only
  python tools/gdl/rttiscan.py --tables       # + non-RTTI vtable-shaped groups
  python tools/gdl/rttiscan.py --json out.json
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
VERSION = "GUNE5D"
DOL = REPO / "orig" / VERSION / "sys" / "main.dol"
SYMBOLS = REPO / "config" / VERSION / "symbols.txt"
SPLITS = REPO / "config" / VERSION / "splits.txt"

# ---- controls: (vtable VA, expected class name, expected base name or None) --
CONTROLS = [
    (0x802383A8, "std::exception", None),
    (0x80238490, "std::bad_exception", "std::exception"),
]

NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_:<>,&*~ .\[\]]{0,95}$")


class Image:
    """Retail DOL, addressed by virtual address."""

    def __init__(self, path: Path):
        blob = path.read_bytes()
        self.blob = blob
        t_off = struct.unpack(">7I", blob[0x00:0x1C])
        d_off = struct.unpack(">11I", blob[0x1C:0x48])
        t_addr = struct.unpack(">7I", blob[0x48:0x64])
        d_addr = struct.unpack(">11I", blob[0x64:0x90])
        t_size = struct.unpack(">7I", blob[0x90:0xAC])
        d_size = struct.unpack(">11I", blob[0xAC:0xD8])
        self.text = [(a, s, o) for o, a, s in zip(t_off, t_addr, t_size) if s]
        self.data = [(a, s, o) for o, a, s in zip(d_off, d_addr, d_size) if s]
        self.segs = self.text + self.data

    def read(self, va, n):
        for a, s, o in self.segs:
            if a <= va and va + n <= a + s:
                return self.blob[o + (va - a): o + (va - a) + n]
        return None

    def u32(self, va):
        b = self.read(va, 4)
        return struct.unpack(">I", b)[0] if b else None

    def in_text(self, va):
        return va is not None and any(a <= va < a + s for a, s, _ in self.text)

    def cstr(self, va, maxlen=160):
        b = self.read(va, maxlen)
        if not b:
            return None
        z = b.find(b"\0")
        if z < 1:
            return None
        try:
            return b[:z].decode("ascii")
        except UnicodeDecodeError:
            return None


def load_symbols():
    """-> (objects{va:(name,size)}, funcs{va:(name,size)})"""
    objects, funcs = {}, {}
    pat = re.compile(
        r"^(\S+)\s*=\s*\.(\w+):0x([0-9A-Fa-f]+);\s*//\s*type:(\w+)(?:\s+size:0x([0-9A-Fa-f]+))?"
    )
    if not SYMBOLS.exists():
        return objects, funcs
    for line in SYMBOLS.read_text(encoding="utf-8", errors="replace").splitlines():
        m = pat.match(line.strip())
        if not m:
            continue
        name, _sec, va, kind, size = m.groups()
        va = int(va, 16)
        size = int(size, 16) if size else 0
        if kind == "function":
            funcs[va] = (name, size)
        else:
            objects[va] = (name, size)
    return objects, funcs


def load_splits():
    """-> [(tu, section, start, end)] sorted."""
    out = []
    if not SPLITS.exists():
        return out
    tu = None
    for line in SPLITS.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        if not line.startswith((" ", "\t")) and s.endswith(":"):
            tu = s[:-1]
            continue
        m = re.match(r"(\S+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", s)
        if m and tu:
            out.append((tu, m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return out


def tu_of(splits, va):
    for tu, sec, start, end in splits:
        if start <= va < end:
            return tu, sec
    return None, None


class Rtti:
    def __init__(self, img):
        self.img = img
        self._cache = {}

    def valid_name(self, s):
        return bool(s) and bool(NAME_RE.match(s))

    def is_record(self, va, depth=0):
        if va is None or va % 4 or depth > 4:
            return False
        key = (va, depth > 0)
        if key in self._cache:
            return self._cache[key]
        ok = False
        b = self.img.read(va, 8)
        if b:
            name_ptr, base_ptr = struct.unpack(">II", b)
            if self.valid_name(self.img.cstr(name_ptr)):
                if base_ptr == 0:
                    ok = True
                else:
                    br = self.img.u32(base_ptr)
                    ok = br is not None and self.is_record(br, depth + 1)
        self._cache[key] = ok
        return ok

    def info(self, va):
        name_ptr, base_ptr = struct.unpack(">II", self.img.read(va, 8))
        return self.img.cstr(name_ptr), name_ptr, base_ptr

    def bases(self, base_ptr, limit=8):
        """Base-descriptor list: { RTTI*, offset, flags } entries."""
        out = []
        if not base_ptr:
            return out
        for i in range(limit):
            a = base_ptr + 12 * i
            w = self.img.read(a, 12)
            if not w:
                break
            r, off, flags = struct.unpack(">III", w)
            if not r or not self.is_record(r):
                break
            out.append({"desc": a, "rtti": r, "name": self.info(r)[0],
                        "offset": off, "flags": flags})
            nxt = self.img.u32(a + 12)
            if nxt is None or not self.is_record(nxt):
                break
        return out


def selftest(img, rt, verbose=True):
    ok = True
    for vt, want_name, want_base in CONTROLS:
        r = img.u32(vt)
        got = base = None
        if r is not None and rt.is_record(r):
            got = rt.info(r)[0]
            bl = rt.bases(rt.info(r)[2])
            base = bl[0]["name"] if bl else None
        passed = (got == want_name) and (base == want_base)
        ok &= passed
        if verbose:
            print(f"  [{'PASS' if passed else 'FAIL'}] vtable 0x{vt:08X} "
                  f"-> RTTI 0x{(r or 0):08X} -> {got!r} base={base!r} "
                  f"(expect {want_name!r} base={want_base!r})")
    return ok


def scan(img, rt, objects):
    """Find every RTTI reference in data; classify vtable vs base descriptor."""
    refs = defaultdict(list)
    for a, s, o in img.data:
        for va in range(a, a + s - 3, 4):
            w = struct.unpack(">I", img.blob[o + (va - a): o + (va - a) + 4])[0]
            if w and w % 4 == 0 and rt.is_record(w):
                refs[w].append(va)

    vtables, descs = [], []
    for r, locs in refs.items():
        name = rt.info(r)[0]
        for loc in locs:
            w1, w2 = img.u32(loc + 4), img.u32(loc + 8)
            if w1 == 0 and img.in_text(w2):
                # bound by the symbol's declared size when we have one
                sym = objects.get(loc)
                if sym and sym[1] >= 12:
                    nwords = sym[1] // 4
                else:
                    nwords = 2
                    while nwords < 128:
                        w = img.u32(loc + 4 * nwords)
                        if w is None or not (img.in_text(w) or w == 0):
                            break
                        nwords += 1
                slots = [img.u32(loc + 4 * k) for k in range(2, nwords)]
                while slots and slots[-1] == 0:
                    slots.pop()
                vtables.append({
                    "vtable": loc, "symbol": sym[0] if sym else None,
                    "size": sym[1] if sym else nwords * 4,
                    "rtti": r, "name": name, "slots": slots,
                    "dtor": slots[0] if slots else None,
                    "pure_slots": sum(1 for s in slots if s == 0),
                })
            else:
                descs.append({"loc": loc, "rtti": r, "name": name})
    return refs, vtables, descs


def find_tables(img, funcs, objects, min_run=3):
    """Vtable-SHAPED groups independent of RTTI: runs of function STARTS.

    Discriminates vtables from switch jump tables: a vtable slot points at a
    function ENTRY, a jump-table entry points at a label INSIDE a function.
    """
    starts = set(funcs)
    out = []
    for a, s, o in img.data:
        va = a
        while va < a + s - 3:
            w = struct.unpack(">I", img.blob[o + (va - a): o + (va - a) + 4])[0]
            if img.in_text(w) and w in starts:
                run, k = [], va
                while k < a + s - 3:
                    x = struct.unpack(">I", img.blob[o + (k - a): o + (k - a) + 4])[0]
                    if (img.in_text(x) and x in starts) or (x == 0 and run):
                        run.append(x)
                        k += 4
                    else:
                        break
                while run and run[-1] == 0:
                    run.pop()
                if len(run) >= min_run:
                    sym = objects.get(va)
                    out.append({"addr": va, "count": len(run), "slots": run,
                                "symbol": sym[0] if sym else None})
                va = max(k, va + 4)
            else:
                va += 4
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true", help="controls only")
    ap.add_argument("--tables", action="store_true",
                    help="also list non-RTTI vtable-shaped groups")
    ap.add_argument("--json", metavar="PATH", help="write the map as JSON")
    args = ap.parse_args()

    if not DOL.exists():
        print(f"rttiscan: missing {DOL}", file=sys.stderr)
        return 2

    img = Image(DOL)
    rt = Rtti(img)
    objects, funcs = load_symbols()
    splits = load_splits()

    print("=== CONTROL VALIDATION (layout is not trusted until these pass) ===")
    if not selftest(img, rt):
        print("rttiscan: CONTROL FAILED - refusing to report unknown records",
              file=sys.stderr)
        return 1
    print("  controls OK\n")
    if args.selftest:
        return 0

    refs, vtables, descs = scan(img, rt, objects)
    vtables.sort(key=lambda v: v["vtable"])
    descs.sort(key=lambda d: d["loc"])

    byname = defaultdict(list)
    for r in refs:
        byname[rt.info(r)[0]].append(r)

    print(f"=== RTTI RECORDS: {len(refs)} records, {len(byname)} distinct class names ===")
    for n in sorted(byname):
        rs = sorted(byname[n])
        print(f"  {n!r:24} records={', '.join('0x%08X' % x for x in rs)}"
              + ("   [duplicate records: base-descriptor copy]" if len(rs) > 1 else ""))

    print(f"\n=== VTABLES: {len(vtables)} ===")
    for v in vtables:
        tu, sec = tu_of(splits, v["dtor"]) if v["dtor"] else (None, None)
        bl = rt.bases(rt.info(v["rtti"])[2])
        base = bl[0]["name"] if bl else None
        print(f"  0x{v['vtable']:08X} {v['symbol'] or '(unnamed)':16} "
              f"class={v['name']!r}"
              + (f" : {base!r}" if base else "  (root)"))
        print(f"      size=0x{v['size']:X} rtti=0x{v['rtti']:08X} "
              f"virtuals={len(v['slots'])} pure={v['pure_slots']}")
        if v["dtor"]:
            fn = funcs.get(v["dtor"])
            print(f"      dtor = 0x{v['dtor']:08X} "
                  f"{('= ' + fn[0]) if fn else '(no symbol)'}   TU={tu}")
        for i, s in enumerate(v["slots"][1:], start=1):
            if s:
                fn = funcs.get(s)
                t2, _ = tu_of(splits, s)
                print(f"      slot{i} = 0x{s:08X} "
                      f"{('= ' + fn[0]) if fn else '(no symbol)'}"
                      + (f"   TU={t2}" if t2 != tu else ""))
            else:
                print(f"      slot{i} = pure virtual (NULL)")

    print(f"\n=== BASE DESCRIPTORS: {len(descs)} ===")
    for d in descs:
        sym = objects.get(d["loc"])
        off = img.u32(d["loc"] + 4)
        flg = img.u32(d["loc"] + 8)
        print(f"  0x{d['loc']:08X} {(sym[0] if sym else '(unnamed)'):16} "
              f"-> {d['name']!r} offset={off} flags={flg}")

    # TU inventory
    print("\n=== TU INVENTORY (C++ evidence per TU) ===")
    tu_hits = defaultdict(lambda: {"classes": set(), "dtors": set(), "vtables": set()})
    for v in vtables:
        for s in [v["dtor"]] + v["slots"]:
            if not s:
                continue
            tu, _ = tu_of(splits, s)
            if tu:
                tu_hits[tu]["classes"].add(v["name"])
                tu_hits[tu]["vtables"].add(v["vtable"])
        if v["dtor"]:
            tu, _ = tu_of(splits, v["dtor"])
            if tu:
                tu_hits[tu]["dtors"].add(v["dtor"])
    for tu in sorted(tu_hits):
        h = tu_hits[tu]
        ext = Path(tu).suffix
        flag = "" if ext == ".cpp" else "   <== built as .c, CONVERSION CANDIDATE"
        print(f"  {tu}{flag}")
        print(f"      classes={sorted(h['classes'])} "
              f"vtables={['0x%08X' % x for x in sorted(h['vtables'])]}")

    if args.tables:
        tables = find_tables(img, funcs, objects)
        # suppress runs nested inside a vtable we already reported (a run that
        # starts at the dtor slot re-detects the tail of a known vtable)
        spans = [(v["vtable"], v["vtable"] + v["size"]) for v in vtables]
        extra = [t for t in tables
                 if not any(lo <= t["addr"] < hi for lo, hi in spans)]
        print(f"\n=== VTABLE-SHAPED GROUPS WITHOUT RTTI: {len(extra)} ===")
        print("    (runs of >=3 function ENTRY pointers; switch jump tables are")
        print("     excluded by construction - their targets are labels INSIDE a")
        print("     function, not entry points)")
        for t in extra:
            tus = {tu_of(splits, s)[0] for s in t["slots"] if s}
            print(f"  0x{t['addr']:08X} {(t['symbol'] or '(unnamed)'):16} "
                  f"n={t['count']} TUs={sorted(x for x in tus if x)}")

    if args.json:
        out = {
            "controls_passed": True,
            "classes": {
                n: {
                    "rtti_records": sorted(byname[n]),
                    "vtables": [v["vtable"] for v in vtables if v["name"] == n],
                    "dtors": [v["dtor"] for v in vtables if v["name"] == n],
                }
                for n in sorted(byname)
            },
            "vtables": vtables,
            "base_descriptors": descs,
        }
        Path(args.json).write_text(json.dumps(out, indent=2), encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
