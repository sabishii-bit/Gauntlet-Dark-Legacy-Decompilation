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

A real-count regression on a fuzzy function whose CURRENT opcode multiset is
IDENTICAL to target at equal insn counts is reported as CONFLICT instead of
REGRESSION: structure fully matches target and the extra real lines can be
pure register-naming churn, which this gate's real-only score cannot see
(two workers independently hit this: a genuine load-schedule win scored as
real 100->104). CONFLICT still fails the gate by default -- arbitrate by
reading the diff and objdiff fuzzy; pass --arbitrate to accept a checked
CONFLICT-only result. Byte-exact functions are never eligible: any drift on
a real-0 function stays REGRESSION.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
FNDIFF = Path(__file__).resolve().parent / "fndiff.py"

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402  (raw_signature: the byte-identity backstop)

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


def arbitrate_regressions(verdicts, unit):
    """Downgrade real-growth REGRESSIONs to CONFLICT when the current state
    is structurally target-identical (equal insn counts, IDENTICAL opcode
    multiset): the growth can be pure naming churn invisible to `real`.
    Never applies to functions that were byte-exact at baseline."""
    bare_unit = re.sub(r"\.(c|cpp)$", "", unit)
    out = []
    for name, verdict, detail in verdicts:
        growth = re.match(r"real (\d+) -> (\d+)$", detail)
        if verdict != "REGRESSION" or not growth or growth.group(1) == "0":
            out.append((name, verdict, detail))
            continue
        ops = subprocess.run(
            [sys.executable, str(FNDIFF), bare_unit, name,
             "--ops", "--no-build"],
            capture_output=True, text=True,
        ).stdout
        identical = re.search(
            r"opcode multiset: IDENTICAL \((\d+)/(\d+)\)", ops)
        if identical and identical.group(1) == identical.group(2):
            out.append((name, "CONFLICT",
                        detail + " BUT opcode multiset IDENTICAL at equal"
                        " insn counts — possible naming churn; arbitrate"
                        " with the diff + objdiff fuzzy, do NOT auto-revert"
                        " (pass --arbitrate to accept)"))
        else:
            out.append((name, verdict, detail))
    return out


def run_fndiff(unit, flag):
    result = subprocess.run(
        [sys.executable, str(FNDIFF), unit, flag],
        capture_output=True, text=True,
    )
    if result.returncode != 0 and "missing:" in (result.stdout + result.stderr):
        raise SystemExit(f"fndiff failed for {unit}:\n{result.stdout}{result.stderr}")
    return result.stdout


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
    renames = {}
    for arg in sys.argv[1:]:
        if arg.startswith("--rename="):
            old, _, new = arg[len("--rename="):].partition("=")
            if old and new:
                renames[old] = new
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
            code = run_single(mode, one, rebuild, update_improved, arbitrate, renames)
            worst = max(worst, code)
        return worst
    return run_single(mode, unit, rebuild, update_improved, arbitrate, renames)


def run_single(mode, unit, rebuild, update_improved, arbitrate, renames=None):
    unit = normalize_unit(unit)
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
    path = gate_path(unit)
    if mode == "baseline":
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(snap, indent=2, sort_keys=True), encoding="utf-8")
        exact = sum(1 for row in snap.values() if row.get("real") == 0)
        print(f"baseline: {len(snap)} functions ({exact} at real 0) -> {path}")
        return 0
    if not path.exists():
        print(f"no baseline at {path}; run `defake_gate.py baseline {unit}` first")
        return 2
    baseline = json.loads(path.read_text(encoding="utf-8"))
    verdicts = compare(baseline, snap, renames)
    verdicts = arbitrate_regressions(verdicts, unit)
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
            path.write_text(json.dumps(snap, indent=2, sort_keys=True),
                            encoding="utf-8")
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
        path.write_text(json.dumps(snap, indent=2, sort_keys=True), encoding="utf-8")
        print(f"baseline updated with {len(improved)} improvement(s)"
              f" (previous archived at {archive.name})")
    print("GATE OK" + (f" ({len(improved)} improved)" if improved else ""))
    if improved and not update_improved:
        print("(flags combine: re-run with --rebuild --update-improved to"
              " bank these in ONE call — no separate second check needed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
