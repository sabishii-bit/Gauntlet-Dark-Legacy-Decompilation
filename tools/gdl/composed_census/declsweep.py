#!/usr/bin/env python3
"""Declaration-order sweep for ONE function, gated on the relocation line.

    python tools/gdl/composed_census/declsweep.py <unit> <function>
    python tools/gdl/composed_census/declsweep.py <unit> <fn> --list-blocks
    python tools/gdl/composed_census/declsweep.py <unit> <fn> --block 2
    python tools/gdl/composed_census/declsweep.py <unit> <fn> --rounds 4
    python tools/gdl/composed_census/declsweep.py <unit> <fn> --out build/x.json

WHAT IT DOES. MWCC colours a function's value webs in an order derived from
the order its locals are DECLARED, so moving one declaration inside its own
block is a source-shape control a lane can actually justify: it changes no
semantics, no types and no statement order. This walks every single-
declaration move (decl i -> position j) of one block, rebuilds the owning
object through the REAL Ninja edge for each, measures, and keeps the source
restored in a `finally`.

WHY THE GATE IS THE POINT (run-63 item 2). A declaration move can buy
differing words by RELOCATING A DIFFERENT DATUM, and the TU-wide
`fndiff --relocs` count does not see it:

  * lane P2 committed `game/game/player::load_player_model_sub`
    real 110 -> 92, words 55 -> 44 (e62b44d5d) on the TU-wide count, then
    REVERTED it (6f266fdf7) after the per-function line read MNEMONIC
    DIVERGENCE 0 -> 5 and RELOC-SYMBOL MISMATCH 0 -> 2: the function had
    started materialising its `.rodata` and `.bss` bases in the wrong
    order, and the CLASS had gone RECOLOR -> SCHEDULE-REORDER.
  * lane B's `game/world/items::SetItem` 516w -> 502w is the same trap
    (two transposed `R_PPC_ADDR16_HA`).

So EVERY candidate here is gated per FUNCTION, on run-63 item 1's line:

    mnemonic divergence      must NOT rise
    RELOC-SYMBOL MISMATCH    must NOT rise   (name screen)
    anonymous-pool datum     must NOT rise   (value screen)
    fndiff --clean real      must DROP

Ranking is by `real`, then by differing words. A candidate that improves
`real` while raising either relocation count is REJECTED and counted, not
quietly ranked below the winner — the rejection tally is printed, because
"no winner" and "twenty winners that all transpose the same two
relocations" are different answers about the function.

BLOCKS, not just the leading run. Lane B kept a separate `b_hand.py` for
functions whose declarations are not in the leading run
(`tower::towerRecordLevelBeaten`, `pb_error::fn_800C1174`,
`combat::someone_will_be_off_screen`, `mb_particle::MBDrawPsys`), with the
block typed out by hand. This parses them instead: `--list-blocks` prints
every brace-opened block in the function with a declaration run of two or
more, and `--block N` sweeps one of them. Block 0 is the function's own
leading run, which is what a bare invocation sweeps.

COST, so a budget can be sized before the run. One candidate costs ONE Ninja
rebuild of the owning object plus one IN-PROCESS measurement. The rebuild
dominates and is a property of the TU, not of this tool; the measurement is
in-process precisely so it does not (three subprocesses per candidate — one
`wf_word_diff`, one `fndiff --clean`, one `fndiff --relocs` — is what lane
P2's predecessor paid, and it is several times the compile).

MEASURED 2026-09-08 in W:/Repositories/GDL-Claude-P5 on `game/game/player`
(7k lines, 92 functions): one bare
`ninja -j2 build/GUNE5D/src/game/game/player.o` after touching the source is
0.26 s, and the two full sweeps below took 32 s for 100 candidates and 28 s
for 81, i.e. **~0.3 s per candidate**. The candidate count is at most
n*(n-1) for a block of n declarations, before duplicate orders are removed:

    n =  4    12 candidates      n = 11   100 candidates   (~32 s here)
    n =  6    30                 n = 15   210              (~70 s here)
    n =  8    56                 n = 20   380              (~2 min here)

Scale by YOUR TU's compile time, which is the only term that varies: lane B
recorded `combat.c`, `select.c`, `newcam.c` and `sfx.c` as the slow ones and
blew a 430 s budget on one `combat::CameraSupervisor` sweep. Time one
`ninja -j2 build/GUNE5D/src/<unit>.o` first and multiply. `--rounds`
multiplies the total by the number of climbing rounds actually taken (a
round that finds no winner ends the climb, so `--rounds 4` on a capped
function still costs one round). `--budget SECONDS` stops at a candidate
boundary and reports what was covered, instead of being killed mid-write.

A KILLED RUN SKIPS THE `finally`. Print-and-check: this refuses to start
when the source file has uncommitted TRACKED changes, so a kill always
leaves a tree `git checkout -- <source>` restores exactly. The recovery
command is printed at the top of every run.

LIVE PROOF, verbatim, 2026-09-08 in W:/Repositories/GDL-Claude-P5.
`game/game/player::load_player_model_sub` is the function lane P2 capped as
RELOCATION-TRANSPOSITION, so NO CHANGE is the expected answer. Run TWICE,
either side of the merge that landed player's recovered `.rodata`:

  at a737babb2 (11 declarations, `fmt`, `tab` and `pot` all pointers):
      baseline: real 110 words 55 mnem 0 reloc 0 anon 0
      round 0: 100 candidate(s)
      CAPPED at real 110 words 55  [20 gate-rejected: mnem 12, reloc 20]
      (32s, 100 candidate(s) built, 0.3s each)

  at 133e9eecd, after `fmt = (u8*) lbl_80113AE0` became a real datum:
      declsweep game/game/player::load_player_model_sub block 0
          (10 declaration(s), depth 0, line 5004)
        RECOVERY after a kill: git checkout -- src/game/game/player.c
        baseline: real 110 words 55 mnem 0 reloc 0 anon 0
        round 0: 81 candidate(s)
        CAPPED at real 110 words 55  [9 gate-rejected: mnem 9, reloc 9]
        (28s, 81 candidate(s) built, 0.3s each)

`git status --short src/game/game/player.c` was empty after both, and at
a737babb2 the rebuilt object hashed identically to the pre-sweep one
(A333712462DBA9DA...), so the `finally` restores the tree exactly.

THE REJECTION COUNT is the number to read, not the CAPPED verdict. Lane P2
measured the 11-declaration block by hand and found "20 word-reducing
candidates and every single one transposes the same two relocations"; this
rejected exactly 20, and the agreement of two independent counts is the
calibration. The drop to 9 after the merge is the same fact getting
smaller: `fmt`, `tab` and `pot` pointed at three different data objects and
recovering one of them removed a whole rank of transposable orders. The
verdict did not move, because the remaining two still compete.
"""
import argparse
import io
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402

OK, NO_WINNER, REFUSED = 0, 0, 2

#: A declaration line at any block depth: optional storage/qualifier words,
#: a type, an optional pointer run, a name, optional array bounds, an
#: optional initializer. Deliberately conservative — a line this does not
#: match ENDS the run rather than being skipped, so the tool never reorders
#: across a statement. A comma-separated declarator list is deliberately NOT
#: matched: splitting one is a source edit, not a reordering.
#:
#: THE TYPE AND THE NAME MUST BE SEPARATED by whitespace or a pointer run.
#: Without that, the two identifier atoms backtrack INSIDE one word and the
#: assignment `rate = 100000.0f;` parses as the declaration `rat e = ...`.
#: Measured 2026-09-08: that false positive put two assignments
#: (`rate = 100000.0f; dt = 1;`, game/mb/mb_particle::MBDrawPsys line 970)
#: into a sweepable block, i.e. the sweeper would have PERMUTED TWO
#: STATEMENTS. It is the reason this pattern is not lane P2's.
DECL_RE = re.compile(
    r"^(?P<indent>[ \t]+)"
    r"(?:(?:static|const|volatile|register|unsigned|signed|struct|union|"
    r"enum|long|short)\s+)*"
    r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\*+\s*|\s+)"
    r"[A-Za-z_][A-Za-z0-9_]*\s*(?:\[[^\]]*\])*\s*"
    r"(?:=[^;]*)?;[ \t]*(?:/\*.*\*/|//.*)?$")

#: Statements whose first word can look like a type to the pattern above.
NOT_DECL = re.compile(r"^\s*(return|if|for|while|do|switch|goto|break|"
                      r"continue|else|case|default|sizeof)\b")


def read_source(path):
    """Text with line endings preserved. GDL sources are CRLF."""
    with io.open(path, "r", encoding="utf-8", newline="") as handle:
        return handle.read()


def write_source(path, text):
    with io.open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)


def source_path(unit):
    for ext in (".c", ".cpp"):
        candidate = os.path.join(ROOT, "src", unit + ext)
        if os.path.exists(candidate):
            return candidate
    return None


def object_path(unit):
    return "build/GUNE5D/src/%s.o" % unit


def function_span(text, fn):
    """(body_start, body_end) for `fn`'s outermost braces, or None.

    `body_start` is the index just after the opening brace. Braces inside
    string and character literals and inside comments are not counted; a
    function whose body cannot be closed returns None rather than a guess.
    """
    pattern = re.compile(r"^[A-Za-z_][^\n;{}]*\b" + re.escape(fn) +
                         r"\s*\([^;{}]*\)\s*\{", re.M)
    match = pattern.search(text)
    if not match:
        return None
    depth, index = 1, match.end()
    while index < len(text) and depth:
        char = text[index]
        if char in "\"'":
            index = _skip_literal(text, index)
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
            continue
        if text.startswith("//", index):
            end = text.find("\n", index)
            index = len(text) if end < 0 else end
            continue
        depth += (char == "{") - (char == "}")
        index += 1
    return None if depth else (match.end(), index - 1)


def _skip_literal(text, index):
    quote, index = text[index], index + 1
    while index < len(text):
        if text[index] == "\\":
            index += 2
            continue
        if text[index] == quote:
            return index + 1
        index += 1
    return index


def blocks(text, fn):
    """Every brace-opened block in `fn` with its leading declaration run.

    [{'start': int, 'decls': [line, ...], 'depth': int, 'line_no': int}],
    outermost first, in source order. Block 0 is the function's own leading
    run; the rest are the nested blocks lane B's `b_hand.py` needed typed
    out by hand.

    The run STOPS at the first blank line or non-declaration, which is the
    conservative reading: a false negative costs a block, and a false
    positive reorders a statement.
    """
    span = function_span(text, fn)
    if span is None:
        return None
    start, end = span
    body = text[start:end]
    found = []
    for offset, depth in _block_openings(body):
        decls, eol, cursor, run_start = [], None, offset, offset
        while True:
            line_end = body.find("\n", cursor)
            if line_end < 0:
                break
            raw = body[cursor:line_end]
            ending = "\r\n" if raw.endswith("\r") else "\n"
            line = raw[:-1] if raw.endswith("\r") else raw
            if not decls and line.strip() == "":
                cursor = run_start = line_end + 1
                continue
            if line.strip() == "" or NOT_DECL.match(line) \
                    or not DECL_RE.match(line):
                break
            decls.append(line)
            eol = eol or ending
            cursor = line_end + 1
        if len(decls) >= 2:
            found.append({"start": start + run_start, "decls": decls,
                          "eol": eol, "depth": depth,
                          "line_no": text[:start + run_start].count("\n") + 1})
    return found


def _block_openings(body):
    """[(offset just after each block-opening newline, depth)] in order.

    Offset 0 (the function's own body) comes first with depth 0.
    """
    out, depth, index = [(0, 0)], 0, 0
    while index < len(body):
        char = body[index]
        if char in "\"'":
            index = _skip_literal(body, index)
            continue
        if body.startswith("/*", index):
            end = body.find("*/", index + 2)
            index = len(body) if end < 0 else end + 2
            continue
        if body.startswith("//", index):
            end = body.find("\n", index)
            index = len(body) if end < 0 else end
            continue
        if char == "{":
            depth += 1
            line_end = body.find("\n", index)
            if line_end >= 0:
                out.append((line_end + 1, depth))
        elif char == "}":
            depth -= 1
        index += 1
    return out


def variants(decls):
    """[(order, label)] for every DISTINCT single-declaration move."""
    total, seen, out = len(decls), {tuple(decls)}, []
    for source in range(total):
        for dest in range(total):
            if source == dest:
                continue
            rest = decls[:source] + decls[source + 1:]
            candidate = rest[:dest] + [decls[source]] + rest[dest:]
            key = tuple(candidate)
            if key in seen:
                continue
            seen.add(key)
            out.append((candidate, "%s -> pos %d"
                        % (decls[source].strip(), dest)))
    return out


def apply_order(text, block, order):
    """`text` with `block`'s declaration run replaced by `order`.

    The block's ORIGINAL line terminator is reused, so a CRLF source stays
    CRLF: rewriting a whole file's endings would show up as a diff of every
    line and as a rebuild of the whole TU, and `git diff --check` would be
    the first thing to notice — after the sweep had already spent its
    budget. The re-read guard below is what catches a block that moved
    under us (or a mixed-ending run), rather than writing at a stale
    offset.
    """
    start, eol = block["start"], block["eol"] or "\n"
    old = "".join(line + eol for line in block["decls"])
    if text[start:start + len(old)] != old:
        raise SystemExit("declsweep: the declaration block at offset %d no"
                         " longer matches the source; re-read the file and"
                         " retry" % start)
    return text[:start] + "".join(line + eol for line in order) \
        + text[start + len(old):]


def ninja_build(target, timeout=1800):
    done = subprocess.run(["ninja", "-j2", target], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)
    text = (done.stdout or "") + (done.stderr or "")
    return (done.returncode == 0 and "FAILED" not in text), text


def measure(unit, fn):
    """The gate row for `fn` from the CURRENT object, or None.

    In-process: `fndiff.parse` and `objdump` memoize on (path, mtime, size),
    so a rebuilt object misses the cache and a stale one cannot be served.
    That is the whole reason this does not shell out per candidate — a
    subprocess per measurement roughly doubled the per-candidate cost.
    """
    target_o = "build/GUNE5D/obj/%s.o" % unit
    ours_o = object_path(unit)
    if not (os.path.exists(os.path.join(ROOT, target_o))
            and os.path.exists(os.path.join(ROOT, ours_o))):
        return None
    target = fndiff.parse(os.path.join(ROOT, target_o))
    ours = fndiff.parse(os.path.join(ROOT, ours_o))
    name = fndiff.resolve_function_name(dict(target, **ours), fn)
    if name is None or name not in target or name not in ours:
        return None
    return fndiff.gate_metrics(unit, name, target[name], ours[name])


def rose(current, base):
    """[field] of the gated columns that ROSE, or that went unmeasurable."""
    out = []
    for field in ("mnem", "reloc", "anon"):
        now, before = current.get(field), base.get(field)
        if now is None or before is None:
            out.append(field + "?")
        elif now > before:
            out.append(field)
    return out


def dirty_source(path):
    """The `git status --porcelain` row for `path`, or '' when clean.

    Scoped to the ONE file this tool rewrites, deliberately. A worker
    worktree always has an untracked LANE_LOCK, so a whole-tree emptiness
    test refuses every legitimate run; and the file that must be clean is
    the one a killed run leaves rewritten.
    """
    done = subprocess.run(["git", "status", "--porcelain", "--", path],
                          cwd=ROOT, capture_output=True, text=True)
    if done.returncode != 0:
        return "git status failed: " + (done.stderr or "").strip()
    return done.stdout.strip()


def sweep(unit, fn, block_index=0, rounds=1, budget=None,
          build=None, measure_fn=None, log=print):
    """Sweep one block of one function. Returns the run record dict.

    `build` and `measure_fn` are injectable so the loop can be driven
    without a compiler in tests; both default to the real Ninja edge and
    the real objects.
    """
    build = build or (lambda: ninja_build(object_path(unit)))
    measure_fn = measure_fn or (lambda: measure(unit, fn))
    path = source_path(unit)
    if path is None:
        raise SystemExit("declsweep: no src/%s.c or .cpp" % unit)
    dirty = dirty_source(os.path.relpath(path, ROOT).replace("\\", "/"))
    if dirty:
        raise SystemExit(
            "declsweep REFUSED: %s has uncommitted changes (%s).\n"
            "  This tool rewrites that file and restores it in a `finally`;"
            " a killed run must leave a tree `git checkout --` repairs"
            " exactly, which an already-dirty file does not."
            % (os.path.relpath(path, ROOT).replace("\\", "/"), dirty))

    original = read_source(path)
    found = blocks(original, fn)
    if found is None:
        raise SystemExit("declsweep: cannot find or close %s's body in %s"
                         % (fn, os.path.relpath(path, ROOT)))
    if block_index >= len(found):
        raise SystemExit(
            "declsweep: %s has %d sweepable block(s); --block %d is out of"
            " range (list them with --list-blocks)"
            % (fn, len(found), block_index))
    block = found[block_index]
    record = {"unit": unit, "function": fn, "block": block_index,
              "blocks": len(found), "declarations": len(block["decls"]),
              "rounds_requested": rounds, "rounds_taken": 0,
              "candidates_built": 0, "candidates_rejected": 0,
              "rejected_by": {}, "verdict": "CAPPED", "winner": None,
              "baseline": None, "best": None, "seconds": 0.0}
    started = time.time()

    log("declsweep %s::%s block %d (%d declaration(s), depth %d, line %d)"
        % (unit, fn, block_index, len(block["decls"]), block["depth"],
           block["line_no"]))
    log("  RECOVERY after a kill: git checkout -- %s"
        % os.path.relpath(path, ROOT).replace("\\", "/"))
    for line in block["decls"]:
        log("      %s" % line.strip())

    try:
        ok, text = build()
        if not ok:
            raise SystemExit("declsweep: the BASELINE build failed; fix the"
                             " tree first\n" + text[-1500:])
        base = measure_fn()
        if base is None or base["real"] is None:
            raise SystemExit("declsweep: no baseline measurement for %s::%s"
                             % (unit, fn))
        record["baseline"] = base
        log("  baseline: real %s words %s mnem %s reloc %s anon %s"
            % (base["real"], base["words"], base["mnem"], base["reloc"],
               base["anon"]))
        best, order = dict(base), list(block["decls"])
        for round_index in range(max(1, rounds)):
            candidates = variants(order)
            log("  round %d: %d candidate(s)" % (round_index, len(candidates)))
            improved = None
            for candidate, label in candidates:
                if budget is not None and time.time() - started > budget:
                    record["verdict"] = "BUDGET"
                    log("  BUDGET %.0fs exhausted at candidate %d/%d"
                        % (budget, record["candidates_built"],
                           len(candidates)))
                    break
                write_source(path, apply_order(original, block, candidate))
                built, _text = build()
                if not built:
                    continue
                record["candidates_built"] += 1
                row = measure_fn()
                if row is None or row["real"] is None:
                    continue
                risen = rose(row, base)
                if risen:
                    record["candidates_rejected"] += 1
                    for field in risen:
                        record["rejected_by"][field] = \
                            record["rejected_by"].get(field, 0) + 1
                    log("    real %-5s words %-5s  %s  REJECTED(%s)"
                        % (row["real"], row["words"], label,
                           " ".join(risen)))
                    continue
                better = row["real"] < best["real"] or (
                    row["real"] == best["real"]
                    and (row["words"] or 0) < (best["words"] or 0))
                log("    real %-5s words %-5s  %s%s"
                    % (row["real"], row["words"], label,
                       "  <== BEST" if better else ""))
                if better:
                    best, improved = dict(row), (list(candidate), label)
            record["rounds_taken"] = round_index + 1
            if record["verdict"] == "BUDGET" or improved is None:
                break
            order, record["winner"] = improved[0], improved[1]
        record["best"] = best
        if record["winner"] and record["verdict"] != "BUDGET":
            record["verdict"] = "IMPROVED"
            log("  IMPROVED real %s -> %s, words %s -> %s via %s"
                % (base["real"], best["real"], base["words"], best["words"],
                   record["winner"]))
            log("  WINNING ORDER:")
            for line in order:
                log("      %s" % line.strip())
        elif record["verdict"] != "BUDGET":
            log("  CAPPED at real %s words %s  [%d gate-rejected: %s]"
                % (base["real"], base["words"], record["candidates_rejected"],
                   ", ".join("%s %d" % item
                             for item in sorted(record["rejected_by"].items()))
                   or "none"))
    finally:
        # The source goes back FIRST, then the object is rebuilt from it, so
        # no later reader can be served an object built from a candidate.
        write_source(path, original)
        os.utime(path, None)
        build()
    record["seconds"] = round(time.time() - started, 1)
    log("  (%.0fs, %d candidate(s) built, %.1fs each)"
        % (record["seconds"], record["candidates_built"],
           record["seconds"] / max(1, record["candidates_built"])))
    return record


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="declsweep.py",
        description="Declaration-order sweep for one function, gated on the"
                    " per-function relocation line.")
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--block", type=int, default=0,
                        help="which declaration block to sweep (default 0,"
                             " the function's leading run)")
    parser.add_argument("--list-blocks", action="store_true",
                        help="print the block inventory and exit")
    parser.add_argument("--rounds", type=int, default=1,
                        help="1 discovers; higher climbs from each winner")
    parser.add_argument("--budget", type=float, default=None,
                        help="stop at a candidate boundary after N seconds")
    parser.add_argument("--out", default=None,
                        help="write the run record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)

    if args.list_blocks:
        path = source_path(unit)
        if path is None:
            print("declsweep: no src/%s.c or .cpp" % unit)
            return REFUSED
        found = blocks(read_source(path), args.function)
        if found is None:
            print("declsweep: cannot find or close %s's body"
                  % args.function)
            return REFUSED
        if not found:
            print("%s: NO block with two or more declarations" % args.function)
            return REFUSED
        for index, block in enumerate(found):
            print("block %d  line %d  depth %d  %d declaration(s)"
                  % (index, block["line_no"], block["depth"],
                     len(block["decls"])))
            for line in block["decls"]:
                print("      %s" % line.strip())
        return OK

    try:
        record = sweep(unit, args.function, block_index=args.block,
                       rounds=args.rounds, budget=args.budget)
    except SystemExit as refusal:
        if isinstance(refusal.code, int):
            raise
        print(str(refusal))
        return REFUSED
    if args.out:
        out = os.path.join(ROOT, args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with io.open(out, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, sort_keys=True)
        print("  wrote %s" % args.out)
    return OK


if __name__ == "__main__":
    sys.exit(main())
