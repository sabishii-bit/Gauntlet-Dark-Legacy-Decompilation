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

Escape hatches (a worker concluded --discard "does not exist" because this
docstring omitted it — the flags below all work):
  --discard          restore the TU to HEAD (the neutral-edit undo)
  --revert-baseline  restore the SESSION's first banked baseline
  --no-bank          score without banking (diagnostic probes)

Two semantics every worker must know before trusting --revert as an undo:
(1) NEUTRAL probes BANK TOO (they may be verified-neutral work worth
keeping), so after a neutral probe --revert restores that neutral edit,
not the pre-edit state — use git to discard a neutral edit you don't
want. (2) The snapshot is the WHOLE TU file: in a multi-function session
a revert takes every function back with it — commit each function's
retained state before probing the next.
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


def bank_snapshot(unit, source, baseline=False):
    snap = snapshot_path(unit, source)
    shutil.copyfile(source, snap)
    head = git_head()
    if head:
        snap.with_suffix(snap.suffix + ".meta").write_text(
            json.dumps({"head": head}), encoding="utf-8")
    # The FIRST baseline of a session is banked separately and never
    # overwritten: NEUTRAL probes re-bank the rolling snapshot, so
    # --revert restores the last neutral edit rather than the pristine
    # state (five workers hit this). --revert-baseline reaches past that.
    if baseline:
        base = snap.with_suffix(snap.suffix + ".base")
        if not base.exists():
            shutil.copyfile(source, base)


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
            for ext in (".meta", ".base"):
                extra = snap.with_suffix(snap.suffix + ext)
                if extra.exists():
                    extra.unlink()
        print("probe state reset")
        return 0
    if "--discard" in sys.argv:
        # Revert the TU to its last COMMITTED state — the undo people
        # actually want after a neutral probe (NEUTRAL banks, so --revert
        # restores the neutral edit; --discard reaches for HEAD).
        if source is None:
            print(f"cannot discard: no src source found for {unit}")
            return 1
        shown = subprocess.run(
            ["git", "show", f"HEAD:{source.as_posix()}"], capture_output=True)
        if shown.returncode != 0:
            print("cannot discard: git show HEAD failed for"
                  f" {source.as_posix()}")
            return 1
        source.write_bytes(shown.stdout)
        print(f"discarded: {source} restored to HEAD (whole file —"
              " uncommitted work on other functions in this TU is gone)")
        return 0
    if "--revert-baseline" in sys.argv:
        if source is None:
            print(f"cannot revert: no src source found for {unit}")
            return 1
        base = snapshot_path(unit, source)
        base = base.with_suffix(base.suffix + ".base")
        if not base.exists():
            print("no session baseline banked for this unit (the first"
                  " BASELINE probe of a session banks it)")
            return 1
        shutil.copyfile(base, source)
        print(f"restored {source} to the SESSION BASELINE (whole file —"
              " uncommitted work on other functions in this TU is gone)")
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
            print("nothing to revert: the banked snapshot IS the current"
                  " working tree (NEUTRAL probes bank too). If you want to"
                  " discard an uncommitted neutral edit, use git"
                  " (`git status` / `git checkout -- <file>`); re-scoring:")
        else:
            shutil.copyfile(snap, source)
            print(f"reverted {source} to {fn}'s banked snapshot —"
                  " NOTE this restores the WHOLE FILE: uncommitted work on"
                  " OTHER functions in this TU since that bank is gone"
                  " (three workers hit this; commit per function, or use"
                  " git for surgical reverts); re-scoring:")

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
    # fndiff strips a trailing _80XXXXXX address suffix from user-supplied
    # names; accept either spelling here so one name works everywhere.
    fn_stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", fn)
    real = None
    for line in count.splitlines():
        match = COUNT_RE.match(line.strip())
        if match and match.group(1) in (fn, fn_stripped):
            _, ti, bi, lines, real_text = match.groups()
            real = int(real_text)
            insns = f"T{ti}/O{bi}"  # target/ours — labeled after a worker
            # mis-read which side was longer reconciling probe vs fndiff
            break
    else:
        if re.search(rf"^OK\s+({re.escape(fn)}|{re.escape(fn_stripped)})\s*$",
                     count, re.M) or not count.strip():
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
        # A real win with a BLOWN-OUT count distance once banked as best
        # (949->802 while the function LOST 157 instructions), poisoning
        # every later verdict until --rebase-best. Refuse to bank those.
        def _cdist(text):
            m = re.match(r"T(\d+)/O(\d+)$", text or "")
            return abs(int(m.group(1)) - int(m.group(2))) if m else None
        pd_, cd_ = _cdist(state.get("last_insns")), _cdist(insns)
        if pd_ is not None and cd_ is not None and cd_ > pd_ + 4:
            verdict = (f"IMPROVED? real {best} -> {real} BUT count"
                       f" distance {pd_} -> {cd_} blew out — best NOT"
                       " updated; this is structural divergence wearing a"
                       " real win. Arbitrate on fresh fuzzy;"
                       " --rebase-best banks a deliberate keep")
        else:
            verdict = (f"IMPROVED  real {best} -> {real} (insns"
                       f" {insns}{tok})  [best updated]")
            state["best_real"] = real
        # Parity-held improvements are the one IMPROVED shape that has
        # regressed fuzzy end-to-end (real 30->24 at unchanged T47/O47
        # was a fuzzy 80.85->71.89 loss; probe+gate both passed it).
        # When insn counts already agreed and did not move, real fell
        # inside an already-parity-exact shell — fuzzy is the arbiter.
        parity = re.match(r"T(\d+)/O(\d+)$", insns or "")
        if (parity and parity.group(1) == parity.group(2)
                and state.get("last_insns") == insns):
            verdict += ("\nPARITY-HELD IMPROVEMENT: counts were already"
                        " equal and unchanged — arbitrate on FRESH objdiff"
                        " fuzzy BEFORE treating this as progress or running"
                        " --update-improved; revert if fuzzy fell")
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
            # Count distance is the one cheap predictor that agreed with
            # fuzzy in all four field arbitrations of this shape —
            # multiset gains do NOT imply fuzzy gains.
            def _cd(text):
                m = re.match(r"T(\d+)/O(\d+)$", text or "")
                return (abs(int(m.group(1)) - int(m.group(2)))
                        if m else None)
            pd, cd = _cd(state.get("last_insns")), _cd(insns)
            if pd is not None and cd is not None and pd != cd:
                # Bounded by three independent lane measurements: the
                # predictor is only sound when the MULTISET IS FLAT —
                # it measures residue after structure is held constant.
                # With the multiset moving it was wrong 4/4 on one
                # function (structure-changing probes), and it must
                # never be weighed against fuzzy itself.
                flat = (multiset_tokens is not None
                        and prev_tokens is not None
                        and multiset_tokens == prev_tokens)
                if flat:
                    trend = ("WORSE — expect a fuzzy loss"
                             if cd > pd else "better — fuzzy may agree")
                    verdict += (f"\nCOUNT DISTANCE {pd} -> {cd} at a"
                                f" flat multiset ({trend}); fuzzy from"
                                " a fresh report remains the arbiter")
                else:
                    verdict += (f"\nCOUNT DISTANCE {pd} -> {cd} but the"
                                " multiset moved — the predictor is NOT"
                                " valid here; arbitrate on fresh fuzzy"
                                " only")
        else:
            # The arrow is previous->current; the CLASSIFICATION is vs
            # best. Printing both without labels read as a contradiction
            # ("REGRESSED 244 -> 234") and cost two re-reads in the field.
            verdict = (f"REGRESSED vs best {best}: real"
                       f" {state.get('last_real', best)} -> {real}"
                       f" (prev -> current; insns {insns}{tok})"
                       "  [revert advised]")
    else:
        verdict = f"NEUTRAL   real {real} (insns {insns}{tok})"
        # real-equal is not structure-equal: a NEUTRAL that moved the
        # insn count further from parity or grew the multiset is WORSE,
        # and banking it made --revert refuse to undo a bad probe (a
        # worker had to restore by hand). Key the verdict on the triple.
        def _dist(text):
            m = re.match(r"T(\d+)/O(\d+)$", text or "")
            return abs(int(m.group(1)) - int(m.group(2))) if m else None
        worse = []
        prev_dist, cur_dist = _dist(state.get("last_insns")), _dist(insns)
        if (prev_dist is not None and cur_dist is not None
                and cur_dist > prev_dist):
            worse.append(f"count distance {prev_dist} -> {cur_dist}")
        if (multiset_tokens is not None and prev_tokens is not None
                and multiset_tokens > prev_tokens):
            worse.append(f"multiset {prev_tokens}t -> {multiset_tokens}t")
        # NEUTRAL scores do NOT prove byte identity (a fuzzy-visible
        # encoding change once passed every score in this tool). Hash the
        # function's raw bytes and say so when they moved. Byte identity
        # is GROUND TRUTH and computed FIRST: the worse-triple check
        # compares against last-probe state that can be stale, and the
        # two once printed a contradictory composite (NEUTRAL-WORSE +
        # NEUTRAL-IDENTICAL) a worker had to untangle by hand.
        bytes_identical = None
        try:
            sys.path.insert(0, str(TOOLS))
            import fndiff as _fndiff
            objfile = Path(f"build/{VERSION}/src/{unit}.o")
            digest = _fndiff.raw_signature(objfile).get(
                fn) or _fndiff.raw_signature(objfile).get(fn_stripped)
            prev_digest = state.get("last_bytes")
            if digest is not None:
                state["last_bytes"] = digest
                if prev_digest is not None:
                    bytes_identical = digest == prev_digest
                if prev_digest is not None and digest != prev_digest:
                    verdict += ("  [NEUTRAL-REARRANGED: OBJECT BYTES"
                                " CHANGED — this compares BUILT OBJECTS"
                                " between probes, not your source vs git"
                                " (source can be identical to HEAD and"
                                " still trip this); neutral scores do not"
                                " prove identity — verify with objdiff"
                                " fuzzy or revert]")
                elif prev_digest is not None:
                    verdict += ("  [NEUTRAL-IDENTICAL: object bytes"
                                " unchanged — the edit FOLDED AWAY before"
                                " codegen. For a spelling probe this is a"
                                " STRONGER negative than a regression:"
                                " the source text never reached the"
                                " compiler's decision point]")
        except Exception:
            pass
        if worse and bytes_identical is not True:
            head = f"NEUTRAL   real {real}"
            verdict = (f"NEUTRAL-WORSE real {real}"
                       f" ({'; '.join(worse)}) — structurally worse at"
                       " equal real; NOT banked, revert with git (not"
                       " --revert) or justify the keep explicitly"
                       + verdict[len(head):])
    state["last_real"] = real
    state["last_insns"] = insns
    if multiset_tokens is not None:
        state["last_multiset"] = multiset_tokens
    state_file.write_text(json.dumps(state), encoding="utf-8")
    print(verdict)

    if "--fuzzy" in sys.argv:
        # One-call fresh-fuzzy readout: build the report and print this
        # function's fuzzy. CONFLICT arbitration used to cost two manual
        # builds per keep; this is that loop, made durable. The report
        # build is a full link — expect it to take as long as ninja.
        rep = subprocess.run(["ninja", f"build/{VERSION}/report.json"],
                             capture_output=True, text=True)
        if rep.returncode != 0:
            print("[--fuzzy: report build FAILED — no fuzzy readout]")
        else:
            try:
                report = json.loads(
                    Path(f"build/{VERSION}/report.json").read_text(
                        encoding="utf-8"))
                bare = re.sub(r"\.(c|cpp)$", "", unit)
                val = None
                for entry in report.get("units", []):
                    if entry.get("name", "").endswith(bare):
                        for func in entry.get("functions", []):
                            if func["name"] in (fn, fn_stripped) or \
                                    func["name"].startswith(fn + "_80"):
                                val = float(
                                    func.get("fuzzy_match_percent", 0.0))
                prev_fz = state.get("last_fuzzy")
                if val is not None:
                    arrow = (f" (prev {prev_fz:.4f})"
                             if isinstance(prev_fz, float) else "")
                    print(f"FUZZY (fresh report): {val:.4f}%{arrow}")
                    state["last_fuzzy"] = val
                    state_file.write_text(json.dumps(state),
                                          encoding="utf-8")
                else:
                    print("[--fuzzy: function not found in report]")
            except Exception as err:
                print(f"[--fuzzy: readout failed: {err}]")

    if verdict.startswith("BASELINE") and source is not None:
        # Scaffold census: pragmas and volatile qualifiers go STALE and
        # nothing in the loop re-audits them — two of four scaffold items
        # in one function were stale in a single session, worth a third
        # of its total gap. File-wide, line-numbered, one line each.
        try:
            lines = Path(source).read_text(
                encoding="utf-8", errors="replace").splitlines()
            rows = [f"  L{i+1}: {ln.strip()[:70]}"
                    for i, ln in enumerate(lines)
                    if re.search(r"#pragma\s+(?!force_active)"
                                 r"|(^|[^\w])volatile[^\w]", ln)]
            if rows:
                print(f"[scaffold census ({len(rows)} file-wide rows —"
                      " re-audit each: is its original premise still"
                      " live?)]")
                for row in rows[:20]:
                    print(row)
                if len(rows) > 20:
                    print(f"  ... and {len(rows) - 20} more")
        except Exception:
            pass

    # Bank a revert point whenever this source state scores at the
    # high-water mark. NEUTRAL banks too: a verified-neutral state (the
    # normal product of de-fakematch batches) is as good as best, and NOT
    # banking it made --revert silently discard gated neutral work twice
    # in the field. To discard a neutral edit you dislike, use git — the
    # snapshot always points at the last state that scored best.
    # --no-bank: score without banking. For DIAGNOSTIC probes (rulers,
    # liveness pokes, calibration instruments) that will be hand-reverted —
    # without it a neutral diagnostic becomes its own revert point and
    # --revert can no longer reach the pre-diagnostic state.
    if source is not None and "--no-bank" not in sys.argv and (
            verdict.startswith("BASELINE")
            or verdict.startswith("IMPROVED")
            or (verdict.startswith("NEUTRAL")
                and not verdict.startswith("NEUTRAL-WORSE"))
            or verdict.startswith("REBASED")):
        bank_snapshot(unit, source,
                      baseline=verdict.startswith("BASELINE"))
        kind = verdict.split()[0]
        # Name WHICH verdict banked: the discard path differs (a banked
        # NEUTRAL means --revert restores the PROBE, so discarding it
        # needs git), and the identical banner hid that twice.
        print(f"[revert point banked ({kind}): probe.py {unit} {fn}"
              " --revert restores THIS state"
              + ("; to discard this neutral edit use git, not --revert"
                 if kind == "NEUTRAL" else "") + "]")
    elif source is not None and "--no-bank" in sys.argv:
        print("[--no-bank: snapshot NOT updated — hand-revert this edit]")

    # A failed probe almost always needs the ops view next — print it
    # unasked (the multiset pass above already fetched it).
    printed_ops = False
    if (verdict.startswith(("REGRESSED", "CONFLICT"))
            and "--ops" not in sys.argv and ops_output):
        print("\n".join(ops_output.strip().splitlines()[:16]))
        printed_ops = True

    if "--ops" in sys.argv:
        if ops_output is None:
            ops_output = subprocess.run(
                [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
                 "--ops", "--no-build"],
                capture_output=True, text=True,
            ).stdout
        print(ops_output.strip())
        printed_ops = True

    # Repeat the verdict LAST: shells routinely tail long output, and two
    # workers misread results when the verdict scrolled above the ops dump.
    if printed_ops:
        print(f"VERDICT (repeated): {verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
