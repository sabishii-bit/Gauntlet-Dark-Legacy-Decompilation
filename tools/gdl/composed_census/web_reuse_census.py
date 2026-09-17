#!/usr/bin/env python3
"""Find byte-exact functions where ONE callee-saved register holds the SAME
role twice — the positive-control finder for LIVENESS questions.

WHY THIS EXISTS. `shapegrep` slices the target by opcode shape and
`lowmatch`/`nearmiss` slice by score, so neither can answer "which C spelling
makes MWCC keep a variable's two DISJOINT live ranges in ONE callee-saved
register?" Measured on game/ui/message::msgDraw at 1e91f9b1, that question is
the entire residual: `savedregs --per-web` reports 25 of 27 ranges in place
and 2 PERMUTED r29->r25, where the target colours both ranges of `lineHeight`
r29 and ours colours the second r25. Hunting a control for it with
`shapegrep addi,srawi --exact-only` returns `ml_mem::AllocFile`, whose two
hits come from three unrelated `>> 10` shifts on separate variables — an
opcode pattern cannot express a liveness property, so the hunt has to run
over DEFINITIONS grouped by register, which is what this does.

WHAT A HIT MEANS. A byte-exact function is by definition the proven output of
its current source, so a hit is a worked example: register rN holds role R at
two distant offsets, the source that produced it is on disk, and reading it
tells you the spelling. A hit is NOT proof that the same spelling transfers —
`AGENTS.md` requires reproducing the residual before acting on a hypothesis.

Roles are normalized by dropping the DESTINATION register and canonicalizing
the remaining register numbers, so `addi r29,r3,2` and `addi r25,r3,2` carry
the same role and pair across streams. Definitions are instructions whose
FIRST operand is a callee-saved GPR; stores (`st*`), multi-word save/restore
(`stmw`/`lmw`), branches and compares are excluded because their first operand
is a source or a condition register. Known blind spot: update-form stores
(`stwu rS,d(rA)`) also write rA, which this does not count.

Usage (from the repository root):
  python tools/gdl/composed_census/web_reuse_census.py
  python tools/gdl/composed_census/web_reuse_census.py --role addi --gap 0x40
  python tools/gdl/composed_census/web_reuse_census.py --distinct-roles
  python tools/gdl/composed_census/web_reuse_census.py \
      --out build/GUNE5D/web_reuse_census.json
"""
from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
ASM_DIR = REPO_ROOT / "build" / "GUNE5D" / "asm"
REPORT = REPO_ROOT / "build" / "GUNE5D" / "report.json"

FN_RE = re.compile(r"^\.fn\s+(\w+)")
ENDFN_RE = re.compile(r"^\.endfn")
INSN_RE = re.compile(r"^/\* (\w{8}) (\w{8})  (?:\w\w ){4}\*/\t(\S+)\s*(.*)$")
REG_RE = re.compile(r"\br(\d{1,2})\b")

# MWCC hands callee-saved GPRs out downward from r31; r13 is the small-data
# base and r14 upward are the callee-saved bank on this ABI.
CALLEE_SAVED = {f"r{n}" for n in range(14, 32)}

# First operand is a SOURCE (or a condition register), not a definition.
NON_DEFINING_PREFIXES = ("st", "b", "cmp", "mt", "lmw", "dcb", "icb", "sync",
                         "twi", "tw", "eieio", "isync", "rfi", "sc")


def is_definition(opcode: str, operands: str) -> str | None:
    """Return the callee-saved register this instruction defines, or None."""
    op = opcode.lower().rstrip(".")
    if op.startswith(NON_DEFINING_PREFIXES):
        return None
    first = operands.split(",")[0].strip()
    return first if first in CALLEE_SAVED else None


def role_of(opcode: str, operands: str) -> str:
    """Signature of what a definition COMPUTES, destination register dropped.

    Remaining register numbers are canonicalized in order of appearance so
    two streams that differ only by which register was chosen still pair.
    """
    parts = [p.strip() for p in operands.split(",")]
    rest = ",".join(parts[1:]) if len(parts) > 1 else ""
    mapping: dict[str, str] = {}

    def canon(m: re.Match) -> str:
        reg = m.group(0)
        if reg not in mapping:
            mapping[reg] = f"%{len(mapping)}"
        return mapping[reg]

    return f"{opcode.lower()} {REG_RE.sub(canon, rest)}".strip()


def load_scores() -> dict[str, float]:
    if not REPORT.exists():
        return {}
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    scores = {}
    for unit in report.get("units", []):
        for fn in unit.get("functions", []):
            scores[fn["name"]] = float(fn.get("fuzzy_match_percent", 0.0))
    return scores


def scan_file(path: Path):
    """Yield (fn_name, [(offset, register, role, text), ...]) per function."""
    fn_name = None
    defs: list[tuple[int, str, str, str]] = []
    base = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = FN_RE.match(line)
        if m:
            if fn_name is not None:
                yield fn_name, defs
            fn_name, defs, base = m.group(1), [], None
            continue
        if ENDFN_RE.match(line):
            if fn_name is not None:
                yield fn_name, defs
            fn_name, defs, base = None, [], None
            continue
        if fn_name is None:
            continue
        m = INSN_RE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if base is None:
            base = addr
        opcode, operands = m.group(3), m.group(4)
        reg = is_definition(opcode, operands)
        if reg:
            defs.append((addr - base, reg, role_of(opcode, operands),
                         f"{opcode} {operands}".strip()))
    if fn_name is not None:
        yield fn_name, defs


def census(min_fuzzy: float, gap: int, role_filter: str | None,
           distinct: bool, grep: str | None):
    scores = load_scores()
    rows = []
    for path in sorted(ASM_DIR.rglob("*.s")):
        for fn_name, defs in scan_file(path):
            if not defs:
                continue
            if grep and grep.lower() not in fn_name.lower():
                continue
            fuzzy = scores.get(fn_name)
            if min_fuzzy is not None:
                if fuzzy is None or fuzzy < min_fuzzy:
                    continue
            by_reg: dict[str, list[tuple[int, str, str]]] = defaultdict(list)
            for off, reg, role, text in defs:
                by_reg[reg].append((off, role, text))
            for reg, entries in sorted(by_reg.items()):
                if len(entries) < 2:
                    continue
                groups: dict[str, list[tuple[int, str]]] = defaultdict(list)
                for off, role, text in entries:
                    groups[role].append((off, text))
                if distinct:
                    if len(groups) < 2:
                        continue
                    offs = sorted(e[0] for e in entries)
                    if offs[-1] - offs[0] < gap:
                        continue
                    rows.append({
                        "function": fn_name, "file": path.name,
                        "unit": str(path.relative_to(ASM_DIR)),
                        "fuzzy": fuzzy, "register": reg, "kind": "distinct",
                        "roles": sorted(groups), "offsets": offs,
                    })
                    continue
                for role, hits in sorted(groups.items()):
                    if len(hits) < 2:
                        continue
                    if role_filter and not role.startswith(role_filter):
                        continue
                    offs = sorted(o for o, _ in hits)
                    if offs[-1] - offs[0] < gap:
                        continue
                    rows.append({
                        "function": fn_name, "file": path.name,
                        "unit": str(path.relative_to(ASM_DIR)),
                        "fuzzy": fuzzy, "register": reg, "kind": "repeated",
                        "role": role, "offsets": offs,
                        "text": hits[0][1], "count": len(hits),
                    })
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min-fuzzy", type=float, default=100.0,
                    help="only functions at or above this fuzzy score "
                         "(default 100.0 = byte-exact positive controls)")
    ap.add_argument("--any-fuzzy", action="store_true",
                    help="do not filter by score at all")
    ap.add_argument("--gap", type=lambda s: int(s, 0), default=0x20,
                    help="minimum byte distance between the two definitions "
                         "(default 0x20); filters adjacent redefinition")
    ap.add_argument("--role", help="only roles whose signature starts with this")
    ap.add_argument("--distinct-roles", action="store_true",
                    help="instead report registers hosting two DIFFERENT "
                         "roles (a register reused across variables)")
    ap.add_argument("--grep", help="substring filter on function name")
    ap.add_argument("--limit", type=int, default=40)
    ap.add_argument("--out", help="write all rows as JSON here")
    args = ap.parse_args()

    if not ASM_DIR.exists():
        print(f"web_reuse_census: {ASM_DIR} missing — run a full ninja first")
        return 2

    rows = census(None if args.any_fuzzy else args.min_fuzzy,
                  args.gap, args.role, args.distinct_roles, args.grep)
    if not rows:
        print("no hits (try --any-fuzzy, a smaller --gap, or --distinct-roles)")
        return 0

    rows.sort(key=lambda r: (-(r["offsets"][-1] - r["offsets"][0]),
                             r["function"]))
    kind = "distinct roles" if args.distinct_roles else "repeated role"
    print(f"{len(rows)} hit(s), {kind}, gap >= 0x{args.gap:X}"
          + ("" if args.any_fuzzy else f", fuzzy >= {args.min_fuzzy}"))
    print(f"{'function':34} {'reg':4} {'spread':>7} {'offsets':22} role")
    print("-" * 104)
    for r in rows[:args.limit]:
        offs = ",".join(f"0x{o:X}" for o in r["offsets"][:4])
        spread = r["offsets"][-1] - r["offsets"][0]
        role = r.get("role") or " | ".join(r.get("roles", []))[:44]
        print(f"{r['function']:34} {r['register']:4} 0x{spread:<5X} "
              f"{offs:22} {role}")
    if len(rows) > args.limit:
        print(f"... ({len(rows) - args.limit} more; raise --limit)")
    print("\nread the SOURCE of a hit: its unit is named in the `unit` column "
          "of --out, or src/<unit with .s -> .c>")
    if args.out:
        Path(args.out).write_text(json.dumps(rows, indent=1))
        print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
