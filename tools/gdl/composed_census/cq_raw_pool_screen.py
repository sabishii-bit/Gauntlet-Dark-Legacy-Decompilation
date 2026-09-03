#!/usr/bin/env python3
"""Screen a `fndiff --clean` POOL-DEFECT row against the RAW compiler output.

A POOL-DEFECT row is an observation about the POSTPROCESSED object at one
offset.  Two things can make it lie, and this tool removes both (see
claim.law.CQ_a-pool-defect-row-is-decided-by-the-raw-objects-value-multiset-
not-by-position.20260903.v1):

  1. WRONG OBJECT.  For a webfrank-pinned function the postprocessed
     object's register fields come from the RULE, not the compiler, so a row
     can be a rule artifact (game/enemy/enemy::move_logic00 is a shipped
     one).  This reads `.postprocess/body/<tu>.o` when it exists.
  2. WRONG PAIRING.  Relocation index k names the same instruction on both
     sides only where the instruction words already agree.  This compares
     the MULTISET of resolved pool VALUES instead, which is alignment-free.

Each datum is read at its own symbol SIZE (an 8-byte window over an f32
entry reads its neighbour and manufactures differences), and a string datum
is compared over its whole NUL-terminated run (dtk names a string by the
nearest preceding lbl_, so a fixed window truncates it).

Usage, from the repository root:
  python tools/gdl/composed_census/cq_raw_pool_screen.py game/enemy/enemy move_logic00

SCOPE, measured run 43 and NOT the same question `cr_datum_screen.py` asks:
this screen resolves POOL data only (our anonymous `@N` entries and the
splitter's `lbl_*`) and DROPS every other relocation, so it answers "is a
LITERAL in this function wrong?".  cr_datum_screen keys every relocation,
including named globals and callees.  Over 55 live functions the two verdicts
disagreed 28 times, always this way round (pool-only EQUAL vs all-relocations
DELTA) -- so they are complements, not two halves of one screen.  What they
DID duplicate was infrastructure: a DOL reader, a symbols.txt parser and an
objdump reader that `fndiff` already owned.  Those are now shared
(`fndiff.object_sections`, `fndiff.dol_read`, `fndiff.symbol_addresses`) and
this screen's verdicts are unchanged -- verified byte-for-byte against the
pre-merge tool on all 55 functions.  The RAW-object idea this tool
contributed is now also available as `fndiff --datum --raw`.

Verdicts:
  VALUE MULTISET EQUAL   the compiler already uses exactly the target's pool
                         values - no literal in this function is wrong, and
                         any --clean row it carries is a pairing,
                         displacement or rule-binding artifact.
  TARGET-ONLY/OURS-ONLY  a genuine source-value defect, named by VALUE even
                         where no position can be trusted.
  COUNT MISMATCH         relocation counts differ; the screen is
                         inconclusive, not clean.
"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "gdl"))
import fndiff  # noqa: E402

NBYTES = 48


def load_symbols():
    """(name -> address, name -> size), from fndiff's symbols.txt readers.

    Size defaults to 4 exactly as the private parser did — a symbols.txt
    line with no `size:` field still resolves.
    """
    addresses = fndiff.symbol_addresses()
    sizes = fndiff.symbol_sizes()
    return addresses, {name: sizes.get(name, 4) for name in addresses}


def load_object(obj):
    """{symbol: (section, offset, size)}, {section: bytes} — shared reader.

    Run-43 item 2: this used to be a third private objdump parser beside
    fndiff's and cr_datum_screen's. The screen's own logic below is
    unchanged; only the readers are shared.
    """
    return fndiff.object_sections(obj, readable=None)


def fmt(blob):
    if not blob:
        return "?"
    out = blob.hex()
    if len(blob) >= 8:
        out += f" f64={struct.unpack('>d', blob[:8])[0]!r}"
    if len(blob) >= 4:
        out += f" f32={struct.unpack('>f', blob[:4])[0]!r}"
    head = blob.split(b"\x00")[0]
    if head and all(32 <= b < 127 for b in head):
        out += f" str={head.decode('ascii')!r}"
    return out


def relocs(unit, fn, args):
    r = subprocess.run([sys.executable, "tools/gdl/fnasm.py", unit, fn] + args,
                       cwd=ROOT, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    rows = []
    for line in r.stdout.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(\S+)\s+(.*?)\s+@(\S+?)\((\w+)\)",
                     line)
        if m:
            rows.append((int(m.group(1), 16), m.group(2), m.group(4)))
    return rows


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    unit = sys.argv[1].replace(".cpp", "").replace(".c", "")
    fn = sys.argv[2]

    # fndiff's readers address the DOL, symbols.txt and objdump by
    # repository-relative path, so this tool now runs from the root the way
    # its docstring already required.
    os.chdir(ROOT)
    syms, symsize = load_symbols()

    def dol_bytes(addr, n):
        return fndiff.dol_read(addr, n) or b""

    body = (ROOT / "build" / "GUNE5D" / "src" / unit).parent \
        / ".postprocess" / "body" / (Path(unit).name + ".o")
    ours_args = ["--ours", "--raw"]
    if not body.exists():        # unpinned TU: the final object IS the raw one
        body = ROOT / "build" / "GUNE5D" / "src" / (unit + ".o")
        ours_args = ["--ours"]
    if not body.exists():
        print(f"missing object for {unit}: run ninja first")
        return 2
    symoff, whole = load_object(body)

    def our_bytes(name, n=None):
        ent = symoff.get(name)
        if ent is None:
            return b""
        sect, off, size = ent
        return whole.get(sect, b"")[off:off + (n if n is not None else size)]

    def resolve(sym):
        if sym.startswith("@"):
            blob, wide = our_bytes(sym), our_bytes(sym, NBYTES)
        elif sym.startswith("lbl_") and sym in syms:
            blob = dol_bytes(syms[sym], symsize.get(sym, 4))
            wide = dol_bytes(syms[sym], NBYTES)
        else:
            return None          # a function or named global, not a pool datum
        if not blob:
            return None
        head = wide.split(b"\x00")[0]
        if len(head) >= 4 and all(32 <= b < 127 for b in head):
            return head          # string datum: the whole run
        return blob              # numeric datum: exactly its own bytes

    tgt = relocs(unit, fn, [])
    ours = relocs(unit, fn, ours_args)
    print(f"{unit}::{fn}: target relocs {len(tgt)}, "
          f"{'raw-' if '--raw' in ours_args else ''}ours relocs {len(ours)}")
    if len(tgt) != len(ours):
        print("  COUNT MISMATCH - the screen is inconclusive, not clean")

    tvals, ovals = {}, {}
    for off, _mnem, sym in tgt:
        v = resolve(sym)
        if v:
            tvals.setdefault(v, []).append(f"+0x{off:x}")
    for off, _mnem, sym in ours:
        v = resolve(sym)
        if v:
            ovals.setdefault(v, []).append(f"+0x{off:x}")

    only_t = {k: v for k, v in tvals.items() if k not in ovals}
    only_o = {k: v for k, v in ovals.items() if k not in tvals}
    if not only_t and not only_o:
        print("  VALUE MULTISET EQUAL (the compiler already uses exactly the "
              "target's pool values) -> no source-value defect")
    for k, v in only_t.items():
        print(f"  TARGET-ONLY value {fmt(k)}  at {','.join(v)}")
    for k, v in only_o.items():
        print(f"  OURS-ONLY   value {fmt(k)}  at {','.join(v)}")
    for k in tvals:
        if k in ovals and len(tvals[k]) != len(ovals[k]):
            print(f"  COUNT DIFFERS for {fmt(k)}: target {len(tvals[k])} "
                  f"vs ours {len(ovals[k])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
