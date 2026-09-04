"""mc_pragma_regions.py -- IMAGE-WIDE census of UNCLOSED `#pragma` regions.

Run 54, MEMCARD lane, work-order item 2.

WHY THIS EXISTS RATHER THAN al_pragma_extent.py: that tool answers a
different question and cannot be pointed at the image. Two limits, both
measured at 47ae4d37c:

  (1) its unit list comes from the ADDR16_LO census rosters
      (build/GUNE5D/al_addrlo_positive.json -- it exits if that file is
      absent), so it examines the 45 units carrying a roster function,
      not the 256 .c/.cpp files under src/; and
  (2) it tracks the single pragma family `opt_propagation`.

Its own printed footer says `units examined: 45   with the pragma: 21`.
This tool examines every source file and every stateful pragma family.

WHAT IT REPORTS. For each file it replays the pragma events in text order,
per family, treating `off`/`on` as state changes and `reset` as a return to
the family's entry state. A family still in a NON-DEFAULT state at end of
file is a DANGLING REGION. It is reported with the functions whose opening
brace falls inside it.

A DANGLING REGION IS A CANDIDATE, NOT A BUG, and the split below is the
whole point of the tool:

  top_of_file  -- no region of the SAME family was ever closed in this
                  file, so nothing shows the author using the closing idiom
                  for it. This is what an intentional TU-WIDE SETTING looks
                  like.
  after_paired -- the file already contains a properly paired region of the
                  same family, so the author demonstrably knew the closing
                  idiom. An opener with no closer here looks more like a
                  bracket that lost its closer.

NEITHER READING IS DECIDABLE FROM THE TEXT, and `game/sys/memcard` is the
worked example of why. It classifies `top_of_file`, and its opener does sit
above the first function definition in the file -- but it had already been
dispatched TWICE as a lost-closer bug on the strength of "an `off` with no
`reset`" alone. Measured both times (2026-09-01, 2026-09-04) it is
LOAD-BEARING: closing it after the function it appears to bracket aborts the
build on memCardErrorPrompt's webfrank body hash, and with the pinned
functions held OFF so it links, saveMount (EXACT -> real 24) and vmu_exists
(EXACT -> real 18) both come off their bytes. See
attempt.MC_memcard-pragma-leak-is-load-bearing-reproduced.20260904.v1.
So EVERY row here needs its own per-TU load-bearing test before anyone
"fixes" it:

    <close the region after the function it appears to bracket>
    python tools/gdl/defake_gate.py baseline <unit> --at-head
    python tools/gdl/defake_gate.py check <unit> --rebuild

and a row is only actionable if that comes back GATE OK with no function
off its bytes.

BARE `#pragma reset`: per claim.law.EO_a-bare-pragma-reset-does-not-close-
opt_propagation-off-and-a-text-reorder-relocates-the-leak-across-the-tu
.20260904.v1 a bare reset does NOT close an opt_propagation region. This
tool honours that law: a bare reset is counted and reported per file but
closes nothing. `--bare-closes-all` inverts that assumption so the two
readings can be compared, which is the same both-numbers discipline
al_pragma_extent applies to presence-vs-extent.

PARSER CALIBRATION, TWO-SIDED, against the REAL function roster of every
built object (fndiff.raw_signature) for the 36 census units that have one --
970 real functions, measured at b00285ce3:

    mc_pragma_regions  false-positive 60   missed 63
    al_pragma_extent   false-positive 97   missed 73

Both halves are reported because a coverage count is only trustworthy if the
denominator is. The residual is dominated by two classes, neither a parse
error in the harmful direction: (a) static helpers that exist in the source
and were INLINED or dead-stripped, so they are absent from the object but
genuinely inside the pragma region (most of the 60); and (b) one file,
game/movie/movieplayer, where an `extern "C" {` block wrapping the whole
translation unit means brace depth never returns to 0 and NOTHING parses --
that alone is 52 of the 63 misses. Case (b) is the dangerous direction,
because an unparsed file reports `covered 0` and reads as "no functions
affected", so it is flagged `parse_failed` in the output rather than printed
as a zero. On the one unit with an independent ground truth roster
(game/sys/memcard, defake_gate baseline) the parser is exact: 30 parsed,
30 covered, 30 real, zero either way.

IMPORTABLE CORE: scan_all_pragmas, replay, file_report

Usage: python tools/gdl/composed_census/mc_pragma_regions.py
           [--out PATH] [--src DIR] [--bare-closes-all] [--all]
       (from the repo root; --out defaults under build/GUNE5D/.
        --all also lists files whose regions are all closed.)
"""
import os
import re
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from al_pragma_extent import strip_code
from al_pragma_extent import scan_functions as _scan_functions_raw  # noqa: E402

# `#pragma <family> off|on|reset`, and the BARE `#pragma reset`.
P_STATE = re.compile(r"^\s*#\s*pragma\s+([A-Za-z_]\w*)\s+(off|on|reset)\b")
P_BARE = re.compile(r"^\s*#\s*pragma\s+reset\s*$")

# al_pragma_extent.scan_functions identifies a definition as "the last
# identifier before the '(' of a brace block", which also matches CONTROL
# FLOW: `while (...) { }`, `if (...) { }`, `switch (...) { }` and the
# `while (...)` tail of a do-while all parse as functions. MEASURED at
# b00285ce3 on the memcard TU this lane owns: raw scan_functions returns 35
# "functions", of which two are the SAME `while` (src/game/sys/memcard.c
# lines 1046 and 1068) -- while defake_gate's roster for the same file is
# 30 real functions. That is where the "33 of 35 functions" figure in
# attempt.AL_optpropagation-coverage-does-not-predict-the-addr16lo-class...
# .20260904.v1 and in this lane's own work order comes from: an inflated
# denominator AND an inflated numerator. al_pragma_extent then stores
# coverage in a DICT keyed by name, so the duplicate silently collapses and
# its two counts (33 covered of 35 parsed) are not even taken over the same
# population.
KEYWORDS = frozenset("""
if else while for switch do return sizeof case default goto break continue
typedef struct union enum static extern const volatile register auto signed
unsigned catch try throw new delete operator
""".split())


def strip_directives(code):
    """Blank every preprocessor directive line (and its `\\` continuations),
    preserving line structure.

    A multi-line `#define` with a brace body otherwise parses as a function
    definition: memcard's CARD_RETRY_PROMPT and mb_blit's SIH_OFF both did.
    The pragma scan runs on the UNSTRIPPED text, so this cannot hide an
    event from the replay.
    """
    out = []
    cont = False
    for line in code.split("\n"):
        directive = cont or line.lstrip().startswith("#")
        cont = directive and line.rstrip().endswith("\\")
        out.append(" " * len(line) if directive else line)
    return "\n".join(out)


def scan_functions(code, lines):
    """al_pragma_extent.scan_functions with preprocessor bodies, control-flow
    heads, and repeated names removed."""
    code = strip_directives(code)
    lines = code.split("\n")
    seen = set()
    out = []
    for name, head, open_ln, close_ln in _scan_functions_raw(code, lines):
        if name in KEYWORDS or name in seen:
            continue
        seen.add(name)
        out.append((name, head, open_ln, close_ln))
    return out


def scan_all_pragmas(lines):
    """-> ([(lineno, family, action)], bare_reset_lines).

    lineno is 0-based. `action` is 'off' | 'on' | 'reset'. A bare
    `#pragma reset` is returned separately: it names no family, so it
    cannot be replayed as one family's event.
    """
    events = []
    bare = []
    for ln, line in enumerate(lines):
        m = P_STATE.match(line)
        if m:
            events.append((ln, m.group(1), m.group(2)))
            continue
        if P_BARE.match(line):
            bare.append(ln)
    return events, bare


def replay(events, bare, nlines, bare_closes_all=False):
    """Replay pragma state in text order.

    -> (state_by_family, open_regions, paired_families)
       state_by_family: {family: [bool_active_per_line]}
       open_regions:    [{family, opened_line, closed: False}] still open at EOF
       paired_families: set of families that CLOSED at least one region
    """
    active = {}          # family -> opened_line, while a region is open
    per_line = {}        # family -> list of bools
    paired = set()
    order = sorted([(ln, f, a) for ln, f, a in events] +
                   [(ln, None, "bare") for ln in bare])

    fams = sorted({f for _l, f, _a in events})
    for f in fams:
        per_line[f] = [False] * (nlines + 1)

    idx = 0
    for ln in range(nlines):
        while idx < len(order) and order[idx][0] == ln:
            _l, fam, act = order[idx]
            idx += 1
            if act == "bare":
                if bare_closes_all:
                    for f in list(active):
                        paired.add(f)
                        del active[f]
                continue
            if act == "off":
                if fam not in active:
                    active[fam] = ln
            else:  # 'on' or 'reset' returns the family to its default
                if fam in active:
                    paired.add(fam)
                    del active[fam]
        for f in fams:
            per_line[f][ln] = f in active

    open_regions = [{"family": f, "opened_line": l + 1}
                    for f, l in sorted(active.items(), key=lambda kv: kv[1])]
    return per_line, open_regions, paired


def file_report(path, rel, bare_closes_all=False):
    text = open(path, encoding="utf-8", errors="replace").read()
    code = strip_code(text)
    lines = code.split("\n")
    events, bare = scan_all_pragmas(lines)
    if not events and not bare:
        return None
    per_line, open_regions, paired = replay(events, bare, len(lines),
                                            bare_closes_all)
    fns = scan_functions(code, lines)

    for reg in open_regions:
        fam = reg["family"]
        opened = reg["opened_line"] - 1
        covered = [name for name, _h, ol, _c in fns if per_line[fam][ol]]
        reg["covered_fns"] = covered
        reg["covered"] = len(covered)
        # Was a region of this SAME family properly closed earlier in the
        # file? Then the author knew the idiom here.
        reg["shape"] = "after_paired" if fam in paired else "top_of_file"
        # first event of this family at all?
        first = min(ln for ln, f, _a in events if f == fam)
        reg["is_first_event_of_family"] = (first == opened)

    return {
        "unit": rel,
        # zero parsed functions in a file that HAS pragmas is a parse
        # failure, not a finding: it reads as "no functions affected".
        "parse_failed": len(fns) == 0,
        "functions_parsed": len(fns),
        "events": len(events),
        "bare_resets": len(bare),
        "families": sorted({f for _l, f, _a in events}),
        "open_regions": open_regions,
    }


def main():
    out = os.path.join("build", "GUNE5D", "mc_pragma_regions.json")
    src = "src"
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]
    if "--src" in sys.argv:
        src = sys.argv[sys.argv.index("--src") + 1]
    bare_closes_all = "--bare-closes-all" in sys.argv
    show_all = "--all" in sys.argv

    files = []
    for dirpath, _dirs, names in os.walk(src):
        for n in sorted(names):
            if n.endswith((".c", ".cpp")):
                p = os.path.join(dirpath, n)
                rel = os.path.relpath(p, src).replace("\\", "/")
                rel = re.sub(r"\.(c|cpp)$", "", rel)
                files.append((p, rel))
    files.sort(key=lambda t: t[1])

    reports = []
    for p, rel in files:
        r = file_report(p, rel, bare_closes_all)
        if r:
            reports.append(r)

    dangling = [r for r in reports if r["open_regions"]]
    by_shape = {"top_of_file": [], "after_paired": []}
    for r in dangling:
        for reg in r["open_regions"]:
            by_shape[reg["shape"]].append((r["unit"], reg))

    res = {
        "note": "A DANGLING REGION IS A CANDIDATE, NOT A BUG. Each row needs "
                "its own per-TU load-bearing test (close the region, "
                "defake_gate baseline --at-head, defake_gate check "
                "--rebuild) before anyone fixes it; game/sys/memcard is the "
                "worked counter-example, an `after_paired` row that is "
                "nonetheless load-bearing.",
        "bare_reset_assumption": ("a bare `#pragma reset` CLOSES every open "
                                  "region (--bare-closes-all)"
                                  if bare_closes_all else
                                  "a bare `#pragma reset` closes NOTHING "
                                  "(claim.law.EO_a-bare-pragma-reset-does-"
                                  "not-close-opt_propagation-off...)"),
        "files_scanned": len(files),
        "files_with_pragmas": len(reports),
        "files_with_dangling_regions": len(dangling),
        "dangling_regions": sum(len(r["open_regions"]) for r in dangling),
        "reports": reports if show_all else dangling,
    }
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(res, open(out, "w", encoding="utf-8"), indent=1)

    print("=== UNCLOSED #pragma REGIONS (image-wide)")
    print("    %s" % res["bare_reset_assumption"])
    print()
    print("%-30s %-22s %6s %8s  %s" %
          ("unit", "family", "opened", "covered", "shape"))
    failed = 0
    for shape in ("after_paired", "top_of_file"):
        for unit, reg in by_shape[shape]:
            row = next(r for r in dangling if r["unit"] == unit)
            if row["parse_failed"]:
                failed += 1
                print("%-30s %-22s %6d %9s %s" %
                      (unit, reg["family"], reg["opened_line"],
                       "PARSE-FAIL", reg["shape"]))
                continue
            print("%-30s %-22s %6d %5d/%-3d %s" %
                  (unit, reg["family"], reg["opened_line"], reg["covered"],
                   row["functions_parsed"], reg["shape"]))
    print()
    if failed:
        print("  %d region(s) in files where NOTHING parsed (an `extern \"C\" "
              "{` block wrapping the TU does this) — their coverage is "
              "UNMEASURED, not zero." % failed)
    print("files scanned: %d   with any pragma: %d   with a dangling "
          "region: %d   dangling regions: %d"
          % (res["files_scanned"], res["files_with_pragmas"],
             res["files_with_dangling_regions"], res["dangling_regions"]))
    print("  by shape: after_paired %d, top_of_file %d"
          % (len(by_shape["after_paired"]), len(by_shape["top_of_file"])))
    print("wrote", out)


if __name__ == "__main__":
    main()
