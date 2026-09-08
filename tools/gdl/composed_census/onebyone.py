#!/usr/bin/env python3
"""Apply each candidate source edit ALONE and say which ones move bytes.

    python tools/gdl/composed_census/onebyone.py <unit> <candidates.json>
    python tools/gdl/composed_census/onebyone.py <unit> <candidates.json> --bank
    python tools/gdl/composed_census/onebyone.py <unit> <candidates.json> \\
        --tag e62b --out build/e_lane/e_onebyone.json

WHY. Every de-fakematch lane in run 62 wrote the same driver again --
`a_paddrive.py`, `a_pragmadrive.py`, `a_paddrive2.py`, `a_pragmadrive2.py`,
`a_pairprobe.py`, `a_castdrive.py`, `a_offedrive.py`, `d_hdrgate.py` -- and
each rewrite re-learned the same three lessons the hard way. This is that
loop, once, with the lessons built in:

  ONE AT A TIME, FROM A CLEAN FILE. Every probe starts from the file's
  committed content, not from the previous probe's leftovers. A driver that
  edits cumulatively measures the SUM of everything before it and attributes
  it to the last change.

  GROUPS ARE ATOMIC. A scoped `#pragma X off` and its `reset`, a multi-line
  declaration, a member and the cast that reads it -- applying half of one is
  not a smaller experiment, it is a different (usually uncompilable) one.
  Candidates sharing a `group` are applied and measured together, exactly as
  `a_pairprobe.py` had to re-probe the pragma pairs `a_pragmadrive2.py`
  measured singly (4 inert pairs that neither half showed alone).

  NINJA'S EXIT CODE, NOT ITS TEXT. The rebuild goes through the real build
  edge for the object and is judged by its exit code plus any FAILED line.
  A COMPILE-FAIL is a distinct verdict, never a silent NEUTRAL.

  RESTORE IN A `finally`. Interrupt, exception, ninja crash: the files go
  back. The tool also REFUSES to start when any file it would touch is dirty,
  because "the committed content" is what it restores to, and it will not
  silently discard a lane's uncommitted work.

VERDICTS, per candidate group:

  NEUTRAL       the object's functions and positional relocations are
                unchanged (or only anonymous-pool renumbered)
  CHANGED       named functions moved; the report gives the differing word
                count and the size delta per function
  COMPILE-FAIL  ninja refused; the object is whatever it was before
  REFUSED       the comparison could not be made (stale bank, missing object)

THE CALLER/INLINEE TRAP, and why the report warns about it. A `static`
function with no standalone symbol in the object was INLINED into its
callers. Editing it changes those callers' bodies, and the changed-function
list then names the callers -- so a reader concludes the caller's own source
is load-bearing when the edit was in the inlinee. The report names any
inlined static whose source span contains a candidate's edit, alongside the
functions that changed. The span comes from a `static ... (` line to the next
column-0 `}`; that is a text heuristic, not a parse, and it is reported as a
warning to check rather than as a finding.

WHAT IS NOT COMPARED. objneutral's limits are this tool's limits: function
bodies and positional relocations only. Data, BSS, exception records and
section sizes are not compared, and a full `ninja` link is not run. A NEUTRAL
verdict here is not a link.

LIVE RUN, verbatim (2026-09-08, worktree W:/Repositories/GDL-Claude-Tools62,
branch claude/tools-62b-20260908 at commit 538fda07d, unit
`game/sound/sounds`, two candidates in tools/gdl/tests/fixtures/t62b_onebyone_live.json --
a comment reflow, and `AudioWelcome`'s `sndFxQueAddEx(1, 0xC0084, -1.0f,
10.0f, 224, extra, 2)` with the 224 changed to 225):

    python tools/gdl/composed_census/onebyone.py game/sound/sounds \\
        tools/gdl/tests/fixtures/t62b_onebyone_live.json --tag e62b_live --bank

    ONEBYONE game/sound/sounds (bank tag e62b_live): 2 candidate group(s) \\
        over src/game/sound/sounds.c
      NEUTRAL       comment-only             src/game/sound/sounds.c
      CHANGED       welcome-extra-224-to-225 src/game/sound/sounds.c
                    CHANGED  AudioWelcome  1 word(s)  size 132 -> 132
      CHANGED 1, NEUTRAL 1

`git status --porcelain -- src/ include/` was empty afterwards, and the final
rebuild restored the object the bank was taken from.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import objneutral  # noqa: E402

REPO = HERE.parents[2]
VERSION = "GUNE5D"


class Refused(RuntimeError):
    """A probe run that must not start, or must not be believed."""


def read_source(path):
    """Exact text, line endings preserved.

    Not `Path.read_text(newline=...)`: that keyword is 3.13+, and this repo's
    interpreter is 3.11. Universal-newline reading would rewrite a CRLF file
    to LF on restore -- a whole-file diff dressed up as a restore.
    """
    with open(path, "r", encoding="utf-8", newline="") as handle:
        return handle.read()


def write_source(path, text):
    """Write exactly these bytes, then stamp the mtime forward.

    Ninja compares timestamps. A same-second write, or any tool that
    preserves them, leaves the previous object in place and the next probe
    measures the wrong compile.
    """
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)
    now = time.time()
    os.utime(path, (now, now))


# --------------------------------------------------------------------------
# Candidate parsing and grouping (pure)


def load_candidates(payload):
    """[{'id', 'group', 'edits': [{'file', 'old', 'new'}]}] from JSON rows.

    Rows carrying the same `group` become ONE candidate applied together.
    Order inside a group is preserved: a `#pragma ... off` and its `reset`
    must be substituted in the order the author listed them.
    """
    if not isinstance(payload, list):
        raise Refused("the candidate file must hold a JSON LIST of edits")
    order, groups = [], {}
    for index, row in enumerate(payload):
        if not isinstance(row, dict):
            raise Refused("candidate %d is not an object" % index)
        for key in ("file", "old", "new"):
            if not isinstance(row.get(key), str) or (key != "new"
                                                     and not row[key]):
                raise Refused("candidate %d needs a non-empty string %r"
                              % (index, key))
        if row["old"] == row["new"]:
            raise Refused("candidate %d changes nothing: old == new" % index)
        group = str(row.get("group") or row.get("id") or "candidate%d" % index)
        if group not in groups:
            groups[group] = {"id": str(row.get("id") or group),
                             "group": group, "edits": []}
            order.append(group)
        groups[group]["edits"].append(
            {"file": row["file"].replace("\\", "/").strip("/"),
             "old": row["old"], "new": row["new"],
             "count": int(row.get("count", 0))})
    return [groups[name] for name in order]


def apply_edits(text, edits):
    """(new text, [applied counts]) or raise when a substitution cannot bind.

    An `old` that does not occur, or that occurs more times than the author
    said, refuses: a probe that silently patched nothing reports NEUTRAL and
    means "the experiment did not happen".
    """
    counts = []
    for edit in edits:
        found = text.count(edit["old"])
        if found == 0:
            raise Refused("text not found in %s: %r"
                          % (edit["file"], edit["old"][:80]))
        wanted = edit.get("count") or 0
        if wanted and found != wanted:
            raise Refused("%s: expected %d occurrence(s) of %r, found %d"
                          % (edit["file"], wanted, edit["old"][:60], found))
        text = text.replace(edit["old"], edit["new"])
        counts.append(found)
    return text, counts


# --------------------------------------------------------------------------
# The caller/inlinee trap (pure)


def static_function_spans(text):
    """[{'name', 'first_line', 'last_line'}] for `static` definitions.

    A TEXT HEURISTIC, deliberately: from a line beginning `static` that
    carries a `(` and is not a prototype, to the next line whose first
    character is `}`. It is used only to WARN, never to conclude.
    """
    spans = []
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if not line.startswith("static") or "(" not in line or ";" in line:
            continue
        head = line.split("(")[0].replace("*", " ")
        parts = head.split()
        if len(parts) < 2:
            continue
        name = parts[-1]
        if not name.isidentifier():
            continue
        last = len(lines)
        for end in range(index + 1, len(lines)):
            if lines[end].startswith("}"):
                last = end + 1
                break
        spans.append({"name": name, "first_line": index + 1,
                      "last_line": last})
    return spans


def edit_lines(text, old):
    """1-based line numbers where `old` occurs (its first line each time)."""
    lines, hits, at = [], [], text.find(old)
    while at != -1:
        hits.append(text.count("\n", 0, at) + 1)
        at = text.find(old, at + 1)
    lines.extend(hits)
    return lines


def inlining_warnings(text, edits, emitted_names, changed_names):
    """[str] for each inlined static whose span holds one of these edits.

    `emitted_names` is what the OBJECT actually defines. A static that is not
    in it was inlined away (or dead-stripped), so the bytes its source
    controls live inside other functions -- the ones `changed_names` lists.
    """
    if not changed_names:
        return []
    spans = static_function_spans(text)
    warnings = []
    for span in spans:
        if span["name"] in emitted_names:
            continue
        touched = any(span["first_line"] <= line <= span["last_line"]
                      for edit in edits
                      for line in edit_lines(text, edit["old"]))
        if not touched:
            continue
        warnings.append(
            "static %s (lines %d-%d) has NO standalone symbol in the object:"
            " it was inlined or dead-stripped, so this edit's bytes surface"
            " inside its CALLERS. The changed function(s) below (%s) may be"
            " reporting the inlinee's change, not their own source."
            % (span["name"], span["first_line"], span["last_line"],
               ", ".join(sorted(changed_names)[:6])))
    return warnings


# --------------------------------------------------------------------------
# The impure loop


def _git(*args):
    done = subprocess.run(["git", *args], cwd=str(REPO), capture_output=True,
                          text=True)
    return done.stdout if done.returncode == 0 else None


def dirty_files(paths):
    """[path] among `paths` that git reports modified, staged or untracked."""
    status = _git("status", "--porcelain", "--", *paths)
    if status is None:
        raise Refused("git could not report the status of %s"
                      % ", ".join(paths))
    return sorted({line[3:].strip().strip('"').replace("\\", "/")
                   for line in status.splitlines() if line.strip()})


def object_target(unit):
    """Repo-relative Ninja output for `unit`'s object."""
    path, _note = objneutral.current_object(unit)
    return Path(path).resolve().relative_to(REPO.resolve()).as_posix()


def rebuild(target, ninja="ninja", jobs=2):
    """(ok, message) from the REAL build edge, judged by the exit code."""
    done = subprocess.run([ninja, "-j%d" % jobs, target], cwd=str(REPO),
                          capture_output=True, text=True)
    failed = [line for line in (done.stdout + done.stderr).splitlines()
              if line.startswith("FAILED:")]
    text = " ".join((done.stdout + "\n" + done.stderr).split())
    return (done.returncode == 0 and not failed), text[-400:]


def probe(unit, candidate, saved, tag, ninja="ninja", jobs=2):
    """One candidate group, applied alone, measured, and undone."""
    row = {"id": candidate["id"], "group": candidate["group"],
           "files": sorted({edit["file"] for edit in candidate["edits"]}),
           "edits": len(candidate["edits"])}
    by_file = {}
    try:
        for edit in candidate["edits"]:
            by_file.setdefault(edit["file"], []).append(edit)
        for name, edits in by_file.items():
            text, counts = apply_edits(saved[name], edits)
            row.setdefault("substitutions", []).extend(counts)
            write_source(REPO / name, text)
        target = object_target(unit)
        ok, message = rebuild(target, ninja, jobs)
        if not ok:
            row.update(verdict="COMPILE-FAIL", detail=message)
            return row
        try:
            result = objneutral.check(unit, tag, ninja=ninja)
        except objneutral.Unavailable as error:
            row.update(verdict="REFUSED", detail=str(error))
            return row
        changed = [fn for fn in result["functions"]
                   if fn["verdict"] not in ("IDENTICAL", "RENUMBERED")]
        row["counts"] = result["counts"]
        row["changed"] = [
            {"function": fn["function"], "verdict": fn["verdict"],
             "differing_words": fn.get("differing_words", 0),
             "size_before": fn.get("size_before"),
             "size_after": fn.get("size_after")} for fn in changed]
        row["verdict"] = "NEUTRAL" if result["neutral"] else "CHANGED"
        emitted = {fn["function"] for fn in result["functions"]}
        row["warnings"] = []
        for name, edits in by_file.items():
            row["warnings"] += inlining_warnings(
                saved[name], edits, emitted,
                {fn["function"] for fn in changed})
        return row
    except Refused as error:
        row.update(verdict="REFUSED", detail=str(error))
        return row
    finally:
        for name in by_file:
            write_source(REPO / name, saved[name])


def run(unit, candidates, tag="default", ninja="ninja", jobs=2,
        bank_first=False):
    """Probe every candidate group, always restoring every file."""
    files = sorted({edit["file"] for candidate in candidates
                    for edit in candidate["edits"]})
    for name in files:
        if not (REPO / name).is_file():
            raise Refused("no such file: " + name)
    dirty = dirty_files(files)
    if dirty:
        raise Refused(
            "refusing to start: %s modified or untracked. Every probe"
            " restores the COMMITTED content, so starting from a dirty file"
            " would discard that work. Commit or set it aside first."
            % ", ".join(dirty))
    saved = {name: read_source(REPO / name) for name in files}
    if bank_first:
        target = object_target(unit)
        ok, message = rebuild(target, ninja, jobs)
        if not ok:
            raise Refused("the clean tree does not build: " + message)
        objneutral.bank(unit, tag, ninja=ninja)
    rows = []
    try:
        for candidate in candidates:
            rows.append(probe(unit, candidate, saved, tag, ninja, jobs))
    finally:
        for name in files:
            write_source(REPO / name, saved[name])
        rebuild(object_target(unit), ninja, jobs)
    return {"schema_version": 1, "tool": "tools/gdl/composed_census/onebyone.py",
            "unit": unit, "tag": tag, "candidates": len(candidates),
            "files": files, "rows": rows,
            "limits": ["Function bodies and positional relocations only"
                       " (objneutral's limits): data, BSS, exception records"
                       " and section sizes are NOT compared.",
                       "No full link is run; a NEUTRAL verdict is not a"
                       " green DOL.",
                       "The inlined-static warning is a TEXT heuristic over"
                       " `static ... (` spans, not a parse."]}


def format_run(result):
    out = ["ONEBYONE %s (bank tag %s): %d candidate group(s) over %s"
           % (result["unit"], result["tag"], result["candidates"],
              ", ".join(result["files"]))]
    for row in result["rows"]:
        out.append("  %-13s %-24s %s"
                   % (row["verdict"], row["id"], ", ".join(row["files"])))
        for change in row.get("changed", []):
            out.append("                %s  %s  %d word(s)  size %s -> %s"
                       % (change["verdict"], change["function"],
                          change["differing_words"], change["size_before"],
                          change["size_after"]))
        if row.get("detail"):
            out.append("                %s" % row["detail"][:300])
        for warning in row.get("warnings", []):
            out.append("                WARNING: %s" % warning)
    counts = {}
    for row in result["rows"]:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
    out.append("  " + ", ".join("%s %d" % pair for pair in sorted(counts.items())))
    out.append("  LIMITS: " + " ".join(result["limits"]))
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("unit", help="unit key, e.g. game/sound/sounds")
    parser.add_argument("candidates", type=Path,
                        help="JSON list of {file, old, new, group?, id?}")
    parser.add_argument("--tag", default="default",
                        help="objneutral bank tag to compare against")
    parser.add_argument("--bank", action="store_true",
                        help="build the clean tree and bank it first")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", type=Path, help="write the JSON result here")
    args = parser.parse_args(argv)
    if args.out is not None and not args.out.resolve().is_relative_to(
            (REPO / "build").resolve()):
        parser.error("--out must be under this checkout's build/")
    try:
        payload = json.loads(args.candidates.read_text(encoding="utf-8"))
        candidates = load_candidates(payload)
        if not candidates:
            raise Refused("no candidates in " + str(args.candidates))
        unit = args.unit.replace("\\", "/").strip("/")
        unit = unit[4:] if unit.startswith("src/") else unit
        for suffix in (".cpp", ".c"):
            if unit.endswith(suffix):
                unit = unit[:-len(suffix)]
        result = run(unit, candidates, args.tag, args.ninja, args.jobs,
                     args.bank)
    except (Refused, OSError, ValueError) as error:
        print("ONEBYONE REFUSED: %s" % error)
        return 2
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=1) + "\n",
                            encoding="utf-8")
    print(json.dumps(result, indent=1) if args.json else format_run(result))
    return 0 if all(row["verdict"] in ("NEUTRAL", "CHANGED")
                    for row in result["rows"]) else 1


if __name__ == "__main__":
    sys.exit(main())
