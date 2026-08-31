#!/usr/bin/env python3
"""One-shot probe scorer for the matching iteration loop.

Collapses the ubiquitous edit-loop pair (`ninja <obj>` then `fndiff --count`)
into a single call with explicit verdicts — no more comparing numbers against
scrollback by eye. Tracks best/last per function in build/GUNE5D/gate/.

Usage:
  python tools/gdl/probe.py game/game/player do_players          # build+score
  python tools/gdl/probe.py game/game/player do_players --ops    # + ops scan
  python tools/gdl/probe.py game/game/player do_players --revert # restore last
                                                                 # banked good
                                                                 # source, then
                                                                 # build+score
  python tools/gdl/probe.py game/game/player do_players --reset  # forget best
  python tools/gdl/probe.py game/game/player do_players --rebase-best
      # after a fuzzy/--ops-arbitrated keep of a real-regressed state:
      # accept the CURRENT state as the new best and revert point

Output: one line per probe —
  IMPROVED  real 1109 -> 1070 (best was 1109)     [best updated]
  REGRESSED real 1070 -> 1093 (best 1070)         [revert advised]
  NEUTRAL   real 1070 (insns 1174/1160)
The verdict compares against the BEST recorded real, so a probe sequence
never loses track of the high-water mark even across reverts.

Every BASELINE or IMPROVED probe banks a snapshot of the TU source; a later
`--revert` copies it back and re-scores in the same call, replacing the
edit -> probe -> hand-retype-revert -> probe cycle. The snapshot covers the
TU's own .c/.cpp only — header edits are yours to manage — and the banked
state is per-unit, so probe a BASELINE before your first edit of a session.
"""

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
TOOLS = Path(__file__).resolve().parent

COUNT_RE = re.compile(
    r"^DIFF\s+(\S+)\s+insns\s+(\d+)/(\d+)\s+lines\s+(\d+)\s+real\s+(\d+)"
)


def normalize_unit(unit):
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    return re.sub(r"\.(c|cpp)$", "", unit)


def state_path(unit, fn):
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", f"{unit}_{fn}")
    path = Path(f"build/{VERSION}/gate/probe_{slug}.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def source_path(unit):
    for suffix in (".c", ".cpp"):
        candidate = Path("src") / (unit + suffix)
        if candidate.exists():
            return candidate
    return None


def snapshot_path(unit, source):
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", unit)
    path = Path(f"build/{VERSION}/gate/snap_{slug}{source.suffix}")
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def git_head():
    result = subprocess.run(["git", "rev-parse", "HEAD"],
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def bank_snapshot(unit, source):
    snap = snapshot_path(unit, source)
    shutil.copyfile(source, snap)
    head = git_head()
    if head:
        snap.with_suffix(snap.suffix + ".meta").write_text(
            json.dumps({"head": head}), encoding="utf-8")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2 or args[0] in ("--help", "-h"):
        print(__doc__)
        return 2
    unit, fn = normalize_unit(args[0]), args[1]
    state_file = state_path(unit, fn)
    source = source_path(unit)
    if "--reset" in sys.argv:
        if state_file.exists():
            state_file.unlink()
        if source is not None:
            snap = snapshot_path(unit, source)
            if snap.exists():
                snap.unlink()
            meta = snap.with_suffix(snap.suffix + ".meta")
            if meta.exists():
                meta.unlink()
        print("probe state reset")
        return 0
    if "--revert" in sys.argv:
        if source is None:
            print(f"cannot revert: no src source found for {unit}")
            return 1
        snap = snapshot_path(unit, source)
        if not snap.exists():
            print("cannot revert: no banked snapshot for this unit yet"
                  " (a BASELINE or IMPROVED probe banks one)")
            return 1
        # A snapshot banked before a commit is STALE: restoring it would
        # silently destroy the committed state (observed in the field —
        # claim.law.probe-revert-snapshot-goes-stale-across-commits).
        meta_file = snap.with_suffix(snap.suffix + ".meta")
        if meta_file.exists():
            banked_head = json.loads(
                meta_file.read_text(encoding="utf-8")).get("head")
            head = git_head()
            if banked_head and head and banked_head != head:
                committed = subprocess.run(
                    ["git", "show",
                     f"HEAD:{source.as_posix()}"],
                    capture_output=True)
                if committed.returncode == 0 and \
                        committed.stdout != snap.read_bytes():
                    print("REFUSED: commits landed since this snapshot was"
                          " banked and the committed source differs from it"
                          " — reverting would destroy committed work. Run a"
                          " fresh probe on the current state to re-bank,"
                          " or use git to inspect history.")
                    return 1
        if snap.read_bytes() == source.read_bytes():
            print("nothing to revert: source already matches the banked"
                  " snapshot; re-scoring:")
        else:
            shutil.copyfile(snap, source)
            print(f"reverted {source} to the last banked good state;"
                  " re-scoring:")

    build = subprocess.run(
        ["ninja", f"build/{VERSION}/src/{unit}.o"],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        print("BUILD FAILED:")
        print((build.stdout + build.stderr).strip()[-1500:])
        return 1

    count = subprocess.run(
        [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
         "--count", "--no-build"],
        capture_output=True, text=True,
    ).stdout
    real = None
    for line in count.splitlines():
        match = COUNT_RE.match(line.strip())
        if match and match.group(1) == fn:
            _, ti, bi, lines, real_text = match.groups()
            real = int(real_text)
            insns = f"{ti}/{bi}"
            break
    else:
        if re.search(rf"^OK\s+{re.escape(fn)}\s*$", count, re.M) or not count.strip():
            # fndiff --count prints nothing for byte-identical functions
            real, insns = 0, "exact"

    if real is None:
        print(f"could not score {fn}:")
        print(count.strip()[:800])
        return 1

    # The opcode-multiset token count is the STRUCTURE metric: `real` is a
    # linear diff that reads catastrophically worse mid-way through any
    # all-or-nothing conversion (three workers independently reported
    # near-reverting correct multi-step wins on `real` alone). Track it and
    # never advise a revert while structure is improving.
    ops_output = None
    multiset_tokens = None
    if real > 0:
        ops_output = subprocess.run(
            [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
             "--ops", "--no-build"],
            capture_output=True, text=True,
        ).stdout
        for line in ops_output.splitlines():
            if "opcode multiset: IDENTICAL" in line:
                multiset_tokens = 0
                break
            if "opcode multiset: DIFFERS" in line:
                multiset_tokens = sum(
                    int(n) for n in re.findall(r"[+-](\d+) ", line))
                break
    elif real == 0:
        multiset_tokens = 0

    state = {}
    if state_file.exists():
        state = json.loads(state_file.read_text(encoding="utf-8"))
    best = state.get("best_real")
    prev_tokens = state.get("last_multiset")
    tok = (f", multiset {multiset_tokens}t"
           if multiset_tokens is not None else "")
    if "--rebase-best" in sys.argv:
        # After fuzzy/--ops arbitration keeps a real-regressed state, the old
        # banked best is dead and every later probe misreports REGRESSED.
        # Accept the current state as the new best and revert point.
        verdict = (f"REBASED   best {best} -> {real} (insns {insns}{tok})"
                   f"  [arbitrated keep]")
        state["best_real"] = real
    elif best is None:
        verdict = f"BASELINE  real {real} (insns {insns}{tok})"
        state["best_real"] = real
    elif real < best:
        verdict = (f"IMPROVED  real {best} -> {real} (insns {insns}{tok})"
                   "  [best updated]")
        state["best_real"] = real
    elif real > best:
        structure_improved = (multiset_tokens is not None
                              and prev_tokens is not None
                              and multiset_tokens < prev_tokens)
        if structure_improved:
            verdict = (f"CONFLICT  real {state.get('last_real', best)} ->"
                       f" {real} (best {best}, insns {insns}) but multiset"
                       f" {prev_tokens}t -> {multiset_tokens}t IMPROVED —"
                       " structure is converging; read the diff and"
                       " arbitrate, do NOT auto-revert"
                       " (--rebase-best banks an arbitrated keep)")
        else:
            verdict = (f"REGRESSED real {state.get('last_real', best)} ->"
                       f" {real} (best {best}, insns {insns}{tok})"
                       "  [revert advised]")
    else:
        verdict = f"NEUTRAL   real {real} (insns {insns}{tok})"
    state["last_real"] = real
    if multiset_tokens is not None:
        state["last_multiset"] = multiset_tokens
    state_file.write_text(json.dumps(state), encoding="utf-8")
    print(verdict)

    # Bank a revert point whenever this source state is the high-water mark.
    if source is not None and (verdict.startswith("BASELINE")
                               or verdict.startswith("IMPROVED")
                               or verdict.startswith("REBASED")):
        bank_snapshot(unit, source)
        print(f"[revert point banked: probe.py {unit} {fn} --revert"
              " restores it]")

    # A failed probe almost always needs the ops view next — print it
    # unasked (the multiset pass above already fetched it).
    if (verdict.startswith(("REGRESSED", "CONFLICT"))
            and "--ops" not in sys.argv and ops_output):
        print("\n".join(ops_output.strip().splitlines()[:16]))

    if "--ops" in sys.argv:
        if ops_output is None:
            ops_output = subprocess.run(
                [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
                 "--ops", "--no-build"],
                capture_output=True, text=True,
            ).stdout
        print(ops_output.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
