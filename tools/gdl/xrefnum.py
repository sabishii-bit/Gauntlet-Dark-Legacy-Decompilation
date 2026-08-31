#!/usr/bin/env python3
"""Magic-number cross-reference: who else uses this constant?

The fastest route to struct authority for an unknown raw offset is finding
another TU that already puzzled out the same byte offset (memcard.c's
TexHeaderView and WinGlobalsView both came from exactly this hunt, done by
hand). This tool makes it one command.

Usage:
  python tools/gdl/xrefnum.py 0x5B8              # one constant, all of src/
  python tools/gdl/xrefnum.py 23360 0x320        # several at once
  python tools/gdl/xrefnum.py 0x320 --cast-only  # only near cast/PF patterns

Each constant is searched in hex (0x320, case-insensitive) and decimal (800)
spellings. --cast-only restricts hits to lines that look like raw-offset
access (a `*(T*)(` cast or PF( macro on the line), cutting array-size and
enum noise. include/ is scanned too — a hit there IS the authority.
"""

import re
import sys
from pathlib import Path

CAST_HINT_RE = re.compile(r"\*\s*\(\s*\w[\w\s]*\*+\s*\)\s*\(|\bPF\s*\(")
COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def spellings(token):
    value = int(token, 0)
    forms = {rf"\b{value}\b"}
    if value >= 10:
        forms.add(rf"0[xX]0*{value:x}\b")
    return value, [re.compile(form, re.I) for form in forms]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    cast_only = "--cast-only" in sys.argv
    if not args or args[0] in ("--help", "-h"):
        print(__doc__)
        return 1
    targets = [spellings(token) for token in args]
    total = 0
    for base in ("src", "include"):
        root = Path(base)
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.[ch]*")):
            # src/Runtime.PPCEABI.H is a DIRECTORY that matches the glob
            if not path.is_file() or path.suffix.lower() not in (
                    ".c", ".cpp", ".h"):
                continue
            text = COMMENT_RE.sub(
                lambda match: re.sub(r"[^\n]", " ", match.group(0)),
                path.read_text(encoding="utf-8", errors="replace"))
            for number, line in enumerate(text.splitlines(), 1):
                if cast_only and not CAST_HINT_RE.search(line):
                    continue
                for value, patterns in targets:
                    if any(pattern.search(line) for pattern in patterns):
                        relative = str(path).replace("\\", "/")
                        print(f"{value:#x} {relative}:{number}: "
                              f"{line.strip()[:140]}")
                        total += 1
                        break
    print(f"[{total} hit(s); an include/ hit is the struct authority,"
          " a sibling-TU hit is corroboration to verify]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
