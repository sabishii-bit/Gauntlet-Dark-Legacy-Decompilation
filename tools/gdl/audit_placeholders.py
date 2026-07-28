#!/usr/bin/env python3
"""Find stale fn_/lbl_ references whose address already has a real name.

The placeholder spelling embeds its virtual address, so this audit does not
need disassembly or source parsing.  It builds an address-to-name index from
symbols.txt, scans source files for placeholders, and reports canonical names
already assigned to the same address.

Run from the repository root:

    python tools/gdl/audit_placeholders.py
    python tools/gdl/audit_placeholders.py --path src/game/game/player.c
    python tools/gdl/audit_placeholders.py --apply
"""

from __future__ import annotations

import argparse
import re
from collections import defaultdict
from pathlib import Path


SYMBOL_RE = re.compile(
    r"^([A-Za-z_.$@][A-Za-z0-9_.$@]*)\s*=\s*[^:;]+:0x([0-9A-Fa-f]+)"
)
PLACEHOLDER_RE = re.compile(r"\b(?:fn|lbl)_([0-9A-Fa-f]{8})\b")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".s", ".S"}


def is_placeholder(name: str) -> bool:
    return bool(re.fullmatch(r"(?:fn|lbl)_[0-9A-Fa-f]{8}", name))


def name_score(name: str) -> tuple[int, int, str]:
    """Prefer ordinary source identifiers over linker/compiler spellings."""
    ordinary = int(bool(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name)))
    return (ordinary, -len(name), name)


def load_canonical_names(symbols_path: Path) -> dict[int, str]:
    names: dict[int, list[str]] = defaultdict(list)
    for line in symbols_path.read_text(encoding="utf-8").splitlines():
        match = SYMBOL_RE.match(line)
        if not match:
            continue
        name, address = match.groups()
        if not is_placeholder(name):
            names[int(address, 16)].append(name)
    return {
        address: max(candidates, key=name_score)
        for address, candidates in names.items()
    }


def iter_source_files(root: Path, paths: list[str]) -> list[Path]:
    if paths:
        candidates = [root / path for path in paths]
    else:
        candidates = [root / "src", root / "include"]

    files: list[Path] = []
    for candidate in candidates:
        if candidate.is_file():
            files.append(candidate)
        elif candidate.is_dir():
            files.extend(
                path
                for path in candidate.rglob("*")
                if path.is_file() and path.suffix in SOURCE_SUFFIXES
            )
        else:
            raise SystemExit(f"path does not exist: {candidate}")
    return sorted(set(files))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--symbols",
        default="config/GUNE5D/symbols.txt",
        help="symbol file relative to the repository root",
    )
    parser.add_argument(
        "--path",
        action="append",
        default=[],
        help="file or directory to scan; repeatable (default: src and include)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="replace every reported placeholder in the scanned files",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    canonical = load_canonical_names(root / args.symbols)
    files = iter_source_files(root, args.path)
    replacements: dict[str, str] = {}
    touched: list[Path] = []

    for path in files:
        text = path.read_text(encoding="utf-8")
        for match in PLACEHOLDER_RE.finditer(text):
            old = match.group(0)
            new = canonical.get(int(match.group(1), 16))
            if new and new != old:
                replacements[old] = new

        if args.apply and replacements:
            updated = text
            for old, new in replacements.items():
                updated = re.sub(rf"\b{re.escape(old)}\b", new, updated)
            if updated != text:
                path.write_text(updated, encoding="utf-8", newline="")
                touched.append(path)

    for old in sorted(replacements):
        print(f"{old}\t{replacements[old]}")

    print(
        f"mappings={len(replacements)} files_scanned={len(files)} "
        f"files_changed={len(touched)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
