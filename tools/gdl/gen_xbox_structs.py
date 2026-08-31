#!/usr/bin/env python3
"""Regenerate research/xbox_symbols/xbox_structs.tsv from the PDB dump headers.

The dump headers (misc.h, game.h, audio.h, ...) are machine-generated with a
rigid shape:

    struct camera_data// Size=0x6c (Id=3269)
    {
        int flags;// Offset=0x0 Size=0x4
        unsigned long Type:5;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x5
        char title[16];// Offset=0x14 Size=0x10
    };

Every struct/union carries Size and a PDB Id; every field line carries an
absolute Offset/Size (bitfields share an offset). Anonymous records are all
spelled `__unnamed` and are disambiguated here by Id (`__unnamed_3269`), per
claim.offsetcast-recoverable-blockers-ranked (the old hand-built table
indexed 227 of ~1,885 named records, leaving the struct op blind to 88%).

Output format matches memory_graph.core._import_pdb_types exactly:
    S<TAB>name<TAB>size-decimal<TAB>category
    F<TAB>offset-decimal<TAB>size-decimal<TAB>fieldname

Duplicate names across headers keep the FIRST definition seen; headers are
processed game-side first so game records win name collisions. Rerun after
any header refresh, then `python memory_graph/gdlmem.py build`.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DUMP_DIR = REPO_ROOT / "research" / "xbox_symbols"
OUT_PATH = DUMP_DIR / "xbox_structs.tsv"

# Game-relevant headers first: first definition wins name collisions.
HEADER_ORDER = (
    "game.h", "misc.h", "audio.h", "ps2.h", "math.h", "util.h",
    "graphics.h", "d3d.h", "xbox.h", "windows_gdi.h", "windows_kernel.h",
)

RECORD_RE = re.compile(
    r"^(struct|union)\s+(\w+)//\s*Size=0x([0-9A-Fa-f]+)\s*\(Id=(\d+)\)\s*$")
FIELD_RE = re.compile(r"//\s*Offset=0x([0-9A-Fa-f]+)\s+Size=0x([0-9A-Fa-f]+)")
# Last identifier of the declarator, allowing [array] and :bitfield suffixes.
FIELD_NAME_RE = re.compile(
    r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*(?::\s*\d+\s*)?$")


def parse_header(path: Path):
    """Yield (name, size, category, fields) per record; fields are
    (offset, size, fieldname) with absolute offsets, nested blocks
    flattened onto the owning record."""
    category = path.stem
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    i = 0
    while i < len(lines):
        match = RECORD_RE.match(lines[i].strip())
        if not match:
            i += 1
            continue
        name = match.group(2)
        size = int(match.group(3), 16)
        record_id = int(match.group(4))
        if name == "__unnamed":
            name = f"__unnamed_{record_id}"
        fields = []
        depth = 0
        i += 1
        started = False
        while i < len(lines):
            line = lines[i]
            depth += line.count("{") - line.count("}")
            if "{" in line:
                started = True
            field = FIELD_RE.search(line)
            if field and ";" in line:
                decl = line.split(";", 1)[0].strip()
                name_match = FIELD_NAME_RE.search(decl)
                if name_match:
                    fields.append((int(field.group(1), 16),
                                   int(field.group(2), 16),
                                   name_match.group(1)))
            i += 1
            if started and depth <= 0:
                break
        yield name, size, category, fields


def main() -> int:
    seen: dict[str, tuple[int, int]] = {}
    out_lines: list[str] = []
    records = fields_total = collisions = 0
    for header in HEADER_ORDER:
        path = DUMP_DIR / header
        if not path.exists():
            print(f"warning: {path} missing, skipped", file=sys.stderr)
            continue
        for name, size, category, fields in parse_header(path):
            if name in seen:
                if seen[name] != (size, len(fields)):
                    collisions += 1
                continue
            seen[name] = (size, len(fields))
            out_lines.append(f"S\t{name}\t{size}\t{category}")
            for offset, fsize, fname in fields:
                out_lines.append(f"F\t{offset}\t{fsize}\t{fname}")
            records += 1
            fields_total += len(fields)
    OUT_PATH.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT_PATH}")
    print(f"records: {records}  fields: {fields_total}"
          f"  name-collisions (first kept): {collisions}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
