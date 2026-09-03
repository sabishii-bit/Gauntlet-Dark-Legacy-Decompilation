#!/usr/bin/env python3
"""ES run-41 sweep: functions that READ an `extern f32/f64 lbl_` scaffold
symbol, joined with their measured object-diff `real`, ranked ascending.

The run-40 law claim.law.NM_extern-scaffold-float-steals-the-low-callee-saved
-fpr-from-the-generated-conversion-constants closed adsInitFromHeader
byte-exact by deleting two such scaffold locals.  NM's screen only covered
the PURE symptom (every differing line is an FPR renaming).  This sweep
covers the MIXED symptom: scaffold read present + ANY residual.

Output: --out JSON (default build/GUNE5D/es_extern_sweep.json) + a ranked
table on stdout.

Run from the repository root.
"""

import argparse
import difflib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
from fndiff import (count_real, instruction_lines, normalized_reloc_lines,  # noqa: E402
                    opcodes, parse, relocation_symbols)

FPR_RE = re.compile(r"\bf(\d{1,2})\b")


def fp_shape(t_lines, b_lines):
    """(multiset_identical, fp_only_divergence, n_diff_insns).

    `fp_only_divergence`: every differing instruction pair has the same
    mnemonic and the same operands once FPR numbers are erased — i.e. the
    residual is a pure floating-register RECOLOUR, the shape the run-40 law
    (and its 20260831 companion) both close by literalising the scaffold.
    """
    ti, bi = instruction_lines(t_lines), instruction_lines(b_lines)
    ident = sorted(opcodes(ti)) == sorted(opcodes(bi))
    if len(ti) != len(bi):
        return ident, False, None
    diff = [(a, b) for a, b in zip(ti, bi) if a != b]
    if not diff:
        return ident, False, 0
    fp_only = all(FPR_RE.sub("f#", a) == FPR_RE.sub("f#", b) for a, b in diff)
    return ident, fp_only, len(diff)


POOL_SYM_RE = re.compile(r"^(lbl_[0-9A-Fa-f]{6,8}|@\d+|@@\d+)([+-].*)?$")


def pool_kind_rows(t_rows, b_rows):
    """Positional pool-symbol mismatches: the run-40 STRICT mechanism.

    fndiff's set delta keys `lbl_80349318` and `@273` both to `<local>`, and
    --clean normalizes both to `<local>` text, so a function whose ONLY
    residual is "target names its own sdata2 pool entry where we emit an
    anonymous compiler constant (or a DIFFERENT named one)" reads as
    real 0 / MATCH in every existing view.  That was the whole mechanism of
    adsInitFromHeader's run-40 byte-exact close.  Compare POSITIONALLY.
    """
    if len(t_rows) != len(b_rows):
        return None  # counts disagree: not a pure pool-naming question
    rows = []
    for i, ((tt, ts), (bt, bs)) in enumerate(zip(t_rows, b_rows)):
        if tt != bt:
            return None
        ts, bs = ts.strip(), bs.strip()
        if ts == bs:
            continue
        if POOL_SYM_RE.match(ts) and POOL_SYM_RE.match(bs):
            rows.append((i, tt, ts, bs))
    return rows


VERSION = "GUNE5D"
REPORT = REPO / "build" / VERSION / "report.json"

EXTERN_RE = re.compile(r"^\s*extern\s+(f32|f64)\s+(lbl_[0-9A-Fa-f]{8})\s*;")
LABEL_RE = re.compile(r"\b(lbl_[0-9A-Fa-f]{8})\b")
# a function definition head: identifier '(' ... at brace depth 0
HEAD_RE = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)\s*\(")

SKIP_KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "else",
                 "do", "static", "extern", "typedef", "struct", "union",
                 "enum", "case", "default"}


def strip_comments(text):
    """Blank out comments and string/char literals, preserving line count."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            q = c
            j = i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_spans(text):
    """[(name, body_text)] for top-level function definitions."""
    src = strip_comments(text)
    spans = []
    depth = 0
    i, n = 0, len(src)
    stmt_start = 0
    while i < n:
        c = src[i]
        if c == "{":
            if depth == 0:
                head = src[stmt_start:i]
                # a function head ends with ')' (possibly K&R decls after)
                name = None
                if ")" in head:
                    # last identifier before the FIRST '(' of the head's tail
                    cands = HEAD_RE.findall(head)
                    for cand in reversed(cands):
                        if cand not in SKIP_KEYWORDS:
                            name = cand
                            break
                    # take the FIRST plausible identifier-with-paren instead:
                    # `void Foo(struct Bar *b)` -> Foo, not Bar
                    for cand in cands:
                        if cand not in SKIP_KEYWORDS:
                            name = cand
                            break
                if name and "=" not in head.split("(")[0]:
                    body_start = i
                    d2 = 0
                    j = i
                    while j < n:
                        if src[j] == "{":
                            d2 += 1
                        elif src[j] == "}":
                            d2 -= 1
                            if d2 == 0:
                                break
                        j += 1
                    spans.append((name, src[body_start:j + 1]))
                    i = j + 1
                    stmt_start = i
                    continue
            depth += 1
        elif c == "}":
            depth -= 1
            if depth <= 0:
                depth = 0
                stmt_start = i + 1
        elif c == ";" and depth == 0:
            stmt_start = i + 1
        i += 1
    return spans


def poolval(tu_path):
    r = subprocess.run([sys.executable, str(REPO / "tools" / "gdl" / "poolval.py"),
                        "--sweep", str(tu_path), "--json"],
                       cwd=str(REPO), capture_output=True, text=True)
    if r.returncode:
        return {}
    try:
        data = json.loads(r.stdout)
    except json.JSONDecodeError:
        return {}
    return {row["name"]: row for row in data.get("labels", [])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(REPO / "build" / VERSION / "es_extern_sweep.json"))
    ap.add_argument("--max-real", type=int, default=0,
                    help="print only rows with real <= N (0 = no limit)")
    args = ap.parse_args()

    # 1. per-unit fuzzy from report.json (skip complete/linked units)
    report = json.loads(REPORT.read_text())
    fuzzy = {}
    incomplete = set()
    for u in report.get("units", []):
        unit = u.get("name", "").removeprefix("main/")
        if u.get("metadata", {}).get("complete"):
            continue
        incomplete.add(unit)
        for f in u.get("functions", []):
            fuzzy[(unit, f.get("name"))] = (f.get("fuzzy_match_percent", 0.0),
                                            int(f.get("size", 0) or 0))

    # 2. scaffold declarations per TU
    rows = []
    tu_summary = []
    for src_file in sorted((REPO / "src").rglob("*.c")):
        text = src_file.read_text(errors="replace")
        decls = {m.group(2): m.group(1) for m in
                 (EXTERN_RE.match(line) for line in text.splitlines()) if m}
        if not decls:
            continue
        unit = src_file.relative_to(REPO / "src").as_posix()[:-2]
        if unit not in incomplete:
            tu_summary.append({"unit": unit, "decls": len(decls),
                               "state": "Matching/linked (skipped)"})
            continue

        vals = poolval(src_file)
        pool = {k: v for k, v in vals.items() if v.get("disposition") == "POOL"}

        # 3. per-function label reads
        target_obj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
        base_obj = REPO / "build" / VERSION / "src" / f"{unit}.o"
        tfns = bfns = trel = brel = None
        if target_obj.exists() and base_obj.exists():
            tfns, bfns = parse(target_obj), parse(base_obj)
            trel, brel = relocation_symbols(target_obj), relocation_symbols(base_obj)

        n_rows = 0
        for name, body in function_spans(text):
            used = sorted(set(LABEL_RE.findall(body)) & set(decls))
            if not used:
                continue
            used_pool = [u for u in used if u in pool]
            if not used_pool:
                continue
            pct, size = fuzzy.get((unit, name), (None, 0))
            real = real_clean = None
            pool_rows = []
            if tfns is not None and name in tfns and name in bfns:
                clean = difflib.unified_diff(normalized_reloc_lines(tfns[name]),
                                             normalized_reloc_lines(bfns[name]),
                                             lineterm="", n=0)
                real_clean = sum(1 for line in clean
                                 if line[:1] in "+-"
                                 and not line.startswith(("+++", "---")))
                raw = [line for line in
                       difflib.unified_diff(tfns[name], bfns[name],
                                            lineterm="", n=0)
                       if line[:1] in "+-" and not line.startswith(("+++", "---"))]
                real = count_real(raw)
                pool_rows = pool_kind_rows(trel.get(name, []),
                                           brel.get(name, [])) or []
                named = [p for p in pool_rows
                         if p[2].startswith("lbl_") and p[3].startswith("lbl_")]
                t_named = {s.strip() for _, s in trel.get(name, [])
                           if s.strip().startswith("lbl_")}
                b_named = {s.strip() for _, s in brel.get(name, [])
                           if s.strip().startswith("lbl_")}
                ours_only_named = sorted(b_named - t_named)
                ident, fp_only, n_diff = fp_shape(tfns[name], bfns[name])
            if real is None:
                continue
            if real == 0 and not named and not ours_only_named:
                continue
            rows.append({
                "unit": unit, "function": name, "real": real,
                "multiset_identical": ident, "fp_only": fp_only,
                "n_diff_insns": n_diff,
                "named_mismatch": len(named),
                "ours_only_named": ours_only_named,
                "named_rows": [{"idx": i, "type": t, "target": ts, "ours": bs}
                               for i, t, ts, bs in named],
                "real_clean": real_clean, "pool_kind": len(pool_rows),
                "pool_rows": [{"idx": i, "type": t, "target": ts, "ours": bs}
                              for i, t, ts, bs in pool_rows],
                "fuzzy": pct, "size": size,
                "labels": [{"name": u, "value": pool[u].get("f64") if decls[u] == "f64"
                            else pool[u].get("f32"),
                            "type": decls[u], "addr": pool[u]["addr"],
                            "section": pool[u]["section"]} for u in used_pool],
            })
            n_rows += 1
        tu_summary.append({"unit": unit, "decls": len(decls),
                           "pool_decls": len(pool), "candidate_fns": n_rows,
                           "state": "NonMatching"})

    rows.sort(key=lambda r: (r["real"], r["unit"], r["function"]))
    out = {"rows": rows, "tus": tu_summary, "n_rows": len(rows)}
    Path(args.out).write_text(json.dumps(out, indent=1))

    print(f"{len(rows)} candidate functions (scaffold POOL label read + "
          f"real > 0 or a pool-symbol KIND mismatch), ranked by real asc")
    print(f"{'real':>5} {'clean':>6} {'NMIS':>5} {'OONLY':>5} {'MSET':>5} "
          f"{'FPONLY':>6} {'fuzzy':>8} {'size':>6}  {'unit':<24} {'function':<30}")
    for r in rows:
        if args.max_real and r["real"] > args.max_real:
            continue
        pct = f"{r['fuzzy']:.2f}" if r["fuzzy"] is not None else "-"
        print(f"{r['real']:>5} {r['real_clean']:>6} {r['named_mismatch']:>5} "
              f"{len(r['ours_only_named']):>5} "
              f"{'IDENT' if r['multiset_identical'] else 'DIFF':>5} "
              f"{'FP-ONLY' if r['fp_only'] else '-':>6} "
              f"{pct:>8} {r['size']:>6}  {r['unit']:<24} {r['function']:<30}")
    print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
