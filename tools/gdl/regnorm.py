#!/usr/bin/env python3
"""Register-normalized residual scan — the mandatory REGISTER_ONLY gate.

Per claim.law.identical-multiset-is-blind-to-displacements: instruction
count, opcode multiset, and `real` can all corroborate "register churn"
while hiding a wrong displacement or operand (a semantic field-selection
bug shipped under exactly that agreement). This tool answers the one
question those scores cannot: which differing words are explainable by a
register renaming, and which are STRUCTURAL?

Run it BEFORE labeling any residual REGISTER_ONLY, and after every
retained edit on such a residual. Two prior workers scripted this by
hand from fndiff --clean; this is that script, made durable.

Usage:
  python tools/gdl/regnorm.py game/game/pmotion get_player_pos
  python tools/gdl/regnorm.py game/enemy/enemy move_logic15 --map

Output per aligned instruction pair:
  SAME        raw-identical (not printed unless --all)
  RENAMING    differs only in register fields — contributes to the map
  STRUCTURAL  differs beyond registers (opcode/displacement/immediate/reloc)
UNPAIRED lines (count mismatch) are always printed. --map prints the
ours->target register tally per register with an INCONSISTENT flag on any
register mapped to more than one target (the web-split tell: a colour
table needs def-site grouping, which a positional tally cannot see —
treat the map as a hint, not a sigma).
"""

import difflib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402

VERSION = "GUNE5D"


def aligned_pairs(target_lines, base_lines):
    """Align on register-erased text so renaming never breaks pairing."""
    t_norm = [fndiff.erase_registers(ln) for ln in target_lines]
    b_norm = [fndiff.erase_registers(ln) for ln in base_lines]
    matcher = difflib.SequenceMatcher(None, t_norm, b_norm, autojunk=False)
    pairs, t_only, b_only = [], [], []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            for k in range(i2 - i1):
                pairs.append((i1 + k, j1 + k))
        else:
            span = min(i2 - i1, j2 - j1)
            for k in range(span):
                pairs.append((i1 + k, j1 + k))
            t_only.extend(range(i1 + span, i2))
            b_only.extend(range(j1 + span, j2))
    return pairs, t_only, b_only


def register_tokens(line):
    return fndiff.REGISTER_RE.findall(line)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    show_map = "--map" in sys.argv
    if len(args) != 2:
        print(__doc__)
        return 2
    unit, fn = args
    unit = unit.replace("\\", "/").removeprefix("src/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target = fndiff.parse(Path(f"build/{VERSION}/obj/{unit}.o"))
    ours = fndiff.parse(Path(f"build/{VERSION}/src/{unit}.o"))
    if fn not in target or fn not in ours:
        print(f"missing: {fn} (target: {fn in target}, ours: {fn in ours})")
        return 1
    t, b = target[fn], ours[fn]
    pairs, t_only, b_only = aligned_pairs(t, b)
    renaming, structural = [], []
    mapping = {}
    # Instruction-relative byte offset per line index (reloc annotation
    # lines share their instruction's offset instead of inflating it).
    t_off, running = [], 0
    for ln in t:
        t_off.append(running)
        if not ln.startswith("    "):
            running += 4
    for ti, bi in pairs:
        if t[ti] == b[bi]:
            if show_all:
                print(f"SAME       {b[bi]}")
            continue
        t_is_reloc = t[ti].startswith("    ")
        b_is_reloc = b[bi].startswith("    ")
        if t_is_reloc and b_is_reloc:
            # A relocation annotation differing only in spelling (pool @N
            # vs lbl_, alias vs resolved address) rides on a register-only
            # change and is NOT structural — the first regnorm banner
            # falsely failed an actually-clean function on exactly this
            # (claim.law.regnorm-counts-reloc-annotation-rows-as-
            # structural). Compare signatures, which normalize all of it.
            if (fndiff.relocation_signature(t[ti].strip())
                    == fndiff.relocation_signature(b[bi].strip())):
                if show_all:
                    print(f"RELOC-SAME {b[bi].strip()}")
                continue
            structural.append((ti, t[ti], b[bi]))
            print(f"STRUCTURAL @{t_off[ti]:#06x}  T {t[ti].strip()}"
                  f"   O {b[bi].strip()}  [reloc target differs]")
            continue
        if fndiff.erase_registers(t[ti]) == fndiff.erase_registers(b[bi]):
            renaming.append((ti, t[ti], b[bi]))
            for ours_reg, tgt_reg in zip(register_tokens(b[bi]),
                                         register_tokens(t[ti])):
                mapping.setdefault(ours_reg, {}).setdefault(tgt_reg, 0)
                mapping[ours_reg][tgt_reg] += 1
            print(f"RENAMING   @{t_off[ti]:#06x}  T {t[ti]}   O {b[bi]}")
        else:
            structural.append((ti, t[ti], b[bi]))
            print(f"STRUCTURAL @{t_off[ti]:#06x}  T {t[ti]}   O {b[bi]}")
    for ti in t_only:
        print(f"UNPAIRED-T @{ti*4:#06x}  {t[ti]}")
    for bi in b_only:
        print(f"UNPAIRED-O @{bi*4:#06x}  {b[bi]}")
    if show_map and mapping:
        print("-- register map (ours -> target, positional tally) --")
        for ours_reg in sorted(mapping, key=lambda r: (r[0], int(r[1:]))
                               if r[1:].isdigit() else (r, 0)):
            targets = mapping[ours_reg]
            flag = "  INCONSISTENT" if len(targets) > 1 else ""
            body = " ".join(f"{tr}x{n}" for tr, n in
                            sorted(targets.items(), key=lambda kv: -kv[1]))
            print(f"  {ours_reg} -> {body}{flag}")
    if not structural and not t_only and not b_only:
        verdict = "EXACT" if not renaming else "CLEAN-RENAMING"
    elif structural:
        verdict = "STRUCTURAL-PRESENT"
    else:
        verdict = "COUNT-MISMATCH"
    print(f"== {fn}: {len(pairs)} paired, {len(renaming)} renaming,"
          f" {len(structural)} STRUCTURAL, {len(t_only)+len(b_only)} unpaired"
          f" -> {verdict}")
    print("VERDICT (repeated):", verdict,
          "-- REGISTER_ONLY label is only honest at 0 STRUCTURAL, 0 unpaired")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
