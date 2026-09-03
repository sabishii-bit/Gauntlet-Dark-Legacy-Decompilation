"""WHOEMITS: does OUR build emit this instruction shape anywhere -- and does
it emit it in a function that is already byte-exact?

IMPORTABLE CORE: compile_pattern, match_sites, exactness_map, scan_object
— pure over objdump text and parsed objects; no build, no side effects.

THE PARK-GATE SCREEN (run-45 item 2). A park that says "MWCC will not emit
this shape" is refuted the moment our own build emits it, and refuted at the
strongest possible strength when it emits it inside a function whose bytes
already MATCH RETAIL: that function's source is a worked example of the shape
under this project's exact cflags. tools/gdl/composed_census/
nm_branchpair_census.py is the 40-line precedent for one hard-coded shape;
this generalises it to an arbitrary consecutive-mnemonic pattern and adds the
column that makes it a screen rather than a count -- the EXACT one.

    python tools/gdl/composed_census/t15_whoemits.py frsqrte
    python tools/gdl/composed_census/t15_whoemits.py 'cmpwi' 'b(eq|ne)'
    python tools/gdl/composed_census/t15_whoemits.py lwzu --operands 'r1,'
    python tools/gdl/composed_census/t15_whoemits.py stfsu --side target
    python tools/gdl/composed_census/t15_whoemits.py fmul --exact-only --top 20

PATTERN. Each positional argument is a regex matched against ONE
instruction's mnemonic, and the arguments together must match CONSECUTIVE
instructions. `--operands RE` additionally requires that regex somewhere in
the operand text of the LAST instruction of the match. Anchoring is implicit
(`re.fullmatch` on the mnemonic), so `b` matches only the unconditional
branch and `b.*` matches every branch.

SIDES. `--side ours` (default) reads the RAW pre-postprocess bodies under
build/GUNE5D/src/**/.postprocess/body when they exist, because a webfrank
rule rewrites register fields AFTER the compiler and a postprocessed body is
not evidence about what MWCC emits. `--side target` reads the dtk-extracted
retail objects, which answers the other half: does RETAIL contain the shape.

THE EXACT COLUMN is computed by comparing our raw body bytes against the
target's for the same symbol: `exact` means byte-identical (so the shape is
source-reachable under these cflags, in code we have), `pinned` means a
webfrank rule serves the function, and `open` means neither.

VERDICT LINE. `sites`/`functions` are counts; `EXACT-FUNCTION SITES` is the
one that settles a park. Zero exact sites is NOT proof the compiler cannot
emit the shape -- it is the absence of a worked example, which is a weaker
statement, and the tool says so rather than letting a reader round it up.

LIMIT. The pattern language is consecutive MNEMONICS plus an operand regex;
it has no branch-DESTINATION arithmetic, so a shape defined by where a branch
lands (nm_branchpair_census.py's `branch over exactly one instruction`) is not
expressible here -- use that tool for that shape.

TWO-SIDED CALIBRATION, measured at f9e0853f8 on a green build:

  POSITIVES (shapes our build demonstrably emits, with worked examples)
    frsqrte   81 sites / 51 functions, 37 EXACT-function sites
    fnmsub   362 sites / 71 functions, 165 EXACT-function sites
    stfsu     14 sites /  7 functions,   5 EXACT-function sites
  NEGATIVES (shapes it never emits: the screen returns clean zeros)
    fsqrt      0 sites -- the 750 has no fsqrt and MWCC never emits one
    lswi       0 sites
  THE THIRD ANSWER, which is why the verdict line is worded as it is
    eieio      1 site / 1 function, 0 EXACT-function sites: the shape exists
               in our build and there is NO worked example of it in a
               byte-exact function. Counting that as either "yes" or "no"
               would be wrong; it is "yes, unverified".

A FINDING FROM THE CALIBRATION ITSELF. `stfsu` is the shape behind a standing
user-facing question (the swbos stfsu-vs-stfs class, open since run 36): our
build emits it in three byte-EXACT functions -- MSL/sincos::__sinit_trigf_c,
game/sys/recorder::LoadStage and game/world/worldcol::WorldObjCollide -- so
any park resting on "source cannot produce stfsu here" is refuted by worked
example. `--side target` adds the other half: retail carries 17 sites in 7
functions, and game/game/combat::someone_will_be_off_screen is a function
where RETAIL emits stfsu and our build does not.
"""
import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402

OBJDUMP = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump.exe")
if not os.path.exists(OBJDUMP):
    OBJDUMP = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump")
OURS = os.path.join(ROOT, "build", "GUNE5D", "src")
TARGET = os.path.join(ROOT, "build", "GUNE5D", "obj")
WEBFRANK_JSON = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")

LINE = re.compile(
    r"^\s*([0-9a-f]+):\s+[0-9a-f ]+\t([a-z][a-z0-9._+]*)\s*(.*)$")
SYM = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")


def compile_pattern(patterns):
    """Mnemonic regexes, fullmatch-anchored so `b` is not `beq`."""
    return [re.compile(p + r"\Z") for p in patterns]


def match_sites(instructions, pattern, operands=None):
    """[(offset, "mnem mnem ...")] for every consecutive match."""
    hits = []
    width = len(pattern)
    for index in range(len(instructions) - width + 1):
        window = instructions[index:index + width]
        if not all(rule.match(word[1]) for rule, word in zip(pattern, window)):
            continue
        if operands and not operands.search(window[-1][2]):
            continue
        hits.append((window[0][0], " ".join(word[1] for word in window)))
    return hits


def scan_object(path, pattern, operands=None):
    """{function: [(offset, matched mnemonics)]} for one object file."""
    try:
        text = subprocess.run([OBJDUMP, "-d", "-j", ".text", path],
                              capture_output=True, text=True,
                              timeout=180).stdout
    except Exception:  # noqa: BLE001
        return {}
    hits, current, instructions = {}, None, []

    def flush():
        if current and instructions:
            rows = match_sites(instructions, pattern, operands)
            if rows:
                hits.setdefault(current, []).extend(rows)

    for line in text.splitlines():
        symbol = SYM.match(line.strip())
        if symbol:
            flush()
            current, instructions = symbol.group(1), []
            continue
        row = LINE.match(line)
        if row and current:
            instructions.append((int(row.group(1), 16), row.group(2),
                                 row.group(3)))
    flush()
    return hits


def _load(path):
    data = bytearray(open(path, "rb").read())
    return data, wf._sections(data)


def _text_functions(path):
    data, sections = _load(path)
    out = {}
    for symbol in wf._symbols(data, sections):
        if symbol.size and 0 <= symbol.section_index < len(sections):
            section = sections[symbol.section_index]
            if section.name == ".text":
                out[symbol.name] = bytes(
                    data[section.offset + symbol.value:][:symbol.size])
    return out


def exactness_map(unit):
    """{function: 'exact'|'open'} for one unit, RAW body against target."""
    our_path = raw_object(os.path.join(OURS, unit + ".o"))
    target_path = os.path.join(TARGET, unit + ".o")
    if not (os.path.exists(our_path) and os.path.exists(target_path)):
        return {}
    try:
        ours = _text_functions(our_path)
        targets = _text_functions(target_path)
    except Exception:  # noqa: BLE001
        return {}
    return {name: ("exact" if targets.get(name) == body else "open")
            for name, body in ours.items()}


def raw_object(unit_obj):
    head, tail = os.path.split(unit_obj)
    body = os.path.join(head, ".postprocess", "body", tail)
    return body if os.path.exists(body) else unit_obj


def pinned_functions():
    import json
    out = set()
    try:
        config = json.load(open(WEBFRANK_JSON))
    except (OSError, ValueError):
        return out
    for unit, rules in config["units"].items():
        for rule in rules:
            out.add((unit.replace("\\", "/"), rule["function"]))
    return out


def objects_for(side, unit_filter=None):
    """[(unit, object path)] -- the RAW body for `ours` wherever one exists.

    The `.postprocess` tree is never walked directly: it holds one body per
    pinned unit and would enumerate those units twice, once postprocessed and
    once raw. `raw_object()` maps each unit to its raw body instead.
    """
    base = OURS if side == "ours" else TARGET
    out = []
    for dirpath, _dirnames, filenames in os.walk(base):
        if ".postprocess" in dirpath.replace("\\", "/"):
            continue
        for name in sorted(filenames):
            if not name.endswith(".o"):
                continue
            path = os.path.join(dirpath, name)
            unit = os.path.relpath(path, base).replace("\\", "/")[:-2]
            if unit_filter and unit_filter not in unit:
                continue
            out.append((unit, raw_object(path) if side == "ours" else path))
    return sorted(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pattern", nargs="+",
                        help="one regex per consecutive instruction")
    parser.add_argument("--operands", help="regex the LAST match's operands"
                                           " must contain")
    parser.add_argument("--side", choices=("ours", "target"), default="ours")
    parser.add_argument("--unit")
    parser.add_argument("--exact-only", action="store_true")
    parser.add_argument("--top", type=int, default=30)
    args = parser.parse_args(argv)

    if not os.path.exists(OBJDUMP):
        print("objdump not found at %s -- run `ninja -j2` first" % OBJDUMP)
        return 2
    pattern = compile_pattern(args.pattern)
    operands = re.compile(args.operands) if args.operands else None
    pins = pinned_functions()

    rows, sites, exact_sites = [], 0, 0
    for unit, path in objects_for(args.side, args.unit):
        hits = scan_object(path, pattern, operands)
        if not hits:
            continue
        exactness = exactness_map(unit) if args.side == "ours" else {}
        for function, found in sorted(hits.items()):
            state = exactness.get(function, "?")
            if state != "exact" and (unit, function) in pins:
                state = "pinned"
            sites += len(found)
            if state == "exact":
                exact_sites += len(found)
            elif args.exact_only:
                continue
            rows.append((unit, function, state, found))

    print("PATTERN %s%s   side=%s"
          % (" ".join(args.pattern),
             (" operands~/%s/" % args.operands) if args.operands else "",
             args.side))
    print("  sites %d in %d function(s)" % (sites, len(rows)))
    if args.side == "ours":
        print("  EXACT-FUNCTION SITES: %d -- our build emits this shape inside"
              " %s byte-identical to retail." % (
                  exact_sites,
                  "functions that are" if exact_sites else "NO function"))
        if not exact_sites:
            print("  (Absence of a worked example is NOT proof the compiler"
                  " cannot emit the shape; it is the absence of an example.)")
    print()
    for unit, function, state, found in rows[:args.top]:
        where = ", ".join("0x%x %s" % row for row in found[:4])
        print("  %-9s %-26s %-34s %s"
              % (state, unit, function, where))
    if len(rows) > args.top:
        print("  ... %d more (--top)" % (len(rows) - args.top))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
