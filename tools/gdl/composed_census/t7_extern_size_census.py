#!/usr/bin/env python3
"""Cross-check every `extern T name[N]` against config/GUNE5D/symbols.txt.

A wrong array bound on a file-scope extern is not cosmetic and it is not
reliably score-visible either way, so it survives ordinary review:

  * it can be SCORE-VISIBLE, which is how the one known instance was found
    at all. src/game/ui/select.c:101 declares `extern char lbl_80347F4C[12]`
    while symbols.txt gives `size:0x8`. Twelve bytes sits ABOVE MWCC's
    8-byte small-data threshold and eight sits below it, so the wrong bound
    changes the ADDRESSING FORM the compiler selects — our `lis`+`addi`
    against the target's single `li rN,0 @lbl_80347F4C(EMB_SDA21)`. UB found
    it by hand while working update_class_spec
    (attempt.UB_update-class-spec-branch-pair-plus-sdata-size-are-one-
    coupled-lever.20260902.v1).
  * it can equally be SILENT — a bound that is merely too large changes no
    codegen at all until something reads past the real object.

One hand-found instance is not a population. This is the census: it reports
every declaration whose declared byte size disagrees with the linker's, so
the class can be sized instead of guessed at.

MEASURE-AND-REPORT ONLY. This tool never edits source; the rows are handed
to the lane that owns the fixes.

Run FROM THE REPOSITORY ROOT:
    python tools/gdl/composed_census/t7_extern_size_census.py
    python tools/gdl/composed_census/t7_extern_size_census.py --json
    python tools/gdl/composed_census/t7_extern_size_census.py --out PATH
"""
import argparse
import json
import os
import re
import sys

ROOT = os.getcwd()
VERSION = "GUNE5D"

# `name = section:0xADDR; // type:object size:0xNN ...`
SYMBOL_RE = re.compile(
    r"^\s*(\S+)\s*=\s*[^:]+:0x[0-9A-Fa-f]+\s*;\s*//\s*(.*)$")
SIZE_RE = re.compile(r"\bsize:0x([0-9A-Fa-f]+)")
TYPE_RE = re.compile(r"\btype:(\w+)")

# `extern <type> <name>[N][M];` — one declaration, one line. Deliberately
# does NOT match pointers (`extern T* name[]`): the element size there is a
# pointer's, and mixing the two would report a type error as a size error.
EXTERN_RE = re.compile(
    r"^\s*extern\s+(?:const\s+)?(?:struct\s+|unsigned\s+|signed\s+)?"
    r"(\w+)\s+(\w+)\s*((?:\[\s*(?:0[xX][0-9A-Fa-f]+|\d+)\s*\])+)\s*;")
DIM_RE = re.compile(r"\[\s*(0[xX][0-9A-Fa-f]+|\d+)\s*\]")

# Element sizes we can assert. A type absent from this table is REPORTED AS
# UNKNOWN rather than assumed: a struct whose layout this tool cannot see
# would otherwise generate a confident wrong row, which is worse than a gap.
ELEMENT_SIZES = {
    "char": 1, "s8": 1, "u8": 1, "bool": 1, "BOOL": 1,
    "short": 2, "s16": 2, "u16": 2,
    "int": 4, "long": 4, "s32": 4, "u32": 4, "f32": 4, "float": 4,
    "void": 4,
    "f64": 8, "double": 8, "s64": 8, "u64": 8,
}

SOURCE_SUFFIXES = (".c", ".cpp", ".h")
SCAN_DIRS = ("src", "include")


ADDR_RE = re.compile(r"=\s*([A-Za-z0-9_.]+):0x([0-9A-Fa-f]+)\s*;")


def symbol_table(root=None):
    """{symbol: {size, type, section, address, gap_to_next}}.

    `gap_to_next` is the distance to the next symbol in the same section and
    is the reliability check on `size`. THE SIZES IN symbols.txt ARE NOT ALL
    DECLARED SIZES — many are inferred by dtk from the layout, so a
    disagreement does not by itself mean the SOURCE is wrong. The census
    would be actively misleading without this column: `gPlayers` is recorded
    `size:0x9F1` (2545 bytes), which is not even divisible by 4 and so
    cannot be the size of a 4-element player array, while the source's
    `[4][0x335C]` is obviously the real shape. Reporting that as a source
    defect alongside a genuine one is the run-37 item-3 failure mode
    (conflating populations) in a new place.
    """
    root = root or ROOT
    path = os.path.join(root, "config", VERSION, "symbols.txt")
    out = {}
    if not os.path.exists(path):
        return out
    ordered = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            match = SYMBOL_RE.match(line)
            addr = ADDR_RE.search(line)
            if not match or not addr:
                continue
            name, comment = match.group(1), match.group(2)
            size = SIZE_RE.search(comment)
            kind = TYPE_RE.search(comment)
            entry = {
                "size": int(size.group(1), 16) if size else None,
                "type": kind.group(1) if kind else "?",
                "section": addr.group(1),
                "address": int(addr.group(2), 16),
                "gap_to_next": None,
            }
            out[name] = entry
            ordered.append(entry)
    by_section = {}
    for entry in ordered:
        by_section.setdefault(entry["section"], []).append(entry)
    for entries in by_section.values():
        entries.sort(key=lambda e: e["address"])
        for this, nxt in zip(entries, entries[1:]):
            this["gap_to_next"] = nxt["address"] - this["address"]
    return out


def declared_bytes(element_type, dims):
    """Declared size in bytes, or None when the element size is unknown."""
    element = ELEMENT_SIZES.get(element_type)
    if element is None:
        return None
    total = element
    for dim in dims:
        total *= int(dim, 16) if dim.lower().startswith("0x") else int(dim)
    return total


def scan_sources(root=None):
    """[(path, lineno, type, name, dims, declared_bytes)] for array externs."""
    root = root or ROOT
    rows = []
    for top in SCAN_DIRS:
        base = os.path.join(root, top)
        for folder, _dirs, files in os.walk(base):
            for name in files:
                if not name.endswith(SOURCE_SUFFIXES):
                    continue
                full = os.path.join(folder, name)
                try:
                    with open(full, encoding="utf-8",
                              errors="replace") as handle:
                        lines = handle.readlines()
                except OSError:
                    continue
                for number, line in enumerate(lines, 1):
                    match = EXTERN_RE.match(line)
                    if not match:
                        continue
                    element, symbol, dim_text = match.groups()
                    dims = DIM_RE.findall(dim_text)
                    rows.append((
                        os.path.relpath(full, root).replace(os.sep, "/"),
                        number, element, symbol, dims,
                        declared_bytes(element, dims)))
    return rows


def classify_row(declared, linker, gap):
    """Which of the three populations a disagreement belongs to.

    * `under_declared` — the source declares FEWER bytes than the linker
      records. The source sees less of the object than exists, and the
      linker size is corroborated by real data. This is the trustworthy
      bucket: gIdentityMatrix is declared [12] where the linker says 0x40,
      i.e. the 16 floats of a 4x4 matrix.
    * `over_declared_overlaps` — the source declares MORE bytes than the
      distance to the NEXT symbol, so the declaration as written runs into
      another named object. Either the bound or the map is wrong; both are
      worth a look.
    * `over_declared_within_gap` — declared exceeds the recorded size but
      still fits before the next symbol, which is exactly what an INFERRED
      size looks like. Weakest bucket, and the largest.

    The one INDEPENDENTLY CONFIRMED defect in the corpus — UB's
    `extern char lbl_80347F4C[12]` at select.c:101, where the linker says 8
    and the correction changes the addressing form MWCC selects — lands in
    `over_declared_overlaps`, the SMALLEST bucket. That is the census's
    validation: the bucket built to be actionable is the one the known-true
    row falls into, rather than the 46-row bucket it would have been swept
    into by a flat mismatch list.
    """
    if declared < linker:
        return "under_declared"
    if gap is not None and declared > gap:
        return "over_declared_overlaps"
    return "over_declared_within_gap"


def census(root=None):
    """Buckets of array-bound disagreements — pure over disk."""
    symbols = symbol_table(root)
    buckets = {"under_declared": [], "over_declared_overlaps": [],
               "over_declared_within_gap": []}
    unknown, unlinked, checked = [], [], 0
    for path, line, element, symbol, dims, declared in scan_sources(root):
        entry = {"file": path, "line": line, "symbol": symbol,
                 "element_type": element, "dims": dims,
                 "declared_bytes": declared}
        info = symbols.get(symbol)
        if info is None or info["size"] is None:
            unlinked.append(entry)
            continue
        entry["linker_bytes"] = info["size"]
        entry["symbol_type"] = info["type"]
        entry["section"] = info["section"]
        entry["gap_to_next"] = info["gap_to_next"]
        if declared is None:
            unknown.append(entry)
            continue
        checked += 1
        if declared != info["size"]:
            entry["delta"] = declared - info["size"]
            buckets[classify_row(declared, info["size"],
                                 info["gap_to_next"])].append(entry)
    total = sum(len(rows) for rows in buckets.values())
    return {"buckets": buckets, "mismatch_total": total,
            "unknown_element": unknown, "unlinked": unlinked,
            "checked": checked}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", default=None,
                        help="write JSON here (default under build/GUNE5D/)")
    args = parser.parse_args()

    result = census()
    if args.json or args.out:
        out = args.out or os.path.join(
            ROOT, "build", VERSION, "t7_extern_size_census.json")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1, sort_keys=True)
        print(f"wrote {out}")
        if not args.json:
            return 0

    headings = {
        "under_declared":
            "UNDER-DECLARED — the source sees FEWER bytes than the linker"
            " records. Trustworthy bucket: fix the bound.",
        "over_declared_overlaps":
            "OVER-DECLARED AND OVERLAPPING — the declaration runs past the"
            " NEXT symbol. Either the bound or the map is wrong.",
        "over_declared_within_gap":
            "OVER-DECLARED BUT WITHIN THE GAP — consistent with an INFERRED"
            " linker size. Weakest bucket; screen individually.",
    }
    print("-- extern array-bound census vs config/GUNE5D/symbols.txt --")
    print(f"[{result['checked']} declarations checked against a linker size;"
          f" {result['mismatch_total']} DISAGREE]")
    print("NOTE the sizes in symbols.txt are NOT all declared sizes — many"
          " are inferred by dtk from")
    print("the layout, so a disagreement does not by itself mean the SOURCE"
          " is wrong. gPlayers is")
    print("recorded size:0x9F1 (2545B), not divisible by 4 and so not a"
          " 4-player array size at all.")
    for name, heading in headings.items():
        rows = result["buckets"][name]
        print(f"\n== {name} ({len(rows)}) == {heading}")
        for row in sorted(rows, key=lambda r: -abs(r["delta"])):
            dims = "".join(f"[{d}]" for d in row["dims"])
            gap = row["gap_to_next"]
            print(f"  {row['file']}:{row['line']}"
                  f"  extern {row['element_type']} {row['symbol']}{dims};")
            print(f"      declared {row['declared_bytes']}B vs linker"
                  f" {row['linker_bytes']}B (delta {row['delta']:+d}),"
                  f" {row['section']}, gap-to-next"
                  f" {gap if gap is not None else '?'}B")
    print(f"\n[{len(result['unknown_element'])} skipped: element size unknown"
          " to this tool (struct/typedef element) —"
          " NOT cleared, just unmeasured]")
    print(f"[{len(result['unlinked'])} skipped: no sized symbol of that name"
          " in symbols.txt]")
    print("MEASURE-AND-REPORT ONLY: this tool never edits source.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
