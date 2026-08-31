#!/usr/bin/env python3
"""Whole-tree extern type-conflict census.

Finds linker symbols declared with conflicting C types across TUs — the bug
class behind InitCamera's fctiwz tell (combat.c said `extern s32
lbl_80344524;` while camera.c said `extern f32` for the same symbol, forcing
a float->int narrowing at the write site). MWCC trusts the local declaration,
so the TU with the wrong type silently compiles wrong-typed accesses.

Usage:
  python tools/gdl/externcheck.py            # scan src/, report conflicts
  python tools/gdl/externcheck.py --all      # also list benign multi-declared
  python tools/gdl/externcheck.py --objects  # skip function-prototype scan

WIDENED 2026-08-31 (worker GH, work_claim.ghost-resweep-hygiene). The original
tool flagged ONLY a float-class/int-class mix, so it was structurally blind to
three defect classes that later waves had to rediscover by hand:

  * u32-vs-s32 and other signedness/width splits (same "int" class before),
  * pointer-vs-int (explicitly demoted to --all as "benign word traffic"),
  * FUNCTION PROTOTYPE disagreements — the regex required a `;` right after
    the declarator, so `extern void *F(f64 maxDist, ...)` never matched at
    all. That is exactly the FindClosestWaypoint f64-vs-f32 defect
    (attempt.findclosestwaypoint-param-width-sets-slot-floor-exact.20260901.v1),
    which sets the callee's parameter-save-area width and therefore its whole
    local-block floor.

Now: any disagreement in the canonical type class of one symbol is reported,
each declaration carries a CLASS column, and prototypes are compared on
return type plus per-parameter class.

Severities:
  CONFLICT  conclusive class split among known scalars/pointers (float vs int,
            signedness, width, pointer vs scalar). Fix these.
  HAZARD    `int` vs `s32`(= signed long) only. Same width/signedness/ABI, but
            a DIFFERENT MWCC front-end type node, and per
            claim.law.int-vs-signed-long-extern-reorders-schedule.20260831.v1
            harmonizing it is NOT codegen-neutral: it is a per-site hazard, so
            gate every such fix individually and keep the divergence where it
            refuses.
  REVIEW    at least one declaration uses an unknown/aggregate typedef, so the
            split cannot be judged mechanically.

Exit 1 when any CONFLICT exists so it can gate CI/passes.
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

EXTERN_RE = re.compile(
    r"^\s*extern\s+((?:const\s+|volatile\s+|unsigned\s+|signed\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\*+)?)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*(?:\s*,\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*"
    r"(?:\[[^\]]*\]\s*)?;", re.M
)

# extern <ret-type> <name>(<params>);   — prototypes, which EXTERN_RE cannot see.
PROTO_RE = re.compile(
    r"^\s*extern\s+((?:const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\*+)?)\s*"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\(([^;{)]*)\)\s*;", re.M
)

COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

# canonical class -> the spellings that mean exactly it
_SCALARS = {
    "s8": ("s8", "signed char", "char"),
    "u8": ("u8", "unsigned char"),
    "s16": ("s16", "short", "signed short", "short int"),
    "u16": ("u16", "unsigned short", "unsigned short int"),
    "s32/long": ("s32", "long", "signed long", "signed long int", "long int"),
    "s32/int": ("int", "signed int", "signed", "BOOL"),
    "u32/long": ("u32", "unsigned long", "unsigned long int"),
    "u32/int": ("unsigned", "unsigned int"),
    "s64": ("s64", "long long", "signed long long"),
    "u64": ("u64", "unsigned long long"),
    "f32": ("f32", "float"),
    "f64": ("f64", "double"),
    "void": ("void",),
}
SPELLING_TO_CLASS = {
    spelling: cls for cls, spellings in _SCALARS.items() for spelling in spellings
}

# classes that differ only as MWCC front-end type NODES, not in width/sign/ABI
NODE_ONLY_PAIRS = {frozenset({"s32/long", "s32/int"}),
                   frozenset({"u32/long", "u32/int"})}


def canonical_class(type_text):
    """-> (class, conclusive). `class` is comparable across declarations."""
    base = type_text.replace("const", " ").replace("volatile", " ")
    stars = base.count("*")
    base = " ".join(base.replace("*", " ").split())
    if stars:
        inner, _ = canonical_class(base) if base else ("void", True)
        return ("ptr" * stars + "->" + inner, True)
    if not base:
        return ("void", True)
    if base.startswith("struct "):
        return ("agg:" + base.split()[-1], False)
    cls = SPELLING_TO_CLASS.get(base)
    if cls:
        return (cls, True)
    return ("agg:" + base.split()[-1], False)


def severity(classes, conclusive):
    """classes: set of canonical class strings seen for one symbol."""
    if len(classes) < 2:
        return None
    if not conclusive:
        return "REVIEW"
    bare = {c for c in classes}
    if bare in NODE_ONLY_PAIRS:
        return "HAZARD"
    return "CONFLICT"


def _strip(text):
    return COMMENT_RE.sub(
        lambda match: re.sub(r"[^\n]", " ", match.group(0)), text)


def _paths(src_root, include_root):
    paths = []
    for root in (src_root, include_root):
        for pattern in ("*.c", "*.cpp", "*.h"):
            paths.extend(Path(root).rglob(pattern))
    return sorted(p for p in paths if p.is_file())


def scan(src_root, include_root="include"):
    """{symbol: {type_text: [(file, line)]}} for every extern OBJECT decl.

    Headers are scanned too (explicit patterns — a bare "*.c*" glob skips
    any header without ".c" in its name, which hid camera.h's canonical
    gCameras declaration from the first version of this tool).
    """
    declarations = defaultdict(lambda: defaultdict(list))
    for path in _paths(src_root, include_root):
        text = _strip(path.read_text(encoding="utf-8", errors="replace"))
        for match in EXTERN_RE.finditer(text):
            type_text = " ".join(match.group(1).split())
            line = text.count("\n", 0, match.start()) + 1
            for name in re.split(r"\s*,\s*", match.group(2)):
                declarations[name.strip()][type_text].append(
                    (str(path).replace("\\", "/"), line))
    return declarations


def _param_types(param_text):
    """['f64', 'f32 *', 's32'] from a prototype parameter list."""
    body = " ".join(param_text.split())
    if not body or body == "void":
        return []
    out = []
    for part in body.split(","):
        part = part.strip()
        if part == "...":
            out.append("...")
            continue
        # drop the parameter NAME (last identifier) but keep any '*'
        part = re.sub(r"\[[^\]]*\]", " *", part)
        tokens = part.split()
        if len(tokens) > 1 and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", tokens[-1]):
            tokens = tokens[:-1]
        out.append(" ".join(tokens) or "void")
    return out


def scan_prototypes(src_root, include_root="include"):
    """{symbol: {signature_text: [(file, line)]}} for extern FUNCTION decls."""
    protos = defaultdict(lambda: defaultdict(list))
    for path in _paths(src_root, include_root):
        text = _strip(path.read_text(encoding="utf-8", errors="replace"))
        for match in PROTO_RE.finditer(text):
            ret = " ".join(match.group(1).split())
            name = match.group(2)
            params = _param_types(match.group(3))
            sig = "%s (%s)" % (ret, ", ".join(params) or "void")
            line = text.count("\n", 0, match.start()) + 1
            protos[name][sig].append((str(path).replace("\\", "/"), line))
    return protos


def _classes_for_signature(sig):
    """Canonical class tuple for a whole prototype: (ret, p0, p1, ...)."""
    ret, _, rest = sig.partition(" (")
    params = rest.rstrip(")")
    parts = [ret] + ([] if params == "void" else
                     [p.strip() for p in params.split(",") if p.strip()])
    classes, conclusive = [], True
    for part in parts:
        if part == "...":
            classes.append("...")
            continue
        cls, ok = canonical_class(part)
        classes.append(cls)
        conclusive = conclusive and ok
    return tuple(classes), conclusive


def conflicts(declarations):
    """Yield (name, {type_text: (sites, class)}, severity) for every split."""
    for name, by_type in sorted(declarations.items()):
        if len(by_type) < 2:
            continue
        classed, classes, conclusive = {}, set(), True
        for type_text, sites in by_type.items():
            cls, ok = canonical_class(type_text)
            classed[type_text] = (sites, cls)
            classes.add(cls)
            conclusive = conclusive and ok
        yield name, classed, severity(classes, conclusive)


def proto_conflicts(protos):
    for name, by_sig in sorted(protos.items()):
        if len(by_sig) < 2:
            continue
        classed, classes, conclusive = {}, set(), True
        for sig, sites in by_sig.items():
            cls, ok = _classes_for_signature(sig)
            classed[sig] = (sites, "(" + ", ".join(cls) + ")")
            classes.add(cls)
            conclusive = conclusive and ok
        if len(classes) < 2:
            continue  # same classes, different spelling — benign
        sev = "REVIEW" if not conclusive else "CONFLICT"
        # a pure int/long node split anywhere and nothing else -> HAZARD
        if conclusive:
            diffs = {frozenset(c[i] for c in classes)
                     for i in range(min(len(c) for c in classes))}
            diffs = {d for d in diffs if len(d) > 1}
            if diffs and all(d in NODE_ONLY_PAIRS for d in diffs):
                sev = "HAZARD"
        yield name, classed, sev


def _emit(name, classed, sev, width=20):
    print(f"{sev:<8} {name}")
    for text, (sites, cls) in sorted(classed.items()):
        first = sites[0]
        more = f" (+{len(sites) - 1} more)" if len(sites) > 1 else ""
        print(f"    {text:<{width}} {cls:<14} {first[0]}:{first[1]}{more}")


def main():
    show_all = "--all" in sys.argv
    counts = defaultdict(int)

    for name, classed, sev in conflicts(scan("src")):
        if sev is None:
            if show_all:
                _emit(name, classed, "multi")
            continue
        counts[sev] += 1
        _emit(name, classed, sev)

    if "--objects" not in sys.argv:
        for name, classed, sev in proto_conflicts(scan_prototypes("src")):
            counts[sev] += 1
            _emit(name, classed, sev, width=44)

    print("[%d CONFLICT, %d HAZARD, %d REVIEW]"
          % (counts["CONFLICT"], counts["HAZARD"], counts["REVIEW"]))
    return 1 if counts["CONFLICT"] else 0


if __name__ == "__main__":
    sys.exit(main())
