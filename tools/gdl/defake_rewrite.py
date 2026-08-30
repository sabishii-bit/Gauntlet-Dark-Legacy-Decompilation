#!/usr/bin/env python3
"""Table-driven raw-offset -> offsetof() rewriter for de-fakematch passes.

Replaces the per-base scratch scripts every defake worker hand-writes:
given a base expression and an offset->field map, rewrites the two
canonical raw-cast shapes in place and reports what it could NOT map,
instead of silently leaving it.

  *(T*)((u8*)BASE + N)   ->  *(T*)((u8*)BASE + offsetof(Type, field))
  *(T*)(BASE + N)        ->  *(T*)(BASE + offsetof(Type, field))

Usage:
  python tools/gdl/defake_rewrite.py src/game/x.c --base "c->hdr" \
      --type CritterFileHeader --map 0x124=nodes,0x110=movecount [--write]

Dry-run by default: prints each planned rewrite as file:line old => new.
--write applies them. The map accepts hex or decimal offsets; field may
contain dots/brackets (e.g. pos[0], objgrp.worldmat). ALWAYS gate after:
  python tools/gdl/defake_gate.py check <unit> --rebuild
This tool is textual — it does not prove neutrality, the gate does.
"""

import re
import sys
from pathlib import Path


def build_pattern(base):
    base_re = re.escape(base).replace(r"\ ", r"\s*")
    return re.compile(
        r"\*\s*\(\s*(?P<cast>[\w\s]+\*+)\s*\)\s*\(\s*"
        r"(?P<inner>\(\s*u8\s*\*\s*\)\s*)?"
        rf"(?P<base>{base_re})\s*\+\s*(?P<off>0[xX][0-9A-Fa-f]+|\d+)\s*\)"
    )


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("--help", "-h"):
        print(__doc__)
        return 2
    path = Path(args[0])
    write = "--write" in args

    def opt(name):
        return args[args.index(name) + 1] if name in args else None

    base, type_name, raw_map = opt("--base"), opt("--type"), opt("--map")
    if not (path.is_file() and base and type_name and raw_map):
        print(__doc__)
        return 2
    mapping = {}
    for pair in raw_map.split(","):
        off_text, field = pair.split("=", 1)
        mapping[int(off_text, 0)] = field.strip()

    text = path.read_text(encoding="utf-8", errors="replace")
    pattern = build_pattern(base)
    unmapped = {}
    planned = []

    def substitute(match):
        offset = int(match.group("off"), 0)
        field = mapping.get(offset)
        if field is None:
            line = text.count("\n", 0, match.start()) + 1
            unmapped.setdefault(hex(offset), []).append(line)
            return match.group(0)
        inner = match.group("inner") or ""
        new = (f"*({match.group('cast').strip()})({inner}{match.group('base')}"
               f" + offsetof({type_name}, {field}))")
        line = text.count("\n", 0, match.start()) + 1
        planned.append((line, match.group(0), new))
        return new

    rewritten = pattern.sub(substitute, text)
    for line, old, new in planned:
        print(f"{path}:{line}: {old}\n{'':>{len(str(path)) + len(str(line)) + 2}}=> {new}")
    if unmapped:
        print("UNMAPPED offsets (left raw — extend --map or leave with cause):")
        for offset, lines in sorted(unmapped.items()):
            print(f"  {offset}: lines {', '.join(map(str, lines[:12]))}")
    if not planned:
        print("nothing matched — check --base spelling (whitespace-insensitive)")
        return 1
    if write:
        if "offsetof(" in rewritten and "#define offsetof" not in rewritten \
                and "<stddef.h>" not in rewritten and "offsetof" not in text:
            print("NOTE: file gains offsetof() — ensure the project's"
                  " fallback macro or include is present")
        path.write_text(rewritten, encoding="utf-8")
        print(f"[applied {len(planned)} rewrite(s); {len(unmapped)} offset(s)"
              " unmapped — now GATE: defake_gate.py check <unit> --rebuild]")
    else:
        print(f"[dry-run: {len(planned)} rewrite(s) planned,"
              f" {len(unmapped)} unmapped — add --write to apply]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
