"""Generate a flat struct/field table from research/xbox_symbols/*.h
(for the Ghidra /XboxPDB type import).

Parse research/xbox_symbols/*.h (PDB-extracted, Offset= comments) into a
flat table for the Ghidra import script.

Output format (tab-separated):
  S <name> <size> <category>
  F <offset> <size> <fieldname>
"""
import re
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "research" / "xbox_symbols"
OUT = Path(__file__).resolve().parent.parent / "research" / "xbox_symbols" / "xbox_structs.tsv"

CATEGORIES = ["game", "audio", "math", "util", "graphics", "ps2"]

STRUCT_RE = re.compile(r"^(?:struct|class)\s+(\w+)\s*//\s*Size=0x([0-9A-Fa-f]+)")
FIELD_RE = re.compile(r"^\s*(.*?)\s*(\w+)(\[[^\]]*\])?\s*;\s*//\s*Offset=0x([0-9A-Fa-f]+)\s+Size=0x([0-9A-Fa-f]+)")

seen = set()
lines_out = []
count = 0

for cat in CATEGORIES:
    text = (SRC / f"{cat}.h").read_text(encoding="utf-8", errors="replace")
    cur = None
    fields = {}
    depth = 0

    def flush():
        global cur, fields, count
        if cur and cur[0] not in seen and fields:
            seen.add(cur[0])
            lines_out.append(f"S\t{cur[0]}\t{cur[1]}\t{cur[2]}")
            for off in sorted(fields):
                fsz, fname = fields[off]
                lines_out.append(f"F\t{off}\t{fsz}\t{fname}")
            count += 1
        cur, fields = None, {}

    for line in text.splitlines():
        m = STRUCT_RE.match(line)
        if m:
            flush()
            cur = (m.group(1), int(m.group(2), 16), cat)
            depth = 0
            continue
        if cur is None:
            continue
        depth += line.count("{") - line.count("}")
        if line.startswith("};") or (depth < 0):
            flush()
            continue
        m = FIELD_RE.match(line)
        if m:
            _, name, arr, off, fsz = m.groups()
            off, fsz = int(off, 16), int(fsz, 16)
            if off not in fields and name not in ("Size",):
                fields[off] = (fsz, name)
    flush()

OUT.write_text("\n".join(lines_out) + "\n", encoding="utf-8")
print(f"{count} structs -> {OUT}")
