#!/usr/bin/env python3
"""Mechanical per-function gate for de-fakematch and matching passes.

Snapshots every function's fndiff state for a TU, then verifies later edits
against it: a matched function must stay byte-identical and a fuzzy function
must measure equal-or-better on real diff lines, or the check fails. This
turns the AGENTS.md gate rule into tooling instead of discipline — always
scored from the real ninja-built object via fndiff (matchtool presets can
diverge from a TU's actual cflags).

Usage:
  python tools/gdl/defake_gate.py baseline game/enemy/enemy.c
  python tools/gdl/defake_gate.py check game/enemy/enemy.c --rebuild
  python tools/gdl/defake_gate.py check game/enemy/enemy.c --update-improved
  python tools/gdl/defake_gate.py check game/x/y.c --bank-arbitrated=<fn>
      (accept a fuzzy-arbitrated keep for ONE function without
      re-anchoring any sibling — the mandate-correct way to bank a
      keep that `real` reads as a regression)
  python tools/gdl/defake_gate.py check game/audio/sndfx.c,game/ui/attract.c --rebuild

A comma-separated unit list gates every named TU in one call (exit code =
worst) — use it for paired fixes (a signature change plus its callers) so
the second TU can never be forgotten.

--rebuild runs the unit's ninja object target first, so rebuild+gate is one
call and a stale object can never be gated. On any REGRESSION the check
automatically prints each regressing function's fndiff --ops summary so the
diagnosis doesn't need a separate command.

`baseline` writes build/GUNE5D/gate/<unit>.json. `check` exits 1 on any
regression and lists it; improvements are reported (and, with
--update-improved, become the new baseline so later edits are gated against
the better score).

`baseline <unit> --at-head` rebuilds the baseline the CURRENT COMMIT
implies, setting any working-tree edits aside and restoring them after.
Baselines are KEYED TO THE COMMIT rather than committed to git (see
save_baseline for the reasoning): each records the commit and source
sha1 it was taken at, `check` says so when HEAD has moved, and --at-head
regenerates it anywhere.

A real-count regression on a fuzzy function whose CURRENT opcode multiset is
IDENTICAL to target at equal insn counts is reported as CONFLICT instead of
REGRESSION: structure fully matches target and the extra real lines can be
pure register-naming churn, which this gate's real-only score cannot see
(two workers independently hit this: a genuine load-schedule win scored as
real 100->104). CONFLICT still fails the gate by default -- arbitrate by
reading the diff and objdiff fuzzy; pass --arbitrate to accept a checked
CONFLICT-only result. Byte-exact functions are never eligible: any drift on
a real-0 function stays REGRESSION.

The second CONFLICT route is the STRUCTURE ARBITER: real rose while the
function's GENUINE structural rows (regnorm's count, artifacts excluded)
FELL. Closing one compensating error re-aligns every instruction after
it, so a strictly-nearer stream can score worse on `real` alone -- that
shape read as a flat REGRESSION and had to be overridden by hand. Only
the disputed functions are re-measured, so the check stays cheap.
"""

import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

VERSION = "GUNE5D"
FNDIFF = Path(__file__).resolve().parent / "fndiff.py"

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402  (raw_signature: the byte-identity backstop)
import regnorm  # noqa: E402  (genuine structural rows: the CONFLICT arbiter)

COUNT_RE = re.compile(
    r"^DIFF\s+(\S+)\s+insns\s+(\d+)/(\d+)\s+lines\s+(\d+)\s+real\s+(\d+)\s*$"
)
CLASSIFY_RE = re.compile(
    r"^(EXACT|RELOCATION_ONLY|REGISTER_ONLY|SCHEDULE_CANDIDATE|OPERAND_DIFF"
    r"|STRUCTURAL|BASE_ONLY|TARGET_ONLY)\s+(\S+)(?:\s+insns\s+(\d+)/(\d+))?\s*$"
)


def parse_classify(text):
    """Roster of every function with its fndiff --classify category."""
    roster = {}
    for line in text.splitlines():
        match = CLASSIFY_RE.match(line.strip())
        if match:
            category, name, ti, bi = match.groups()
            roster[name] = {
                "status": category,
                "ti": int(ti) if ti else None,
                "bi": int(bi) if bi else None,
            }
    return roster


def parse_count(text):
    """Per-function real-diff counts from fndiff --count (mismatchers only)."""
    counts = {}
    for line in text.splitlines():
        match = COUNT_RE.match(line.strip())
        if match:
            name, ti, bi, lines, real = match.groups()
            counts[name] = {
                "ti": int(ti), "bi": int(bi),
                "lines": int(lines), "real": int(real),
            }
    return counts


def snapshot(classify_text, count_text):
    """Merge the two fndiff views into {fn: {status, ti, bi, real}}."""
    roster = parse_classify(classify_text)
    counts = parse_count(count_text)
    merged = {}
    for name, entry in roster.items():
        row = dict(entry)
        if entry["status"] in ("BASE_ONLY", "TARGET_ONLY"):
            row["real"] = None
        elif name in counts:
            row.update(counts[name])
        else:
            row["real"] = 0  # EXACT (or noise-only): no DIFF line emitted
        merged[name] = row
    return merged


def compare(baseline, current, renames=None):
    """Verdicts per function; regression = matched fell or real grew.

    ``renames`` maps old baseline names to new current names (--rename
    old=new): a deliberate symbol rename otherwise reads as vanished+new
    and fails the gate on a change that may have added exacts — a worker
    had to arbitrate a 3-exact rename by hand.
    """
    renames = renames or {}
    verdicts = []
    for name, base in sorted(baseline.items()):
        cur = current.get(renames.get(name, name))
        if cur is None:
            verdicts.append((name, "REGRESSION", "function vanished from object"))
            continue
        # Byte-identity backstop: every score in this gate derives from
        # normalized text, and a change once passed NEUTRAL real, IDENTICAL
        # multiset, unchanged clusters AND this gate while regressing fuzzy
        # (claim.law.neutral-real-and-identical-multiset-do-not-prove-byte-
        # identity). The raw hash cannot be fooled: any byte or reloc-line
        # change in a previously-neutral-scored function fails here.
        if (base.get("bytes") and cur.get("bytes")
                and base["bytes"] != cur["bytes"]
                and base.get("real", 1) == 0 and cur.get("real", 1) == 0):
            # Words-hash discriminant: if every instruction WORD is
            # unchanged, the drift lives entirely in reloc lines — and
            # real 0 already proved the resolved reloc ADDRESSES match.
            # That is a symbol rename / naming-only change (ghost fixes,
            # data-symbol renames), which four workers hand-arbitrated
            # before this classification existed.
            if (base.get("words") and cur.get("words")
                    and base["words"] == cur["words"]):
                verdicts.append(
                    (name, "NAMING-DRIFT",
                     "reloc lines renamed, instruction words + resolved"
                     " addresses unchanged — benign; re-baseline with"
                     " --update-improved when done")
                )
                continue
            verdicts.append(
                (name, "REGRESSION",
                 "raw bytes/relocs changed although every score reads"
                 " neutral — encoding or reloc-payload drift; revert or"
                 " verify with objdiff fuzzy before keeping")
            )
            continue
        # A byte-exact function must STAY byte-exact: real normalizes
        # relocation payloads, so EXACT -> RELOCATION_ONLY demotions kept
        # real at 0 and slipped through this gate (4 shipped before the
        # census caught them — claim.law.never-literalize-inside-a-real-
        # zero-function). Status is checked unconditionally now.
        if base["status"] == "EXACT" and cur["status"] != "EXACT":
            verdicts.append(
                (name, "REGRESSION",
                 f"status EXACT -> {cur['status']} (byte-exact demoted;"
                 " real can stay 0 for relocation-payload changes)")
            )
            continue
        base_real, cur_real = base.get("real"), cur.get("real")
        if base_real is None or cur_real is None:
            if base["status"] != cur["status"]:
                verdicts.append(
                    (name, "REGRESSION",
                     f"status {base['status']} -> {cur['status']}")
                )
            continue
        if base_real == 0 and cur_real > 0:
            verdicts.append(
                (name, "REGRESSION", f"was byte-identical, now real {cur_real}")
            )
        elif cur_real > base_real:
            verdicts.append(
                (name, "REGRESSION", f"real {base_real} -> {cur_real}")
            )
        elif cur_real < base_real:
            # Carrier-change discriminant: an equal-count opcode respell
            # can improve real while regressing fuzzy — one such state
            # passed this gate, ninja, AND the DOL sha1 end-to-end. When
            # the opcode multiset changed at equal counts, the win must
            # be arbitrated on fuzzy from a FRESH report before banking.
            if (base.get("opset") and cur.get("opset")
                    and base["opset"] != cur["opset"]
                    and base.get("ti") == cur.get("ti")
                    and base.get("bi") == cur.get("bi")):
                verdicts.append(
                    (name, "IMPROVED-CARRIER",
                     f"real {base_real} -> {cur_real} BUT the opcode"
                     " multiset changed at equal counts — arbitrate on"
                     " fuzzy from a fresh report BEFORE banking (a real"
                     " win of this shape regressed fuzzy end-to-end)")
                )
            else:
                verdicts.append(
                    (name, "IMPROVED", f"real {base_real} -> {cur_real}")
                )
    renamed_targets = set(renames.values())
    for name in sorted(set(current) - set(baseline) - renamed_targets):
        verdicts.append((name, "NEW", "function absent from baseline"))
    return verdicts


def genuine_counts(unit, names):
    """{fn: genuine structural rows} for the named functions.

    `real` is the gate's only score and it cannot see structure: a
    respell that moves the residual strictly nearer target can RAISE real
    while the genuine structural rows fall, because closing one
    compensating error re-aligns every instruction after it. That shape
    read as a flat REGRESSION and had to be overridden by hand
    (attempt.LG_get-vmu-directory-shared-constant-and-branch-pair-
    carriers.20260901.v2, real 48 -> 65 at fuzzy 90.04 -> 92.72).

    Kept to the disputed functions only — two objdumps plus a difflib
    pass each, not a TU-wide census.
    """
    bare = re.sub(r"\.(c|cpp)$", "", unit)
    counts = {}
    try:
        target, ours, resolver = regnorm.load_tables(bare)
    except Exception:
        return counts
    for name in names:
        fn_t = regnorm.resolve_name(target, name)
        fn_o = regnorm.resolve_name(ours, name)
        if fn_t is None or fn_o is None:
            continue
        try:
            result = regnorm.analyze(target[fn_t], ours[fn_o], resolver)
        except Exception:
            continue
        counts[name] = len(result.genuine)
    return counts


def ops_text(bare_unit, name):
    return subprocess.run(
        [sys.executable, str(FNDIFF), bare_unit, name, "--ops",
         "--no-build"], capture_output=True, text=True).stdout


def arbitrate_regressions(verdicts, unit, baseline=None, genuine_fn=None,
                          ops_fn=None):
    """Downgrade real-growth REGRESSIONs to CONFLICT when the current state
    is structurally target-identical (equal insn counts, IDENTICAL opcode
    multiset): the growth can be pure naming churn invisible to `real`.
    Never applies to functions that were byte-exact at baseline."""
    bare_unit = re.sub(r"\.(c|cpp)$", "", unit)
    baseline = baseline or {}
    genuine_fn = genuine_fn or genuine_counts
    ops_fn = ops_fn or ops_text
    disputed = [name for name, verdict, detail in verdicts
                if verdict == "REGRESSION"
                and re.match(r"real (\d+) -> (\d+)$", detail)
                and not detail.startswith("real 0 ")]
    genuine_now = genuine_fn(bare_unit, disputed) if disputed else {}
    out = []
    for name, verdict, detail in verdicts:
        growth = re.match(r"real (\d+) -> (\d+)$", detail)
        if verdict != "REGRESSION" or not growth or growth.group(1) == "0":
            out.append((name, verdict, detail))
            continue
        ops = ops_fn(bare_unit, name)
        identical = re.search(
            r"opcode multiset: IDENTICAL \((\d+)/(\d+)\)", ops)
        if identical and identical.group(1) == identical.group(2):
            out.append((name, "CONFLICT",
                        detail + " BUT opcode multiset IDENTICAL at equal"
                        " insn counts — possible naming churn; arbitrate"
                        " with the diff + objdiff fuzzy, do NOT auto-revert"
                        " (pass --arbitrate to accept)"))
            continue
        # Structure arbiter: real rose but the GENUINE structural rows
        # fell, so the stream is nearer target and `real` is reading the
        # re-alignment, not a regression.
        was = baseline.get(name, {}).get("genuine")
        now = genuine_now.get(name)
        if was is None or now is None:
            if was is None and now is not None:
                out.append((name, verdict, detail + (
                    " [no genuine-row count in this baseline (taken before"
                    " run 29) — the structure arbiter is UNAVAILABLE here;"
                    " re-take the baseline to enable it]")))
                continue
            out.append((name, verdict, detail))
            continue
        if now < was:
            out.append((name, "CONFLICT",
                        detail + f" BUT genuine structural rows {was} ->"
                        f" {now} FELL — the residual moved nearer target"
                        " and real is reading the re-alignment; arbitrate"
                        " on fuzzy from a fresh report, do NOT auto-revert"
                        " (pass --arbitrate to accept)"))
        else:
            out.append((name, verdict,
                        detail + f" (genuine structural rows {was} -> {now})"))
    return out


def run_fndiff(unit, flag):
    result = subprocess.run(
        [sys.executable, str(FNDIFF), unit, flag],
        capture_output=True, text=True,
    )
    if result.returncode != 0 and "missing:" in (result.stdout + result.stderr):
        raise SystemExit(f"fndiff failed for {unit}:\n{result.stdout}{result.stderr}")
    return result.stdout


def git_head():
    result = subprocess.run(["git", "rev-parse", "HEAD"],
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def source_path(unit):
    bare = re.sub(r"\.(c|cpp)$", "", unit)
    for suffix in (".c", ".cpp"):
        candidate = Path("src") / (bare + suffix)
        if candidate.exists():
            return candidate
    return None


def source_digest(unit):
    src = source_path(unit)
    if src is None:
        return None
    return hashlib.sha1(src.read_bytes()).hexdigest()


def load_baseline(path):
    """(functions, meta). Accepts the pre-run-29 bare-dict format."""
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict) and "functions" in data and "meta" in data:
        return data["functions"], data["meta"]
    return data, {}


def save_baseline(path, snap, unit):
    """Anchor every baseline to the commit and source bytes it was taken
    at.

    DURABILITY DECISION (run 29, item 3): gate snapshots are KEYED TO THE
    COMMIT, not committed to git. build/GUNE5D/gate/ is build output and
    a per-worktree working state; committing it would put every lane's
    baselines in every other lane's merge path. What actually failed was
    RECONSTRUCTION — a lane needed run 26's baseline, the worktree had
    been pruned, and it had to rebuild one from clean HEAD bytes by hand
    and prove the reconstruction by matching a function count quoted in a
    record. So the file now records the commit and the source sha1 it was
    taken at, `check` says so when HEAD has moved, and `baseline
    --at-head` performs that reconstruction as a command.
    """
    meta = {
        "unit": unit,
        "head": git_head(),
        "source_sha1": source_digest(unit),
        "taken_at": datetime.now(timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ"),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"meta": meta, "functions": snap}, indent=2,
                   sort_keys=True), encoding="utf-8")
    return meta


def gate_path(unit):
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", re.sub(r"\.(c|cpp)$", "", unit))
    return Path(f"build/{VERSION}/gate/{slug}.json")


def normalize_unit(unit):
    """Accept src/-prefixed, backslashed, or extensioned unit spellings."""
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    return unit


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update_improved = "--update-improved" in sys.argv
    rebuild = "--rebuild" in sys.argv or "--build" in sys.argv
    arbitrate = "--arbitrate" in sys.argv
    at_head = "--at-head" in sys.argv
    renames = {}
    for arg in sys.argv[1:]:
        if arg.startswith("--rename="):
            old, _, new = arg[len("--rename="):].partition("=")
            if old and new:
                renames[old] = new
    bank_arbitrated = next(
        (a.split("=", 1)[1] for a in sys.argv
         if a.startswith("--bank-arbitrated=")), None)
    if len(args) != 2 or args[0] not in ("baseline", "check"):
        print(__doc__)
        return 2
    mode, unit = args
    # Paired-fix lanes touch coupled TUs (a signature change and its
    # callers); accept a comma-separated unit list so both sides are gated
    # in one call instead of relying on worker judgment to gate the second.
    if "," in unit:
        worst = 0
        for one in unit.split(","):
            one = one.strip()
            if not one:
                continue
            print(f"==== {one} ====")
            code = run_single(mode, one, rebuild, update_improved, arbitrate,
                              renames, bank_arbitrated, at_head)
            worst = max(worst, code)
        return worst
    return run_single(mode, unit, rebuild, update_improved, arbitrate,
                      renames, bank_arbitrated, at_head)


def run_single(mode, unit, rebuild, update_improved, arbitrate, renames=None,
               bank_arbitrated=None, at_head=False):
    unit = normalize_unit(unit)
    if at_head:
        # Reconstruct the baseline the CURRENT COMMIT implies, with the
        # working tree's edits temporarily out of the way. This is the
        # by-hand procedure a lane had to invent when a pruned worktree
        # took its predecessor's baseline with it (swap the file out,
        # write HEAD's bytes back, baseline, restore).
        if mode != "baseline":
            print("--at-head only applies to `baseline`")
            return 2
        src = source_path(unit)
        if src is None:
            print(f"--at-head: no source found for {unit}")
            return 2
        shown = subprocess.run(["git", "show", f"HEAD:{src.as_posix()}"],
                               capture_output=True)
        if shown.returncode != 0:
            print(f"--at-head: git show HEAD:{src.as_posix()} failed")
            return 1
        saved = src.read_bytes()
        dirty = saved != shown.stdout
        try:
            if dirty:
                src.write_bytes(shown.stdout)
            print(f"--at-head: baselining {unit} from commit"
                  f" {(git_head() or '?')[:9]}"
                  + (" (working-tree edits temporarily set aside)"
                     if dirty else " (working tree already matches HEAD)"))
            return run_single(mode, unit, True, update_improved, arbitrate,
                              renames, bank_arbitrated, at_head=False)
        finally:
            if dirty:
                src.write_bytes(saved)
                subprocess.run(["ninja", f"build/{VERSION}/src/"
                                f"{re.sub(r'[.](c|cpp)$', '', unit)}.o"],
                               capture_output=True, text=True)
                print("--at-head: working-tree source restored and"
                      " rebuilt")
    if rebuild:
        obj = re.sub(r"\.(c|cpp)$", "", unit)
        build = subprocess.run(
            ["ninja", f"build/{VERSION}/src/{obj}.o"],
            capture_output=True, text=True,
        )
        if build.returncode != 0:
            print("BUILD FAILED (gate not run):")
            print((build.stdout + build.stderr).strip()[-1500:])
            return 1
    snap = snapshot(run_fndiff(unit, "--classify"), run_fndiff(unit, "--count"))
    objfile = Path(
        f"build/{VERSION}/src/{re.sub(r'[.](c|cpp)$', '', unit)}.o")
    if objfile.exists():
        for name, digest in fndiff.raw_signature(objfile).items():
            if name in snap:
                snap[name]["bytes"] = digest
        for name, digest in fndiff.raw_words_signature(objfile).items():
            if name in snap:
                snap[name]["words"] = digest
        for name, digest in fndiff.opcode_multiset_signature(objfile).items():
            if name in snap:
                snap[name]["opset"] = digest
    # Genuine structural rows for every function `real` calls imperfect —
    # the structure arbiter's baseline half. Byte-exact rows can never be
    # disputed, so they are skipped and the count stays cheap.
    mismatching = [name for name, row in snap.items() if row.get("real")]
    if mismatching:
        for name, count in genuine_counts(unit, mismatching).items():
            if name in snap:
                snap[name]["genuine"] = count
    path = gate_path(unit)
    if mode == "baseline":
        meta = save_baseline(path, snap, unit)
        exact = sum(1 for row in snap.values() if row.get("real") == 0)
        print(f"baseline: {len(snap)} functions ({exact} at real 0) -> {path}")
        print(f"  anchored to commit {(meta.get('head') or '?')[:9]},"
              f" source sha1 {(meta.get('source_sha1') or '?')[:9]}"
              " — rebuild this exact baseline anywhere with"
              f" `defake_gate.py baseline {unit} --at-head` on that commit")
        return 0
    if not path.exists():
        print(f"no baseline at {path}; run `defake_gate.py baseline {unit}` first")
        return 2
    baseline, meta = load_baseline(path)
    if meta:
        head = git_head()
        if meta.get("head") and head and meta["head"] != head:
            print(f"[baseline was taken at commit {meta['head'][:9]}, HEAD is"
                  f" now {head[:9]} — it still gates, but say WHICH commit"
                  " it anchors to when quoting its numbers]")
    else:
        print("[baseline predates run 29: no commit anchor and no"
              " genuine-row counts — re-take it to enable the structure"
              " arbiter and make it reconstructable]")
    if bank_arbitrated:
        # Re-anchor ONE function's row over a fuzzy-arbitrated keep the
        # mandate accepts but `real` reads as a regression. Re-running a
        # full `baseline` to launder such a keep silently discarded the
        # ability to catch a LATER genuine sibling regression — two lanes
        # hit that. Every other row keeps its original anchor.
        target_row = None
        for name in (bank_arbitrated,):
            if name in snap:
                target_row = name
            else:
                for cand in snap:
                    if (cand.startswith(name + "_80")
                            or name.startswith(cand + "_80")):
                        target_row = cand
                        break
        if target_row is None:
            print(f"--bank-arbitrated: {bank_arbitrated} not in snapshot")
            return 2
        archive = path.with_suffix(".prev.json")
        archive.write_text(path.read_text(encoding="utf-8"),
                           encoding="utf-8")
        baseline[target_row] = snap[target_row]
        save_baseline(path, baseline, unit)
        print(f"banked arbitrated keep for {target_row} (that row only —"
              " every sibling still gates against its original anchor;"
              " record the arbitration + its fuzzy in the attempt record)")
    verdicts = compare(baseline, snap, renames)
    verdicts = arbitrate_regressions(verdicts, unit, baseline)
    conflicts = [v for v in verdicts if v[1] == "CONFLICT"]
    regressions = [v for v in verdicts if v[1] == "REGRESSION"]
    for name, verdict, detail in verdicts:
        print(f"{verdict:10} {name}  {detail}")
    if conflicts and not regressions and arbitrate:
        if update_improved:
            # Law-sanctioned keep (fuzzy/slot arbitration): re-anchor the
            # baseline over the arbitrated state so later checks gate
            # against it, instead of leaving the override as an
            # undocumented manual `baseline` re-run.
            archive = path.with_suffix(".prev.json")
            archive.write_text(path.read_text(encoding="utf-8"),
                               encoding="utf-8")
            save_baseline(path, snap, unit)
            print(f"GATE OK (arbitrated: {len(conflicts)} CONFLICT accepted;"
                  " baseline RE-ANCHORED over the arbitrated state — record"
                  " the arbitration + its metric in the attempt record)")
            return 0
        print(f"GATE OK (arbitrated: {len(conflicts)} CONFLICT accepted —"
              " record the arbitration in the attempt record; add"
              " --update-improved to re-anchor the baseline on it)")
        return 0
    if conflicts and not regressions:
        print(f"GATE FAILED: {len(conflicts)} CONFLICT — arbitrate (diff +"
              " objdiff fuzzy), then re-run with --arbitrate to accept or"
              " revert")
        return 1
    if regressions:
        print(f"GATE FAILED: {len(regressions)} regression(s) — revert or fix"
              " before committing")
        bare_unit = re.sub(r"\.(c|cpp)$", "", unit)
        for name, _verdict, detail in regressions[:4]:
            if "vanished" in detail:
                continue
            print(f"---- fndiff --ops {name} ----")
            ops = subprocess.run(
                [sys.executable, str(FNDIFF), bare_unit, name,
                 "--ops", "--no-build"],
                capture_output=True, text=True,
            ).stdout
            print("\n".join(ops.strip().splitlines()[:14]))
        return 1
    improved = [v for v in verdicts if v[1] == "IMPROVED"]
    if improved and update_improved:
        # Archive the outgoing baseline so the session-start census stays
        # reconstructable (a worker had to rebuild it from transcripts).
        archive = path.with_suffix(".prev.json")
        archive.write_text(path.read_text(encoding="utf-8"),
                           encoding="utf-8")
        save_baseline(path, snap, unit)
        print(f"baseline updated with {len(improved)} improvement(s)"
              f" (previous archived at {archive.name})")
    print("GATE OK" + (f" ({len(improved)} improved)" if improved else ""))
    if improved and not update_improved:
        print("(flags combine: re-run with --rebuild --update-improved to"
              " bank these in ONE call — no separate second check needed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
