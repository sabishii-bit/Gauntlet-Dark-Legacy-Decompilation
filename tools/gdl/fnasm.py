#!/usr/bin/env python3
"""Compact target-asm dump for one function, from the dtk-extracted object.

Usage:
  python tools/gdl/fnasm.py game/pb_window pbProjCalc          # whole function
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 40:120   # insn index slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 0x68:0xa0  # offset slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc --ours   # OUR built object
  python tools/gdl/fnasm.py game/pb_window pbProjCalc --diff   # target|ours aligned
  python tools/gdl/fnasm.py game/pb_window                     # list functions

--diff prints target and ours side-by-side, sequence-aligned on the opcode
stream (the same correspondence fndiff --ops uses) — immune to the
upstream-drift trap of comparing both dumps at the same absolute offset.
The columns are labelled: LEFT is T = TARGET (retail), RIGHT is O = OURS
(compiled). Reading the view backwards inverts every conclusion drawn
from it, so the header names both sides and the footer repeats them.

Output is one line per instruction: function-relative hex offset, mnemonic,
operands, with any relocation folded onto the same line ("@sym"). Branch
targets stay as real function-relative offsets so label adjacency can be
verified (bge-to-next vs bge-over-one is visible at a glance).

Slices with a 0x prefix are byte-offset ranges (matching the printed offsets
and fndiff --ops @0x annotations); bare numbers are instruction indices.

Default reads build/GUNE5D/obj/<unit>.o (the dtk-extracted target), so it
works before any source exists and is immune to stale-object issues.
--ours reads build/GUNE5D/src/<unit>.o (our compile) instead — run ninja on
the object first or the dump is stale.

OBJDUMP is resolved to an absolute path so this works from any cwd/script.
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = (Path(__file__).resolve().parents[2]
           / "build" / "binutils" / "powerpc-eabi-objdump.exe")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ours = "--ours" in sys.argv
    if not args or args[0] in ("--help", "-h", "help"):
        print(__doc__)
        return 1
    unit = args[0].replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    fn = args[1] if len(args) > 1 else None
    lo, hi = 0, 1 << 30
    by_offset = False
    if len(args) > 2 and ":" in args[2]:
        a, b = args[2].split(":")
        by_offset = a.startswith("0x") or b.startswith("0x")
        conv = (lambda s, d: int(s, 16) if s else d) if by_offset \
            else (lambda s, d: int(s) if s else d)
        lo, hi = conv(a, 0), conv(b, 1 << 30)

    diff = "--diff" in sys.argv
    rows, names, err = parse_fn(unit, fn, ours=ours and not diff)
    if err:
        print(err)
        return 1
    if fn is None:
        print("\n".join(names))
        return 0
    if not rows:
        print(f"function {fn} not found; has: {', '.join(names)}")
        return 1
    if diff:
        our_rows, _, our_err = parse_fn(unit, fn, ours=True)
        if our_err:
            print(our_err)
            return 1
        return diff_view(rows, our_rows, lo, hi, by_offset)
    for i, (off, ins) in enumerate(rows):
        key = off if by_offset else i
        if lo <= key < hi:
            print(f"{off:4x}: {ins}")
    print(f"[{len(rows)} insns{' (ours)' if ours else ''}]")
    return 0


def parse_fn(unit, fn, *, ours):
    kind = "src" if ours else "obj"
    obj = Path(f"build/{VERSION}/{kind}/{unit}.o")
    if not obj.exists() and not ours:
        # dtk merges runs of tiny fns into auto_03_* objects and names auto
        # units after their first fn; try the common variants before giving up
        for cand in (f"auto_03_{unit}_text", f"auto_{unit}_text",
                     f"auto_03_{unit.replace('fn_', '')}_text"):
            c = Path(f"build/{VERSION}/obj/{cand}.o")
            if c.exists():
                obj = c
                break
    if not obj.exists():
        hint = (f"run ninja build/{VERSION}/src/{unit}.o first" if ours
                else "run ninja once so dtk extracts it")
        return [], [], f"missing {obj} ({hint})"
    out = subprocess.run([str(OBJDUMP), "-dr", str(obj)],
                         capture_output=True, text=True).stdout
    cur = None
    base = 0
    rows = []      # (offset, text) for the selected function
    names = []
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur = m.group(2)
            base = int(m.group(1), 16)
            names.append(cur)
            continue
        if cur != fn:
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line)
        if m:
            off = int(m.group(1), 16) - base
            ins = m.group(2).strip()
            # rewrite absolute branch targets as function-relative offsets
            bm = re.search(r"\b([0-9a-f]+) <[^>]*>\s*$", ins)
            if bm:
                tgt = int(bm.group(1), 16) - base
                ins = ins[: bm.start()] + f"->{tgt:x}"
            ins = re.sub(r"\s+", " ", ins)
            rows.append([off, ins])
        elif "R_PPC" in line and rows:
            parts = line.strip().split()
            rows[-1][1] += f"  @{parts[-1]}({parts[-2].replace('R_PPC_', '')})"
    return rows, names, None


def diff_view(target_rows, our_rows, lo, hi, by_offset):
    """Aligned side-by-side target/ours view.

    Alignment uses opcode-stream sequence matching (the same correspondence
    fndiff --ops reports), so upstream instruction-count drift cannot point
    the reader at the wrong window — the trap of comparing both streams at
    the same absolute offset by hand. `=` rows agree on the full instruction
    text, `~` rows agree on opcode only (register/operand/reloc delta), and
    `<`/`>` rows exist on one side only.
    """
    import difflib
    t_ops = [row[1].split()[0] for row in target_rows]
    o_ops = [row[1].split()[0] for row in our_rows]
    sm = difflib.SequenceMatcher(None, t_ops, o_ops, autojunk=False)
    width = max((len(row[1]) for row in target_rows), default=20)
    width = min(width, 52)
    shown = 0
    # Label the columns. Which side was which was inferable only from the
    # trailing legend, and reading the view backwards inverts every
    # conclusion drawn from it.
    left_head = "T = TARGET (retail)"
    right_head = "O = OURS (compiled)"
    print(f"{left_head:<{width + 6}}   {right_head}")
    print(f"{'-' * min(len(left_head), width + 6):<{width + 6}}   "
          f"{'-' * len(right_head)}")
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        span = max(i2 - i1, j2 - j1)
        for k in range(span):
            ti = i1 + k if i1 + k < i2 else None
            oi = j1 + k if j1 + k < j2 else None
            key = (target_rows[ti][0] if ti is not None
                   else our_rows[oi][0]) if by_offset else (ti if ti is not None else i2)
            if not (lo <= key < hi):
                continue
            left = (f"{target_rows[ti][0]:4x}: {target_rows[ti][1]}"
                    if ti is not None else "")
            right = (f"{our_rows[oi][0]:4x}: {our_rows[oi][1]}"
                     if oi is not None else "")
            if ti is None:
                mark = ">"
            elif oi is None:
                mark = "<"
            elif tag == "equal":
                mark = "=" if target_rows[ti][1] == our_rows[oi][1] else "~"
            else:
                mark = "|"
            print(f"{left:<{width + 6}} {mark} {right}")
            shown += 1
    print(f"[LEFT = TARGET ({len(target_rows)} insns) / RIGHT = OURS"
          f" ({len(our_rows)} insns); {shown} rows shown;"
          " = same  ~ opcode-only match  | replaced"
          "  < target-only  > ours-only]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
