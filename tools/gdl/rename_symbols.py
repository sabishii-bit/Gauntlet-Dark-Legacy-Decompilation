#!/usr/bin/env python3
"""Apply a reviewed symbol-name map as exact identifier replacements.

The default is a dry run.  Only UTF-8 text files tracked by Git are scanned,
unless one or more --path roots are supplied.  This keeps semantic recovery
separate from the mechanical, repository-wide rename and makes each mapping
batch reproducible.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
TEXT_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".inc",
    ".json",
    ".md",
    ".py",
    ".rst",
    ".s",
    ".txt",
    ".yml",
    ".yaml",
}


def tracked_files(root: Path) -> list[Path]:
    output = subprocess.check_output(
        ["git", "ls-files", "-co", "--exclude-standard", "-z"], cwd=root
    )
    return [root / item.decode() for item in output.split(b"\0") if item]


def selected_files(root: Path, paths: list[str]) -> list[Path]:
    if not paths:
        return tracked_files(root)

    result: set[Path] = set()
    for value in paths:
        path = (root / value).resolve()
        if path.is_file():
            result.add(path)
        elif path.is_dir():
            result.update(p for p in path.rglob("*") if p.is_file())
        else:
            raise SystemExit(f"path does not exist: {value}")
    return sorted(result)


def load_map(path: Path) -> dict[str, str]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or not data:
        raise SystemExit("mapping file must be a non-empty JSON object")

    mapping: dict[str, str] = {}
    for old, new in data.items():
        if not isinstance(old, str) or not isinstance(new, str):
            raise SystemExit("all mapping keys and values must be strings")
        if not IDENTIFIER.fullmatch(old) or not IDENTIFIER.fullmatch(new):
            raise SystemExit(f"not a C identifier: {old!r} -> {new!r}")
        if old == new:
            raise SystemExit(f"identity mapping is not useful: {old}")
        if new in data:
            raise SystemExit(f"chained mappings are not supported: {old} -> {new}")
        mapping[old] = new

    return mapping


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mapping", help="JSON object mapping old identifiers to new names")
    parser.add_argument("--path", action="append", default=[], help="file or directory to scan")
    parser.add_argument("--apply", action="store_true", help="write replacements")
    args = parser.parse_args()

    # Git for Windows reports an MSYS-style /w/... path from rev-parse when
    # invoked by native Python.  Requiring invocation from the worktree root
    # avoids platform-specific path conversion and matches the other GDL tools.
    root = Path.cwd().resolve()
    if not (root / ".git").exists():
        raise SystemExit("run this tool from the repository root")
    map_path = (root / args.mapping).resolve()
    mapping = load_map(map_path)
    patterns = {
        old: re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        for old in mapping
    }

    totals = {old: 0 for old in mapping}
    changed = 0
    for path in selected_files(root, args.path):
        if path.resolve() == map_path or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        try:
            original = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue

        updated = original
        file_counts: dict[str, int] = {}
        for old, new in mapping.items():
            updated, count = patterns[old].subn(new, updated)
            if count:
                totals[old] += count
                file_counts[old] = count
        if updated == original:
            continue

        changed += 1
        detail = ", ".join(f"{old}:{count}" for old, count in file_counts.items())
        print(f"{path.relative_to(root)} ({detail})")
        if args.apply:
            path.write_text(updated, encoding="utf-8")

    missing = [old for old, count in totals.items() if count == 0]
    print(f"mode={'apply' if args.apply else 'dry-run'} files={changed} symbols={len(mapping)}")
    if missing:
        print("unreferenced: " + ", ".join(missing))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
