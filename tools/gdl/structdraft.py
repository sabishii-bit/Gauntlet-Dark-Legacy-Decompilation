#!/usr/bin/env python3
"""Draft a struct from the raw-offset accesses a TU already performs.

Three independent de-fakematch workers asked for this: before you can convert
`*(T*)(base + 0xNN)` into a named field you need the record's LAYOUT, and
today that is reconstructed by hand from grep output, one base at a time.

structdraft clusters every observed `base + CONST` access in a TU by base
expression, records the ACCESS WIDTH implied by each cast type, detects
`base + i*S + K` striding (array-of-records), scores candidate layouts from
the project headers and the Xbox PDB dump, and emits a padded C skeleton
ready for a worker to verify, name, and feed to defake_rewrite.py.

Usage:
  python tools/gdl/structdraft.py game/sfx/psfx.c              # all bases
  python tools/gdl/structdraft.py game/sfx/psfx.c --base row   # one base
  python tools/gdl/structdraft.py game/ui/screensaver.c --min-sites 8
  python tools/gdl/structdraft.py game/enemy/critter.c --base desc --json
  python tools/gdl/structdraft.py game/game/shop.c --verify Player

Output per cluster:
  - the observed offset/width table with per-site counts and example lines
  - stride evidence (`base += N`, `+ i*N`) and the implied record size
  - ranked candidate layouts (project headers first, then Xbox PDB)
  - a draft `struct <Base>View { ... }` with unkNN fields and pad runs

WHAT THIS TOOL IS NOT: authority. It reports what the CODE does, which is
evidence for a layout, not proof of one. Field names come only from a
matched candidate; everything else is `unkNN` by construction (AGENTS.md:
never invent names). Always gate a conversion with:
  python tools/gdl/defake_gate.py check <unit> --rebuild

--verify <TypeName> scores the draft against an already-recovered struct and
reports offset/width agreement -- the tool's own ground-truth test.
"""

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

# --------------------------------------------------------------- site scanning
# Kept deliberately in sync with memory_graph/core.py::_DEBT_CAST_RE so the
# draft covers exactly the population `gdlmem.py debt` counts.
CAST_RE = re.compile(
    r"\*\s*\(\s*(?P<ty>(?:const\s+)?(?:[us](?:8|16|32|64)|f32|f64|int|char"
    r"|short|long|float|double|void\s*\*|\w+\s*\*))\s*\*?\s*\)\s*\(")
COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
FN_START_RE = re.compile(r"^(?:static\s+)?\w[\w\s\*]*?\b(\w+)\s*\([^;{]*?\)\s*\{",
                         re.M)
KEYWORDS = {
    "return", "case", "else", "do", "if", "while", "for", "switch", "sizeof",
    "offsetof", "goto", "default", "break", "continue",
}
TYPEISH_RE = re.compile(
    r"^\s*(?:const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+|union\s+)*"
    r"[A-Za-z_]\w*\s*\**\s*$")

WIDTH = {
    "u8": 1, "s8": 1, "char": 1,
    "u16": 2, "s16": 2, "short": 2,
    "u32": 4, "s32": 4, "int": 4, "long": 4, "f32": 4, "float": 4,
    "u64": 8, "s64": 8, "f64": 8, "double": 8,
}
CTYPE = {1: "u8", 2: "u16", 4: "u32", 8: "f64"}


def strip_comments(text):
    return COMMENT_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def relpath(path, root):
    """Repo-relative when possible; absolute for a --file outside the tree."""
    try:
        return str(path.relative_to(root)).replace("\\", "/")
    except ValueError:
        return str(path).replace("\\", "/")


def hexoff(value):
    """Hex offset that survives negatives (`*(T*)(p - 0x20)` is a real form:
    a record header read behind a pointer that aims at the payload)."""
    return f"-0x{-value:X}" if value < 0 else f"0x{value:X}"


def match_open(text, close_idx):
    depth = 0
    for i in range(close_idx, -1, -1):
        if text[i] == ")":
            depth += 1
        elif text[i] == "(":
            depth -= 1
            if depth == 0:
                return i
    return -1


def is_binary_multiply(text, star_idx):
    """True when this `*` is arithmetic, not a dereference.

    `rate * (f32)(u32)gFrameTicks` matches the census regex but is a multiply;
    3.6% of the repo-wide census (133/3685 sites, 2026-08-31) is this shape.
    """
    j = star_idx - 1
    while j >= 0 and text[j] in " \t\r\n":
        j -= 1
    if j < 0:
        return False
    ch = text[j]
    if ch == "]" or ch.isdigit():
        return True
    if ch.isalpha() or ch == "_":
        k = j
        while k >= 0 and (text[k].isalnum() or text[k] == "_"):
            k -= 1
        return text[k + 1:j + 1] not in KEYWORDS
    if ch == ")":
        op = match_open(text, j)
        if op < 0:
            return False
        if TYPEISH_RE.match(text[op + 1:j]):
            return False                      # a cast -> the `*` is a deref
        k = op - 1
        while k >= 0 and text[k] in " \t\r\n":
            k -= 1
        m = k
        while m >= 0 and (text[m].isalnum() or text[m] == "_"):
            m -= 1
        return text[m + 1:k + 1] not in KEYWORDS
    return False


def balanced(text, open_idx):
    depth = 0
    for i in range(open_idx, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[open_idx + 1:i], i + 1
    return "", open_idx + 1


def split_top_plus(expr):
    parts, depth, cur, i = [], 0, "", 0
    while i < len(expr):
        ch = expr[i]
        if ch in "([":
            depth += 1
            cur += ch
        elif ch in ")]":
            depth -= 1
            cur += ch
        elif ch in "+-" and depth == 0:
            nxt = expr[i + 1] if i + 1 < len(expr) else ""
            if ch == "-" and nxt == ">":
                cur += "->"
                i += 2
                continue
            if nxt in "+-=":
                cur += ch + nxt
                i += 2
                continue
            prev = cur.rstrip()
            if prev and prev[-1] not in "+-*/<>&|^%~!=(,":
                parts.append(cur.strip())
                cur = "" if ch == "+" else "-"
                i += 1
                continue
            cur += ch
        else:
            cur += ch
        i += 1
    if cur.strip():
        parts.append(cur.strip())
    return [p for p in parts if p]


NUM_RE = re.compile(r"^-?(?:0[xX][0-9a-fA-F]+|\d+)$")
MUL_RE = re.compile(r"^(?P<a>[^*]+?)\s*\*\s*(?P<b>[^*]+)$")
SHL_RE = re.compile(r"^(?P<a>.+?)\s*<<\s*(?P<b>\d+)$")
LEADCAST_RE = re.compile(r"^\(\s*(?:const\s+)?\w+\s*\**\s*\)\s*(?P<rest>.+)$")
BASEISH_RE = re.compile(r"^[A-Za-z_][\w\.\[\]\s\*\(\)>-]*$")
UPDATE_RE = re.compile(r"(\+\+|--|\+=|-=)")


def as_int(tok):
    """Parse a C integer literal. Never use int(tok, 0): Python's base-0
    rejects a leading-zero decimal ("05"), which C sources do produce."""
    tok = tok.strip()
    if not NUM_RE.match(tok):
        return None
    neg = tok.startswith("-")
    body = tok[1:] if neg else tok
    val = (int(body, 16) if body[:2].lower() == "0x" else int(body, 10))
    return -val if neg else val


# A float literal is never a record stride, and its exponent digits
# ("3.05e-05 * x") otherwise read as a multiplicand -- blank them out
# before scanning an expression for stride evidence.
FLOAT_LIT_RE = re.compile(
    r"\b\d+\.\d*(?:[eE][+-]?\d+)?[fF]?"
    r"|\b\d+[eE][+-]?\d+[fF]?"
    r"|\.\d+(?:[eE][+-]?\d+)?[fF]?")


def decompose(inner):
    """inner -> {base, const, terms:[{index,stride}], update}"""
    const, terms, bases, residual = 0, [], [], []
    for part in split_top_plus(inner):
        neg = part.startswith("-")
        body = (part[1:] if neg else part).strip()
        val = as_int(body)
        if val is not None:
            const += -val if neg else val
            continue
        stripped = body
        while stripped.startswith("(") and stripped.endswith(")"):
            inner2, nxt = balanced(stripped, 0)
            if nxt != len(stripped):
                break
            stripped = inner2.strip()
        m = SHL_RE.match(stripped)
        if m:
            terms.append((m.group("a").strip(), 1 << int(m.group("b"))))
            continue
        m = MUL_RE.match(stripped)
        if m:
            a, b = m.group("a").strip(), m.group("b").strip()
            va, vb = as_int(a), as_int(b)
            if vb is not None and va is None:
                terms.append((a, vb))
                continue
            if va is not None and vb is None:
                terms.append((b, va))
                continue
        cand = stripped
        lc = LEADCAST_RE.match(cand)
        if lc:
            cand = lc.group("rest").strip()
        if BASEISH_RE.match(cand) and not neg and "*(" not in cand:
            bases.append(cand)
        else:
            residual.append(part)
    base = bases[0] if bases else (residual[0] if residual else "")
    return {"base": base, "const": const, "terms": terms,
            "update": bool(UPDATE_RE.search(inner))}


def scan_tu(path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    text = strip_comments(raw)
    lines = text.split("\n")
    marks = [(m.start(), m.group(1)) for m in FN_START_RE.finditer(text)]
    sites = []
    for m in CAST_RE.finditer(text):
        if is_binary_multiply(text, m.start()):
            continue
        inner, _ = balanced(text, m.end() - 1)
        if not inner.strip():
            continue
        ty = re.sub(r"\s+", " ", m.group("ty")).strip()
        dec = decompose(inner)
        lineno = text.count("\n", 0, m.start()) + 1
        owner = "<file-scope>"
        for pos, name in marks:
            if pos <= m.start():
                owner = name
            else:
                break
        key = ty.replace(" ", "").rstrip("*")
        sites.append({
            "line": lineno, "fn": owner, "type": ty,
            "width": WIDTH.get(key, 4 if ty.replace(" ", "").endswith("*")
                               else None),
            "is_ptr": ty.replace(" ", "").endswith("*"),
            "base": dec["base"], "const": dec["const"],
            "terms": dec["terms"], "update": dec["update"],
            "src": lines[lineno - 1].strip() if lineno - 1 < len(lines) else "",
        })
    return sites, text


# ------------------------------------------------------------- candidate layouts
HDR_LEAD_RE = re.compile(r"^\s*/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*(?P<decl>[^;]+);")
HDR_TRAIL_RE = re.compile(
    r"^\s*(?P<decl>[A-Za-z_][^;/]*?);\s*/\*\s*(0x[0-9A-Fa-f]+)\b")
HDR_OPEN_RE = re.compile(r"^\s*typedef\s+struct\s+(\w+)?\s*\{|^\s*struct\s+(\w+)\s*\{")
HDR_CLOSE_RE = re.compile(r"^\s*\}\s*(\w+)?\s*;")
PADNAME_RE = re.compile(r"^(pad|_res|_blk|unk|unused|reserved|_pad|filler)", re.I)
NAME_RE = re.compile(r"(\w+)\s*(?:\[[^\]]*\])*\s*$")

PDB_OPEN_RE = re.compile(
    r"^(?:typedef\s+)?(?:struct|union|class)\s+(?P<name>[A-Za-z_]\w*)"
    r"(?:\s*:[^/]*)?//\s*Size=(?P<size>0x[0-9a-fA-F]+)\s*\(Id=(?P<id>\d+)\)")
PDB_FIELD_RE = re.compile(
    r"^\s{4}(?P<decl>.+?);//\s*Offset=(?P<off>0x[0-9a-fA-F]+)"
    r"\s+Size=(?P<size>0x[0-9a-fA-F]+)")


SCALAR_W = {
    "u8": 1, "s8": 1, "char": 1, "bool": 1,
    "u16": 2, "s16": 2, "short": 2,
    "u32": 4, "s32": 4, "int": 4, "long": 4, "f32": 4, "float": 4,
    "u64": 8, "s64": 8, "f64": 8, "double": 8,
}
ARR_RE = re.compile(r"\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\]")


def decl_width(decl):
    """Byte width of a project-header field declaration, or None."""
    if "*" in decl:
        base = 4
    else:
        head = decl.split("[")[0].strip().split()
        base = None
        for tok in head:
            if tok in SCALAR_W:
                base = SCALAR_W[tok]
                break
        if base is None:
            return None
    for n in ARR_RE.findall(decl):
        base *= int(n, 0)
    return base


STRUCT_BODY_RE = re.compile(
    r"typedef\s+struct\s+(?P<tag>\w+)?\s*\{(?P<body>.*?)\}\s*(?P<name>\w+)\s*;",
    re.S)
FIELD_LINE_RE = re.compile(
    r"^[ \t]*(?:/\*\s*(?P<mark>0x[0-9A-Fa-f]+)\s*\*/\s*)?"
    r"(?P<decl>[A-Za-z_][^;{}]*?)\s*;", re.M)


def compute_layout(body, known, name_hint=""):
    """Walk a struct body and accumulate real offsets from declared sizes.

    Needed because the project's file-local view structs (screensaver's
    PanelConfigBlob, critter's CritterPackedType) express layout IMPLICITLY
    -- nested types and array dimensions, not /* 0xNN */ comments -- so a
    comment scraper sees nothing. Alignment is natural (PPC EABI).
    """
    fields, off = [], 0
    for m in FIELD_LINE_RE.finditer(body):
        decl = re.sub(r"\s+", " ", m.group("decl")).strip()
        if not decl or decl.startswith(("//", "/*")):
            continue
        nm = NAME_RE.search(re.sub(r"\[[^\]]*\]", "", decl))
        if not nm:
            continue
        fname = nm.group(1)
        # width: scalar/pointer, else a known struct type
        w = decl_width(decl)
        align = None
        if w is None:
            toks = decl.split("[")[0].strip().split()
            base_ty = None
            for t in toks[:-1]:
                if t in known:
                    base_ty = t
            if base_ty is None:
                continue
            unit = known[base_ty].get("size")
            if not unit:
                continue
            count = 1
            for n in ARR_RE.findall(decl):
                count *= int(n, 0)
            w = unit * count
            align = 4
        else:
            scalar = w
            for n in ARR_RE.findall(decl):
                scalar //= int(n, 0) if int(n, 0) else 1
            align = min(max(scalar, 1), 4) if scalar in (1, 2, 4, 8) else 4
            if scalar == 8:
                align = 8
        # explicit /* 0xNN */ marker overrides the running cursor
        if m.group("mark"):
            off = int(m.group("mark"), 16)
        elif align:
            off = (off + align - 1) // align * align
        fields.append({
            "offset": off, "name": fname, "decl": decl,
            "pad": bool(PADNAME_RE.match(fname)), "size": w,
        })
        off += w
    return fields, off


def load_local_structs(path, root):
    """Parse the TU's OWN file-local view structs as candidates.

    AGENTS.md: 'before inventing ANY file-local view, grep the TU for an
    existing typedef struct of the same purpose -- critter.c accumulated two
    CONFLICTING partial reconstructions of one struct because nobody
    checked.' This makes that check automatic.
    """
    text = strip_comments_keep_offsets(
        path.read_text(encoding="utf-8", errors="replace"))
    out, known = {}, {}
    for m in STRUCT_BODY_RE.finditer(text):
        name = m.group("name") or m.group("tag")
        if not name:
            continue
        fields, size = compute_layout(m.group("body"), known, name)
        sm = re.search(r"\}\s*" + re.escape(name) + r"\s*;\s*/\*\s*size\s*"
                       r"(0x[0-9A-Fa-f]+)", text[m.start():m.end() + 60], re.I)
        rec = {
            "size": int(sm.group(1), 16) if sm else size,
            "fields": fields,
            "src": relpath(path, root) + " (file-local)",
            "authority": "file-local view struct in this TU",
        }
        out[name] = rec
        known[name] = rec
    return out


KEEP_OFF_RE = re.compile(r"/\*(?!\s*0x[0-9A-Fa-f]+\s*\*/).*?\*/|//[^\n]*", re.S)


def strip_comments_keep_offsets(text):
    """Blank comments EXCEPT bare /* 0xNN */ offset markers."""
    return KEEP_OFF_RE.sub(
        lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def load_project_headers(root):
    out = {}
    for h in sorted((root / "include").rglob("*.h")):
        lines = h.read_text(encoding="utf-8", errors="replace").split("\n")
        stack, cur = [], None
        for idx, raw in enumerate(lines):
            m = HDR_OPEN_RE.match(raw)
            if m:
                if cur:
                    stack.append(cur)
                cur = {"name": m.group(1) or m.group(2), "fields": []}
                continue
            if cur and re.match(r"^\s*\}", raw):
                cm = HDR_CLOSE_RE.match(raw)
                name = (cm.group(1) if cm else None) or cur["name"]
                sm = re.search(r"(?:size|sizeof\s*==)\s*(0x[0-9A-Fa-f]+)", raw,
                               re.I)
                if name:
                    out[name] = {
                        "size": int(sm.group(1), 16) if sm else None,
                        "fields": sorted(cur["fields"],
                                         key=lambda f: f["offset"]),
                        "src": f"{h.relative_to(root)}".replace("\\", "/"),
                        "authority": "project header (GC-verified)",
                    }
                cur = stack.pop() if stack else None
                continue
            if not cur:
                continue
            m = HDR_LEAD_RE.match(raw)
            off = decl = None
            if m:
                off, decl = int(m.group(1), 16), m.group("decl")
            else:
                m = HDR_TRAIL_RE.match(raw)
                if m:
                    off, decl = int(m.group(2), 16), m.group("decl")
            if off is None:
                continue
            decl = decl.strip()
            nm = NAME_RE.search(decl)
            fname = nm.group(1) if nm else f"unk{off:04X}"
            cur["fields"].append({
                "offset": off, "name": fname, "decl": decl,
                "pad": bool(PADNAME_RE.match(fname)),
                "size": decl_width(decl),
            })
    return out


def load_pdb(root):
    """Parse research/xbox_symbols/*.h -- 1885 structs, vs the 227 that
    xbox_structs.tsv (and therefore `gdlmem struct`) indexes."""
    out = {}
    xs = root / "research" / "xbox_symbols"
    if not xs.is_dir():
        return out
    for h in sorted(xs.glob("*.h")):
        cur = None
        for raw in h.read_text(encoding="utf-8", errors="replace").split("\n"):
            m = PDB_OPEN_RE.match(raw)
            if m:
                name = m.group("name")
                key = name
                if name == "__unnamed" or key in out:
                    key = f"{name}#Id{m.group('id')}"
                cur = {"size": int(m.group("size"), 16), "fields": [],
                       "src": f"research/xbox_symbols/{h.name}",
                       "id": m.group("id"), "anon": name == "__unnamed",
                       "authority": "Xbox PDB (reference only -- verify vs GC)"}
                out[key] = cur
                continue
            if cur is None:
                continue
            if raw.startswith("}"):
                cur = None
                continue
            m = PDB_FIELD_RE.match(raw)
            if not m:
                continue
            decl = m.group("decl").strip()
            if "(" in decl and "*" in decl.split("(")[0]:
                pass
            nm = NAME_RE.search(re.sub(r"\[[^\]]*\]", "", decl))
            cur["fields"].append({
                "offset": int(m.group("off"), 16),
                "size": int(m.group("size"), 16),
                "name": nm.group(1) if nm else "?",
                "decl": decl, "pad": False,
            })
    for v in out.values():
        v["fields"].sort(key=lambda f: f["offset"])
    return out


def score_candidate(layout, observed, rec_size):
    """How well does `layout` explain the observed offset/width evidence?

    A hit requires the offset to exist AND the width to agree. Offset-only
    agreement is nearly worthless: any densely-packed 4-byte struct matches
    any other one's offsets, which is how an early build of this tool
    "named" a psfx damage row from include/game/psys.h. Width agreement is
    what actually discriminates, so a width conflict scores NEGATIVE.
    """
    fields = layout["fields"]
    if not fields:
        return None
    byoff = {f["offset"]: f for f in fields}
    hit = wrong_width = miss = unknown_w = 0
    for off, info in observed.items():
        f = byoff.get(off)
        if f is None:
            miss += 1
            continue
        fw, ow = f.get("size"), info["width"]
        if fw is None or ow is None:
            unknown_w += 1
        elif fw == ow:
            hit += 1
        else:
            wrong_width += 1
    total = len(observed)
    if not total:
        return None
    size_ok = (rec_size is not None and layout.get("size") == rec_size)
    score = (hit - 1.5 * wrong_width + 0.25 * unknown_w) / float(total)
    if size_ok:
        score += 0.5
    over = max(observed)
    if layout.get("size") and over >= layout["size"]:
        score -= 0.6                       # observed access past the end
    return {"score": round(score, 3), "hit": hit, "wrong_width": wrong_width,
            "miss": miss, "unknown_width": unknown_w, "total": total,
            "size_match": size_ok,
            "nameable": (wrong_width == 0 and size_ok and total
                         and hit >= max(4, 0.75 * total))}


# ------------------------------------------------------------------- clustering
def cluster(sites, text, by_function=False):
    groups = defaultdict(list)
    for s in sites:
        if s["base"]:
            key = f"{s['base']} @{s['fn']}" if by_function else s["base"]
            groups[key].append(s)
    out = {}
    for base, ss in groups.items():
        observed = {}
        for s in ss:
            o = observed.setdefault(s["const"], {
                "count": 0, "widths": Counter(), "types": Counter(),
                "lines": [], "fns": Counter(), "width": None, "ptr": False})
            o["count"] += 1
            if s["width"]:
                o["widths"][s["width"]] += 1
            o["types"][s["type"]] += 1
            o["fns"][s["fn"]] += 1
            o["ptr"] = o["ptr"] or s["is_ptr"]
            if len(o["lines"]) < 3:
                o["lines"].append((s["line"], s["fn"], s["src"][:110]))
        for o in observed.values():
            o["width"] = o["widths"].most_common(1)[0][0] if o["widths"] else None
        strides = Counter()
        for s in ss:
            for _idx, st in s["terms"]:
                strides[st] += 1
        esc = re.escape(base.split(" @")[0])
        # `base += N` -- a walked record pointer; strongest stride evidence.
        for m in re.finditer(esc + r"\s*\+=\s*(0x[0-9A-Fa-f]+|\d+)", text):
            val = as_int(m.group(1))
            if val is not None:
                strides[val] += 4
        # `base = <expr> + i * N` / `+ N * i` -- indexed record selection.
        for m in re.finditer(esc + r"\s*=\s*([^;]{0,160});", text):
            rhs = FLOAT_LIT_RE.sub(" ", m.group(1))
            for mm in re.finditer(
                    r"\*\s*(0x[0-9A-Fa-f]+|\d+)\b|\b(0x[0-9A-Fa-f]+|\d+)\s*\*",
                    rhs):
                val = as_int(mm.group(1) or mm.group(2))
                if val is not None and val >= 8:
                    strides[val] += 3
            for mm in re.finditer(r"<<\s*(\d+)", rhs):
                strides[1 << int(mm.group(1))] += 3
        out[base] = {"sites": len(ss), "observed": observed,
                     "strides": strides,
                     "update": any(s["update"] for s in ss),
                     "fns": Counter(s["fn"] for s in ss)}
    return out


def infer_record_size(info):
    """Best stride > max observed offset -> the record size."""
    if not info["strides"]:
        return None, []
    top = info["strides"].most_common()
    maxoff = max(info["observed"]) if info["observed"] else 0
    good = [(st, n) for st, n in top if st > maxoff and st >= 8]
    return (good[0][0] if good else None), top


def draft_struct(name, info, rec_size, named_map=None):
    """Emit the padded C skeleton."""
    named_map = named_map or {}
    observed = info["observed"]
    lines = [f"/* DRAFT -- generated by tools/gdl/structdraft.py from"
             f" {info['sites']} observed accesses.",
             " * Field names are ONLY those a matched candidate supplied;"
             " everything else",
             " * is unkNN by construction. VERIFY offsets against the GC"
             " target asm before",
             " * adopting, then gate with defake_gate.py. */",
             f"typedef struct {name} {{"]
    cursor = 0
    for off in sorted(observed):
        o = observed[off]
        if off < cursor:
            lines.append(f"    /* !! 0x{off:X} overlaps the previous field"
                         f" ({o['count']} sites, width {o['width']}) --"
                         f" union or mis-sized neighbour */")
            continue
        if off > cursor:
            lines.append(f"    u8  _pad{cursor:04X}[0x{off - cursor:X}];")
        w = o["width"] or 4
        base_ty = "void*" if o["ptr"] else CTYPE.get(w, "u32")
        tys = ",".join(t for t, _ in o["types"].most_common(3))
        fname = named_map.get(off) or f"unk{off:04X}"
        fns = ",".join(f for f, _ in o["fns"].most_common(2))
        lines.append(
            f"    /* 0x{off:04X} */ {base_ty:6s} {fname};"
            f"  /* {o['count']}x [{tys}] {fns} */")
        cursor = off + w
    if rec_size and rec_size > cursor:
        lines.append(f"    u8  _pad{cursor:04X}[0x{rec_size - cursor:X}];")
        cursor = rec_size
    lines.append(f"}} {name};  /* size 0x{cursor:X}"
                 + (" (stride-confirmed)" if rec_size else " (observed span"
                    " only -- true size unknown)") + " */")
    return "\n".join(lines)


def resolve_unit(root, unit):
    unit = unit.replace("\\", "/")
    if unit.startswith("src/"):
        unit = unit[4:]
    for cand in (unit, unit + ".c", unit + ".cpp"):
        p = root / "src" / cand
        if p.is_file():
            return p
    matches = [p for p in (root / "src").rglob("*.c*")
               if unit.rstrip(".c") in str(p).replace("\\", "/")]
    if len(matches) == 1:
        return matches[0]
    return None


def main():
    ap = argparse.ArgumentParser(
        description="Draft a struct from a TU's raw-offset accesses.",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("unit", nargs="?", help="TU path, e.g. game/sfx/psfx.c")
    ap.add_argument("--file", help="scan this exact file instead of resolving"
                                   " `unit` under src/ (use with a checkout of"
                                   " an older revision to draft from history)")
    ap.add_argument("--base", help="only this base expression")
    ap.add_argument("--by-function", action="store_true",
                    help="cluster by (base, enclosing function) instead of by"
                         " base alone -- use when one variable name is reused"
                         " for different records across functions (the"
                         " 'multi-record cluster' warning flags this)")
    ap.add_argument("--min-sites", type=int, default=4,
                    help="skip clusters smaller than this (default 4)")
    ap.add_argument("--max-clusters", type=int, default=12)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--verify", metavar="TYPE",
                    help="score the draft against an already-recovered struct")
    ap.add_argument("--candidates", type=int, default=4,
                    help="how many candidate layouts to rank (default 4)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if args.file:
        path = Path(args.file).resolve()
        if not path.is_file():
            print(f"structdraft: --file {args.file!r} not found",
                  file=sys.stderr)
            return 2
    elif args.unit:
        path = resolve_unit(root, args.unit)
        if not path:
            print(f"structdraft: cannot resolve unit {args.unit!r} under"
                  f" {root / 'src'}", file=sys.stderr)
            return 2
    else:
        ap.error("give a unit path or --file")

    sites, text = scan_tu(path)
    clusters = cluster(sites, text, by_function=args.by_function)
    if args.base:
        clusters = {k: v for k, v in clusters.items() if k == args.base}
        if not clusters:
            print(f"structdraft: no sites with base {args.base!r}; bases seen:"
                  f" {sorted(set(s['base'] for s in sites if s['base']))[:25]}",
                  file=sys.stderr)
            return 2
    ranked = sorted(clusters.items(), key=lambda kv: -kv[1]["sites"])
    ranked = [(b, i) for b, i in ranked if i["sites"] >= args.min_sites]
    ranked = ranked[:args.max_clusters]

    proj = load_project_headers(root)
    pdb = load_pdb(root)
    local = load_local_structs(path, root)
    rel = relpath(path, root)

    result = {"tu": rel, "total_sites": len(sites),
              "clusters_total": len(clusters), "clusters": []}

    for base, info in ranked:
        rec_size, stride_top = infer_record_size(info)
        observed = info["observed"]
        cands = []
        for src, table in (("local", local), ("project", proj), ("pdb", pdb)):
            for name, layout in table.items():
                sc = score_candidate(layout, observed, rec_size)
                if not sc or sc["score"] <= 0.15:
                    continue
                cands.append({"name": name, "origin": src,
                              "size": layout.get("size"),
                              "authority": layout["authority"],
                              "src": layout["src"], **sc})
        ORIGIN_RANK = {"local": 0, "project": 1, "pdb": 2}
        cands.sort(key=lambda c: (ORIGIN_RANK[c["origin"]], -c["score"]))
        cands = cands[:args.candidates]

        # Names are adopted ONLY from a candidate that agrees on record size
        # and every observed width. Anything weaker stays unkNN and is
        # reported as a lead, never written into the draft.
        named_map = {}
        named_from = None
        if cands and cands[0].get("nameable"):
            named_from = cands[0]
            top = {"local": local, "project": proj,
                   "pdb": pdb}[named_from["origin"]][named_from["name"]]
            for f in top["fields"]:
                if f["offset"] in observed and not f["pad"]:
                    named_map[f["offset"]] = f["name"]

        ident = re.sub(r"\W+", "_", base).strip("_") or "Base"
        sname = ident[:1].upper() + ident[1:] + "View"
        span = (max(observed) - min(observed)) if observed else 0
        # Any stride evidence smaller than the observed span means the sites
        # cannot all belong to one record of that stride -- either the
        # variable name is reused for a second record, or the base walks an
        # outer array. Either way the single-struct draft would be wrong.
        # Only a plausible RECORD stride can contradict the span. A bare
        # `i*4` element step is not a record size and must not trip the
        # warning (combat.c's `cs` did exactly that).
        top_stride = max((s for s, _n in stride_top if s >= 0x10),
                         default=0) if stride_top else 0
        eff = rec_size or top_stride
        entry = {
            "base": base, "sites": info["sites"], "record_size": rec_size,
            "span": span, "stride_hint": top_stride,
            "multi_record": bool(eff and span >= 2 * eff),
            "strides": stride_top[:5], "update_web": info["update"],
            "functions": info["fns"].most_common(6),
            "offsets": {hexoff(o): {
                "count": v["count"], "width": v["width"],
                "types": dict(v["types"]), "ptr": v["ptr"],
                "examples": v["lines"]} for o, v in sorted(observed.items())},
            "candidates": cands,
            "named_from": named_from["name"] if named_from else None,
            "draft": draft_struct(sname, info, rec_size, named_map),
        }
        result["clusters"].append(entry)

    if args.verify:
        gt = local.get(args.verify) or proj.get(args.verify) \
            or pdb.get(args.verify)
        if not gt:
            print(f"structdraft: --verify type {args.verify!r} not found",
                  file=sys.stderr)
            return 2
        gto = {f["offset"]: f for f in gt["fields"] if not f["pad"]}
        agg = {}
        for e in result["clusters"]:
            for k, v in e["offsets"].items():
                o = -int(k[3:], 16) if k.startswith("-0x") else int(k, 16)
                cur = agg.setdefault(o, {"count": 0, "width": v["width"]})
                cur["count"] += v["count"]
        exact = [o for o in agg if o in gto]
        interior = [o for o in agg if o not in gto and any(
            f["offset"] < o < f["offset"] + 64 for f in gt["fields"])]
        result["verify"] = {
            "type": args.verify, "source": gt["src"],
            "ground_truth_named_fields": len(gto),
            "observed_offsets": len(agg),
            "landed_on_named_field": len(exact),
            "landed_interior_or_pad": len(agg) - len(exact),
            "coverage_of_ground_truth": (
                round(len(exact) * 100.0 / len(gto), 1) if gto else None),
            "precision_of_draft": (
                round(len(exact) * 100.0 / len(agg), 1) if agg else None),
            "missed_named_fields": sorted(
                f"0x{o:X}:{gto[o]['name']}" for o in gto if o not in agg)[:30],
            "note": "interior hits are sub-field accesses (arr[i], matrix"
                    " rows) -- correct code, not draft errors",
        }
        _ = interior

    if args.json:
        print(json.dumps(result, indent=1, default=str))
        return 0

    print(f"structdraft: {rel}")
    print(f"  {result['total_sites']} raw-offset sites in"
          f" {result['clusters_total']} base clusters"
          f" (showing {len(result['clusters'])} with >= {args.min_sites}"
          f" sites)\n")
    for e in result["clusters"]:
        print("=" * 72)
        print(f"BASE {e['base']!r}   {e['sites']} sites"
              + (f"   record size 0x{e['record_size']:X}"
                 if e["record_size"] else "   record size UNKNOWN"))
        if e["update_web"]:
            print("  !! load-update web present (`base += N`) --"
                  " claim.law.lwzu-idiom-web-retention applies;"
                  " convert AROUND it")
        if e.get("multi_record"):
            st = e["record_size"] or e["stride_hint"]
            print(f"  !! MULTI-RECORD CLUSTER: offsets span 0x{e['span']:X}"
                  f" but the record stride is 0x{st:X} -- this base almost"
                  f" certainly covers TWO different records (a reused"
                  f" variable name, or a walk over an outer array)."
                  f" Re-run with --by-function and split the offsets before"
                  f" trusting this draft.")
        if e["strides"]:
            print("  strides seen: " + ", ".join(
                f"0x{s:X}(x{n})" for s, n in e["strides"]))
        print("  functions: " + ", ".join(f"{f}({n})" for f, n in
                                          e["functions"]))
        print(f"\n  offsets ({len(e['offsets'])} distinct):")
        for k, v in e["offsets"].items():
            ex = v["examples"][0] if v["examples"] else (0, "", "")
            print(f"    {k:>8s}  w{v['width']}  x{v['count']:<3d}"
                  f" {'ptr' if v['ptr'] else '   '}  L{ex[0]} {ex[1]}:"
                  f" {ex[2][:70]}")
        if e["candidates"]:
            print("\n  candidate layouts (LEADS -- verify before adopting):")
            for c in e["candidates"]:
                flag = " <== names adopted" if c.get("nameable") else ""
                print(f"    {c['score']:5.2f}  {c['name']:28s}"
                      f" size={c['size'] and hex(c['size'])}"
                      f" hit={c['hit']}/{c['total']} wrongw={c['wrong_width']}"
                      f" [{c['origin']}] {c['src']}{flag}")
            if not e["named_from"]:
                print("    (no candidate met the naming bar: exact record-size"
                      " match + zero width conflicts -> draft stays unkNN)")
        else:
            print("\n  candidate layouts: NONE -- this record is unrecovered"
                  " project-wide")
        print(f"\n{e['draft']}\n")

    if "verify" in result:
        v = result["verify"]
        print("=" * 72)
        print(f"VERIFY against {v['type']}  ({v['source']})")
        print(f"  ground-truth named fields : {v['ground_truth_named_fields']}")
        print(f"  offsets the draft observed: {v['observed_offsets']}")
        print(f"  landed on a named field   : {v['landed_on_named_field']}"
              f"  (precision {v['precision_of_draft']}%)")
        print(f"  coverage of ground truth  : "
              f"{v['coverage_of_ground_truth']}%")
        print(f"  note: {v['note']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
