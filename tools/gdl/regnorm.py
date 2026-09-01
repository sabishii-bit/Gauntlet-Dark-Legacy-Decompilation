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
  python tools/gdl/regnorm.py game/mb/mb_particle      # TU census mode:
      one summary row per common function (slow-ish: re-parses per fn)

Output per aligned instruction pair:
  SAME        raw-identical (not printed unless --all)
  RENAMING    differs only in register fields — contributes to the map
  STRUCTURAL  differs beyond registers (opcode/displacement/immediate/reloc)
UNPAIRED lines (count mismatch) are always printed. --map prints the
ours->target register tally per register with an INCONSISTENT flag on any
register mapped to more than one target (the web-split tell: a colour
table needs def-site grouping, which a positional tally cannot see —
treat the map as a hint, not a sigma).

Offsets: @0xNN on paired/T rows is the TARGET function-relative byte
offset; on UNPAIRED-O rows it is OURS-relative. Both skip reloc
annotation lines. CLEAN-RENAMING is necessary, not sufficient, for a
recolor rule: webfrank_audit can still reject on a per-web
inconsistency the positional tally cannot see (live case: 0/0 verdict
with an f0/f2 FPR conflict) — always run the audit before authoring.
"""

import difflib
import re
import sys
from collections import Counter
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
    if len(args) not in (1, 2):
        print(__doc__)
        return 2
    if len(args) == 1:
        # TU census mode: one summary row per common function. Two runs
        # of briefs ordered a "regnorm census first" that the tool could
        # not do; workers substituted hand-rolled loops.
        import subprocess
        unit = args[0]
        bare = re.sub(r"\.(c|cpp)$", "",
                      unit.replace("\\", "/").removeprefix("src/"))
        t_tab = fndiff.parse(Path(f"build/{VERSION}/obj/{bare}.o"))
        o_tab = fndiff.parse(Path(f"build/{VERSION}/src/{bare}.o"))
        common = sorted(set(t_tab) & set(o_tab))
        print(f"-- regnorm census: {bare} ({len(common)} paired"
              " functions; rank by GENUINE structural rows, then"
              " unpaired — real inverts tractability) --")
        for name in common:
            out = subprocess.run(
                [sys.executable, __file__, bare, name],
                capture_output=True, text=True).stdout
            for line in out.splitlines():
                if line.startswith("== "):
                    print(line)
        return 0
    unit, fn = args
    unit = unit.replace("\\", "/").removeprefix("src/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target = fndiff.parse(Path(f"build/{VERSION}/obj/{unit}.o"))
    ours = fndiff.parse(Path(f"build/{VERSION}/src/{unit}.o"))

    def resolve(table, name):
        # allow a dtk _80XXXXXX suffix on either side (suffix mismatches
        # made this tool unavailable for exactly its roster's functions)
        if name in table:
            return name
        for cand in table:
            if cand.startswith(name + "_80") or name.startswith(cand + "_80"):
                return cand
        return None

    fn_t, fn_o = resolve(target, fn), resolve(ours, fn)
    if fn_t is None or fn_o is None:
        print(f"missing: {fn} (target: {fn_t is not None},"
              f" ours: {fn_o is not None})")
        return 1
    t, b = target[fn_t], ours[fn_o]
    pairs, t_only, b_only = aligned_pairs(t, b)
    renaming, structural = [], []
    disp_artifacts = []
    reloc_artifacts = []
    mapping = {}
    # Instruction-relative byte offset per line index (reloc annotation
    # lines share their instruction's offset instead of inflating it).
    t_off, running = [], 0
    for ln in t:
        t_off.append(running)
        if not ln.startswith("    "):
            running += 4
    # Ours-side table too: UNPAIRED rows used to print line_index*4, which
    # counts reloc annotation lines as instructions — a worker pasted those
    # "offsets" into fnasm and landed in an unrelated region.
    b_off, running = [], 0
    for ln in b:
        b_off.append(running)
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
            t_txt, b_txt = t[ti].strip(), b[bi].strip()
            pool = re.compile(r"@@?\d+")
            named = re.compile(r"(lbl_|jumptable_)[0-9A-Fa-f]+")
            if ((pool.search(t_txt) and named.search(b_txt))
                    or (named.search(t_txt) and pool.search(b_txt))):
                # @NNNN on one side vs a splitter-invented lbl_/jumptable_
                # name on the other is usually the SAME constant spelled
                # two ways (a lane hand-classified 4 of 5 "genuine" rows
                # as exactly this). Verify the constants once per TU, not
                # per row.
                structural.append((ti, t[ti], b[bi]))
                reloc_artifacts.append(ti)
                print(f"STRUCT-RELOC-NAMING @{t_off[ti]:#06x}  T {t_txt}"
                      f"   O {b_txt}  [pool-vs-named spelling — likely"
                      " the same constant; verify once per TU]")
                continue
            structural.append((ti, t[ti], b[bi]))
            print(f"STRUCTURAL @{t_off[ti]:#06x}  T {t_txt}"
                  f"   O {b_txt}  [reloc target differs]")
            continue
        t_op = t[ti].split()[0] if not t_is_reloc else ""
        b_op = b[bi].split()[0] if not b_is_reloc else ""
        if t_op != b_op and Counter(
                ln.split()[0] for ln in t if not ln.startswith("    ")) == \
                Counter(ln.split()[0] for ln in b if not ln.startswith("    ")):
            # Fabricated-row guard: when the opcode MULTISETS are identical,
            # a paired row with differing opcodes is arithmetically an
            # alignment artifact, not a real opcode change — a whole record
            # batch inherited such rows as "codegen-form gaps".
            structural.append((ti, t[ti], b[bi]))
            print(f"STRUCTURAL @{t_off[ti]:#06x}  T {t[ti]}   O {b[bi]}"
                  "  [ALIGNMENT ARTIFACT? multisets identical — confirm"
                  " against the aligned fnasm --diff before believing]")
            continue
        t_stripbr = re.sub(r"^(b[a-z+.]*)\s+\S+", r"\1", t[ti])
        b_stripbr = re.sub(r"^(b[a-z+.]*)\s+\S+", r"\1", b[bi])
        if (t_op == b_op and t_op.startswith("b") and t_op != "bl"
                and (t_only or b_only) and t_stripbr == b_stripbr
                and t[ti] != b[bi]):
            # Branch differing ONLY in its displacement, in a function
            # with a count delta: the shifted code moves every branch
            # target, so a 1-insn delta manufactures a flood of these
            # artifact rows that swamps the genuine signal (a lane
            # measured 19 artifacts hiding 1 real row).
            structural.append((ti, t[ti], b[bi]))
            disp_artifacts.append(ti)
            print(f"STRUCT-BRANCH-DISP @{t_off[ti]:#06x}  T {t[ti]}   O"
                  f" {b[bi]}  [displacement-only + count delta ->"
                  " alignment artifact, usually not real]")
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
        print(f"UNPAIRED-T @{t_off[ti]:#06x}  {t[ti]}")
    for bi in b_only:
        print(f"UNPAIRED-O @{b_off[bi]:#06x}  {b[bi]}")
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
    art_bits = []
    if disp_artifacts:
        art_bits.append(f"{len(disp_artifacts)} branch-displacement")
    if reloc_artifacts:
        art_bits.append(f"{len(reloc_artifacts)} reloc-naming")
    art = (f" ({' + '.join(art_bits)} artifacts — read the genuine"
           " rows first)" if art_bits else "")
    print(f"== {fn}: {len(pairs)} paired, {len(renaming)} renaming,"
          f" {len(structural)} STRUCTURAL{art},"
          f" {len(t_only)+len(b_only)} unpaired"
          f" -> {verdict}")
    print("VERDICT (repeated):", verdict,
          "-- REGISTER_ONLY label is only honest at 0 STRUCTURAL, 0 unpaired")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
