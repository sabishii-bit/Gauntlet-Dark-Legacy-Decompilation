#!/usr/bin/env python3
"""CR lane: decide a flagged reloc row by the DATA BEHIND both symbols.

`cr_reloc_setdelta.py` compares symbol NAMES restricted to the census rows.
That is enough to expose most order artifacts, but it has one measured blind
spot: when retail's compensating half is a dtk `lbl_ADDR` and ours is an
ANONYMOUS `@N` pool entry, the name-multiset shows the target reading a datum
"one more time" while our object holds exactly the same bytes under a
different name.  game/shop/shop::show_gold is the worked case -- retail loads
`lbl_80348370` (0x4330000080000000, the s32->f64 bias) into f30 and
`lbl_803483B0` (0.00390625) into f31; we load them into the opposite
registers and spell the bias `@193`, whose .sdata2 bytes are
`43300000 80000000`.  Same data, swapped registers, zero defects.

So compare BYTES, not names, exactly as
claim.law.T11_a-pool-rows-verdict-is-the-value-behind-the-two-symbols-not-the-
kind-of-their-names requires, and honour its granularity trap by comparing on
the PREFIX of the shorter entry (dtk names a whole contiguous .rodata run with
one symbol while MWCC emits one @N per literal).

  VALUE-EQUAL   every datum retail reads, we read -- the row is an emission
                order / register-assignment artifact.  No correctness content.
  VALUE-DELTA   a datum appears on one side only.  This is the row worth a
                source edit, and the delta names it.

Run from the repository root:
    python tools/gdl/composed_census/cr_datum_screen.py <unit> <fn>
    python tools/gdl/composed_census/cr_datum_screen.py --image \
        --out build/GUNE5D/cr_image_datum.txt

Calibrated 2026-09-03 against four live controls before shipping:
  player::PlayerRestoreState  VALUE-DELTA (100.0/500.0 vs 0.5/30.0)  [true +]
  player::start_magic         VALUE-DELTA (1.5 vs -1.5)              [true +]
  atree::AtreeNodeInit        VALUE-DELTA (the two error strings)    [true +]
  enemy::init_enemy           VALUE-EQUAL (byte-exact, kind-differing
                              pool row)                              [true -]
  shop::show_gold             VALUE-EQUAL (bias/1-256 swapped between
                              f30 and f31, ours anonymous)           [true -]
KNOWN LIMIT, by construction: a TRANSPOSITION preserves the multiset, so this
screen cannot see one (enemy::move_logic00's swapped pi/2pi reads VALUE-EQUAL
here).  It is the complement of es_named_reloc_census.py, not a replacement:
that tool finds wrong-OPERAND rows, this one finds wrong-DATUM rows.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
from fndiff import parse  # noqa: E402

VERSION = "GUNE5D"
LBL_RE = re.compile(r"^lbl_[0-9A-Fa-f]{6,8}$")
OBJDUMP = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
PREFIX = 256         # full-entry compare; poolval's 16-byte preview missed a
                     # string that first differs at +0x21 (AtreeNodeInit)
INITIALIZED = {".data", ".sdata", ".sdata2", ".rodata", ".text", ".init"}
_obj_cache = {}
_dol = None
_syms = None


def dol_image():
    """address -> bytes reader over the retail DOL (exact, no 16-byte cap)."""
    global _dol
    if _dol is None:
        raw = (REPO / "orig" / VERSION / "sys" / "main.dol").read_bytes()
        import struct
        off = struct.unpack(">18I", raw[0x00:0x48])
        adr = struct.unpack(">18I", raw[0x48:0x90])
        siz = struct.unpack(">18I", raw[0x90:0xD8])
        _dol = [(a, s, o) for o, a, s in zip(off, adr, siz) if s]
    return _dol


def dol_read(addr, size):
    for a, s, o in dol_image():
        if a <= addr < a + s:
            n = min(size, a + s - addr)
            raw = (REPO / "orig" / VERSION / "sys" / "main.dol").read_bytes()
            return raw[o + (addr - a): o + (addr - a) + n]
    return None


def symtable():
    """symbols.txt: name -> (section, address, size). BSS/SBSS carry no bytes."""
    global _syms
    if _syms is None:
        _syms = {}
        pat = re.compile(r"^(\S+)\s*=\s*(\.\w+):0x([0-9A-Fa-f]+);.*?size:0x([0-9A-Fa-f]+)")
        for line in (REPO / "config" / VERSION / "symbols.txt").read_text().splitlines():
            m = pat.match(line.strip())
            if m:
                _syms[m.group(1)] = (m.group(2), int(m.group(3), 16), int(m.group(4), 16))
    return _syms


_pool_cache = {}


def poolbytes(labels):
    """lbl_ADDR -> {'section','size','bytes'} read from the RETAIL DOL."""
    st = symtable()
    for x in labels:
        if x in _pool_cache or x not in st:
            continue
        sec, addr, size = st[x]
        blob = dol_read(addr, min(size, PREFIX)) if sec in INITIALIZED else None
        _pool_cache[x] = {"section": sec, "size": size, "bytes": blob}
    return _pool_cache


def objdata(objpath):
    """local symbol -> (section, first-16-bytes) for one ELF object."""
    key = str(objpath)
    if key in _obj_cache:
        return _obj_cache[key]
    syms = {}
    r = subprocess.run([str(OBJDUMP), "-t", str(objpath)], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        m = re.match(r"^([0-9A-Fa-f]{8})\s+\S+\s+O\s+(\S+)\s+([0-9A-Fa-f]{8})\s+(?:\.hidden\s+)?(\S+)$", line)
        if m:
            syms[m.group(4)] = (m.group(2), int(m.group(1), 16), int(m.group(3), 16))
    sections = {}
    for sec in {s[0] for s in syms.values()}:
        rr = subprocess.run([str(OBJDUMP), "-s", "-j", sec, str(objpath)],
                            capture_output=True, text=True)
        buf = bytearray()
        base = None
        for line in rr.stdout.splitlines():
            m = re.match(r"^\s*([0-9A-Fa-f]+)\s((?:[0-9A-Fa-f]{2,8}\s){1,4})\s", line)
            if not m:
                continue
            off = int(m.group(1), 16)
            if base is None:
                base = off
            chunk = bytes.fromhex(m.group(2).replace(" ", ""))
            while len(buf) < off - base:
                buf.append(0)
            buf[off - base:off - base + len(chunk)] = chunk
        sections[sec] = bytes(buf)
    out = {}
    for name, (sec, off, size) in syms.items():
        blob = (sections.get(sec, b"")[off:off + min(size, PREFIX)]
                if sec in INITIALIZED else None)
        out[name] = (sec, size, blob)
    _obj_cache[key] = out
    return out


def relocs(lines):
    """symbol (with its addend) -> count, over the whole function."""
    out = Counter()
    for line in lines:
        if line.startswith("    "):
            parts = line.strip().split(maxsplit=1)
            if len(parts) > 1:
                out[parts[1].strip()] += 1
    return out


def is_pointer_table(blob, name=""):
    """A relocation-filled table: resolved addresses in the retail image,
    zeros in our object (our entries live in the relocation table)."""
    if name.startswith("jumptable_"):
        return True
    if not blob or len(blob) < 8 or len(blob) % 4:
        return False
    if not any(blob):
        return True
    words = [int.from_bytes(blob[i:i + 4], "big") for i in range(0, len(blob), 4)]
    return all(0x80000000 <= w < 0x80400000 or w == 0 for w in words)


def split_addend(sym):
    if "+" in sym:
        base, off = sym.split("+", 1)
        try:
            return base, int(off, 0)
        except ValueError:
            return base, 0
    return sym, 0


def datum_key(sym, local):
    """Bytes when we can read them, else the ADDRESS, else the symbol name.

    Three refinements, each forced by a measured false positive:
      * resolve through symbols.txt for ANY name, not just `lbl_*` -- our
        source spells a literal MWCC pools as `@N` where retail's splitter
        named the same bytes `sPi`/`pmissile_sfxidx`;
      * key by ADDRESS when there are no bytes, so our `gControllerButtons+0x4`
        and retail's `sFlags` (0x803445CC either way) are one datum;
      * a relocation-filled table (`jumptable_*`, an address array) reads as
        resolved addresses in the retail image and as ZEROS in our object,
        because our entries live in the relocation table -- that is a
        representation difference, never a datum difference.
    """
    base, addend = split_addend(sym)
    if base in local and local[base][2] is not None:
        if is_pointer_table(local[base][2], base):
            return "P:%d" % local[base][1], local[base][1]
        blob = local[base][2][addend:] if addend else local[base][2]
        if blob:
            return "B:" + blob.hex(), local[base][1]
    p = poolbytes([base]).get(base)
    if p:
        if p["bytes"] and is_pointer_table(p["bytes"], base):
            return "P:%d" % p["size"], p["size"]
        if p["bytes"] and addend < len(p["bytes"]):
            return "B:" + p["bytes"][addend:].hex(), p["size"]
        st = symtable().get(base)
        if st:
            return "A:0x%08X" % (st[1] + addend), p["size"]
    st = symtable().get(base)
    if st:
        return "A:0x%08X" % (st[1] + addend), st[2]
    return "N:" + sym, None


def prefix_merge(keys):
    """T11 granularity law: a shorter entry that is a PREFIX of a longer one
    is the same datum seen at a different splitting granularity."""
    return keys


def screen(unit, fn):
    tobj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
    bobj = REPO / "build" / VERSION / "src" / f"{unit}.o"
    tfns, bfns = parse(tobj), parse(bobj)
    if fn not in tfns or fn not in bfns:
        return None
    tl, bl = objdata(tobj), objdata(bobj)
    tc, bc = relocs(tfns[fn]), relocs(bfns[fn])
    poolbytes(list(tc) + list(bc))
    tk, bk = Counter(), Counter()
    label = {}
    for s, n in tc.items():
        k, sz = datum_key(s, tl)
        tk[k] += n
        label.setdefault(k, []).append(f"T:{s}({sz})")
    for s, n in bc.items():
        k, sz = datum_key(s, bl)
        bk[k] += n
        label.setdefault(k, []).append(f"O:{s}({sz})")
    only_t, only_b = tk - bk, bk - tk
    # prefix reconciliation: a target run that CONTAINS our shorter entry
    for k in list(only_b):
        if not k.startswith("B:"):
            continue
        mine = bytes.fromhex(k[2:])
        for tkey in list(only_t):
            if tkey.startswith("B:") and (bytes.fromhex(tkey[2:]).startswith(mine)
                                          or mine.startswith(bytes.fromhex(tkey[2:]))):
                n = min(only_t[tkey], only_b[k])
                only_t[tkey] -= n
                only_b[k] -= n
                if only_t[tkey] <= 0:
                    del only_t[tkey]
                if only_b[k] <= 0:
                    del only_b[k]
                break
    return only_t, only_b, label


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("unit", nargs="?")
    ap.add_argument("fn", nargs="?")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--image", action="store_true",
                    help="every function of every NonMatching unit pair")
    ap.add_argument("--census", default=str(REPO / "build" / VERSION / "cr_es_census.json"))
    ap.add_argument("--out", default=None,
                    help="write the report here (default: stdout; generated "
                         "artifacts belong under build/, never beside the script)")
    args = ap.parse_args()

    out = open(args.out, "w", encoding="utf-8") if args.out else sys.stdout

    def emit(*a):
        print(*a, file=out)

    if args.image:
        todo = []
        report = json.loads((REPO / "build" / VERSION / "report.json").read_text())
        for u in report.get("units", []):
            unit = u.get("name", "").removeprefix("main/")
            if u.get("metadata", {}).get("complete"):
                continue
            tobj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
            bobj = REPO / "build" / VERSION / "src" / f"{unit}.o"
            if not (tobj.exists() and bobj.exists()):
                continue
            for fn in parse(tobj):
                todo.append((unit, fn))
    elif args.unit and args.fn:
        todo = [(args.unit.removeprefix("src/"), args.fn)]
    else:
        todo = [(f["unit"], f["function"]) for f in
                json.loads(Path(args.census).read_text())]
    counts = Counter()
    for unit, fn in todo:
        res = screen(unit, fn)
        if res is None:
            if not args.image:
                emit(f"MISSING     {unit}::{fn}")
            continue
        only_t, only_b, label = res
        verdict = "VALUE-EQUAL" if not only_t and not only_b else "VALUE-DELTA"
        counts[verdict] += 1
        if args.image and verdict == "VALUE-EQUAL":
            continue
        emit(f"{verdict:<12} {unit}::{fn}")
        for k, n in sorted(only_t.items()):
            emit(f"    TARGET-ONLY x{n}  {k}   {label.get(k)}")
        for k, n in sorted(only_b.items()):
            emit(f"    OURS-ONLY   x{n}  {k}   {label.get(k)}")
    summary = "  ".join(f"{k}={v}" for k, v in sorted(counts.items()))
    emit("\n" + summary)
    if out is not sys.stdout:
        out.close()
        print(f"wrote {args.out}\n  {summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
