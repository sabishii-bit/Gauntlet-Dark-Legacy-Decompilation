#!/usr/bin/env python3
"""Compact target-asm dump for one function, from the dtk-extracted object.

Usage:
  python tools/gdl/fnasm.py game/pb_window pbProjCalc          # whole function
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 40:120   # insn index slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 0x68:0xa0  # offset slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc --ours   # OUR built object
  python tools/gdl/fnasm.py game/pb_window pbProjCalc --diff   # target|ours aligned
  python tools/gdl/fnasm.py game/sys/sysservice sysClearFlags --raw --diff
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

POSTPROCESSOR TRAP (why --raw exists): build/GUNE5D/src/<unit>.o is the
POSTPROCESSED object for every unit listed in config/GUNE5D/webfrank.json or
p6frank.json.  A pinned function's postprocessed body is byte-identical to
the target by construction, so `--ours`/`--diff` on one compares the target
against a copy of itself and prints an all-`=` view that looks like a perfect
match and says nothing about the compiler's actual output.  --raw reads the
pre-postprocess object instead (.postprocess/frank/<unit>.o when a Frank
profile stage runs, else .postprocess/body/<unit>.o) — that is the stream a
source edit can move.  Whenever the postprocessed view is used on a pinned
function, a PINNED banner is printed naming the rule and pointing at --raw.

OBJDUMP is resolved to an absolute path so this works from any cwd/script.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402  (stale-object marker: one owner, one spelling)

VERSION = "GUNE5D"
ROOT = Path(__file__).resolve().parents[2]
OBJDUMP = ROOT / "build" / "binutils" / "powerpc-eabi-objdump.exe"


def pinned_functions(unit, *, root=None):
    """Map function name -> postprocessor rule for `unit`'s pinned functions.

    Reads the two postprocessor configs directly; a missing or malformed
    config yields {} rather than raising, because this is a WARNING path and
    must never take the dump down with it.
    """
    root = Path(root) if root is not None else ROOT
    pins = {}
    for rule, name in (("webfrank", "webfrank.json"),
                       ("p6frank", "p6frank.json")):
        try:
            cfg = json.loads(
                (root / "config" / VERSION / name).read_text(encoding="utf-8"))
            entry = cfg.get("units", {}).get(unit)
        except Exception:
            continue
        if entry is None:
            continue
        # webfrank stores a LIST of rules per unit; p6frank a single dict.
        for item in (entry if isinstance(entry, list) else [entry]):
            fn = isinstance(item, dict) and item.get("function")
            if fn:
                pins[fn] = rule
    return pins


def pin_warning(unit, fn, kind, *, root=None):
    """Banner text when `kind` is the postprocessed view of a pinned fn."""
    if kind != "ours" or not fn:
        return None
    rule = pinned_functions(unit, root=root).get(fn)
    if rule is None:
        return None
    return (f"!! PINNED: {fn} is {rule}-pinned in config/{VERSION}/"
            f"{rule}.json — build/{VERSION}/src/{unit}.o is the POSTPROCESSED\n"
            f"!! object, byte-identical to the target here by construction. "
            f"This view is target-vs-target\n"
            f"!! and cannot show a source-level residual. Re-run with --raw "
            f"to read the compiler output.")


def raw_obj_path(unit, *, root=None):
    """Pre-postprocess compiler object for `unit`, or None if not staged."""
    root = Path(root) if root is not None else ROOT
    src = root / "build" / VERSION / "src" / f"{unit}.o"
    # frank runs before the object postprocessor when both are configured,
    # so its output is the postprocessor's actual input.
    for stage in ("frank", "body"):
        cand = src.parent / ".postprocess" / stage / src.name
        if cand.exists():
            return cand
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    raw = "--raw" in sys.argv
    ours = "--ours" in sys.argv or raw
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
    # The view actually read for the right-hand/only stream.
    # --diff reads the `ours` object too, even without --ours.
    our_kind = ("raw" if raw else "ours") if (ours or diff) else "target"
    warn = pin_warning(unit, fn, our_kind)
    if warn:
        # stdout, not stderr: the failure mode this closes is "the reader
        # piped the dump and never saw the warning".
        print(warn)
    rows, names, err = parse_fn(unit, fn, ours=ours and not diff, raw=raw)
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
        our_rows, _, our_err = parse_fn(unit, fn, ours=True, raw=raw)
        if our_err:
            print(our_err)
            return 1
        return diff_view(rows, our_rows, lo, hi, by_offset, raw=raw)
    for i, (off, ins) in enumerate(rows):
        key = off if by_offset else i
        if lo <= key < hi:
            print(f"{off:4x}: {ins}")
    label = f" ({'raw, pre-postprocess' if raw else 'ours'})" if ours else ""
    print(f"[{len(rows)} insns{label}]")
    return 0


def parse_fn(unit, fn, *, ours, raw=False):
    kind = "src" if ours else "obj"
    obj = Path(f"build/{VERSION}/{kind}/{unit}.o")
    if ours and raw:
        pre = raw_obj_path(unit)
        if pre is None:
            return [], [], (
                f"--raw: {unit} has no .postprocess stage — it is not "
                f"WebFrank/P6Frank postprocessed, so build/{VERSION}/src/"
                f"{unit}.o IS the raw compiler output; drop --raw")
        obj = pre
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
    # A postprocessor refusal leaves the PREVIOUS object here (run-35 item
    # 4). Reading assembly out of it is the quietest possible wrong answer:
    # the listing is well-formed, it just is not the source in your tree.
    stale = fndiff.stale_object_warning(obj)
    if stale:
        print(stale, file=sys.stderr)
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


def diff_view(target_rows, our_rows, lo, hi, by_offset, raw=False):
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
    right_head = ("O = OURS (raw, pre-postprocess)" if raw
                  else "O = OURS (compiled)")
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
          f"{' RAW' if raw else ''}"
          f" ({len(our_rows)} insns); {shown} rows shown;"
          " = same  ~ opcode-only match  | replaced"
          "  < target-only  > ours-only]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
