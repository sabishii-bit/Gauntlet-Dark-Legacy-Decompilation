#!/usr/bin/env python3
"""Class the image-wide VALUE-DELTA census into actionable defect families."""
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
TXT = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "build" / "GUNE5D" / "cr_image_datum.txt"

# retail spells the circle constants as 10-significant-digit decimal
# literals; a full-precision pi/2pi/pi-2 differs only in the low mantissa.
TRUNC = "54524550"      # ...FB54524550  = 3.141592654 family
FULL = "54442d18"       # ...FB54442D18  = M_PI family

blocks, cur = [], None
for line in TXT.read_text(errors="replace").splitlines():
    m = re.match(r"^VALUE-DELTA\s+(\S+)::(\S+)$", line)
    if m:
        cur = [m.group(1), m.group(2), []]
        blocks.append(cur)
    elif cur is not None and line.startswith("    "):
        cur[2].append(line.strip())

fams = defaultdict(list)
for unit, fn, body in blocks:
    t_only = [b for b in body if b.startswith("TARGET-ONLY")]
    o_only = [b for b in body if b.startswith("OURS-ONLY")]
    joined = " ".join(body)
    if TRUNC in joined and FULL in joined:
        fams["pi-literal-precision"].append((unit, fn, body))
    elif any(re.search(r"B:(?:[2-7][0-9a-f]){6,}", b) for b in body):
        fams["string"].append((unit, fn, body))
    elif len(t_only) + len(o_only) <= 4:
        fams["small-constant"].append((unit, fn, body))
    else:
        fams["broad (structural divergence likely)"].append((unit, fn, body))

for fam in ("pi-literal-precision", "string", "small-constant",
            "broad (structural divergence likely)"):
    rows = fams[fam]
    print(f"\n=== {fam}: {len(rows)} function(s) "
          f"in {len({r[0] for r in rows})} TU(s) ===")
    for unit, fn, body in rows:
        print(f"  {unit}::{fn}")
        if fam != "broad (structural divergence likely)":
            for b in body:
                print(f"      {b}")
print(f"\ntotal value-delta functions: {len(blocks)}")
