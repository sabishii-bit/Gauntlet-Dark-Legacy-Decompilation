"""SP lane, run 54 -- census of the STRING-BLOB EXTERN class.

A blob-extern site is a `char*`/`u8*` view of a retail .rodata string pool
(`extern char lbl_ADDR[];`) consumed as `base + N`, where the retail bytes at
ADDR+N decode as a NUL-terminated, 4-byte-aligned string-pool entry.  Run 54
proved these are recoverable as REAL C STRING LITERALS under the TUs' existing
`-str reuse,readonly` cflags: MWCC emits one 4-aligned .rodata object per
literal and pools the ADDRESSING into one base register plus `addi` offsets,
which is byte-identical to what retail does.

Emits a ranked per-TU roster: sites, distinct pool entries, consuming
functions, and the Object() state from configure.py.

  python tools/gdl/composed_census/sp_blob_census.py \
      [--out build/GUNE5D/sp_blob_census.json]

IMPORTABLE CORE: pool_entry, looks_like_pool, rodata_owners, owner_of,
scan_file, object_states -- pure over parsed data, no build, import is
side-effect free apart from reading the retail DOL and splits.txt.
"""
import argparse
import json
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
# tools/gdl/composed_census/<this> -> repo root
ROOT = os.environ.get("GDL_ROOT") or os.path.abspath(
    os.path.join(HERE, os.pardir, os.pardir, os.pardir))
DOL = os.path.join(ROOT, "build", "GUNE5D", "main.retail.dol")
SRCDIR = os.path.join(ROOT, "src")
CONFIGURE = os.path.join(ROOT, "configure.py")

# ---------------------------------------------------------------- retail DOL
# Loaded LAZILY: importing this module must have no side effects, or it joins
# the library-hostile set AGENTS.md names under IMPORTABLE CORE.
_DOL_CACHE = None


def _dol():
    global _DOL_CACHE
    if _DOL_CACHE is None:
        data = open(DOL, "rb").read()
        _DOL_CACHE = (data,
                      struct.unpack(">18I", data[0x00:0x48]),
                      struct.unpack(">18I", data[0x48:0x90]),
                      struct.unpack(">18I", data[0x90:0xD8]))
    return _DOL_CACHE


def dol_read(vaddr, n):
    data, offs, addrs, sizes = _dol()
    for o, a, s in zip(offs, addrs, sizes):
        if s and a <= vaddr < a + s:
            avail = min(n, (a + s) - vaddr)
            return data[o + (vaddr - a):o + (vaddr - a) + avail]
    return None


PRINTABLE = set(range(0x20, 0x7F)) | {0x09, 0x0A, 0x0D}


def pool_entry(vaddr):
    """Return the NUL-terminated printable string at vaddr, or None."""
    b = dol_read(vaddr, 256)
    if not b:
        return None
    z = b.find(b"\0")
    if z <= 0:
        return None
    s = b[:z]
    if not all(c in PRINTABLE for c in s):
        return None
    return s.decode("latin1")


def looks_like_pool(vaddr, entries=3):
    """True if vaddr starts >=`entries` consecutive 4-aligned pool entries."""
    if vaddr % 4:
        return False
    a = vaddr
    for _ in range(entries):
        s = pool_entry(a)
        if s is None:
            return False
        a += (len(s) + 1 + 3) & ~3
    return True


# ---------------------------------------------------------------- source scan
EXTERN_BLOB = re.compile(
    r"\bextern\s+(?:const\s+)?(?:unsigned\s+)?(?:char|u8|s8)\s*\*?\s*"
    r"(lbl_[0-9A-Fa-f]{8})\s*\[")
ASSIGN = re.compile(
    r"\b(?:const\s+)?(?:unsigned\s+)?(?:char|u8|s8)\s*\*\s*(\w+)\s*=\s*"
    r"(lbl_[0-9A-Fa-f]{8})\b")
FNDEF = re.compile(r"^[A-Za-z_][A-Za-z0-9_ \t\*]*\b(\w+)\s*\(")


def scan_file(path):
    txt = open(path, encoding="utf-8", errors="replace").read()
    lines = txt.split("\n")
    blobs = {}          # symbol -> vaddr, only those that decode as a pool
    for m in EXTERN_BLOB.finditer(txt):
        sym = m.group(1)
        va = int(sym[4:], 16)
        if looks_like_pool(va):
            blobs[sym] = va
    if not blobs:
        return None

    sites = []
    aliases = {}
    fn = "?"
    for i, ln in enumerate(lines, 1):
        if ln.startswith("}"):
            aliases = {}
        m = FNDEF.match(ln)
        if m and not ln.rstrip().endswith(";") and "=" not in ln.split("(")[0]:
            fn = m.group(1)
        for mm in ASSIGN.finditer(ln):
            if mm.group(2) in blobs:
                aliases[mm.group(1)] = blobs[mm.group(2)]
        names = dict(aliases)
        for sym, va in blobs.items():
            names[sym] = va
        for var, va in names.items():
            for mm in re.finditer(
                    r"\b" + re.escape(var) + r"\s*\+\s*(0x[0-9a-fA-F]+|\d+)\b", ln):
                n = int(mm.group(1), 0)
                lit = pool_entry(va + n)
                if lit is not None:
                    sites.append(dict(line=i, function=fn, base=va, off=n,
                                      literal=lit))
    return dict(blobs={k: "0x%08X" % v for k, v in blobs.items()}, sites=sites)


def rodata_owners():
    """[(start, end, unit)] for every unit's .rodata run, from splits.txt."""
    p = os.path.join(ROOT, "config", "GUNE5D", "splits.txt")
    runs = []
    unit = None
    for ln in open(p, encoding="utf-8", errors="replace"):
        m = re.match(r"^(\S.*):\s*$", ln)
        if m:
            unit = m.group(1)
            continue
        m = re.match(r"^\s+(\.rodata|\.data|\.sdata2)\s+start:(0x[0-9A-Fa-f]+)"
                     r"\s+end:(0x[0-9A-Fa-f]+)", ln)
        if m and unit:
            runs.append((int(m.group(2), 16), int(m.group(3), 16), unit,
                         m.group(1)))
    return runs


_RUNS_CACHE = None


def owner_of(vaddr):
    global _RUNS_CACHE
    if _RUNS_CACHE is None:
        _RUNS_CACHE = rodata_owners()
    for a, b, unit, sec in _RUNS_CACHE:
        if a <= vaddr < b:
            return unit, sec
    return None, None


def object_states():
    """unit path -> Matching/NonMatching, read from configure.py."""
    txt = open(CONFIGURE, encoding="utf-8", errors="replace").read()
    out = {}
    for m in re.finditer(r"Object\(\s*(\w+)\s*,\s*\"([^\"]+)\"", txt):
        out[m.group(2)] = m.group(1)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(
        "build", "GUNE5D", "sp_blob_census.json"))
    args = ap.parse_args()

    states = object_states()
    rows = []
    for dirpath, _dirs, files in os.walk(SRCDIR):
        for f in files:
            if not f.endswith((".c", ".cpp")):
                continue
            p = os.path.join(dirpath, f)
            r = scan_file(p)
            if not r or not r["sites"]:
                continue
            unit = os.path.relpath(p, SRCDIR).replace("\\", "/")
            fns = sorted({s["function"] for s in r["sites"]})
            ents = sorted({(s["base"], s["off"]) for s in r["sites"]})
            own = {}
            for sym, hexva in r["blobs"].items():
                o, sec = owner_of(int(hexva, 16))
                if o == unit:
                    cls = "SELF"
                elif o is None:
                    cls = "OPEN"      # unsplit .rodata: no unit claims it yet
                else:
                    cls = "FOREIGN"   # another unit owns the bytes
                own[sym] = dict(addr=hexva, owner=o, section=sec, cls=cls)

            def site_cls(s):
                return own.get("lbl_%08X" % s["base"], {}).get("cls", "OPEN")

            per = {"SELF": 0, "OPEN": 0, "FOREIGN": 0}
            for s in r["sites"]:
                per[site_cls(s)] += 1
            rows.append(dict(
                unit=unit,
                object_state=states.get(unit, "?"),
                blobs=r["blobs"],
                blob_ownership=own,
                blob_classes={k: v["cls"] for k, v in own.items()},
                sites_self=per["SELF"],
                sites_open=per["OPEN"],
                sites_foreign=per["FOREIGN"],
                convertible_sites=per["SELF"] + per["OPEN"],
                sites=len(r["sites"]),
                distinct_entries=len(ents),
                functions=fns,
                n_functions=len(fns),
                examples=[s["literal"] for s in r["sites"][:5]],
                detail=r["sites"],
            ))
    rows.sort(key=lambda x: (-x["convertible_sites"], -x["sites"], x["unit"]))

    outp = os.path.join(ROOT, args.out) if not os.path.isabs(args.out) else args.out
    os.makedirs(os.path.dirname(outp), exist_ok=True)
    json.dump(dict(rows=rows,
                   totals=dict(tus=len(rows),
                               sites=sum(r["sites"] for r in rows),
                               entries=sum(r["distinct_entries"] for r in rows),
                               functions=sum(r["n_functions"] for r in rows))),
              open(outp, "w", encoding="utf-8"), indent=1)

    print("STRING-BLOB EXTERN CENSUS  (retail-verified 4-aligned pools only)")
    print("blob .rodata ownership per config/GUNE5D/splits.txt: SELF = this "
          "unit already owns the run; OPEN = UNSPLIT .rodata, no unit claims "
          "it (convertible; a TU flip additionally needs a splits.txt run); "
          "FOREIGN = another unit owns the bytes (hard blocker).")
    print("%-30s %-12s %-5s %-5s %-5s %-4s %-4s %s" %
          ("unit", "state", "SELF", "OPEN", "FRGN", "ent", "fns", "example"))
    for r in rows:
        print("%-30s %-12s %-5d %-5d %-5d %-4d %-4d %r" %
              (r["unit"][:30], r["object_state"], r["sites_self"],
               r["sites_open"], r["sites_foreign"], r["distinct_entries"],
               r["n_functions"],
               (r["examples"][0] if r["examples"] else "")[:24]))
    t = dict(tus=len(rows), sites=sum(r["sites"] for r in rows),
             self_=sum(r["sites_self"] for r in rows),
             open_=sum(r["sites_open"] for r in rows),
             frgn=sum(r["sites_foreign"] for r in rows),
             entries=sum(r["distinct_entries"] for r in rows),
             functions=sum(r["n_functions"] for r in rows))
    print()
    print("TOTALS: %d TU(s), %d site(s) = %d SELF + %d OPEN + %d FOREIGN; "
          "%d distinct pool entr(ies), %d function(s)"
          % (t["tus"], t["sites"], t["self_"], t["open_"], t["frgn"],
             t["entries"], t["functions"]))
    print()
    print("FOREIGN blobs (owned by another unit -- NOT convertible in place):")
    any_f = False
    for r in rows:
        for sym, meta in sorted(r["blob_ownership"].items()):
            if meta["cls"] == "FOREIGN":
                any_f = True
                print("  %-28s %s -> %s" % (r["unit"], sym, meta["owner"]))
    if not any_f:
        print("  (none)")
    print("wrote", outp)


if __name__ == "__main__":
    main()
