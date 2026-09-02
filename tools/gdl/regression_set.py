"""Frozen regression set: does the knowledge graph actually improve outcomes?

The campaign's knowledge graph (`memory_graph/`) is asserted to make matching
work cheaper. That assertion has never been MEASURED. This harness builds the
instrument: a frozen set of functions that are STRICT-matched today, each with
the pre-match source state that a past agent actually started from. Rewinding
one function reproduces a real, historical matching problem whose answer is
known -- so two agents can be pointed at the same problem, one with graph
access and one without, and the difference scored.

Terms, exactly as the campaign defines them:

  STRICT-matched   the COMPILER's output is byte-identical -- report.json
                   fuzzy_match_percent >= 100 AND the function is not pinned
                   in config/GUNE5D/webfrank.json. A WebFrank-pinned function
                   is EQUIVALENT-tier, not STRICT: its residual was closed by
                   the postprocessor, so rewinding its source does not pose a
                   source-level matching problem at all. Pinned functions are
                   excluded from the manifest by construction.
  pre commit       the PARENT of the matching commit. The TU's source at that
                   commit is the state the closing agent actually faced.
  post commit      the commit whose subject closes the function ("Match <fn>").

The rewind is FUNCTION-SCOPED, not whole-file: probe.py's `scoped_revert`
restores only the hunks inside the function's own span, so the rest of the TU
stays at HEAD. That matters -- a whole-file rewind would drag in months of
unrelated header/API drift and measure the wrong thing. When a hunk straddles
the function boundary, scoped_revert refuses loudly and the entry is rejected
from the manifest rather than silently widened.

TWO-ARM TRIAL PROTOCOL (this harness sets up the problem; the trials are a
future run's budget decision -- see the methodology record
claim.method.RS_graph-utility-regression-set-protocol):

  Setup      `regression_set.py setup <entry-id> --worktree W` puts ONE
             function back to its pre state in an isolated worktree. The agent
             is told the function, the TU, and nothing else -- in particular
             not the mechanism, the family, or the matching commit.
  Arm A      full `gdlmem.py` access (brief / context / laws / find / search).
  Arm B      gdlmem queries FORBIDDEN at the prompt level. Every other tool
             (fnasm, fndiff, probe, regnorm, Ghidra, the PDB) stays available,
             so the arms differ in RETRIEVAL ONLY, not in capability.
  Budget     identical and fixed per arm: N probe builds (recommend 12, the
             median probe count of a closed function in the campaign's attempt
             corpus) and a wall-clock cap.
  Outcome    reached-exact yes/no (`verify` says EXACT), probe count consumed,
             build count consumed. Secondary: which axis closed it, and whether
             it is the same mechanism the historical commit used.
  Blinding   the scorer runs `verify`, which does not read the agent's
             reasoning -- only the object bytes.

Falsifier for the whole instrument: if arm B reaches exact at the same rate
and probe count as arm A across the frozen set, the graph is not paying for
its retrieval cost on closable functions, and the campaign's per-function
`context` mandate should be re-scoped to parks and vetoes only.

Commands
--------
  regression_set.py list [--manifest P]
  regression_set.py probe --unit U --fn F --pre SHA --post SHA [--family X]
        measure one candidate and print its manifest row (this is how the
        manifest is BUILT: a row exists only because it was measured)
  regression_set.py setup <entry-id> [--worktree W]
  regression_set.py verify <entry-id> [--worktree W]
  regression_set.py restore --unit U [--worktree W]

All paths are repo-root-relative; run from the root of the worktree you mean
to act in (`--worktree` only labels the run for reporting).
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import probe as probelib  # noqa: E402  (reuses the campaign's proven span logic)

VERSION = "GUNE5D"
DEFAULT_MANIFEST = HERE / "RS_regression_manifest.json"

FAMILIES = {
    "addressing-mode", "branch-pair", "constant-hoist", "copy-form",
    "cse-share", "eh-scaffold", "frame-slot", "inline-boundary",
    "live-zero-remat", "pool-order", "prologue-form", "regalloc-web",
    "reloc-naming", "save-area", "schedule-window",
    "unclassified", "no-residual", "structural",
}


def run(cmd, cwd=None, check=False):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    if check and proc.returncode != 0:
        raise SystemExit(f"command failed: {' '.join(cmd)}\n{proc.stderr[:2000]}")
    return proc


def git(*args, cwd=None):
    return run(["git"] + list(args), cwd=cwd)


def source_path(unit):
    """`main/game/x/y` or `game/x/y` -> Path('src/game/x/y.c'|'.cpp')."""
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("main/"):
        unit = unit[len("main/"):]
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    for suffix in (".c", ".cpp"):
        candidate = Path("src") / (unit + suffix)
        if candidate.exists():
            return candidate
    return None


def unit_key(unit):
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("main/"):
        unit = unit[len("main/"):]
    return re.sub(r"\.(c|cpp)$", "", unit)


def object_target(unit):
    return f"build/{VERSION}/src/{unit_key(unit)}.o"


def build_object(unit, jobs=2):
    return run(["ninja", "-j", str(jobs), object_target(unit)])


def fndiff_count(unit, fn):
    """(status, real) for one function from `fndiff --clean`.

    real is the campaign's residual number: 0 with a MATCH status means the
    compiler's own output is byte-identical.
    """
    src = f"{unit_key(unit)}.c"
    if not (Path("src") / src).exists():
        src = f"{unit_key(unit)}.cpp"
    proc = run([sys.executable, str(HERE / "fndiff.py"), src, fn, "--clean"])
    out = proc.stdout + proc.stderr
    # The status token must admit '_' and digits: fndiff emits REGISTER_ONLY,
    # SCHEDULE_CANDIDATE, STACK_LAYOUT and MATCH-MODULO-RELOC-NAMING as well as
    # "MATCH (pool-name noise only)". An earlier class that omitted '_' silently
    # rejected every register/schedule-class entry as "unscored", which biased
    # the whole candidate pool to STRUCTURAL residuals -- the exact diversity
    # the manifest is supposed to carry.
    m = re.search(r"^==\s+(\S+):\s+([A-Za-z0-9_ \-()]+?),\s+(\d+)\s+real diff",
                  out, re.M)
    if m:
        return m.group(2).strip(), int(m.group(3)), out
    return None, None, out


def read_pre_text(pre_sha, path):
    proc = git("show", f"{pre_sha}:{path.as_posix()}")
    if proc.returncode != 0:
        return None, proc.stderr.strip()[:300]
    return proc.stdout, None


def rewind(unit, fn, pre_sha, write=True):
    """Splice the pre-commit body of `fn` into the current TU source.

    Returns (path, new_text, notes). Raises SystemExit with the reason when
    the entry is not usable as a regression-set member.
    """
    path = source_path(unit)
    if path is None:
        raise SystemExit(f"no source file for unit {unit}")
    cur_text = path.read_text(encoding="utf-8", errors="surrogateescape")
    pre_text, err = read_pre_text(pre_sha, path)
    if pre_text is None:
        raise SystemExit(f"pre state unavailable ({pre_sha}:{path}): {err}")
    try:
        new_text, notes = probelib.scoped_revert(pre_text, cur_text, fn)
    except ValueError as exc:
        raise SystemExit(f"scoped rewind refused: {exc}")
    if new_text == cur_text:
        raise SystemExit(
            f"rewind is a no-op for {fn}: the pre state and HEAD are identical "
            "inside the function span (the named commit did not change this "
            "function's body)")
    if write:
        path.write_text(new_text, encoding="utf-8", errors="surrogateescape")
    return path, new_text, notes


def restore(unit):
    path = source_path(unit)
    if path is None:
        raise SystemExit(f"no source file for unit {unit}")
    proc = git("checkout", "--", path.as_posix())
    if proc.returncode != 0:
        raise SystemExit(f"restore failed: {proc.stderr[:400]}")
    os.utime(path, None)   # Copy-Item/checkout can leave ninja thinking nothing changed
    return path


def measure(unit, fn, pre_sha, post_sha=None, family="unclassified",
            jobs=2, keep=False):
    """Full per-entry measurement: HEAD must be exact, pre state must not be."""
    result = {
        "fn": fn, "unit": unit_key(unit), "pre": pre_sha, "post": post_sha,
        "family": family,
    }
    src = source_path(unit)
    if src is None:
        result["status"] = "REJECT:no-source"
        return result
    result["source"] = src.as_posix()

    # --- arm 0: HEAD must be STRICT-exact right now -------------------------
    restore(unit)
    build = build_object(unit, jobs)
    if build.returncode != 0:
        result["status"] = "REJECT:head-build-failed"
        result["detail"] = build.stdout[-600:] + build.stderr[-600:]
        return result
    status, real, _ = fndiff_count(unit, fn)
    result["head_status"], result["head_real"] = status, real
    if real != 0:
        result["status"] = "REJECT:head-not-exact"
        return result

    # --- arm 1: the pre state must reproduce a nonzero residual -------------
    try:
        _, _, notes = rewind(unit, fn, pre_sha)
    except SystemExit as exc:
        restore(unit)
        result["status"] = "REJECT:rewind"
        result["detail"] = str(exc)
        return result
    result["rewind_notes"] = notes
    build = build_object(unit, jobs)
    if build.returncode != 0:
        result["status"] = "REJECT:pre-build-failed"
        result["detail"] = (build.stdout[-800:] + build.stderr[-800:])
        if not keep:
            restore(unit)
            build_object(unit, jobs)
        return result
    status, real, out = fndiff_count(unit, fn)
    result["pre_status"], result["pre_real"] = status, real
    if real is None:
        result["status"] = "REJECT:pre-unscored"
        # Keep the raw scorer output: an unscored entry is a fact about the
        # MEASUREMENT, not necessarily about the function, and silently
        # dropping it would hide a parser bug behind a rejection count.
        result["detail"] = out[-800:]
    elif real == 0:
        result["status"] = "REJECT:pre-already-exact"
    else:
        result["status"] = "OK"
    if not keep:
        restore(unit)
        build_object(unit, jobs)
    return result


def load_manifest(path):
    p = Path(path)
    if not p.exists():
        raise SystemExit(f"manifest not found: {p}")
    return json.loads(p.read_text(encoding="utf-8"))


def find_entry(manifest, entry_id):
    for e in manifest["entries"]:
        if e["id"] == entry_id or e["fn"] == entry_id:
            return e
    raise SystemExit(f"no manifest entry {entry_id!r}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list")

    p = sub.add_parser("probe")
    p.add_argument("--unit", required=True)
    p.add_argument("--fn", required=True)
    p.add_argument("--pre", required=True)
    p.add_argument("--post", default=None)
    p.add_argument("--family", default="unclassified")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--keep", action="store_true",
                   help="leave the rewound state in place (for setup)")

    p = sub.add_parser("setup")
    p.add_argument("entry")
    p.add_argument("--worktree", default=None)
    p.add_argument("--jobs", type=int, default=2)

    p = sub.add_parser("verify")
    p.add_argument("entry")
    p.add_argument("--worktree", default=None)
    p.add_argument("--jobs", type=int, default=2)

    p = sub.add_parser("restore")
    p.add_argument("--unit", required=True)

    args = ap.parse_args()

    if args.cmd == "list":
        man = load_manifest(args.manifest)
        print(f"# {man['name']} ({len(man['entries'])} entries, frozen "
              f"{man['frozen_at']})")
        print(f"{'id':<34} {'family':<16} {'pre_real':>8}  unit")
        for e in man["entries"]:
            print(f"{e['id']:<34} {e['family']:<16} {e['pre_real']:>8}  "
                  f"{e['unit']}")
        return

    if args.cmd == "probe":
        if args.family not in FAMILIES:
            raise SystemExit(f"unknown family {args.family!r}; vocabulary: "
                             + ", ".join(sorted(FAMILIES)))
        row = measure(args.unit, args.fn, args.pre, args.post, args.family,
                      args.jobs, keep=args.keep)
        print(json.dumps(row, indent=1))
        return 0 if row.get("status") == "OK" else 1

    if args.cmd == "restore":
        print("restored", restore(args.unit))
        return

    man = load_manifest(args.manifest)
    entry = find_entry(man, args.entry)

    if args.cmd == "setup":
        path, _, notes = rewind(entry["unit"], entry["fn"], entry["pre"])
        build = build_object(entry["unit"], args.jobs)
        print(f"REWOUND {entry['fn']}  ({notes})")
        print(f"  source : {path}")
        print(f"  build  : {'ok' if build.returncode == 0 else 'FAILED'}")
        status, real, _ = fndiff_count(entry["unit"], entry["fn"])
        print(f"  residual: {status} real={real} "
              f"(manifest recorded real={entry['pre_real']})")
        if real != entry["pre_real"]:
            print("  WARNING: residual differs from the frozen manifest value;"
                  " the surrounding TU has drifted since freezing.")
        print("\nThe agent under trial is told ONLY: the function, the TU, and"
              " the budget.\nDo not reveal the family, the mechanism, or the"
              " matching commit.")
        return

    if args.cmd == "verify":
        build = build_object(entry["unit"], args.jobs)
        if build.returncode != 0:
            print("BUILD FAILED")
            return 1
        status, real, out = fndiff_count(entry["unit"], entry["fn"])
        verdict = "EXACT" if real == 0 else f"OPEN (real={real})"
        print(f"{entry['id']}: {verdict}  [{status}]")
        return 0 if real == 0 else 1


if __name__ == "__main__":
    sys.exit(main() or 0)
