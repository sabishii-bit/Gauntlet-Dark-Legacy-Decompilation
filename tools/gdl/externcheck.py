#!/usr/bin/env python3
"""Whole-tree extern type-conflict census.

Finds linker symbols declared with conflicting C types across TUs — the bug
class behind InitCamera's fctiwz tell (combat.c said `extern s32
lbl_80344524;` while camera.c said `extern f32` for the same symbol, forcing
a float->int narrowing at the write site). MWCC trusts the local declaration,
so the TU with the wrong type silently compiles wrong-typed accesses.

Usage:
  python tools/gdl/externcheck.py            # scan src/, report conflicts
  python tools/gdl/externcheck.py --all      # also list benign multi-declared

A conflict is any symbol whose declared-type set mixes a float class
(f32/f64/float/double) with an int class, or two different sizes. Exit 1 when
conflicts exist so it can gate CI/passes.
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

EXTERN_RE = re.compile(
    r"^\s*extern\s+((?:const\s+|volatile\s+|unsigned\s+|signed\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\*+)?)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*(?:\s*,\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*"
    r"(?:\[[^\]]*\]\s*)?;", re.M
)
COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

FLOAT_TYPES = {"f32", "f64", "float", "double"}


def type_class(type_text):
    base = type_text.replace("const", "").replace("volatile", "").strip()
    if "*" in base:
        return "pointer"
    return "float" if base.split()[-1] in FLOAT_TYPES else "int"


def scan(src_root, include_root="include"):
    """{symbol: {type_text: [(file, line)]}} for every extern declaration.

    Headers are scanned too (explicit patterns — a bare "*.c*" glob skips
    any header without ".c" in its name, which hid camera.h's canonical
    gCameras declaration from the first version of this tool).
    """
    declarations = defaultdict(lambda: defaultdict(list))
    paths = []
    for root in (src_root, include_root):
        for pattern in ("*.c", "*.cpp", "*.h"):
            paths.extend(Path(root).rglob(pattern))
    for path in sorted(paths):
        text = COMMENT_RE.sub(
            lambda match: re.sub(r"[^\n]", " ", match.group(0)),
            path.read_text(encoding="utf-8", errors="replace"))
        for match in EXTERN_RE.finditer(text):
            type_text = " ".join(match.group(1).split())
            line = text.count("\n", 0, match.start()) + 1
            for name in re.split(r"\s*,\s*", match.group(2)):
                declarations[name.strip()][type_text].append(
                    (str(path).replace("\\", "/"), line))
    return declarations


def conflicts(declarations):
    for name, by_type in sorted(declarations.items()):
        if len(by_type) < 2:
            continue
        classes = {type_class(type_text) for type_text in by_type}
        # Only a float/non-float mix changes the emitted instructions
        # (lfs/stfs vs lwz/stw, or a forced fctiwz conversion). Pointer vs
        # int is same-width word traffic — benign, shown only with --all.
        yield name, by_type, "float" in classes and len(classes) > 1


def main():
    show_all = "--all" in sys.argv
    declarations = scan("src")
    conflict_count = 0
    for name, by_type, is_conflict in conflicts(declarations):
        if not is_conflict and not show_all:
            continue
        marker = "CONFLICT" if is_conflict else "multi   "
        conflict_count += is_conflict
        print(f"{marker} {name}")
        for type_text, sites in sorted(by_type.items()):
            first = sites[0]
            more = f" (+{len(sites) - 1} more)" if len(sites) > 1 else ""
            print(f"    {type_text:<20} {first[0]}:{first[1]}{more}")
    print(f"[{conflict_count} float/int-class conflict(s)]")
    return 1 if conflict_count else 0


if __name__ == "__main__":
    sys.exit(main())
