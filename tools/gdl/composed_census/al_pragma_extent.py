"""al_pragma_extent.py -- opt_propagation COVERAGE by definition-order EXTENT,
cross-tabbed against the ADDR16_LO home-copy class both ways.

Lever 1 of run 53. attempt.EO_enemy-c-textorder-rewrite-lands-and-both-its-
regressions-were-one-cause-a-relocated-pragma-region.20260904.v1 measured that
`#pragma opt_propagation off` DELETED a home copy from OUR stream
(fn_8004DF58 lost the target's `addi r29,r3,0` at +0x14c, EXACT -> STRUCTURAL).
The ADDR16_LO class defect is the mirror image: ours pays a copy the target
does not. So the question this census answers, BOTH WAYS, is whether
opt_propagation coverage predicts which side pays the copy.

The pragma-leak law (claim.law.EO_a-bare-pragma-reset-does-not-close-
opt_propagation-off-and-a-text-reorder-relocates-the-leak-across-the-tu
.20260904.v1) is honoured exactly: coverage is DEFINITION-ORDER EXTENT, and a
bare `#pragma reset` does NOT close an `opt_propagation off` region. Presence of
the pragma anywhere in the file is NOT coverage and is reported separately, so
the two can be compared.

IMPORTABLE CORE: scan_pragmas, scan_functions, coverage_map

Usage: python tools/gdl/composed_census/al_pragma_extent.py [--census PATH]
       [--out PATH]   (from the repo root; both default under build/GUNE5D/.
       Run al_addrlo_positive.py first -- it writes the census this reads.)
"""
import os
import re
import sys
import json
import collections

ROOT = os.getcwd()
SRC = os.path.join(ROOT, "src")

# `#pragma opt_propagation off|on|reset`, and the BARE `#pragma reset`.
P_OPT = re.compile(r"^\s*#\s*pragma\s+opt_propagation\s+(off|on|reset)\b")
P_BARE = re.compile(r"^\s*#\s*pragma\s+reset\s*$")
P_ANY = re.compile(r"^\s*#\s*pragma\s+(\w+)")

# a plausible function-definition head: the last identifier before the '('
HEAD = re.compile(r"([A-Za-z_]\w*)\s*\([^;]*$")


def strip_code(text):
    """Blank out comments and string/char literals, preserving line structure,
    so brace counting and pragma detection cannot be fooled by them."""
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


def scan_pragmas(lines):
    """-> (events, any_opt_present). events = [(lineno, kind)] where kind is
    'off' | 'on' | 'reset' | 'bare_reset'."""
    events = []
    present = False
    for ln, line in enumerate(lines):
        m = P_OPT.match(line)
        if m:
            present = True
            events.append((ln, m.group(1)))
            continue
        if P_BARE.match(line):
            events.append((ln, "bare_reset"))
    return events, present


def scan_functions(code, lines):
    """Definition-order function list: [(name, head_line, open_line, close_line)].
    Depth-0 brace tracking; the name is the last identifier before the '(' in
    the accumulated head text."""
    fns = []
    depth = 0
    head_start = 0
    pos = 0
    line_of = []
    for ln, line in enumerate(lines):
        line_of.append(pos)
        pos += len(line) + 1

    def lineno(off):
        lo, hi = 0, len(line_of) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if line_of[mid] <= off:
                lo = mid
            else:
                hi = mid - 1
        return lo

    open_off = None
    for i, ch in enumerate(code):
        if ch == "{":
            if depth == 0:
                open_off = i
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0 and open_off is not None:
                head = code[head_start:open_off]
                m = None
                for m in HEAD.finditer(head):
                    pass
                if m and "=" not in head.split("(")[0]:
                    fns.append((m.group(1), lineno(head_start),
                                lineno(open_off), lineno(i)))
                head_start = i + 1
                open_off = None
            if depth < 0:
                depth = 0
        elif ch == ";" and depth == 0:
            head_start = i + 1
    return fns


def coverage_map(path):
    """-> {name: 'covered'|'uncovered'}, plus diagnostics for the unit."""
    text = open(path, encoding="utf-8", errors="replace").read()
    code = strip_code(text)
    lines = code.split("\n")
    events, present = scan_pragmas(lines)
    fns = scan_functions(code, lines)

    # Build the definition-order extent: walk lines, tracking whether
    # opt_propagation is OFF. A bare `#pragma reset` does NOT close it.
    state = [False] * (len(lines) + 1)
    on = False
    ev = dict()
    for ln, kind in events:
        ev.setdefault(ln, []).append(kind)
    for ln in range(len(lines)):
        for kind in ev.get(ln, []):
            if kind == "off":
                on = True
            elif kind in ("reset", "on"):
                on = False
            # bare_reset: deliberately does NOT close it (the leak law)
        state[ln] = on

    cov = {}
    for name, hl, ol, cl in fns:
        # a function is COVERED if the pragma is active at its opening brace
        cov[name] = "covered" if state[ol] else "uncovered"

    dangling = 0
    on2 = False
    for ln, kind in events:
        if kind == "off":
            if on2:
                pass
            on2 = True
        elif kind in ("reset", "on"):
            on2 = False
    dangling = 1 if on2 else 0

    return cov, {
        "pragma_present": present,
        "events": [{"line": ln + 1, "kind": k} for ln, k in events],
        "bare_resets": sum(1 for _l, k in events if k == "bare_reset"),
        "dangling_opener": dangling,
        "functions_parsed": len(fns),
        "covered": sum(1 for v in cov.values() if v == "covered"),
    }


def main():
    out = os.path.join("build", "GUNE5D", "al_pragma_extent.json")
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]
    cpath = os.path.join("build", "GUNE5D", "al_addrlo_positive.json")
    if "--census" in sys.argv:
        cpath = sys.argv[sys.argv.index("--census") + 1]
    if not os.path.exists(cpath):
        sys.exit(f"missing census {cpath} -- run "
                 f"tools/gdl/composed_census/al_addrlo_positive.py first")
    census = json.load(open(cpath, encoding="utf-8"))

    # class membership per (unit, fn)
    cls = {}
    for key, roster in (("P", census.get("P", [])), ("Q", census.get("Q", [])),
                        ("S", census.get("S", []))):
        for row in roster:
            cls[(row["unit"], row["fn"])] = key
    exact = set()
    for key in ("P", "Q", "S"):
        for row in census.get(key, []):
            if row.get("exact"):
                exact.add((row["unit"], row["fn"]))

    units = sorted({u for (u, _f) in cls})
    per_unit = {}
    tab = collections.Counter()
    rows = []
    for unit in units:
        path = os.path.join(SRC, unit + ".c")
        if not os.path.exists(path):
            per_unit[unit] = {"error": "no source"}
            continue
        cov, diag = coverage_map(path)
        per_unit[unit] = diag
        for (u, f), k in sorted(cls.items()):
            if u != unit:
                continue
            c = cov.get(f, "NOT-PARSED")
            kk = k
            if k == "Q":
                kk = "Q-exact" if (u, f) in exact else "Q-fuzzy"
            tab[(kk, c)] += 1
            rows.append({"unit": u, "fn": f, "class": kk, "coverage": c,
                         "unit_pragma_present": diag.get("pragma_present")})

    res = {
        "note": "coverage = DEFINITION-ORDER EXTENT at the function's opening "
                "brace; a bare `#pragma reset` does NOT close opt_propagation "
                "off (the leak law). unit_pragma_present is the PRESENCE screen "
                "the leak law says is the wrong question -- reported so the two "
                "can be compared.",
        "crosstab": {f"{k[0]}/{k[1]}": v for k, v in sorted(tab.items())},
        "rows": rows,
        "per_unit": per_unit,
    }
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(res, open(out, "w", encoding="utf-8"), indent=1)

    print("=== opt_propagation COVERAGE (definition-order extent) x ADDR16_LO class")
    for k, v in sorted(tab.items()):
        print(f"  {k[0]:<9} {k[1]:<12} {v}")
    print()
    print("=== units whose source contains ANY opt_propagation pragma")
    npres = 0
    for u in units:
        d = per_unit.get(u, {})
        if d.get("pragma_present"):
            npres += 1
            print(f"  {u:<28} events={len(d['events']):<3} "
                  f"bare_resets={d['bare_resets']} "
                  f"dangling={d['dangling_opener']} "
                  f"covered_fns={d['covered']}/{d['functions_parsed']}")
    if not npres:
        print("  (none)")
    print(f"\nunits examined: {len(units)}   with the pragma: {npres}")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
