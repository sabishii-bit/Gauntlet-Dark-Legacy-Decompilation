#!/usr/bin/env python3
"""Which tools/gdl modules can a caller IMPORT instead of spawning?

The FALSIFIER for the `IMPORTABLE CORE:` docstring convention (run-43
item 10, documented in AGENTS.md). A tool with an importable core lets a
sweep do two object parses in one process; a tool without one forces a
subprocess per function, which is what T12 measured.

Method: import each module in a FRESH subprocess with a bare argv (the
shape a library import has), and report whether the import succeeds and
whether anything is printed — a module that does work at import cannot be
used as a library at all.

Measured 2026-09-03 (62 modules):
    importable and silent   51
    importable but PRINTS    9   abicheck, add_remat_census, addr16_census,
                                 addr16_homing_census, addrlo_dest_census,
                                 addrlo_home_general_census,
                                 addrlo_inplace_census, addrlo_shadow_probe,
                                 build_rule
    NOT importable           2   pdb20_dump, splice_rules

Usage (from the repository root):
    python tools/gdl/composed_census/t13_importable_census.py [--out PATH]
"""
from __future__ import annotations

import argparse
import contextlib
import importlib
import io
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GDL = ROOT / "tools" / "gdl"


def import_one(name: str) -> int:
    """The child half: import ONE module and report what it did."""
    sys.path.insert(0, str(GDL))
    sys.argv = [name]
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        module = importlib.import_module(name)
    publics = [key for key in vars(module)
               if not key.startswith("_") and callable(getattr(module, key))]
    sys.stderr.write(json.dumps({
        "printed": buffer.getvalue()[:200],
        "has_main": hasattr(module, "main"),
        "publics": len(publics),
        "marker": "IMPORTABLE CORE:" in (module.__doc__ or ""),
    }) + "\n")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--import-one", metavar="MODULE",
                        help="internal: the child half of the census")
    parser.add_argument(
        "--out",
        default=str(ROOT / "build" / "GUNE5D" / "t13_importable.json"))
    args = parser.parse_args(argv)
    if args.import_one:
        return import_one(args.import_one)

    rows = []
    for path in sorted(GDL.glob("*.py")):
        start = time.time()
        result = subprocess.run(
            [sys.executable, str(Path(__file__).resolve()),
             "--import-one", path.stem],
            cwd=str(ROOT), capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=180)
        payload, ok = {}, result.returncode == 0
        if ok:
            try:
                payload = json.loads(result.stderr.strip().splitlines()[-1])
            except (ValueError, IndexError):
                ok = False
        rows.append({
            "module": path.stem, "importable": ok,
            "seconds": round(time.time() - start, 2),
            "printed": bool(payload.get("printed")),
            "has_main": bool(payload.get("has_main")),
            "marker": bool(payload.get("marker")),
            "publics": payload.get("publics", 0),
            "error": "" if ok else
                     (result.stderr.strip().splitlines() or [""])[-1],
        })

    good = [r for r in rows if r["importable"] and not r["printed"]]
    noisy = [r for r in rows if r["importable"] and r["printed"]]
    bad = [r for r in rows if not r["importable"]]
    print(f"{len(rows)} modules in tools/gdl:")
    print(f"  importable and silent : {len(good)}")
    print(f"  importable but PRINTS : {len(noisy)}"
          + ("  " + ", ".join(r["module"] for r in noisy) if noisy else ""))
    print(f"  NOT importable        : {len(bad)}")
    for row in bad:
        print(f"    {row['module']:26s} {row['error'][:100]}")
    marked = [r["module"] for r in rows if r["marker"]]
    print(f"  carrying IMPORTABLE CORE: {len(marked)}  {', '.join(marked)}")
    broken_promise = [r["module"] for r in rows if r["marker"]
                      and (not r["importable"] or r["printed"])]
    if broken_promise:
        print("  MARKER IS A LIE for: " + ", ".join(broken_promise))
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    print(f"wrote {out}")
    return 1 if broken_promise else 0


if __name__ == "__main__":
    raise SystemExit(main())
