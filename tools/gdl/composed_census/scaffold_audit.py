#!/usr/bin/env python3
"""Re-audit every pragma scaffold: is its original premise still live?

`AGENTS.md` § "Direct investigation and handoff" requires a historical stop
claim to carry a current measured premise, and `probe.py` prints a standing
reminder ("N pragma/volatile scaffold row(s) in this TU -- re-audit each") that
nothing automated. This does the audit.

For every `#pragma X off|on ... #pragma X reset` region in a NonMatching TU it
records `real` for EVERY function the region spans, deletes the pragma pair,
rebuilds the TU object, re-measures, and restores with git. Verdicts:

  HARMFUL       every spanned function's real is <= baseline and at least one
                improves -- the scaffold is costing match quality
  LOAD-BEARING  some spanned function's real rises without it -- keep it
  DEAD          the object is byte-identical -- a proved class boundary, and
                the pragma can be deleted for clarity with no codegen effect

MEASURING ONLY THE NON-EXACT FUNCTION IS A TRAP, and it cost a lane one bad
keep (measured at e28d467f). A region routinely spans several functions; taking
`real` for the one non-exact function found four "HARMFUL" regions, and a
TU-wide objdiff fuzzy then showed THREE of them regressing the TU
(screensaver -0.1846, mb_particle -0.0144, pb_diag -0.0032) because removal
helped the measured function and hurt its neighbours. Spanning every function
catches that without a full report build.

`real` remains a SCREEN, not the arbiter. A HARMFUL verdict must still be
confirmed with a fresh objdiff fuzzy and a TU-wide gate before anything is kept
-- `real` and fuzzy read register-colour cascades differently and the project's
rule is that a fresh fuzzy decides.

Usage (from the repository root):
  python tools/gdl/composed_census/scaffold_audit.py --list
  python tools/gdl/composed_census/scaffold_audit.py --tu game/boss/bosscam
  python tools/gdl/composed_census/scaffold_audit.py --out build/GUNE5D/scaffold_audit.json
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
REPORT = REPO / "build" / "GUNE5D" / "report.json"
PRAGMA = re.compile(r"^\s*#pragma\s+(\S+)\s+(off|on|reset|\d+)\s*$")
FNDEF = re.compile(r"^[A-Za-z_][A-Za-z0-9_ \*]*\b(\w+)\s*\([^;]*$")
REAL_RE = re.compile(r"real (\d+)")


def nonmatching_tus():
    cfg = (REPO / "configure.py").read_text(errors="replace")
    return {re.sub(r"\.(c|cpp)$", "", m)
            for m in re.findall(r'Object\(NonMatching,\s*"([^"]+)"', cfg)}


def fuzzy_index():
    if not REPORT.exists():
        return {}
    out = {}
    for u in json.loads(REPORT.read_text())["units"]:
        key = u["name"].split("/", 1)[1] if "/" in u["name"] else u["name"]
        for f in u.get("functions", []):
            out[(key, f["name"])] = float(f.get("fuzzy_match_percent", 0))
    return out


def source_of(tu):
    for ext in (".c", ".cpp"):
        p = REPO / "src" / (tu + ext)
        if p.exists():
            return p
    return None


def regions(tu, scores):
    """[(pragma, opener_line, reset_line, [functions spanned])] for one TU."""
    path = source_of(tu)
    if path is None:
        return []
    lines = path.read_text(errors="replace").split("\n")
    stack, out = [], []
    for i, line in enumerate(lines, 1):
        m = PRAGMA.match(line)
        if not m:
            continue
        name, state = m.group(1), m.group(2)
        if state == "reset":
            for k in range(len(stack) - 1, -1, -1):
                if stack[k][0] == name:
                    pname, pstate, start = stack.pop(k)
                    fns = []
                    for j in range(start, i):
                        fm = FNDEF.match(lines[j])
                        if fm and "{" in "\n".join(lines[j:j + 3]):
                            fn = fm.group(1)
                            if (tu, fn) in scores:
                                fns.append(fn)
                    out.append((f"{pname} {pstate}", start, i, fns))
                    break
        else:
            stack.append((name, state, i))
    return out


def real_of(tu, fn):
    """`real` for one function, with 0 for an already-exact one.

    fndiff prints `DIFF <fn> ... real N` only for a function that differs; an
    exact one is `OK <fn>` and a pool-name-only one is `POOL <fn>`, NEITHER of
    which carries a `real` field. Reading only the `real` token returned None
    for all of them, and they were reported UNMEASURED -- 55 of 139 regions at
    a9c09f62, and exactly the ones that matter, because an EXACT function going
    non-zero is the regression signal this audit exists to catch.
    """
    r = subprocess.run([sys.executable, "tools/gdl/fndiff.py", f"{tu}.c", fn,
                        "--count"], capture_output=True, text=True, cwd=REPO,
                       timeout=600)
    for line in reversed((r.stdout + r.stderr).splitlines()):
        m = REAL_RE.search(line)
        if m:
            return int(m.group(1))
        if re.match(rf"^(OK|POOL)\s+{re.escape(fn)}\b", line.strip()):
            return 0
    return None


def build_tu(tu):
    r = subprocess.run(["ninja", f"build/GUNE5D/src/{tu}.o"],
                       capture_output=True, text=True, cwd=REPO, timeout=900)
    return r.returncode == 0


def verdict_for_deltas(deltas):
    """(verdict, worsened, improved) from {fn: (before, after)} real counts.

    ANY function worsening makes the region LOAD-BEARING even when another
    improves. Ranking a mixed region HARMFUL on the improvement alone is the
    trap this tool exists to close: three regions read HARMFUL that way and a
    TU-wide fuzzy then showed all three regressing the TU.
    """
    worse = sorted(f for f, d in deltas.items() if d[1] > d[0])
    better = sorted(f for f, d in deltas.items() if d[1] < d[0])
    if worse:
        return "LOAD-BEARING", worse, better
    if better:
        return "HARMFUL", worse, better
    return "DEAD", worse, better


def audit_region(tu, pragma, a, b, fns):
    path = source_of(tu)
    base = {f: real_of(tu, f) for f in fns}
    lines = path.read_text(errors="replace").split("\n")
    if not lines[a - 1].strip().startswith("#pragma"):
        return {"verdict": "SKIPPED", "why": f"line {a} is not a pragma"}
    original = "\n".join(lines)
    del lines[b - 1]
    del lines[a - 1]
    path.write_text("\n".join(lines))
    try:
        if not build_tu(tu):
            return {"verdict": "BUILD-FAILED"}
        after = {f: real_of(tu, f) for f in fns}
    finally:
        path.write_text(original)
        build_tu(tu)
    deltas = {f: (base[f], after[f]) for f in fns
              if base.get(f) is not None and after.get(f) is not None}
    if not deltas:
        return {"verdict": "UNMEASURED"}
    verdict, worse, better = verdict_for_deltas(deltas)
    return {"verdict": verdict, "deltas": deltas,
            "worse": worse, "better": better}


def tu_fuzzy(tu):
    """This TU's objdiff fuzzy from a FRESH full report, or None."""
    r = subprocess.run(["ninja", "-j2"], capture_output=True, text=True,
                       cwd=REPO, timeout=3600)
    if r.returncode != 0 or not REPORT.exists():
        return None
    for u in json.loads(REPORT.read_text())["units"]:
        key = u["name"].split("/", 1)[1] if "/" in u["name"] else u["name"]
        if key == tu:
            return float(u["measures"]["fuzzy_match_percent"])
    return None


def confirm_harmful(tu, a, b):
    """Re-measure a HARMFUL region against the ARBITER: fresh objdiff fuzzy.

    `real` counts differing diff lines; fuzzy scores stream similarity, and an
    edit can cut the line count while making the stream less similar. All three
    HARMFUL regions found at 0681db82 did exactly that -- real improved,
    TU fuzzy fell (screensaver -0.1846, mb_particle -0.0144, pb_diag -0.0032).
    So a HARMFUL verdict is NOT actionable until this runs.
    """
    path = source_of(tu)
    before = tu_fuzzy(tu)
    original = path.read_text(errors="replace")
    lines = original.split("\n")
    del lines[b - 1]
    del lines[a - 1]
    path.write_text("\n".join(lines))
    try:
        after = tu_fuzzy(tu)
    finally:
        path.write_text(original)
        tu_fuzzy(tu)
    if before is None or after is None:
        return {"confirmed": None, "why": "fuzzy unavailable"}
    return {"confirmed": after > before, "fuzzy_before": before,
            "fuzzy_after": after, "delta": round(after - before, 4)}


VOLATILE = re.compile(
    r"^(\s*)((?:register\s+)?)volatile(\s+[A-Za-z_][\w \*]*\b\w+\s*"
    r"(?:\[[^\]]*\])?\s*;.*)$")


def brace_depth(lines, start, end):
    """Net `{` minus `}` over lines[start:end], ignoring // comments."""
    d = 0
    for k in range(start, end):
        s = re.sub(r"//.*", "", lines[k])
        d += s.count("{") - s.count("}")
    return d


def enclosing_function(lines, index, scores, tu):
    """The measured function whose BODY a line sits in, or None.

    Nearest definition above is not sufficient: a FILE-SCOPE `volatile u32 g;`
    placed after a function body would take that function's name and then be
    audited, stripping `volatile` from a global -- the one case where the
    keyword is least likely to be decoration. Requiring positive brace depth
    between the definition and the line rejects that. Measured at 6547a375 the
    guard rejects none of the 81 real sites, so it costs nothing and closes the
    hazard.
    """
    for j in range(index - 1, -1, -1):
        m = FNDEF.match(lines[j])
        if m and (tu, m.group(1)) in scores:
            if brace_depth(lines, j, index) > 0:
                return m.group(1)
            return None
    return None


def volatile_sites(tu, scores):
    """[(line, function, text)] for every volatile local in a measured fn."""
    path = source_of(tu)
    if path is None:
        return []
    lines = path.read_text(errors="replace").split("\n")
    out = []
    for i, line in enumerate(lines):
        if not VOLATILE.match(line):
            continue
        fn = enclosing_function(lines, i, scores, tu)
        if fn:
            out.append((i + 1, fn, line.strip()))
    return out


VERDICT_RE = re.compile(r"^(OK|POOL|DIFF)\s+(\S+)")


def obj_path(tu):
    return REPO / "build" / "GUNE5D" / "src" / (tu + ".o")


def obj_sha1(tu):
    """sha1 of this TU's object, or None. MWCC output here is deterministic:
    two untouched rebuilds of bosscam.o gave the same digest (6547a375)."""
    p = obj_path(tu)
    if not p.exists():
        return None
    import hashlib
    return hashlib.sha1(p.read_bytes()).hexdigest()


def tu_reals(tu):
    """{function: real} for EVERY function of one TU in a single fndiff call.

    `real_of` spends a process per function. fndiff already prints the whole
    TU -- `OK`/`POOL` rows are real 0, `DIFF` rows carry the count -- so one
    call is both cheaper and COMPLETE, which matters because the pragma
    campaign's one bad keep came from measuring a subset of the functions in
    scope.
    """
    r = subprocess.run([sys.executable, "tools/gdl/fndiff.py", f"{tu}.c",
                        "--count"], capture_output=True, text=True, cwd=REPO,
                       timeout=900)
    out = {}
    for line in (r.stdout + r.stderr).splitlines():
        m = VERDICT_RE.match(line.strip())
        if not m:
            continue
        kind, name = m.group(1), m.group(2)
        rm = REAL_RE.search(line)
        out[name] = int(rm.group(1)) if rm else (0 if kind != "DIFF" else None)
    return out


def audit_volatile(tu, line_no, fn):
    """Drop the `volatile` keyword on one local and re-measure the whole TU.

    DEAD here is PROOF, not a screen: the object is compared by sha1, so a DEAD
    verdict means MWCC emitted the identical object without the qualifier. When
    the object does move, every function in the TU is re-measured, because a
    local's frame slot is not guaranteed to be the only thing that shifts.

    For a DECOMPILATION a dead `volatile` is removable even though `volatile`
    is not semantically a no-op in C: the target object is the specification,
    these qualifiers are scaffolds added to coerce codegen, and a byte-identical
    object says this compiler at these flags ignored it. That reasoning does NOT
    extend to a volatile that moves the object, nor to one on a global or a
    hardware address -- `enclosing_function` rejects those by brace depth.
    """
    path = source_of(tu)
    original = path.read_text(errors="replace")
    lines = original.split("\n")
    m = VOLATILE.match(lines[line_no - 1])
    if not m:
        return {"verdict": "SKIPPED", "why": f"line {line_no} is not volatile"}
    before_sha = obj_sha1(tu)
    base = tu_reals(tu)
    lines[line_no - 1] = m.group(1) + m.group(2) + m.group(3).lstrip()
    path.write_text("\n".join(lines))
    after_sha = after = None
    try:
        if not build_tu(tu):
            return {"verdict": "BUILD-FAILED"}
        after_sha = obj_sha1(tu)
        if after_sha != before_sha:
            after = tu_reals(tu)
    finally:
        path.write_text(original)
        build_tu(tu)
        restored = obj_sha1(tu)
    if restored != before_sha:
        return {"verdict": "RESTORE-FAILED", "why": "object did not return to "
                f"{before_sha}; tree left at {restored}"}
    if after_sha == before_sha:
        return {"verdict": "DEAD", "identical": True, "deltas": {},
                "worse": [], "better": []}
    deltas = {f: (base[f], after[f]) for f in base
              if base.get(f) is not None and after.get(f) is not None}
    if not deltas:
        return {"verdict": "UNMEASURED"}
    verdict, worse, better = verdict_for_deltas(deltas)
    if verdict == "DEAD":
        # object moved but no `real` did: a difference `real` cannot see.
        verdict = "MOVED-UNSCORED"
    return {"verdict": verdict, "identical": False,
            "deltas": {f: d for f, d in deltas.items() if d[0] != d[1]},
            "worse": worse, "better": better}


def strip_volatile_lines(tu, line_nos):
    """Strip `volatile` from several lines of one TU at once.

    Returns (original_text, [lines actually rewritten]). Stripping a keyword
    never changes the line count, so recorded line numbers stay valid across
    every site in the same file -- which is what makes a per-TU batch safe to
    express as a list of independent line edits.
    """
    path = source_of(tu)
    original = path.read_text(errors="replace")
    lines = original.split("\n")
    done = []
    for n in line_nos:
        m = VOLATILE.match(lines[n - 1])
        if m:
            lines[n - 1] = m.group(1) + m.group(2) + m.group(3).lstrip()
            done.append(n)
    path.write_text("\n".join(lines))
    return original, done


def delete_lines(tu, line_nos):
    """Delete whole lines from one TU, HIGHEST FIRST.

    A pragma region is a PAIR of lines, and a TU can hold several regions --
    mb_particle has eight, so sixteen lines. Deleting in ascending order
    invalidates every later line number the audit recorded; descending order
    keeps them all valid without re-deriving anything. Returns
    (original_text, sorted lines actually removed).
    """
    path = source_of(tu)
    original = path.read_text(errors="replace")
    lines = original.split("\n")
    done = []
    for n in sorted(set(line_nos), reverse=True):
        if 1 <= n <= len(lines) and lines[n - 1].strip().startswith("#pragma"):
            del lines[n - 1]
            done.append(n)
    path.write_text("\n".join(lines))
    return original, sorted(done)


def apply_dead(results):
    """Apply every DEAD site, one TU at a time, gated on the object digest.

    THIRTY-TWO INDIVIDUAL PROOFS ARE NOT A PROOF OF THE BATCH. Six of the DEAD
    sites are in ONE function (btricol::LineLineDist3D2D) and four more in
    another (camera::DiffRate_8002951C); each was measured alone, and removing
    all six together can free a frame slot that any one of them alone could
    not. So the batch is re-gated per TU on the same sha1 that made each site
    DEAD, and a TU whose digest moves is reverted whole and reported -- never
    kept on the strength of the individual measurements.
    """
    by_tu = {}
    for r in results:
        if r.get("verdict") != "DEAD":
            continue
        # A volatile record carries one `line`; a pragma record carries the
        # region's `lines` PAIR. Dispatching on shape lets one gate serve both
        # halves of the campaign instead of the pragma half being done by hand.
        if "line" in r:
            by_tu.setdefault((r["tu"], "volatile"), []).append(r["line"])
        else:
            by_tu.setdefault((r["tu"], "pragma"), []).extend(r["lines"])
    out = []
    for tu, kind in sorted(by_tu):
        lines = sorted(by_tu[(tu, kind)])
        before = obj_sha1(tu)
        original, done = (strip_volatile_lines(tu, lines) if kind == "volatile"
                          else delete_lines(tu, lines))
        ok = build_tu(tu)
        after = obj_sha1(tu) if ok else None
        kept = ok and after == before
        if not kept:
            source_of(tu).write_text(original)
            build_tu(tu)
        out.append({"tu": tu, "kind": kind, "lines": done, "kept": kept,
                    "sha1": before, "sha1_after": after,
                    "why": None if kept else
                    ("build failed" if not ok else "object digest moved")})
        print(f"{'KEPT  ' if kept else 'REVERT'} {tu:26} {kind:8} "
              f"{len(done)} line(s) {done}"
              + ("" if kept else f"  -- {out[-1]['why']}"), flush=True)
    return out


def confirm_volatile(tu, line_no):
    """Re-measure one HARMFUL volatile against the ARBITER: fresh objdiff fuzzy.

    Same contract as confirm_harmful for pragmas, and the same reason: `real`
    counts differing diff lines while fuzzy scores stream similarity, and an
    edit can cut the line count while making the stream less similar. Three of
    three HARMFUL pragma regions did exactly that.
    """
    path = source_of(tu)
    before = tu_fuzzy(tu)
    original, done = strip_volatile_lines(tu, [line_no])
    if not done:
        path.write_text(original)
        return {"confirmed": None, "why": f"line {line_no} is not volatile"}
    try:
        after = tu_fuzzy(tu)
    finally:
        path.write_text(original)
        tu_fuzzy(tu)
    if before is None or after is None:
        return {"confirmed": None, "why": "fuzzy unavailable"}
    return {"confirmed": after > before, "fuzzy_before": before,
            "fuzzy_after": after, "delta": round(after - before, 4)}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", help="only this TU")
    ap.add_argument("--list", action="store_true",
                    help="enumerate regions without building anything")
    ap.add_argument("--out", help="write results as JSON here")
    ap.add_argument("--apply-dead", metavar="JSON",
                    help="apply every DEAD site from a --volatiles or pragma "
                         "run, one TU at a time, gated per TU on the object "
                         "digest -- individual proofs are not a batch proof")
    ap.add_argument("--volatiles", action="store_true",
                    help="audit `volatile` locals instead of pragma regions — "
                         "the other half of what probe.py's reminder names")
    ap.add_argument("--confirm", action="store_true",
                    help="re-measure every HARMFUL region against a fresh "
                         "objdiff fuzzy, the arbiter `real` cannot replace")
    args = ap.parse_args()

    scores = fuzzy_index()
    if not scores:
        print("scaffold_audit: build/GUNE5D/report.json missing — run ninja")
        return 2
    tus = sorted(nonmatching_tus())
    if args.tu:
        tus = [t for t in tus if args.tu in t]

    if args.apply_dead:
        results = json.loads(Path(args.apply_dead).read_text())
        applied = apply_dead(results)
        kept = [a for a in applied if a["kept"]]
        n = sum(len(a["lines"]) for a in kept)
        print(f"\n{n} site(s) across {len(kept)} TU(s) kept byte-identical; "
              f"{len(applied) - len(kept)} TU(s) reverted")
        print("Still to run: the WHOLE-PROJECT gate. A per-TU digest says the "
              "object did not move; only `ninja -j2` plus a report read says "
              "the project did not.")
        return 0 if len(kept) == len(applied) else 1

    if args.volatiles:
        sites = [(tu, ln, fn, txt) for tu in tus
                 for ln, fn, txt in volatile_sites(tu, scores)]
        if args.list:
            print(f"{len(sites)} volatile local(s) inside a measured function")
            for tu, ln, fn, txt in sites:
                print(f"  {tu:26} :{ln:<5} {fn:28} {txt[:48]}")
            return 0
        print(f"{'TU':24} {'function':26} {'line':>5}  verdict")
        print("-" * 96)
        vres = []
        for tu, ln, fn, txt in sites:
            r = audit_volatile(tu, ln, fn)
            r.update({"tu": tu, "line": ln, "function": fn, "text": txt})
            vres.append(r)
            moved = ", ".join(f"{f} {a}->{b}"
                              for f, (a, b) in r.get("deltas", {}).items())
            print(f"{tu:24} {fn:26} {ln:5}  {r['verdict']}"
                  + (f"  [{moved}]" if moved else ""), flush=True)
            if r["verdict"] == "RESTORE-FAILED":
                print(f"ABORTING: {r['why']}", file=sys.stderr)
                break
        if args.confirm:
            print("\nconfirming HARMFUL against a fresh objdiff fuzzy")
            for r in vres:
                if r["verdict"] != "HARMFUL":
                    continue
                c = confirm_volatile(r["tu"], r["line"])
                r["confirm"] = c
                if c.get("confirmed") is False:
                    r["verdict"] = "REFUTED-BY-FUZZY"
                print(f"  {r['tu']:24} :{r['line']:<5} {c}", flush=True)
        tally = {}
        for r in vres:
            tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
        print("\n" + "  ".join(f"{k} {v}" for k, v in sorted(tally.items())))
        print("\nDEAD here is PROOF, not a screen: the object is compared by "
              "sha1. MOVED-UNSCORED means the object changed but no `real` "
              "did, a difference `real` cannot see; treat it as LOAD-BEARING.")
        if args.out:
            Path(args.out).write_text(json.dumps(vres, indent=1))
            print(f"wrote {args.out}")
        return 0

    plan = []
    for tu in tus:
        for pragma, a, b, fns in regions(tu, scores):
            if fns:
                plan.append((tu, pragma, a, b, fns))

    if args.list:
        print(f"{len(plan)} pragma region(s) spanning at least one measured "
              f"function, across {len(tus)} NonMatching TU(s)")
        for tu, pragma, a, b, fns in plan:
            nonexact = [f for f in fns if scores.get((tu, f), 100) < 100]
            print(f"  {tu:26} {pragma:22} {a}-{b}  spans {len(fns)} fn(s)"
                  f"{', non-exact: ' + ', '.join(nonexact) if nonexact else ''}")
        return 0

    results = []
    print(f"{'TU':24} {'pragma':22} {'spans':>5}  verdict")
    print("-" * 92)
    for tu, pragma, a, b, fns in plan:
        r = audit_region(tu, pragma, a, b, fns)
        r.update({"tu": tu, "pragma": pragma, "lines": [a, b], "functions": fns})
        results.append(r)
        extra = ""
        if r["verdict"] == "HARMFUL":
            extra = "  improves: " + ", ".join(
                f"{f} {r['deltas'][f][0]}->{r['deltas'][f][1]}"
                for f in r["better"])
        elif r["verdict"] == "LOAD-BEARING":
            extra = "  worsens: " + ", ".join(
                f"{f} {r['deltas'][f][0]}->{r['deltas'][f][1]}"
                for f in r["worse"][:2])
        print(f"{tu:24} {pragma:22} {len(fns):5}  {r['verdict']}{extra[:44]}")
    if args.confirm:
        harmful = [r for r in results if r["verdict"] == "HARMFUL"]
        if harmful:
            print(f"\nconfirming {len(harmful)} HARMFUL region(s) against "
                  f"fresh objdiff fuzzy")
            for r in harmful:
                c = confirm_harmful(r["tu"], *r["lines"])
                r["confirmation"] = c
                if c.get("confirmed") is True:
                    print(f"  CONFIRMED  {r['tu']:24} fuzzy "
                          f"{c['fuzzy_before']:.4f} -> {c['fuzzy_after']:.4f} "
                          f"({c['delta']:+.4f})")
                elif c.get("confirmed") is False:
                    r["verdict"] = "REFUTED-BY-FUZZY"
                    print(f"  REFUTED    {r['tu']:24} fuzzy "
                          f"{c['fuzzy_before']:.4f} -> {c['fuzzy_after']:.4f} "
                          f"({c['delta']:+.4f}) — real improved, fuzzy did not")
                else:
                    print(f"  UNCONFIRMED {r['tu']:24} {c.get('why')}")

    tally = {}
    for r in results:
        tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
    print("\n" + "  ".join(f"{k} {v}" for k, v in sorted(tally.items())))
    print("\n`real` is a SCREEN. Confirm any HARMFUL verdict with a fresh "
          "objdiff fuzzy and a TU-wide gate before keeping it.")
    if args.out:
        Path(args.out).write_text(json.dumps(results, indent=1))
        print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
