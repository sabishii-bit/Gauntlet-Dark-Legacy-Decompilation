#!/usr/bin/env python3
"""One-shot probe scorer for the matching iteration loop.

Collapses the ubiquitous edit-loop pair (`ninja <obj>` then `fndiff --count`)
into a single call with explicit verdicts — no more comparing numbers against
scrollback by eye. Tracks best/last per function in build/GUNE5D/gate/.

Usage:
  python tools/gdl/probe.py game/game/player do_players          # build+score
  python tools/gdl/probe.py game/game/player do_players --ops    # + ops scan
  python tools/gdl/probe.py game/game/player do_players --revert # restore THIS
                                                                 # function from
                                                                 # the banked
                                                                 # source, then
                                                                 # build+score
  python tools/gdl/probe.py game/game/player do_players --revert --whole-file
      # take the whole TU back to the snapshot (the old behaviour)
  python tools/gdl/probe.py game/game/player do_players --fuzzy  # arbitration
                                                                 # READOUT only
  python tools/gdl/probe.py game/game/player do_players --arbitrate
      # ONE call, BOTH states: builds and scores the banked snapshot AND the
      # working tree and prints the (real, fuzzy) pair for each, then restores
      # the working tree. --vs-baseline arbitrates against the session's first
      # banked baseline instead of the rolling snapshot.
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

STRUCTURE OUTRANKS REAL IN THE HEADLINE. `real` is a linear diff; the opcode
multiset token count is what says whether the stream is the right SHAPE, and
where the two disagree the multiset names the verdict:
  CONFLICT-UNARBITRATED  a CONFLICT with no fresh fuzzy on both states.
            It classifies NOTHING and exits 3: `real` and the multiset point
            opposite ways, and the arbiter for that is objdiff fuzzy. Run
            --arbitrate (both halves, one call) or --fuzzy on each state,
            then re-probe. PC recorded a false regression from an
            unarbitrated CONFLICT headline in run 34.
  CONFLICT  real fell BUT the multiset GREW — a shape moving away from
            target wearing a real win. Best is NOT updated; this read plain
            "IMPROVED [best updated]" before, indistinguishable from a probe
            that improved both, and banked the diverged state as the revert
            point.
  CONFLICT  real rose BUT the multiset FELL — structure is converging;
            do not auto-revert.
  IMPROVED-STRUCTURE  real UNCHANGED but the multiset FELL — a structural
            win the plain NEUTRAL headline hid, and which left the
            best_multiset anchor stale at the worse count.
A real move with the multiset flat, or unmeasurable, keeps its real-only
verdict; the multiset-GREW-at-flat-real case stays with annotate_neutral's
NEUTRAL-WORSE, which owns the byte-identity check.

DATA-ONLY EDITS ARE NOT FOLD-AWAYS. A pooled value (an FP literal, a
string, a table) is materialized into .sdata2/.rodata and merely LOADED,
so correcting a wrong constant changes no instruction word — every score
here reads NEUTRAL and the function's text bytes hash identical. The
byte-identity annotation used to call that "the edit FOLDED AWAY before
codegen ... a STRONGER negative than a regression", which is the exact
opposite of the truth and told one lane its five correct constant fixes
(two of them behavioural bugs) were null probes. probe now hashes the
object's NON-TEXT sections too and prints NEUTRAL-DATA-ONLY, naming the
sections that moved and directing to a value audit rather than a revert.

Every BASELINE or IMPROVED probe banks a snapshot of the TU source; a later
`--revert` copies it back and re-scores in the same call, replacing the
edit -> probe -> hand-retype-revert -> probe cycle. The snapshot covers the
TU's own .c/.cpp only — header edits are yours to manage — and the banked
state is per-unit, so probe a BASELINE before your first edit of a session.

TWO revert points, and the second one no longer depends on the first
verdict. The ROLLING snapshot moves with every banking verdict, NEUTRAL
included — that is deliberate (a gated neutral de-fakematch state is as
good as best, and not banking it made --revert throw the work away twice).
The SESSION BASELINE does not move. Before run 36 it was written only when
the verdict was literally BASELINE, so a worker whose first probe landed
NEUTRAL — the normal case when a state file survives from an earlier
session — got no baseline at all and lost the pre-edit state to the next
neutral re-bank (CL, run 35). The first bank on a unit now creates it
whatever the verdict, it is never overwritten silently, and --rebaseline
is the deliberate override.

Escape hatches (a worker concluded --discard "does not exist" because this
docstring omitted it — the flags below all work):
  --discard          restore the TU to HEAD (the neutral-edit undo)
  --revert-baseline  restore the SESSION's first banked baseline
  --no-bank          score without banking (diagnostic probes)
  --raw              score the pre-webfrank compiler output (pinned TUs)
  --rederive-pin     one call: build the raw body object, run
                     wf_rederive_pin --apply (guarded: aborts if a BODY hash
                     moved), configure.py, and rebuild the object to confirm
                     the WEBFRANK stage reapplies — the repair for a downstream
                     permutation pin after an upstream pool renumbering
  --rederive-pin --transient
                     the same, for a THROWAWAY A/B: the pin's pre-probe hashes
                     are banked first, and --revert / --revert-baseline /
                     --discard restore them and drop the bank. Without it a
                     revert restores the SOURCE and leaves the re-derived
                     hashes in webfrank.json, which GW measured as ~2 of 15
                     probe cycles spent on pure pin plumbing
  --slots            force the slotdiff map even without a slot signal
  --no-slots         suppress the auto-invoked slot map (below)
  --rebaseline       deliberately MOVE the session baseline to the current
                     state. The baseline is created by the first bank on a
                     unit whatever verdict caused it, and is never
                     overwritten silently; this is the override
  --no-fuzzy-gate    skip the pre-bank fresh-fuzzy measurement (below).
                     Faster, and how the loop behaved before run 36 — but
                     a keep banked this way is unarbitrated
  --stateless        sweep mode: score only — no state, bank, or verdict
  --verbose          print the pragma/volatile scaffold census. It is NOT
                     printed by default any more: in full it ran 13-22
                     lines on every BASELINE probe and buried the verdict
                     the probe was run for. A BASELINE now prints a
                     one-line row COUNT instead, so the audit obligation
                     is announced without the wall of text
  --scaffold         same as --verbose (kept: it is in muscle memory)
  --scaffold-all     print EVERY scaffold row (the census is otherwise
                     capped at 20, and a TU whose scaffold runs past the
                     cut could not be audited from the loop at all)

Two semantics every worker must know before trusting --revert as an undo:
(1) NEUTRAL probes BANK TOO (they may be verified-neutral work worth
keeping), so after a neutral probe --revert restores that neutral edit,
not the pre-edit state — use git to discard a neutral edit you don't
want. (2) --revert is FUNCTION-SCOPED: the snapshot is the whole TU file,
but only the hunks lying strictly inside the NAMED function are restored,
so a multi-function session no longer loses its other in-progress work to
one revert (five lanes hit that). A hunk straddling the function boundary
is REFUSED loudly, never guessed at; `--revert --whole-file` then takes
the old all-or-nothing restore deliberately. --revert-baseline and
--discard remain whole-file by construction.

(3) NO RESTORE MAY DELETE COMMITTED WORK. Both banked states (the rolling
snapshot and the session .base) are stamped with the commit they were
taken at, exactly as defake_gate anchors its baselines, and any restore
whose bytes differ from HEAD's while the anchor is older than HEAD — or
ABSENT — is REFUSED. Fail-closed on a missing stamp is the point: on
2026-09-02 `--revert --whole-file` refused correctly while
`--revert-baseline` (which had no stamp at all) and an unstamped
`--revert` each deleted a committed line and reported it back as
"+0/-1 vs HEAD — an edit this revert could not reach". `--discard` is
always safe: it restores HEAD itself. Override with
--force-stale-revert only after `git diff` confirms nothing is lost.

THE SLOT MAP ARRIVES WITH THE VERDICT. `real` actively fights frame work —
a design one 4-byte step from a slot-exact map can score REGRESSED while
four chained real wins land further from target — and the loop printed no
slot information whatever for the one residual class whose arbiter IS the
slot map, so a lane wrote its own r1-displacement scanner (which, unlike
slotdiff, could not see `addi rX,r1,N` address-takes). probe now runs
slotdiff.py itself whenever `real > 0`, and prints its map under the
verdict when slotdiff reports a SAVE-SET delta, a frame-size delta, or
exclusive slots. Equal slot sets with differing use counts do NOT trigger
it on their own: that is ordinary register residue, and firing there would
bury every unrelated probe under a 60-line map.

FRESH FUZZY RUNS BEFORE ANY BANK. The four verdicts that move the BEST
anchor — BASELINE, IMPROVED, IMPROVED-STRUCTURE, REBASED — used to bank on
`real` and the opcode multiset alone. Both are computed over the
instruction stream, both read register-color cascades, and in run 35 they
AGREED on a keep whose fresh objdiff fuzzy was a 0.46 REGRESSION; the next
probe, anchored on that poisoned best, then read the run's actual best edit
as a loss (re-applied from the last commit it was +0.33). probe now spends
one report build at exactly those verdicts, before the bank, and:
  FUZZY-REGRESSED  the instruction-stream metrics improved but fresh fuzzy
            FELL below the banked anchor — best NOT updated, nothing
            banked. Revert, or arbitrate and bank deliberately with
            --rebase-best (which is exempt from the gate by construction).
A passing gate banks the measured number as the new fuzzy anchor, so the
anchor stops decaying and later CONFLICTs print their comparison for free.
--no-fuzzy-gate restores the old build-free behaviour. REGRESSED verdicts
now also carry the corollary reminder: re-run a negative from the LAST
COMMIT before recording it, because a negative measured against a bad
anchor is a fact about the anchor.

--fuzzy CACHES what it measures, keyed to the object digest it measured
it on, and when those bytes are the banked BEST state it becomes the
fuzzy ANCHOR. CONFLICT — the one verdict that orders a fuzzy arbitration
— used to print neither half of that comparison even with a number
sitting in the state file, so every arbitration cost two report builds
(one here, one after a revert). It now prints the cached anchor, and when
both halves are cached against the current bytes it prints the delta
outright, for zero builds. A cached number is used ONLY when the digest
proves it describes the bytes in front of you; banking a new best without
a fuzzy measurement CLEARS the anchor rather than carrying a stale one.

--fuzzy is otherwise a PURE READOUT: it builds, prints the scores and this
function's fresh objdiff fuzzy, and computes NO verdict and banks NO
snapshot. Re-running the verdict on bytes that were already scored is
what made a CONFLICT re-read as REGRESSED (see classify()'s BEST-anchored
multiset comparison); an arbitration readout must never be able to do
that. Score and bank with a plain probe, then arbitrate with --fuzzy.

--arbitrate is the WHOLE arbitration in one call. A real/fuzzy disagreement
needs FOUR numbers — (real, fuzzy) for the banked state and for the edited
one — and getting them by hand is probe --fuzzy, probe --revert, probe
--fuzzy, re-apply the edit, probe again: MV measured ~4 extra builds per
disagreement, and the re-apply step is where an edit gets lost. --arbitrate
builds and scores BOTH states itself, prints the pair for each with the
delta, names fuzzy as the arbiter when the two metrics disagree, and
RESTORES the working tree (in a finally, so a failed build restores too).
It banks nothing and computes no verdict: it is a measurement, and the keep
decision stays yours (--rebase-best banks an arbitrated keep). The DATA
column is reported too, since a moved non-text section is invisible to both
arbiters.
"""

import difflib
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


def stale_restore_refusal(banked_head, head, snap_bytes, committed_bytes,
                          label="snapshot"):
    """Refusal message when restoring `snap_bytes` would destroy committed
    work, or None when the restore is safe.

    Every whole-file restore in this tool overwrites the source with bytes
    banked at some earlier moment. When commits landed since that bank, those
    bytes are older than HEAD and the restore silently deletes committed
    work — a lane nearly lost a committed exact this way. defake_gate anchors
    its baselines to the commit they were taken at; this is the same anchor
    for probe's snapshots.

    Fails CLOSED on unknown provenance. `banked_head is None` means the
    snapshot carries no anchor stamp (banked by an older probe, or by a bank
    where `git rev-parse` failed) — measured on 2026-09-02, that case skipped
    the check entirely and destroyed a committed line while printing it as
    "an edit the revert could not reach". Absence of provenance is not
    evidence of freshness.

    Pure over bytes so the decision is tested without a git tree.
    `committed_bytes is None` = the file does not exist in HEAD, so there is
    nothing committed to destroy.
    """
    if committed_bytes is None:
        return None
    if snap_bytes == committed_bytes:
        # Restoring reproduces the committed bytes exactly; nothing is lost
        # however old the bank is.
        return None
    if banked_head and head and banked_head == head:
        # No commit landed since the bank. The difference is uncommitted
        # working-tree work, which is precisely what a revert is for.
        return None
    if not banked_head or not head:
        why = (f"this {label} carries NO commit anchor, so it cannot be"
               " shown to be newer than HEAD")
    else:
        why = (f"commits landed since this {label} was banked"
               f" ({banked_head[:9]} -> {head[:9]})")
    return (f"REFUSED: {why}, and the committed source differs from it —"
            " restoring would destroy committed work. Run a fresh probe on"
            " the current state to re-bank, or use git to inspect history."
            " Override with --force-stale-revert only after confirming with"
            " `git diff` that nothing committed is lost.")


def read_banked_head(meta_file):
    """The commit a snapshot sidecar was stamped with, or None."""
    try:
        return json.loads(
            meta_file.read_text(encoding="utf-8")).get("head")
    except (OSError, json.JSONDecodeError, AttributeError):
        return None


def committed_bytes_for(source):
    """HEAD's bytes for `source`, or None when it is not committed."""
    shown = subprocess.run(["git", "show", f"HEAD:{source.as_posix()}"],
                           capture_output=True)
    return shown.stdout if shown.returncode == 0 else None


def guard_stale_restore(snap, source, label):
    """Print and return a refusal when restoring `snap` over `source` would
    destroy committed work. Returns True when the caller must abort."""
    if "--force-stale-revert" in sys.argv:
        print(f"[--force-stale-revert: {label} staleness check SKIPPED —"
              " you asserted nothing committed is lost]")
        return False
    message = stale_restore_refusal(
        read_banked_head(snap.with_suffix(snap.suffix + ".meta")),
        git_head(), snap.read_bytes(), committed_bytes_for(source),
        label=label)
    if message:
        print(message)
        return True
    return False


def webfrank_pin_hashes(unit):
    """{function: [before_sha256, after_sha256]} for this unit's webfrank
    pins, or {} when the TU has none.

    A permutation/recolor pin freezes a function's body hash; when an UPSTREAM
    instruction-count change shifts a pinned window, the pin is re-derived
    (wf_rederive_pin.py) and its before/after hashes in
    config/GUNE5D/webfrank.json change. --revert restores only the SOURCE, so
    a pin re-derived since the snapshot was banked is left inconsistent with
    the reverted tree — GT had to hand-restore both. Banking these hashes lets
    --revert WARN when that happened (run 34 item 3).
    """
    cfg = Path(f"config/{VERSION}/webfrank.json")
    if not cfg.exists():
        return {}
    try:
        data = json.loads(cfg.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    out = {}
    for rule in data.get("units", {}).get(unit, []):
        fn = rule.get("function")
        if fn is None:
            continue
        if "before_sha256" in rule or "after_sha256" in rule:
            out[fn] = [rule.get("before_sha256"), rule.get("after_sha256")]
    return out


def pin_drift(banked, current):
    """Sorted function names whose banked pin hashes differ from current.

    Pure over two {fn: [before, after]} maps so the drift test is exercised
    without a webfrank.json on disk."""
    if not isinstance(banked, dict) or not isinstance(current, dict):
        return []
    return sorted(fn for fn in set(banked) | set(current)
                  if banked.get(fn) != current.get(fn))


def warn_pin_drift(unit, snap):
    """Warn if webfrank pins changed since this snapshot's .pins was banked."""
    pins_file = snap.with_suffix(snap.suffix + ".pins")
    if not pins_file.exists():
        return
    try:
        banked = json.loads(pins_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return
    drifted = pin_drift(banked, webfrank_pin_hashes(unit))
    if drifted:
        print("WARNING: webfrank pin(s) re-derived since this snapshot was"
              f" banked: {', '.join(drifted)}. --revert restored only the"
              " SOURCE; the pin's before/after_sha256 in"
              f" config/{VERSION}/webfrank.json no longer matches the reverted"
              " tree, so the pinned function's body will not hash-assert."
              " Re-derive with"
              " tools/gdl/composed_census/wf_rederive_pin.py (FULL path,"
              " body hashes must return byte-identical) or hand-restore the"
              " pin — GT had to restore both by hand.")


HEADER_SUFFIXES = (".h", ".hpp", ".hxx", ".inc")


def parse_numstat(text):
    """[(added, deleted, path)] from `git diff --numstat`.

    A binary file reports '-' for both counts; those become None rather than
    0, so "cannot count" is never rendered as "no change".
    """
    rows = []
    for line in (text or "").splitlines():
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 3:
            continue
        added, deleted, path = parts[0], parts[1], parts[-1]
        rows.append((
            None if added == "-" else int(added) if added.isdigit() else None,
            None if deleted == "-" else int(deleted) if deleted.isdigit()
            else None,
            path.replace("\\", "/"),
        ))
    return rows


def outside_edit_warning(rows, unit_source, fn):
    """Edits a function-scoped revert could NOT reach, or "".

    Run-34 criticism (MV): --revert is function-scoped, so MV's
    volatile-in-a-MACRO edit — which lived in a header, not in the reverted
    function — was invisible to the revert and stayed live for every
    subsequent probe. A revert that silently leaves half the edit in place is
    worse than no revert: the next score describes a state the worker
    believes was undone.

    Only two classes are reported, because everything else in a numstat
    belongs to other lanes or other work: the TU's OWN source (edits
    elsewhere in the file) and any HEADER anywhere in the tree (a macro,
    typedef or volatile qualifier that keeps affecting every includer).
    """
    tu_rows = [row for row in rows if unit_source and row[2] == unit_source]
    header_rows = [row for row in rows
                   if row[2].lower().endswith(HEADER_SUFFIXES)]
    if not tu_rows and not header_rows:
        return ""

    def render(row):
        added, deleted, path = row
        counts = ("binary" if added is None and deleted is None
                  else f"+{added}/-{deleted}")
        return f"    {path} ({counts} vs HEAD)"

    lines = ["REVERT IS PARTIAL — edits this revert could NOT reach remain"
             " in the working tree:"]
    if tu_rows:
        scope = f" outside {fn}" if fn else ""
        lines.append(f"  TU source{scope} still differs from HEAD:")
        lines.extend(render(row) for row in tu_rows)
    if header_rows:
        lines.append("  HEADER(S) still differ from HEAD — a function-scoped"
                     " revert can never see these, and a macro/typedef/"
                     "volatile change in one keeps affecting EVERY includer:")
        lines.extend(render(row) for row in header_rows)
    lines.append("  The score you are about to read describes the tree WITH"
                 " these still applied. Undo them with git if the revert was"
                 " meant to be total.")
    return "\n".join(lines)


def warn_outside_edits(source, fn):
    """Run the numstat cross-check and print whatever it finds."""
    diff = subprocess.run(["git", "diff", "--numstat"],
                          capture_output=True, text=True)
    if diff.returncode != 0:
        return
    warning = outside_edit_warning(
        parse_numstat(diff.stdout),
        source.as_posix() if source is not None else None, fn)
    if warning:
        print(warning)


def _wf_rederive_module():
    """The wf_rederive_pin module, or None. Fail-soft by design.

    probe must keep working in a checkout where the postprocessor stack
    cannot import; a missing transient bank is not an error, it is the
    ordinary case for every TU with no pins.
    """
    try:
        for path in (str(TOOLS), str(TOOLS / "composed_census")):
            if path not in sys.path:
                sys.path.insert(0, path)
        import wf_rederive_pin
        return wf_rederive_pin
    except Exception:
        return None


def restore_transient_pins(unit):
    """Put back any pin this TU re-derived with --transient (run 34 item 8).

    A revert restores the SOURCE; without this the re-derived hashes stay in
    webfrank.json and the pin has to be walked back by hand — GW measured ~2
    of 15 probe cycles as exactly that plumbing.
    """
    module = _wf_rederive_module()
    if module is None:
        return
    bank = module.bank_path(unit)
    if not Path(bank).exists():
        return
    config = Path(f"config/{VERSION}/webfrank.json")
    if not config.exists():
        return
    try:
        restored, notes = module.restore_transient(unit, str(config), bank)
    except Exception as error:
        print(f"[transient pin restore FAILED: {error} — webfrank.json still"
              " carries the re-derived hashes; restore the pin by hand]")
        return
    if restored:
        print(f"[transient pin(s) restored to their pre-probe hashes:"
              f" {', '.join(restored)}. Run configure.py before the next"
              " build so the WEBFRANK edge picks the restored rule up.]")
    for note in notes:
        print(f"[transient pin restore: {note}]")


def rederive_pin(unit, fn, transient=False):
    """One-call pin re-derivation: body build + wf_rederive_pin --apply +
    configure + confirm (run 34 item 9).

    An upstream edit that renumbers a TU's anonymous pool invalidates a
    downstream instruction_permutation pin's RELOCATION hashes (the body
    hashes stay byte-identical). Repairing it by hand was ~5 steps over 2
    builds: build the raw body object, run wf_rederive_pin, hand-paste two
    hashes into webfrank.json, run configure.py, rebuild. This sequences all
    of it and ABORTS at the guard wf_rederive_pin enforces — if any BODY hash
    moved the edit changed codegen, so nothing is pasted.
    """
    parts = unit.split("/")
    body = Path(f"build/{VERSION}/src/{'/'.join(parts[:-1])}"
                f"/.postprocess/body/{parts[-1]}.o")
    wf_tool = TOOLS / "composed_census" / "wf_rederive_pin.py"

    print(f"[1/4] building raw body object {body.name}")
    r = subprocess.run(["ninja", str(body)], capture_output=True, text=True)
    if r.returncode != 0:
        print("BODY BUILD FAILED:")
        print((r.stdout + r.stderr).strip()[-1200:])
        return 1

    mode = "--transient" if transient else "--apply"
    print(f"[2/4] re-deriving pin {unit}::{fn} (wf_rederive_pin {mode})")
    r = subprocess.run(
        [sys.executable, str(wf_tool), unit, fn, mode],
        capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode != 0:
        if r.stderr.strip():
            print(r.stderr.strip()[-800:])
        print("rederive-pin ABORTED — a body hash moved (the edit changed"
              " codegen, not just the pool), or the rule has no"
              " instruction_permutation. Nothing was pasted; re-derive the"
              " rule from scratch if codegen changed.")
        return 1

    print("[3/4] configure.py (materialize the WEBFRANK edge for the new hash)")
    c = subprocess.run([sys.executable, "configure.py"],
                       capture_output=True, text=True)
    if c.returncode != 0:
        print("configure.py FAILED:")
        print((c.stdout + c.stderr).strip()[-1200:])
        return 1

    print(f"[4/4] rebuilding build/{VERSION}/src/{unit}.o to confirm the"
          " WEBFRANK stage reapplies")
    b = subprocess.run(["ninja", f"build/{VERSION}/src/{unit}.o"],
                       capture_output=True, text=True)
    if b.returncode != 0:
        print("FULL OBJECT BUILD FAILED after re-derive (pin still stale?):")
        print((b.stdout + b.stderr).strip()[-1200:])
        return 1
    print(f"rederive-pin OK: {unit}::{fn} re-derived, pasted, configured, and"
          " the WEBFRANK object built clean. Run a full `ninja` before"
          " committing.")
    return 0


def head_bytes(source):
    """The committed bytes of ``source`` at HEAD, or None when unavailable."""
    shown = subprocess.run(["git", "show", f"HEAD:{source.as_posix()}"],
                           capture_output=True)
    return shown.stdout if shown.returncode == 0 else None


def bank_divergence(source_bytes, head_text):
    """Changed line count of the working source vs HEAD, or None.

    None means "cannot tell" (the file is untracked, git is unavailable) and
    must never be rendered as "clean" — a manufactured all-clear on the
    question of whether a revert point is the pre-edit state is precisely the
    defect this measures.
    """
    if head_text is None or source_bytes is None:
        return None
    if source_bytes == head_text:
        return 0
    old = split_lines(head_text.decode("latin-1"))
    new = split_lines(source_bytes.decode("latin-1"))
    matcher = difflib.SequenceMatcher(None, old, new, autojunk=False)
    return sum(max(i2 - i1, j2 - j1)
               for tag, i1, i2, j1, j2 in matcher.get_opcodes()
               if tag != "equal")


def bank_warning(kind, changed_lines, unit=None, fn=None):
    """What to say when a revert point is banked from a NON-HEAD state.

    Run-34 criticism (MV): probe banks whatever state it FIRST sees per
    function, so a BASELINE taken after an edit banks the EDITED state as the
    pre-edit reference, and a NEUTRAL probe re-banks the rolling snapshot on
    top of an edit — MV's second probe did exactly that, and --revert then
    restored the BAD state. Both are documented behaviours nothing measured
    or announced. This announces them with the actual divergence.

    BASELINE gets the loud form (its whole meaning is "the state before your
    edits"). NEUTRAL gets the shorter note (banking is deliberate there; what
    was missing is that the banked point is not HEAD). Every other verdict
    banks an edited state ON PURPOSE and is not warned about.
    """
    where = f" {unit} {fn}" if unit and fn else " <unit> <fn>"
    if changed_lines is None:
        if kind != "BASELINE":
            return ""
        return ("WARNING: cannot compare this source against HEAD (untracked"
                " file, or git unavailable), so whether this BASELINE is the"
                " pre-edit state is UNMEASURED. --revert-baseline will"
                " restore whatever was on disk just now.")
    if changed_lines == 0:
        return ""
    if kind == "BASELINE":
        return (
            f"WARNING: BASELINE BANKED FROM AN EDITED TREE — this unit's"
            f" source differs from HEAD by {changed_lines} line(s), so the"
            " 'baseline' revert point is NOT the pre-edit state:"
            " --revert-baseline will restore YOUR EDITS. probe banks whatever"
            " state it FIRST sees per function (the FIRST-BASELINE TRAP), and"
            " that bank is never overwritten. If you meant to baseline the"
            f" clean state, run `probe.py{where} --discard` (restores the TU"
            f" to HEAD), then `probe.py{where} --reset`, then probe again"
            " BEFORE your first edit.")
    if kind == "NEUTRAL":
        return (
            f"NOTE: the revert point just banked is NOT HEAD — it differs by"
            f" {changed_lines} line(s). NEUTRAL probes bank too, so --revert"
            " restores THIS edited state, not a clean tree. To discard the"
            f" edit use git or `probe.py{where} --discard`.")
    return ""


def baseline_bank_decision(kind, base_exists, rebaseline=False):
    """(action, message) for the SESSION BASELINE file on one bank.

    action is "create", "keep" or "overwrite".

    Run-35 criticism (CL): the session baseline was written only when the
    VERDICT was BASELINE. A worker whose first probe on a function landed
    NEUTRAL — the normal case when a state file survives from an earlier
    session, or when the first edit folds away — therefore never got one at
    all, and the rolling snapshot (which NEUTRAL probes deliberately move
    onto the edit) was the only revert point in existence. The pre-edit
    state was then unreachable except through git, and T5's warning about
    it was printed and read past.

    So: the FIRST bank of a session creates the baseline whatever verdict
    caused it, and once it exists nothing overwrites it silently. Refusing
    the ROLLING bank instead would re-introduce the older regression this
    tool already fixed — a gated NEUTRAL de-fakematch state is as good as
    best, and not banking it made --revert throw the work away twice.
    """
    if base_exists and rebaseline:
        return "overwrite", (
            "[--rebaseline: the SESSION BASELINE has been OVERWRITTEN with"
            " the current state. --revert-baseline can no longer reach the"
            " state it held before this call.]")
    if base_exists:
        return "keep", (
            "" if kind != "BASELINE" else
            "[session baseline already banked for this unit and is NOT"
            " overwritten — a BASELINE verdict here re-banked only the"
            " ROLLING revert point. --revert-baseline still reaches the"
            " ORIGINAL baseline; --rebaseline moves it here deliberately.]")
    return "create", (
        "[session baseline banked: probe.py --revert-baseline restores THIS"
        " state]" if kind == "BASELINE" else
        f"[session baseline banked from a {kind} verdict — this is the"
        " FIRST bank on this unit, and the rolling revert point moves with"
        " later NEUTRAL probes while this one does not. --revert-baseline"
        " restores THIS state.]")


def bank_snapshot(unit, source, baseline=False, verdict_kind=None, fn=None,
                  rebaseline=False):
    snap = snapshot_path(unit, source)
    shutil.copyfile(source, snap)
    head = git_head()
    if head:
        snap.with_suffix(snap.suffix + ".meta").write_text(
            json.dumps({"head": head}), encoding="utf-8")
    # Bank this TU's webfrank pin hashes alongside the source so --revert can
    # warn when a pin was re-derived since (run 34 item 3). Written even when
    # empty, so a later drift compare is well-defined rather than reading a
    # stale sidecar from an earlier bank.
    snap.with_suffix(snap.suffix + ".pins").write_text(
        json.dumps(webfrank_pin_hashes(unit), sort_keys=True),
        encoding="utf-8")
    # The FIRST bank of a session is ALSO written to a separate baseline
    # file which nothing overwrites silently: NEUTRAL probes re-bank the
    # rolling snapshot, so --revert restores the last neutral edit rather
    # than the pristine state (five workers hit this). --revert-baseline
    # reaches past that — but only if a baseline exists, which before run
    # 36 required the first verdict to BE a BASELINE.
    base = snap.with_suffix(snap.suffix + ".base")
    kind = verdict_kind or ("BASELINE" if baseline else "BANK")
    action, note = baseline_bank_decision(kind, base.exists(), rebaseline)
    if action in ("create", "overwrite"):
        shutil.copyfile(source, base)
        base.with_suffix(base.suffix + ".pins").write_text(
            json.dumps(webfrank_pin_hashes(unit), sort_keys=True),
            encoding="utf-8")
        # Anchor the baseline to its commit exactly like the rolling
        # snapshot. The .base file is the OLDEST state a session can
        # restore and is whole-file by construction, so it is the most
        # likely of the two to predate HEAD — yet it shipped with no
        # stamp at all, and --revert-baseline destroyed a committed line
        # with it (measured 2026-09-02).
        if head:
            base.with_suffix(base.suffix + ".meta").write_text(
                json.dumps({"head": head}), encoding="utf-8")
    if note:
        print(note)
    # Say WHAT was banked (run 34 item 2). A BASELINE taken over an edited
    # tree is not a baseline, and a NEUTRAL bank moves the revert point onto
    # an edit — both silent until now. (`kind` is the one computed above;
    # its "BANK" fallback matches neither arm, exactly as the old None did.)
    if kind in ("BASELINE", "NEUTRAL"):
        warning = bank_warning(
            kind, bank_divergence(source.read_bytes(), head_bytes(source)),
            unit=unit, fn=fn)
        if warning:
            print(warning)


def strip_noncode(text):
    """Blank out string/char literals and comments, preserving line count.

    Brace matching has to ignore a `}` inside "…" or /* … */; keeping the
    line count intact lets the caller index the ORIGINAL lines by the same
    indices.
    """
    out = []
    i, n = 0, len(text)
    state = None  # None | 'line' | 'block' | '"' | "'"
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state is None:
            if ch == "/" and nxt == "/":
                state, i = "line", i + 2
                out.append("  ")
                continue
            if ch == "/" and nxt == "*":
                state, i = "block", i + 2
                out.append("  ")
                continue
            if ch in "\"'":
                state, i = ch, i + 1
                out.append(" ")
                continue
            out.append(ch)
            i += 1
            continue
        if state == "line":
            if ch == "\n":
                state = None
                out.append(ch)
            else:
                out.append(" ")
            i += 1
            continue
        if state == "block":
            if ch == "*" and nxt == "/":
                state, i = None, i + 2
                out.append("  ")
                continue
            out.append(ch if ch == "\n" else " ")
            i += 1
            continue
        # inside a string or char literal
        if ch == "\\":
            out.append("  ")
            i += 2
            continue
        if ch == state:
            state = None
        out.append(ch if ch == "\n" else " ")
        i += 1
    return "".join(out)


def split_lines(text, keepends=False):
    """Split on '\\n' ONLY, preserving CR as line content.

    str.splitlines() also breaks on \\x0b, \\x0c and U+0085, which a
    latin-1 byte round-trip can manufacture out of ordinary source bytes;
    splitting there would desynchronise line indices from the file.
    """
    parts = text.split("\n")
    lines = [p + "\n" for p in parts[:-1]]
    if parts[-1]:
        lines.append(parts[-1])
    return lines if keepends else [ln.rstrip("\n") for ln in lines]


def function_span(text, fn):
    """[start, end) line indices of ``fn``'s definition, or None.

    A definition is a line starting in column 0 that names the function
    immediately before a '(' — this covers `void foo(void)` and the
    split-return-type form where `foo(int a)` sits alone on the line. The
    body is brace-matched over comment/literal-stripped text.

    Name spellings are resolved the way fndiff resolves them: an object
    symbol `SfxSkipItem` is routinely spelled `SfxSkipItem_80096FF4` in
    the source (and vice versa). Measured over 507 functions in ten TUs,
    accepting both spellings took span resolution from 96.8% to 100%.
    """
    lines = split_lines(strip_noncode(text))
    suffix = r"_80[0-9A-Fa-f]{6}"
    base = re.sub(rf"{suffix}$", "", fn)
    candidates = [re.escape(fn)]
    if base != fn:
        candidates.append(re.escape(base))
    else:
        candidates.append(re.escape(fn) + suffix)
    for candidate in candidates:
        span = _span_for(lines, re.compile(rf"(^|[^\w]){candidate}\s*\("))
        if span is not None:
            return span
    return None


def _span_for(lines, pattern):
    for i, line in enumerate(lines):
        if not line or line[0].isspace():
            continue
        if not pattern.search(line):
            continue
        # Walk forward to the body's opening brace; a prototype or a call
        # terminates at ';' before any '{' and is not a definition.
        depth, start_body = 0, None
        j = i
        while j < len(lines) and start_body is None:
            for ch in lines[j]:
                if ch == "{":
                    start_body = j
                    depth = 1
                    break
                if ch == ";":
                    break
            else:
                j += 1
                continue
            if start_body is None:
                break
            j += 1
        if start_body is None:
            continue
        # Brace-match from just past the opening '{'.
        k = start_body
        offset = lines[k].index("{") + 1
        while k < len(lines):
            for ch in lines[k][offset:]:
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                    if depth == 0:
                        return i, k + 1
            offset = 0
            k += 1
        return None
    return None


def _inside(lo, hi, a, b):
    """Is the [a, b) line range strictly inside the [lo, hi) span?"""
    if a == b:                      # an insertion point
        return lo < a < hi
    return lo <= a and b <= hi


def _outside(lo, hi, a, b):
    if a == b:
        return a <= lo or a >= hi
    return b <= lo or a >= hi


def scoped_revert(snap_text, cur_text, fn):
    """Restore only ``fn``'s hunks from ``snap_text``.

    Returns (new_text, notes). Raises ValueError — loudly, never silently
    widening to the whole file — when the function cannot be located on
    either side or when a hunk straddles its boundary.
    """
    snap_lines = split_lines(snap_text, keepends=True)
    cur_lines = split_lines(cur_text, keepends=True)
    snap_span = function_span(snap_text, fn)
    cur_span = function_span(cur_text, fn)
    if cur_span is None:
        raise ValueError(
            f"cannot locate {fn}'s definition in the working source —"
            " function-scoped revert refuses to guess")
    if snap_span is None:
        raise ValueError(
            f"cannot locate {fn}'s definition in the banked snapshot —"
            " function-scoped revert refuses to guess")
    matcher = difflib.SequenceMatcher(None, snap_lines, cur_lines,
                                      autojunk=False)
    out, reverted, kept, entangled = [], 0, 0, []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            out.extend(cur_lines[j1:j2])
            continue
        in_cur = _inside(cur_span[0], cur_span[1], j1, j2)
        in_snap = _inside(snap_span[0], snap_span[1], i1, i2)
        out_cur = _outside(cur_span[0], cur_span[1], j1, j2)
        out_snap = _outside(snap_span[0], snap_span[1], i1, i2)
        if in_cur and in_snap:
            out.extend(snap_lines[i1:i2])
            reverted += 1
        elif out_cur and out_snap:
            out.extend(cur_lines[j1:j2])
            kept += 1
        else:
            entangled.append((j1 + 1, j2 + 1))
            out.extend(cur_lines[j1:j2])
    if entangled:
        spans = ", ".join(f"L{a}-L{b}" for a, b in entangled)
        raise ValueError(
            f"{len(entangled)} hunk(s) straddle {fn}'s boundary ({spans});"
            " a function-scoped revert cannot separate them. Inspect with"
            " `git diff`, or re-run with --whole-file to take the old"
            " all-or-nothing restore deliberately")
    notes = (f"{reverted} hunk(s) inside {fn} reverted;"
             f" {kept} hunk(s) elsewhere in the TU left untouched")
    return "".join(out), notes


def count_distance(text):
    """|target - ours| from a "T<n>/O<n>" insns string, or None."""
    match = re.match(r"T(\d+)/O(\d+)$", text or "")
    return abs(int(match.group(1)) - int(match.group(2))) if match else None


def object_digest(unit, fn, fn_stripped):
    """Raw-byte signature of the built function, or None if unavailable."""
    try:
        sys.path.insert(0, str(TOOLS))
        import fndiff as _fndiff
        objfile = Path(f"build/{VERSION}/src/{unit}.o")
        signature = _fndiff.raw_signature(objfile)
        return signature.get(fn) or signature.get(fn_stripped)
    except Exception:
        return None


SECTION_HEAD_RE = re.compile(r"^Contents of section (\S+):$")


def parse_section_digests(dump):
    """{section: sha1} over an `objdump -s` dump, .text* excluded.

    Pure text -> dict so the classification below is testable without an
    object file or a toolchain.
    """
    import hashlib
    out, cur, hasher = {}, None, None
    for line in dump.splitlines():
        head = SECTION_HEAD_RE.match(line.strip())
        if head:
            if cur is not None:
                out[cur] = hasher.hexdigest()[:12]
            cur = head.group(1)
            hasher = hashlib.sha1()
            continue
        if cur is not None:
            hasher.update(line.encode())
    if cur is not None:
        out[cur] = hasher.hexdigest()[:12]
    return {name: h for name, h in out.items()
            if not name.startswith(".text")}


def data_digest(unit):
    """Per-section digest of the object's NON-TEXT sections, or None.

    object_digest() above hashes instruction words and relocation lines,
    which is structurally blind to a pooled VALUE: MWCC materializes an
    FP literal (or a string, or a table) into .sdata2/.rodata and the
    function merely loads it, so correcting a wrong constant changes no
    instruction word, no displacement and no relocation target
    (claim.law.SL_pool-constant-errors-are-score-invisible). Hashing the
    data sections is the ONLY thing in this loop that can tell "the edit
    folded away before codegen" apart from "the edit landed exactly where
    no score in this project looks".
    """
    try:
        sys.path.insert(0, str(TOOLS))
        import fndiff as _fndiff
        objfile = Path(f"build/{VERSION}/src/{unit}.o")
        dump = subprocess.run(
            [str(_fndiff.OBJDUMP), "-s", str(objfile)],
            capture_output=True, text=True)
        if dump.returncode != 0:
            return None
        return parse_section_digests(dump.stdout)
    except Exception:
        return None


def moved_sections(prev, cur):
    """Non-text section names that differ between two data digests.

    None (either side unmeasured) means "cannot tell" and is reported as
    an empty list — the data-only verdict must never be manufactured out
    of a missing measurement.
    """
    if not isinstance(prev, dict) or not isinstance(cur, dict):
        return []
    return sorted(name for name in set(prev) | set(cur)
                  if prev.get(name) != cur.get(name))


# dtk/MWCC name the EABI exception tables WITHOUT a leading dot in these
# objects (`extab`/`extabindex`, per `objdump -s`); dotted spellings kept
# for portability across other toolchains.
EH_SECTIONS = ("extab", "extabindex", ".extab", ".extabindex",
               ".eh_frame", ".gcc_except_table")


def data_line(prev_data, data, source_changed=True):
    """The DATA column, printed alongside EVERY verdict.

    probe already measured the object's non-text sections on every probe
    and banked them in `last_data`, but only ever CONSULTED them inside
    annotate_neutral — which main() calls only when the verdict starts
    with NEUTRAL. So the one shape the measurement exists to catch, an
    edit whose .text got BETTER while a data section moved, printed
    nothing at all: a source change that widens a function's callee-saved
    save area silently DESTROYS its TU's extab match while real, fuzzy,
    the opcode multiset and the instruction count all IMPROVE — measured
    on writeGauntletSave at -208 bytes of Data with a win on every .text
    arbiter (claim.law.WS_frame-widening-silently-breaks-the-tus-extab-
    match).

    NEUTRAL-DATA-ONLY owns its own annotation, so main() suppresses this
    line there; this is the alarm for the TEXT-VISIBLE verdicts
    (IMPROVED / REGRESSED / CONFLICT) the neutral path never reaches.
    Returns "" — never a manufactured line — when either side is
    unmeasured or the source did not change, exactly like moved_sections.
    """
    moved = moved_sections(prev_data, data) if source_changed else []
    if not moved:
        return ""
    eh = [name for name in moved if name in EH_SECTIONS]
    line = (f"DATA      non-text section(s) {', '.join(moved)} MOVED — the"
            " verdict above scores the INSTRUCTION STREAM ONLY. real,"
            " --ops, regnorm, the multiset and fuzzy are all computed over"
            " .text and are structurally blind to these bytes, so a keep"
            " that improves every text arbiter can still DESTROY a matched"
            " data section (measured: a frame-widening keep took a"
            " 208-byte .extab match with it while every text arbiter"
            " approved). This is NOT a revert order — it is the half of"
            " the result nothing else here scores. ARBITRATE with"
            " `python tools/gdl/datadiff.py <unit> --sections` before"
            " keeping or reverting")
    if eh:
        line += (f"\n          EXCEPTION-TABLE section(s) {', '.join(eh)}"
                 " moved: these carry no instructions, so every score in"
                 " this loop reads them as absent. A changed save-area"
                 " register (the stmw/lmw IMMEDIATE row) is the usual"
                 " cause — check the table's SIZE against target, and the"
                 " whole-image `ninja` PROGRESS 'Data:' figure, before"
                 " keeping the frame change")
    return line


def fuzzy_anchor_note(best_fuzzy, cur_fuzzy):
    """What the CONFLICT verdict can say about fuzzy without a build.

    CONFLICT is the one verdict that ORDERS a fuzzy arbitration, and it
    used to ship without either half of the comparison — so each
    arbitration cost two report builds (one here, one after a revert) even
    when probe had already measured one of them. Three lanes paid that.
    """
    if best_fuzzy is None:
        return ("\n[no cached fuzzy anchor for the best state — run"
                " `--fuzzy` HERE, then `--revert` and `--fuzzy` again for"
                " the other half. Running `--fuzzy` at each banked best"
                " keeps the anchor warm and makes the next arbitration"
                " cost ONE build]")
    if cur_fuzzy is None:
        return (f"\n[BEST-STATE FUZZY (cached, no build): {best_fuzzy:.4f}"
                " — one `--fuzzy` on THIS state now completes the"
                " comparison; no revert-and-rebuild needed for the other"
                " half]")
    delta = cur_fuzzy - best_fuzzy
    trend = "ROSE" if delta > 0 else ("FELL" if delta < 0 else "is FLAT")
    return (f"\n[FUZZY {best_fuzzy:.4f} -> {cur_fuzzy:.4f} ({delta:+.4f})"
            f" {trend} — both halves cached against these exact bytes, NO"
            " build spent. This is the arbiter: keep with --rebase-best if"
            " it rose, revert if it fell]")


CONFLICT_UNARBITRATED_EXIT = 3


def conflict_gate(verdict, best_fuzzy, cur_fuzzy):
    """(verdict, exit_code) — CONFLICT refuses to classify without fuzzy.

    Run-34 criticism (PC): CONFLICT is the one verdict whose whole meaning is
    "the two arbiters disagree, go measure fuzzy" — and PC skipped the
    mandated arbiter and recorded a FALSE REGRESSION from the headline alone.
    Advice in the verdict text was not enough, because the verdict text still
    LOOKED like a classification.

    So an unarbitrated CONFLICT is no longer a classification at all: the
    headline says UNARBITRATED, the text refuses an outcome, and the process
    exits CONFLICT_UNARBITRATED_EXIT (3) so a script or a worker reading only
    the exit status cannot treat it as a completed probe. It becomes a
    classification the moment both fuzzy halves exist for these exact bytes —
    which --arbitrate, or --fuzzy on each state, produces.

    Nothing is reverted and nothing is banked either way: this gates the
    RECORDING of an outcome, not the work.
    """
    if not verdict.startswith("CONFLICT"):
        return verdict, 0
    if best_fuzzy is not None and cur_fuzzy is not None:
        return verdict + (
            "\n[ARBITRATED: both fuzzy halves are cached against these exact"
            " bytes, so the delta above IS the arbiter and an outcome may be"
            " recorded from it]"), 0
    have = ("the BEST state" if best_fuzzy is not None else
            "this state" if cur_fuzzy is not None else "NEITHER state")
    return (
        verdict.replace("CONFLICT ", "CONFLICT-UNARBITRATED ", 1)
        + "\nOUTCOME REFUSED: this probe classifies NOTHING. A CONFLICT means"
          " `real` and the opcode multiset point opposite ways, and the"
          " project's arbiter for that is objdiff fuzzy from a fresh report"
          f" — which exists for {have} right now. Do NOT record IMPROVED,"
          " REGRESSED, a park, or a law from this line: run"
          " `probe.py <unit> <fn> --arbitrate` (both halves, one call) or"
          " `--fuzzy` on each state, then re-probe. Run-34's PC lane skipped"
          " this arbiter and recorded a regression that was not one."
          f"\n[exit {CONFLICT_UNARBITRATED_EXIT}: unarbitrated CONFLICT —"
          " not a build failure, not a scoring failure]"
    ), CONFLICT_UNARBITRATED_EXIT


SLOT_FRAME_RE = re.compile(r"^frame: target (\S+)\s+ours (\S+)", re.M)
SLOT_VERDICT_RE = re.compile(r"^== .*?-> (.+?)$", re.M)


def slot_arbiter_signal(output):
    """(fires, reason) — does this residual class as frame-slot work?

    Read out of slotdiff.py's OWN output rather than re-derived here. Run-35
    criticism (CL): probe prints no slot information at all for the one
    residual class whose arbiter IS the slot map, so a lane hand-wrote an
    r1-displacement enumerator to get it. A second implementation of "which
    slots differ" is the wrong fix twice over — it is work already done, and
    the hand-rolled one saw displacements but not the `addi rX,r1,N`
    address-takes that slotdiff was specifically taught to collect (48 bytes
    of address-taken arrays hid there once).

    The three signals, in decreasing decisiveness:
      * a SAVE-SET delta — a callee-saved allocation difference, which is
        not a local slot at all and is the single most decisive fact for
        slot work;
      * a frame-size delta;
      * exclusive slots on either side.
    "SLOTS ALIGNED, N use-count deltas" alone does NOT fire: equal slot sets
    with different use counts is ordinary register/schedule residue, and
    firing on it would put a 60-line map under every unrelated probe. It is
    reported only alongside one of the decisive signals above.
    """
    if not output or "== " not in output:
        return False, ""
    reasons = []
    if "SAVE-SET DELTA" in output:
        reasons.append("a callee-saved SAVE-SET delta (an unallocated"
                       " callee-saved register, NOT a local slot)")
    frame = SLOT_FRAME_RE.search(output)
    if frame and frame.group(1) != frame.group(2):
        reasons.append(f"frame size target {frame.group(1)} vs ours"
                       f" {frame.group(2)}")
    verdict = SLOT_VERDICT_RE.search(output)
    text = verdict.group(1).strip() if verdict else ""
    if text.startswith("SLOTS DIFFER"):
        reasons.append(text.lower())
    elif text.startswith("SLOTS ALIGNED") and reasons:
        reasons.append(text.lower())
    if not reasons:
        return False, ""
    return True, "; ".join(reasons)


def slot_arbiter_header(reason):
    """The line that says why a slot map appeared under this verdict."""
    return (
        f"SLOT-CLASS RESIDUAL — {reason}. The verdict above is scored on"
        " `real`, which ACTIVELY FIGHTS slot work: a design one 4-byte step"
        " from a slot-exact map can read REGRESSED while four chained real"
        " wins land further from target (two lanes measured that"
        " independently). ARBITRATE ON THE MAP BELOW, not on the verdict"
        " (claim.law.real-can-underweight-a-large-alignment-gain)."
        " `--no-slots` suppresses this; `--slots` forces it.")


def run_slot_arbiter(unit, fn):
    """slotdiff.py's stdout, or None when it could not run."""
    try:
        done = subprocess.run(
            [sys.executable, str(TOOLS / "slotdiff.py"), unit, fn],
            capture_output=True, text=True)
    except OSError:
        return None
    return done.stdout


def format_genuine_note(n, rows, cap=8):
    """The regnorm GENUINE structural-row note for CONFLICT/NEUTRAL-WORSE.

    probe's CONFLICT and NEUTRAL-WORSE verdicts are set by the opcode-multiset
    TOKEN COUNT, which is UNSOUND under cancelling pairs: closing a genuine
    structural row can RAISE the token count (a +addi/-li pair reads as
    growth) even as the stream moves nearer target. regnorm's GENUINE column —
    structural rows with no artifact explanation — is the sound structure
    signal, and regnorm.analyze runs in-process (run 34 item 2).
    """
    lines = [f"regnorm GENUINE structural rows: {n} — the SOUND structure"
             " signal here. This verdict was set by the opcode-multiset TOKEN"
             " COUNT, which is unsound under cancelling pairs (closing a"
             " genuine row can RAISE it). Trust the genuine count for whether"
             " the stream moved nearer target."]
    for row in rows[:cap]:
        lines.append(f"    {row}")
    if len(rows) > cap:
        lines.append(f"    ... {len(rows) - cap} more genuine row(s)")
    return "\n".join(lines)


def genuine_row_count(unit, fn):
    """(genuine_count, [row_reprs]) via regnorm.analyze, or None.

    Only ever called on a CONFLICT/NEUTRAL-WORSE verdict, where the sounder
    structure signal is worth two objdump reads (no ninja build).
    """
    try:
        sys.path.insert(0, str(TOOLS))
        import regnorm
        bare = re.sub(r"\.(c|cpp)$", "", unit)
        target, ours, resolver = regnorm.load_tables(bare)
        fn_t = regnorm.resolve_name(target, fn)
        fn_o = regnorm.resolve_name(ours, fn)
        if fn_t is None or fn_o is None:
            return None
        result = regnorm.analyze(target[fn_t], ours[fn_o], resolver)
        rows = []
        for r in result.genuine:
            try:
                where = f"@0x{r.offset:x}"
            except (TypeError, ValueError):
                where = f"@{r.offset}"
            rows.append(f"{r.kind} {where}: T {r.target!r}  O {r.ours!r}")
        return len(result.genuine), rows
    except Exception:
        return None


BEST_KEYS = ("best_real", "best_multiset", "best_insns", "best_bytes",
             "best_fuzzy")

# Fuzzy is a float percentage; anything at or above the anchor is "not a
# regression". The epsilon keeps float noise from manufacturing a refusal.
FUZZY_GATE_EPS = 1e-9


def banks_best(verdict):
    """True when this verdict text is one of the four that move the BEST
    anchor: BASELINE, IMPROVED, IMPROVED-STRUCTURE, REBASED.

    `IMPROVED?` (the blown-out-count-distance headline) deliberately does
    NOT bank, so it must not be matched by a bare startswith("IMPROVED").
    """
    return verdict.startswith(("BASELINE", "REBASED", "IMPROVED-STRUCTURE",
                               "IMPROVED "))


def apply_fuzzy_bank_gate(verdict, state, prior_best, prior_best_fuzzy,
                          fuzzy):
    """Refuse to bank a new BEST whose fresh fuzzy fell below the anchor.

    Run-35 criticism (claim.law.PC_real-and-multiset-agreement-does-not-
    license-a-keep): probe banked an IMPROVED whose fresh fuzzy was a 0.46
    REGRESSION, and the next probe — measured against that poisoned anchor
    — read the run's best edit as a loss. real and the opcode multiset
    AGREEING is not a licence to keep: both are computed over the
    instruction stream and both read register-color cascades in ways fuzzy
    does not. The project's own metric-disagreement rule already names
    fuzzy from a fresh report as the arbiter; this makes the loop obey it
    instead of advising it.

    Pure: `prior_best` is the snapshot of the BEST_KEYS taken before any
    branch called bank_best(), and restoring it is exactly the un-bank.
    REBASED is exempt — it IS the deliberate arbitrated keep.
    """
    if not banks_best(verdict) or verdict.startswith("REBASED"):
        return verdict, state
    if fuzzy is None:
        if prior_best_fuzzy is None:
            # No anchor existed and none could be measured: there is
            # nothing to gate against, and saying so on every probe would
            # be noise.
            return verdict, state
        return verdict + (
            "\nFUZZY GATE UNMEASURED: a fuzzy anchor"
            f" ({prior_best_fuzzy:.4f}%) was banked for the previous best,"
            " but this state's fresh fuzzy could not be measured (report"
            " build failed, or --no-fuzzy-gate). The new best is banked"
            " WITHOUT the check that would have caught a fuzzy regression"
            " hiding under a real+multiset win, and the anchor is CLEARED"
            " rather than carried stale. Run --arbitrate before recording"
            " this as progress."), state
    if prior_best_fuzzy is None or fuzzy >= prior_best_fuzzy - FUZZY_GATE_EPS:
        return verdict, state
    state = dict(state)
    for key, value in prior_best.items():
        if value is None:
            state.pop(key, None)
        else:
            state[key] = value
    head = verdict.split("\n", 1)[0]
    gated = (
        f"FUZZY-REGRESSED  fresh objdiff fuzzy {prior_best_fuzzy:.4f}% ->"
        f" {fuzzy:.4f}% ({fuzzy - prior_best_fuzzy:+.4f}) — best NOT updated"
        " and NOTHING banked, even though the instruction-stream metrics"
        " improved. real and the multiset are both computed over .text and"
        " both read register-color cascades; fuzzy from a fresh report is"
        " the arbiter when they disagree. REVERT, or arbitrate and bank the"
        " keep deliberately with --rebase-best."
        f"\n[instruction-stream verdict, SUPERSEDED by the gate: {head}]")
    return gated, state


def classify(state, real, insns, multiset_tokens, rebase_best=False,
             digest=None, source_changed=True, fuzzy=None):
    """Pure verdict function: (verdict_text, new_state).

    ``state`` is the banked gate state; the returned state carries the
    updated best/last fields. No I/O, no globals — every branch below is
    covered by tools/gdl/tests/test_probe.py.

    THE BEST ANCHOR. Both scores compared against the high-water mark are
    read from the BEST-scoring state: `real` from ``best_real`` and the
    opcode-multiset token count from ``best_multiset``. Carrying the
    multiset delta against the PREVIOUS probe instead made the verdict a
    function of probe history rather than of the bytes: after a CONFLICT
    banked its own multiset into ``last_multiset``, re-scoring the very
    same object flipped the verdict to REGRESSED with "[revert advised]"
    (measured on game/sys/memcard get_vmu_directory, real 65 -> 65, run
    29). ``best_multiset`` is absent from states banked before that fix;
    the fallback is stated in the verdict text rather than hidden.
    """
    state = dict(state)
    best = state.get("best_real")
    best_tokens = state.get("best_multiset")
    prev_tokens = state.get("last_multiset")
    tok = (f", multiset {multiset_tokens}t"
           if multiset_tokens is not None else "")

    best_fuzzy = state.get("best_fuzzy")
    # Everything the fuzzy bank gate has to be able to put BACK when it
    # refuses a keep. Captured before any branch can call bank_best().
    prior_best = {key: state.get(key) for key in BEST_KEYS}

    def bank_best():
        state["best_real"] = real
        state["best_multiset"] = multiset_tokens
        state["best_insns"] = insns
        if digest is not None:
            # Which BYTES the best state is, so a later fuzzy readout can
            # prove it is measuring the anchor rather than some other probe.
            state["best_bytes"] = digest
        if fuzzy is not None:
            state["best_fuzzy"] = fuzzy
        else:
            # A stale anchor is worse than none: it would silently compare
            # a fresh number against a number for DIFFERENT bytes, which is
            # the failure this project already has a discipline rule about.
            state.pop("best_fuzzy", None)

    # RE-SCORE GUARD. A probe that measures exactly what the previous
    # probe measured has observed no change and must not manufacture a
    # new verdict — re-running verdict logic over unchanged bytes is the
    # defect this guard closes. Re-emit the standing verdict verbatim.
    # The guard is anchored on the OBJECT DIGEST, not on the score triple
    # alone: scores can read identical while the bytes moved (that is the
    # NEUTRAL-REARRANGED case), and a triple-only guard would hide it.
    # With no digest available the guard does not fire at all. It also
    # requires the SOURCE to be unchanged against the banked snapshot,
    # which separates "you probed again without editing" from "you edited
    # and the edit folded away before codegen" — the latter is a real
    # signal (NEUTRAL-IDENTICAL) and must not be swallowed.
    unchanged = (digest is not None
                 and not source_changed
                 and state.get("last_bytes") == digest
                 and state.get("last_real") == real
                 and state.get("last_insns") == insns
                 and state.get("last_multiset") == multiset_tokens
                 and state.get("last_verdict") is not None
                 and not rebase_best)
    if unchanged:
        verdict = (f"RE-SCORE  real {real} (insns {insns}{tok}) — nothing"
                   " moved since the last probe; the standing verdict"
                   f" below is REPEATED, not recomputed:\n"
                   f"{state['last_verdict']}")
        return verdict, state

    if rebase_best:
        # After fuzzy/--ops arbitration keeps a real-regressed state, the old
        # banked best is dead and every later probe misreports REGRESSED.
        # Accept the current state as the new best and revert point.
        verdict = (f"REBASED   best {best} -> {real} (insns {insns}{tok})"
                   f"  [arbitrated keep]")
        bank_best()
    elif best is None:
        verdict = f"BASELINE  real {real} (insns {insns}{tok})"
        bank_best()
    elif real < best:
        # A real win with a BLOWN-OUT count distance once banked as best
        # (949->802 while the function LOST 157 instructions), poisoning
        # every later verdict until --rebase-best. Refuse to bank those.
        prev_dist = count_distance(state.get("last_insns"))
        cur_dist = count_distance(insns)
        anchor_tokens = best_tokens if best_tokens is not None else prev_tokens
        anchor_name = "best" if best_tokens is not None else "prev"
        structure_diverged = (multiset_tokens is not None
                              and anchor_tokens is not None
                              and multiset_tokens > anchor_tokens)
        if (prev_dist is not None and cur_dist is not None
                and cur_dist > prev_dist + 4):
            verdict = (f"IMPROVED? real {best} -> {real} BUT count"
                       f" distance {prev_dist} -> {cur_dist} blew out — best"
                       " NOT updated; this is structural divergence wearing"
                       " a real win. Arbitrate on fresh fuzzy;"
                       " --rebase-best banks a deliberate keep")
        elif structure_diverged:
            # STRUCTURE OUTRANKS REAL (the mirror of the CONFLICT below).
            # `real` is a linear diff; the opcode multiset is what says
            # whether the stream is the right SHAPE. A real win whose
            # multiset grew is a shape that moved AWAY from target, and
            # the headline used to read plain "IMPROVED [best updated]" —
            # indistinguishable from a probe that improved both, and it
            # banked the diverged state as the revert point.
            verdict = (f"CONFLICT  real {best} -> {real} IMPROVED but"
                       f" multiset {anchor_tokens}t -> {multiset_tokens}t vs"
                       f" {anchor_name} DIVERGED — structure moved AWAY from"
                       " target while the linear diff shrank; best NOT"
                       " updated, do NOT auto-bank. Read the --ops diff and"
                       " arbitrate on fresh fuzzy (--rebase-best banks a"
                       " deliberate keep)")
            verdict += fuzzy_anchor_note(best_fuzzy, fuzzy)
            if best_tokens is None:
                verdict += ("\n[no best_multiset banked (pre-run-29 state) —"
                            " the structure comparison fell back to the"
                            " PREVIOUS probe; --reset then re-probe for a"
                            " BEST-anchored verdict]")
        else:
            verdict = (f"IMPROVED  real {best} -> {real} (insns"
                       f" {insns}{tok})  [best updated]")
            bank_best()
        # Parity-held improvements are the one IMPROVED shape that has
        # regressed fuzzy end-to-end (real 30->24 at unchanged T47/O47
        # was a fuzzy 80.85->71.89 loss; probe+gate both passed it).
        # When insn counts already agreed and did not move, real fell
        # inside an already-parity-exact shell — fuzzy is the arbiter.
        parity = re.match(r"T(\d+)/O(\d+)$", insns or "")
        if (verdict.startswith("IMPROVED")
                and parity and parity.group(1) == parity.group(2)
                and state.get("last_insns") == insns):
            verdict += ("\nPARITY-HELD IMPROVEMENT: counts were already"
                        " equal and unchanged — arbitrate on FRESH objdiff"
                        " fuzzy BEFORE treating this as progress or running"
                        " --update-improved; revert if fuzzy fell")
    elif real > best:
        anchor_tokens = best_tokens if best_tokens is not None else prev_tokens
        anchor_name = "best" if best_tokens is not None else "prev"
        structure_improved = (multiset_tokens is not None
                              and anchor_tokens is not None
                              and multiset_tokens < anchor_tokens)
        if structure_improved:
            verdict = (f"CONFLICT  real {state.get('last_real', best)} ->"
                       f" {real} (best {best}, insns {insns}) but multiset"
                       f" {anchor_tokens}t -> {multiset_tokens}t vs"
                       f" {anchor_name} IMPROVED — structure is converging;"
                       " read the diff and arbitrate, do NOT auto-revert"
                       " (--rebase-best banks an arbitrated keep)")
            verdict += fuzzy_anchor_note(best_fuzzy, fuzzy)
            # Count distance is the one cheap predictor that agreed with
            # fuzzy in all four field arbitrations of this shape —
            # multiset gains do NOT imply fuzzy gains.
            prev_dist = count_distance(state.get("last_insns"))
            cur_dist = count_distance(insns)
            if (prev_dist is not None and cur_dist is not None
                    and prev_dist != cur_dist):
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
                             if cur_dist > prev_dist
                             else "better — fuzzy may agree")
                    verdict += (f"\nCOUNT DISTANCE {prev_dist} ->"
                                f" {cur_dist} at a flat multiset ({trend});"
                                " fuzzy from a fresh report remains the"
                                " arbiter")
                else:
                    # SUPPRESSED, not disclaimed. Printing the figure and
                    # then denying it below leaves the number as the only
                    # concrete thing on the line, and a number in a verdict
                    # reads as evidence no matter what follows it. The
                    # predictor was wrong 4/4 on one function in exactly
                    # this shape, so there is nothing here worth quoting.
                    verdict += ("\nCOUNT DISTANCE: WITHHELD — this predictor"
                                " is sound ONLY at a flat multiset, and the"
                                " multiset moved, so no figure is reported"
                                " (it was wrong 4/4 on one function in this"
                                " exact shape). Arbitrate on fresh fuzzy"
                                " only")
        else:
            # The arrow is previous->current; the CLASSIFICATION is vs
            # best. Printing both without labels read as a contradiction
            # ("REGRESSED 244 -> 234") and cost two re-reads in the field.
            verdict = (f"REGRESSED vs best {best}: real"
                       f" {state.get('last_real', best)} -> {real}"
                       f" (prev -> current; insns {insns}{tok})"
                       "  [revert advised]")
            # Run-35 corollary to the fuzzy-bank-gate law: a negative
            # verdict measured on top of a poisoned anchor is not a fact
            # about the edit. One run's BEST edit read as a loss this way
            # and was re-applied from the last commit at +0.33 fuzzy.
            verdict += (
                "\nRE-RUN THIS NEGATIVE FROM THE LAST COMMIT before"
                " recording it: this verdict is measured against a BEST"
                " banked by earlier probes in this session, and a keep that"
                " should not have been banked makes a good edit read as a"
                " loss. `probe --discard` (back to HEAD), re-apply the edit,"
                " re-probe — or `--arbitrate` for the four-number view.")
        if best_tokens is None:
            # Say it on BOTH outcomes. A legacy state silently reproduces
            # the old prev-anchored answer, and the REGRESSED half is
            # exactly the half that tells a worker to throw work away.
            verdict += ("\n[no best_multiset banked (pre-run-29 state) —"
                        " the structure comparison fell back to the"
                        " PREVIOUS probe, which is the shape that misread"
                        " a CONFLICT as REGRESSED; --reset then re-probe"
                        " for a BEST-anchored verdict]")
    else:
        # real is FLAT. `real` alone has nothing to say here, so the
        # multiset decides: a converging multiset at unchanged real is a
        # structural win the plain NEUTRAL headline hid outright — and
        # because NEUTRAL never re-banked best, best_multiset stayed at the
        # WORSE count and every later probe was anchored on a stale
        # structure number.
        anchor_tokens = best_tokens if best_tokens is not None else prev_tokens
        anchor_name = "best" if best_tokens is not None else "prev"
        if (multiset_tokens is not None and anchor_tokens is not None
                and multiset_tokens < anchor_tokens):
            verdict = (f"IMPROVED-STRUCTURE real {real} UNCHANGED but multiset"
                       f" {anchor_tokens}t -> {multiset_tokens}t vs"
                       f" {anchor_name} FELL — the opcode stream converged at"
                       f" flat real (insns {insns})  [best updated]")
            bank_best()
        else:
            verdict = f"NEUTRAL   real {real} (insns {insns}{tok})"
    # LAST WORD ON EVERY BANK. Nothing above this line may leave a new best
    # standing whose fresh fuzzy fell below the anchor.
    verdict, state = apply_fuzzy_bank_gate(verdict, state, prior_best,
                                           best_fuzzy, fuzzy)
    state["last_real"] = real
    state["last_insns"] = insns
    if multiset_tokens is not None:
        state["last_multiset"] = multiset_tokens
    if digest is not None:
        state["last_bytes"] = digest
    state["last_verdict"] = verdict
    return verdict, state


SCAFFOLD_RE = re.compile(r"#pragma\s+(?!force_active)"
                         r"|(^|[^\w])volatile[^\w]")
SCAFFOLD_HEAD = 20


def scaffold_rows(text):
    """Line-numbered pragma/volatile scaffold rows for a TU."""
    return [f"  L{i + 1}: {line.strip()[:70]}"
            for i, line in enumerate(text.splitlines())
            if SCAFFOLD_RE.search(line)]


def print_scaffold_census(source, full=False):
    """Pragmas and volatile qualifiers go STALE and nothing in the loop
    re-audits them — two of four scaffold items in one function were
    stale in a single session, worth a third of its total gap.

    The list was truncated at 20 rows with no way to see the rest, so a
    TU whose scaffold ran past the cut could not be audited from the
    loop at all: --scaffold-all prints every row, --scaffold asks for
    the census on a probe that is not a BASELINE.
    """
    try:
        text = Path(source).read_text(encoding="utf-8", errors="replace")
    except Exception:
        return
    rows = scaffold_rows(text)
    if not rows:
        return
    print(f"[scaffold census ({len(rows)} file-wide rows — re-audit each:"
          " is its original premise still live?)]")
    shown = rows if full else rows[:SCAFFOLD_HEAD]
    for row in shown:
        print(row)
    if len(rows) > len(shown):
        print(f"  ... and {len(rows) - len(shown)} more — rerun with"
              " --scaffold-all to list every row")


def score_function(unit, fn, fn_stripped, raw_flag=()):
    """(real, insns) via `fndiff --count` over an ALREADY-BUILT object.

    Factored out of main() so --arbitrate scores its two states through the
    exact same path the verdict does; two scorers would be two definitions of
    `real`. Returns (None, None) when the function cannot be scored.
    """
    count = subprocess.run(
        [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
         "--count", "--no-build", *raw_flag],
        capture_output=True, text=True,
    ).stdout
    for line in count.splitlines():
        match = COUNT_RE.match(line.strip())
        if match and match.group(1) in (fn, fn_stripped):
            _, ti, bi, _lines, real_text = match.groups()
            # target/ours — labeled after a worker mis-read which side was
            # longer reconciling probe vs fndiff.
            return int(real_text), f"T{ti}/O{bi}"
    if re.search(rf"^OK\s+({re.escape(fn)}|{re.escape(fn_stripped)})\s*$",
                 count, re.M) or not count.strip():
        # fndiff --count prints nothing for byte-identical functions.
        return 0, "exact"
    return None, None


def report_fuzzy(unit, fn, fn_stripped):
    """Build the objdiff report and return this function's fuzzy, or None.

    The report build is a full link — expect it to take as long as ninja.
    """
    rep = subprocess.run(["ninja", f"build/{VERSION}/report.json"],
                         capture_output=True, text=True)
    if rep.returncode != 0:
        return None
    try:
        report = json.loads(
            Path(f"build/{VERSION}/report.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    bare = re.sub(r"\.(c|cpp)$", "", unit)
    val = None
    for entry in report.get("units", []):
        if entry.get("name", "").endswith(bare):
            for func in entry.get("functions", []):
                if func["name"] in (fn, fn_stripped) or \
                        func["name"].startswith(fn + "_80"):
                    val = float(func.get("fuzzy_match_percent", 0.0))
    return val


def arbitrate_table(label, base_real, base_fuzzy, cur_real, cur_fuzzy,
                    moved=()):
    """The four-number arbitration readout, as pure text.

    A real/fuzzy DISAGREEMENT is the whole reason this mode exists, so the
    recommendation is explicit about which metric decides it: fuzzy, from a
    fresh report build, per the metric-disagreement rule (AGENTS.md
    "Metric and semantic discipline" + residual-work discipline 9c, where a
    real 30->24 that probe and gate both passed was a fuzzy 81->72 loss).
    Fuzzy that is UNMEASURED never becomes a verdict — it prints
    INCONCLUSIVE, because `real` alone cannot arbitrate a disagreement it is
    one half of.
    """

    def fz(value):
        return "n/a" if value is None else f"{value:.4f}%"

    lines = [
        "ARBITRATION (one call, both states built; no verdict computed,"
        " nothing banked, working tree restored)",
        f"  BANKED  ({label})  real {base_real}  fuzzy {fz(base_fuzzy)}",
        f"  CURRENT (working)  real {cur_real}  fuzzy {fz(cur_fuzzy)}",
    ]
    real_delta = None
    if base_real is not None and cur_real is not None:
        real_delta = cur_real - base_real
        fuzzy_text = ("n/a" if base_fuzzy is None or cur_fuzzy is None
                      else f"{cur_fuzzy - base_fuzzy:+.4f}")
        lines.append(f"  DELTA              real {real_delta:+d} "
                     f" fuzzy {fuzzy_text}")
    if base_fuzzy is None or cur_fuzzy is None:
        lines.append(
            "  ARBITER: INCONCLUSIVE — fuzzy is unmeasured on"
            f" {'the banked' if base_fuzzy is None else 'the current'} state"
            " (report build failed, or the function is absent from the"
            " report). `real` alone cannot arbitrate a real/fuzzy"
            " disagreement, so no keep/revert is recommended here.")
    else:
        fuzzy_delta = cur_fuzzy - base_fuzzy
        real_better = real_delta is not None and real_delta < 0
        real_worse = real_delta is not None and real_delta > 0
        if fuzzy_delta > 0:
            verdict = ("fuzzy ROSE — KEEP the current state"
                       + (" (--rebase-best banks it as the new best and"
                          " revert point, which is required because real"
                          " REGRESSED)" if real_worse else ""))
        elif fuzzy_delta < 0:
            verdict = ("fuzzy FELL — REVERT to the banked state"
                       + (" even though real IMPROVED: this is exactly the"
                          " shape where probe and defake_gate both pass a"
                          " fuzzy loss" if real_better else ""))
        else:
            verdict = ("fuzzy is FLAT — fuzzy cannot decide; fall back to"
                       " real"
                       + (" (improved: keep)" if real_better else
                          " (regressed: revert)" if real_worse else
                          " (also flat: this edit is NEUTRAL on both"
                          " arbiters)"))
        lines.append(f"  ARBITER: {verdict}")
        if (real_better and fuzzy_delta < 0) or (real_worse
                                                 and fuzzy_delta > 0):
            lines.append(
                "  METRICS DISAGREE: real and fuzzy point opposite ways."
                " FUZZY from a fresh report is the arbiter here (both"
                " numbers above ARE from fresh report builds); real is a"
                " linear diff and reads register-color cascades in both"
                " directions.")
    if moved:
        lines.append(
            f"  DATA: non-text section(s) {', '.join(moved)} DIFFER between"
            " the two states — neither arbiter above can see them (both are"
            " computed over .text). Arbitrate those bytes separately with"
            " `python tools/gdl/datadiff.py <unit> --sections` before"
            " keeping or reverting.")
    return "\n".join(lines)


def run_arbitrate(unit, fn, fn_stripped, source, raw_flag=(),
                  vs_baseline=False):
    """Build+score BOTH the banked and the working state, then restore.

    The restore is in a `finally`: a failed build, a KeyboardInterrupt or an
    exception must never leave the snapshot's bytes sitting in the worker's
    source file, which is the one way this mode could destroy an edit.
    """
    if source is None:
        print(f"cannot arbitrate: no src source found for {unit}")
        return 1
    snap = snapshot_path(unit, source)
    label = "rolling snapshot"
    if vs_baseline:
        snap = snap.with_suffix(snap.suffix + ".base")
        label = "session baseline"
    if not snap.exists():
        print(f"cannot arbitrate: no banked {label} for this unit yet"
              " (a BASELINE or IMPROVED probe banks one)")
        return 1
    current_bytes = source.read_bytes()
    banked_bytes = snap.read_bytes()
    if current_bytes == banked_bytes:
        print(f"nothing to arbitrate: the working tree IS the banked"
              f" {label}, so both halves would measure the same bytes."
              " Edit first, or use --fuzzy for a single-state readout.")
        return 1

    def measure(which):
        build = subprocess.run(["ninja", f"build/{VERSION}/src/{unit}.o"],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print(f"BUILD FAILED ({which} state):")
            print((build.stdout + build.stderr).strip()[-1200:])
            return None
        real, insns = score_function(unit, fn, fn_stripped, raw_flag)
        if real is None:
            print(f"could not score {fn} in the {which} state")
            return None
        data = data_digest(unit)
        print(f"[{which}] real {real} (insns {insns}) — building the"
              " objdiff report for fuzzy")
        return real, insns, report_fuzzy(unit, fn, fn_stripped), data

    try:
        current = measure("current")
        if current is None:
            return 1
        source.write_bytes(banked_bytes)
        banked = measure("banked")
        if banked is None:
            return 1
    finally:
        # Unconditional: the working tree leaves this call exactly as it
        # arrived, whatever happened in between.
        if source.read_bytes() != current_bytes:
            source.write_bytes(current_bytes)
            print("[working tree restored to your edited state]")
    rebuild = subprocess.run(["ninja", f"build/{VERSION}/src/{unit}.o"],
                             capture_output=True, text=True)
    if rebuild.returncode != 0:
        print("WARNING: the object failed to rebuild after restoring your"
              " edit — the source is restored but build/ now holds the"
              " BANKED object. Re-run a plain probe before trusting any"
              " score.")
    print(arbitrate_table(label, banked[0], banked[2], current[0], current[2],
                          moved=moved_sections(banked[3], current[3])))
    return 0


def fuzzy_readout(unit, fn, fn_stripped, state, state_file, digest=None):
    """Build the report and print this function's fresh objdiff fuzzy.

    CONFLICT arbitration used to cost two manual builds per keep; this is
    that loop, made durable. The report build is a full link — expect it
    to take as long as ninja.

    The measured number is CACHED AGAINST THE OBJECT DIGEST it describes,
    so a later probe can reuse it only when the bytes are provably the
    same ones — and when those bytes are the BEST state's, it also becomes
    the fuzzy anchor a later CONFLICT prints without spending a build.
    """
    try:
        val = report_fuzzy(unit, fn, fn_stripped)
        prev_fz = state.get("last_fuzzy")
        if val is not None:
            arrow = (f" (prev {prev_fz:.4f})"
                     if isinstance(prev_fz, float) else "")
            print(f"FUZZY (fresh report): {val:.4f}%{arrow}")
            state["last_fuzzy"] = val
            if digest is not None:
                state["last_fuzzy_bytes"] = digest
                if state.get("best_bytes") == digest:
                    state["best_fuzzy"] = val
                    print("[cached as the BEST-STATE fuzzy anchor — the next"
                          " CONFLICT prints it without spending a build]")
                else:
                    print("[cached against these exact bytes; it becomes the"
                          " anchor once this state is banked as best]")
            state_file.write_text(json.dumps(state), encoding="utf-8")
        else:
            print("[--fuzzy: no number — the report build FAILED or this"
                  " function is absent from build/GUNE5D/report.json; no"
                  " fuzzy readout, and nothing cached]")
    except Exception as err:
        print(f"[--fuzzy: readout failed: {err}]")


REPLAN_AT = 3


def update_neutral_identical_streak(state, verdict):
    """Consecutive NEUTRAL-IDENTICAL probes on this function.

    A RE-SCORE recomputes nothing and is not a probe, so it neither counts
    nor resets. Every other non-identical verdict resets to zero.
    """
    current = state.get("neutral_identical_streak", 0)
    if verdict.startswith("RE-SCORE"):
        return current
    if verdict.startswith("NEUTRAL") and "NEUTRAL-IDENTICAL" in verdict:
        return current + 1
    return 0


def replan_hint(streak):
    """Advice after an edit that never reached codegen at all.

    NEUTRAL-IDENTICAL means the object bytes did not move: the edit folded
    away BEFORE codegen, so the source text never reached the compiler's
    decision point.

    DECISIVE AT ONE (run-37 item 6). This used to stay silent until the
    THIRD consecutive identical, on the theory that one was only evidence
    about a spelling and three were evidence about the axis. That reasoning
    undercharges the observation: an unchanged OBJECT is not a weak
    measurement of the edit, it is a categorical one — the construct was
    gone before the compiler chose anything, so no score can move and the
    probe answered a question about the FRONT END, not about codegen. UA
    and UB each spent two further probes re-spelling a lever the first
    probe had already shown unreachable. The banner now fires on the first,
    and ESCALATES at REPLAN_AT, where the evidence really has widened from
    one construct to the axis class.
    """
    if streak < 1:
        return None
    if streak < REPLAN_AT:
        return (
            "THIS EDIT NEVER REACHED CODEGEN: the object bytes are"
            " unchanged, so the construct folded away in the front end and"
            " the compiler never made the decision you were probing. That"
            " is CATEGORICAL for this form, not a weak negative — no"
            " further spelling of the SAME construct can reach the decision"
            " point either, and re-spelling it is how UA and UB each spent"
            " two more probes for nothing. Before the next probe, establish"
            " that your lever can reach codegen AT ALL (does the construct"
            " survive to the object? does the target even differ here?), or"
            " change the LEVER — a declaration/type/order change rather"
            " than a statement respell. `gdlmem laws --query <your residual"
            " signature>` first."
        )
    return (
        f"RE-PLAN THE AXIS CLASS: {streak} consecutive NEUTRAL-IDENTICAL"
        " probes — every one of those edits folded away before codegen and"
        " the object bytes never moved. That is a fact about the AXIS, not"
        " about the spellings: this construct does not reach the compiler's"
        " decision point at all, so a further spelling of it cannot either."
        " Change the LEVER (a different mechanism, a different function"
        " boundary, a declaration/type/order change rather than a statement"
        " respell), or record the axis as measured-dead with these"
        f" {streak} probed forms. `gdlmem laws --query <your residual"
        " signature>` before the next probe."
    )


def annotate_neutral(verdict, real, insns, multiset_tokens, prev_tokens,
                     prev_insns, prev_digest, digest,
                     prev_data=None, data=None, source_changed=True,
                     reverted=False):
    """Byte-identity + structural-drift annotations for a NEUTRAL verdict.

    Split out of classify() so the verdict table stays pure and testable;
    this half only formats what the caller already measured.

    ``reverted`` says the bytes this verdict describes were RESTORED by
    --revert rather than edited into place (run-37 item 7). Every
    annotation below compares against the PREVIOUS probe, which after a
    revert is the edit that was just undone — so a successful revert
    printed "OBJECT BYTES CHANGED … verify with objdiff fuzzy or revert"
    and, when the restored state scored structurally below the edit,
    "NEUTRAL-WORSE … NOT banked, revert with git (not --revert)". Both
    read as a FAILED revert and both advise re-doing the thing that just
    succeeded; UB and MC each burned verification calls on it. The
    comparison is still correct and still shown — only the advice, which
    is wrong here, is replaced.
    """
    # real-equal is not structure-equal: a NEUTRAL that moved the insn
    # count further from parity or grew the multiset is WORSE, and banking
    # it made --revert refuse to undo a bad probe (a worker had to restore
    # by hand). Key the verdict on the triple.
    worse = []
    prev_dist = count_distance(prev_insns)
    cur_dist = count_distance(insns)
    if (prev_dist is not None and cur_dist is not None
            and cur_dist > prev_dist):
        worse.append(f"count distance {prev_dist} -> {cur_dist}")
    if (multiset_tokens is not None and prev_tokens is not None
            and multiset_tokens > prev_tokens):
        worse.append(f"multiset {prev_tokens}t -> {multiset_tokens}t")
    # NEUTRAL scores do NOT prove byte identity (a fuzzy-visible encoding
    # change once passed every score in this tool). Hash the function's raw
    # bytes and say so when they moved. Byte identity is GROUND TRUTH and
    # computed FIRST: the worse-triple check compares against last-probe
    # state that can be stale, and the two once printed a contradictory
    # composite (NEUTRAL-WORSE + NEUTRAL-IDENTICAL) a worker had to
    # untangle by hand.
    bytes_identical = None
    if digest is not None and prev_digest is not None:
        bytes_identical = digest == prev_digest
        if not bytes_identical and reverted:
            verdict += ("  [REVERT OK: object bytes changed because the"
                        " REVERT restored them — that is the revert"
                        " working, not a failure. Every comparison in this"
                        " line is against the edit you just undid]")
        elif not bytes_identical:
            verdict += ("  [NEUTRAL-REARRANGED: OBJECT BYTES CHANGED —"
                        " this compares BUILT OBJECTS between probes, not"
                        " your source vs git (source can be identical to"
                        " HEAD and still trip this); neutral scores do not"
                        " prove identity — verify with objdiff fuzzy or"
                        " revert]")
        else:
            # DATA-ONLY comes FIRST because NEUTRAL-IDENTICAL's advice is
            # actively wrong for it. The fold-away reading assumes the
            # object did not move at all; when the instruction stream is
            # identical but a non-text section CHANGED, the edit reached
            # codegen and landed in a data pool, where no score in this
            # project can see it. That message told a lane its five
            # CORRECT constant fixes were null probes
            # (attempt.CS_image-wide-constant-sweep-five-fixes.20260901.v1
            # — five value bugs, two of them behavioural, every one of
            # them reading NEUTRAL-IDENTICAL).
            moved = moved_sections(prev_data, data) if source_changed else []
            if moved:
                verdict += (
                    "  [NEUTRAL-DATA-ONLY: instruction stream byte-identical"
                    " BY CONSTRUCTION, but the object's non-text section(s)"
                    f" {', '.join(moved)} MOVED — this edit reached codegen"
                    " and landed in a data pool. NOT a fold-away and NOT a"
                    " negative result: real, --ops, regnorm and fuzzy are all"
                    " computed over the instruction stream and are"
                    " structurally incapable of scoring a pooled value"
                    " (claim.law.SL_pool-constant-errors-are-score-invisible)."
                    " ARBITRATE WITH A VALUE AUDIT, never on the score: read"
                    " the target's lbl_ out of build/GUNE5D/asm/*.s and"
                    " compare the declared value AND the load width against"
                    " your literal. Reverting on this verdict throws away a"
                    " correct fix]")
            else:
                verdict += ("  [NEUTRAL-IDENTICAL: object bytes unchanged —"
                            " the edit FOLDED AWAY before codegen. For a"
                            " spelling probe this is a STRONGER negative than"
                            " a regression: the source text never reached the"
                            " compiler's decision point]")
    if worse and bytes_identical is not True:
        head = f"NEUTRAL   real {real}"
        if reverted:
            # The "worse" comparison is against the edit this revert just
            # undid, so it describes the RESTORED state — which is the one
            # the worker asked for. Telling them to revert it is backwards.
            verdict = (f"REVERTED real {real}"
                       f" ({'; '.join(worse)} vs the undone edit) — the"
                       " restored state scores structurally below the edit"
                       " you removed. That is a fact about the EDIT (it was"
                       " a structural improvement you have now discarded),"
                       " not a failed revert. Re-apply it deliberately if"
                       " you wanted it" + verdict[len(head):])
        else:
            verdict = (f"NEUTRAL-WORSE real {real}"
                       f" ({'; '.join(worse)}) — structurally worse at equal"
                       " real; NOT banked, revert with git (not --revert) or"
                       " justify the keep explicitly" + verdict[len(head):])
    return verdict


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
            for ext in (".meta", ".base", ".pins", ".base.pins"):
                extra = snap.with_suffix(snap.suffix + ext)
                if extra.exists():
                    extra.unlink()
        print("probe state reset")
        return 0
    if "--rederive-pin" in sys.argv:
        return rederive_pin(unit, fn, transient="--transient" in sys.argv)
    if "--arbitrate" in sys.argv:
        # Both halves of a real/fuzzy arbitration in one call. Dispatched
        # before the ordinary build/score path because it owns its own
        # builds, banks nothing, and computes no verdict.
        return run_arbitrate(
            unit, fn, re.sub(r"_80[0-9A-Fa-f]{6}$", "", fn), source,
            raw_flag=["--raw"] if "--raw" in sys.argv else [],
            vs_baseline="--vs-baseline" in sys.argv)
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
        restore_transient_pins(unit)
        # Even a whole-file discard leaves HEADER edits live (run 34 item 3).
        warn_outside_edits(source, None)
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
        if guard_stale_restore(base, source, "session baseline"):
            return 1
        shutil.copyfile(base, source)
        print(f"restored {source} to the SESSION BASELINE (whole file —"
              " uncommitted work on other functions in this TU is gone)")
        restore_transient_pins(unit)
        warn_pin_drift(unit, base)
        warn_outside_edits(source, None)
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
        # This used to run only when the .meta sidecar existed, which made
        # a missing stamp a silent PASS; it now fails closed in the shared
        # guard.
        if guard_stale_restore(snap, source, "snapshot"):
            return 1
        if snap.read_bytes() == source.read_bytes():
            print("nothing to revert: the banked snapshot IS the current"
                  " working tree (NEUTRAL probes bank too). If you want to"
                  " discard an uncommitted neutral edit, use git"
                  " (`git status` / `git checkout -- <file>`); re-scoring:")
        elif "--whole-file" in sys.argv:
            shutil.copyfile(snap, source)
            print(f"reverted {source} to {fn}'s banked snapshot —"
                  " --whole-file: uncommitted work on OTHER functions in"
                  " this TU since that bank is GONE; re-scoring:")
        else:
            # FUNCTION-SCOPED by default. The whole-file restore silently
            # took every other in-progress function in the TU back with
            # it; five lanes hit that. Only hunks that lie strictly inside
            # the named function are restored, and a hunk straddling the
            # boundary is refused rather than guessed at.
            # latin-1 round-trips every byte and no newline translation
            # happens on either side: writing a source file through text
            # mode would rewrite LF as CRLF wholesale (AGENTS discipline 7
            # — never let a shell or a codec rewrite a source file).
            snap_text = snap.read_bytes().decode("latin-1")
            cur_text = source.read_bytes().decode("latin-1")
            try:
                new_text, notes = scoped_revert(snap_text, cur_text, fn)
            except ValueError as err:
                print(f"REFUSED (function-scoped revert): {err}")
                return 1
            if new_text == cur_text:
                print(f"nothing to revert INSIDE {fn}: the snapshot and the"
                      " working tree differ only elsewhere in this TU"
                      " (--whole-file would take those changes back too);"
                      " re-scoring:")
            else:
                source.write_bytes(new_text.encode("latin-1"))
                print(f"reverted {fn} to its banked snapshot — {notes};"
                      " re-scoring:")
        # A source revert does not restore webfrank.json; warn if a pin was
        # re-derived since this snapshot was banked (run 34 item 3).
        # A pin re-derived with --transient IS restorable, and is restored
        # here rather than warned about (run 34 item 8).
        restore_transient_pins(unit)
        warn_pin_drift(unit, snap)
        # A FUNCTION-SCOPED revert reaches only hunks inside `fn`. Cross-check
        # the whole tree and name what it could not reach — MV's
        # volatile-in-a-macro header edit survived a revert and stayed live.
        warn_outside_edits(source, None if "--whole-file" in sys.argv else fn)

    build = subprocess.run(
        ["ninja", f"build/{VERSION}/src/{unit}.o"],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        print("BUILD FAILED:")
        print((build.stdout + build.stderr).strip()[-1500:])
        return 1

    raw_flag = ["--raw"] if "--raw" in sys.argv else []
    # fndiff strips a trailing _80XXXXXX address suffix from user-supplied
    # names; accept either spelling here so one name works everywhere.
    fn_stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", fn)
    real, insns = score_function(unit, fn, fn_stripped, raw_flag)

    if real is None:
        print(f"could not score {fn} — fndiff --count named no such function"
              " in the built object")
        return 1

    if "--stateless" in sys.argv:
        # Sweep mode: no state read, no banking, no verdict-vs-best —
        # the sticky per-function best made exhaustive-search output
        # chain nonsensically (`REGRESSED vs best 32: real 64 -> 157`).
        print(f"STATELESS real {real} (insns {insns}) — nothing banked"
              " or compared; pair with git for reverts")
        return 0

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
             "--ops", "--no-build", *raw_flag],
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
    prev_tokens = state.get("last_multiset")
    prev_insns = state.get("last_insns")
    prev_digest = state.get("last_bytes")
    digest = object_digest(unit, fn, fn_stripped)
    prev_data = state.get("last_data")
    data = data_digest(unit)
    # A cached fuzzy is usable ONLY when it provably describes the bytes
    # in front of us. Anything looser reintroduces the stale-number defect
    # the --fuzzy readout exists to prevent.
    cached_fuzzy = (state.get("last_fuzzy")
                    if digest is not None
                    and state.get("last_fuzzy_bytes") == digest else None)
    snap = snapshot_path(unit, source) if source is not None else None
    source_changed = True
    if snap is not None and snap.exists() and source.exists():
        source_changed = snap.read_bytes() != source.read_bytes()

    if "--fuzzy" in sys.argv:
        # PURE READOUT. No verdict, no state mutation beyond the fuzzy
        # number itself, no snapshot banked. Arbitration must be able to
        # re-read a state the loop has already scored without the verdict
        # changing underneath it (a CONFLICT re-read as REGRESSED that
        # way, on bytes that had not moved).
        tok = (f", multiset {multiset_tokens}t"
               if multiset_tokens is not None else "")
        print(f"READOUT   real {real} (insns {insns}{tok})"
              "  [--fuzzy: no verdict computed, no revert point banked]")
        standing = state.get("last_verdict")
        if standing:
            print(f"[standing verdict, unchanged by this readout]"
                  f"\n{standing}")
        fuzzy_readout(unit, fn, fn_stripped, state, state_file, digest=digest)
        return 0

    rebase_best = "--rebase-best" in sys.argv
    # The pre-verdict state, kept so the fuzzy gate below can re-run the
    # classification from the SAME inputs. Re-classifying from the state
    # classify() already returned would compare `real` against a best it
    # had just banked and read every improvement as NEUTRAL.
    state_before = dict(state)
    verdict, state = classify(state, real, insns, multiset_tokens,
                              rebase_best=rebase_best,
                              digest=digest, source_changed=source_changed,
                              fuzzy=cached_fuzzy)
    # FRESH FUZZY BEFORE THE BANK (run-35 item 1). The verdict above is
    # provisional whenever it would move the BEST anchor: a real+multiset
    # win can still be a fuzzy LOSS, and banking one poisons every later
    # verdict on the function. Spend the report build here — on the four
    # verdicts that actually change the high-water mark — rather than
    # discovering it two probes later. --rebase-best is exempt (it is the
    # deliberate arbitrated keep) and --no-fuzzy-gate opts out entirely.
    if (banks_best(verdict) and cached_fuzzy is None and not rebase_best
            and "--no-fuzzy-gate" not in sys.argv):
        print(f"[fuzzy gate: {verdict.split()[0]} would bank a new BEST —"
              " measuring this state's fresh objdiff fuzzy FIRST (report"
              " build; --no-fuzzy-gate skips it)]")
        fresh = report_fuzzy(unit, fn, fn_stripped)
        if fresh is not None:
            print(f"FUZZY (fresh report): {fresh:.4f}%")
        else:
            print("[fuzzy gate: no number — the report build FAILED or this"
                  " function is absent from build/GUNE5D/report.json]")
        verdict, state = classify(state_before, real, insns, multiset_tokens,
                                  rebase_best=False, digest=digest,
                                  source_changed=source_changed,
                                  fuzzy=fresh)
        if fresh is not None:
            cached_fuzzy = fresh
            state["last_fuzzy"] = fresh
            if digest is not None:
                state["last_fuzzy_bytes"] = digest
    # A CONFLICT with no fresh fuzzy on BOTH states classifies nothing at all
    # (run 34 item 4): PC recorded a false regression from an unarbitrated
    # CONFLICT headline. The exit code carries the refusal to any script.
    verdict, exit_code = conflict_gate(verdict, state.get("best_fuzzy"),
                                       cached_fuzzy)
    if exit_code:
        state["last_verdict"] = verdict
    if verdict.startswith("NEUTRAL"):
        verdict = annotate_neutral(verdict, real, insns, multiset_tokens,
                                   prev_tokens, prev_insns, prev_digest,
                                   digest, prev_data=prev_data, data=data,
                                   source_changed=source_changed,
                                   reverted="--revert" in sys.argv)
        state["last_verdict"] = verdict
    # A run of edits that never reached codegen is a fact about the axis,
    # not about the spellings tried. Aggregate it and say so.
    streak = update_neutral_identical_streak(state, verdict)
    state["neutral_identical_streak"] = streak
    if data is not None:
        state["last_data"] = data
    hint = replan_hint(streak)
    if hint:
        verdict += "\n" + hint
        state["last_verdict"] = verdict
    state_file.write_text(json.dumps(state), encoding="utf-8")
    print(verdict)
    # The DATA column, printed alongside EVERY verdict (run 34 item 1): the
    # verdict above scores the INSTRUCTION STREAM ONLY, so a moved non-text
    # section — a widened save area losing its .extab match, a corrected pool
    # word — is invisible to real, --ops, regnorm, the multiset and fuzzy
    # alike. data_line returns "" unless a section actually moved.
    # NEUTRAL-DATA-ONLY already names the same sections with advice tuned to
    # a byte-identical stream, so it owns that verdict; printing both would
    # report one measurement twice with two different recommendations.
    if "NEUTRAL-DATA-ONLY" not in verdict:
        dcl = data_line(prev_data, data, source_changed)
        if dcl:
            print(dcl)
    # The regnorm GENUINE structural-row count on the two verdicts the opcode
    # multiset can mislead (run 34 item 2): CONFLICT and NEUTRAL-WORSE are set
    # by the token count, which is unsound under cancelling pairs.
    if verdict.startswith("CONFLICT") or "NEUTRAL-WORSE" in verdict:
        gr = genuine_row_count(unit, fn)
        if gr is not None:
            print(format_genuine_note(*gr))

    # THE SLOT MAP, unasked, when the residual is slot-shaped (run-35 item
    # 3). `real` is the wrong arbiter for frame work and the loop offered no
    # slot information at all, so a lane wrote its own displacement scanner
    # rather than reach for the tool that already existed. Gated on real > 0
    # (no residual, nothing to arbitrate) and on slotdiff's own signal, so a
    # register/schedule probe never carries a 60-line map it does not need.
    if real > 0 and "--no-slots" not in sys.argv:
        slots_output = run_slot_arbiter(unit, fn)
        fires, reason = slot_arbiter_signal(slots_output)
        if fires or ("--slots" in sys.argv and slots_output):
            print(slot_arbiter_header(
                reason or "requested with --slots (no decisive slot signal)"))
            print(slots_output.strip())

    # The census used to print in full on EVERY baseline probe. Measured
    # 2026-09-02: 22 lines per probe on game/world/camera, game/game/combat,
    # game/ui/screensaver and game/game/player, 13 on game/sys/memcard —
    # the same file-wide rows re-listed every time, burying the verdict the
    # worker actually ran the probe for. It is now opt-in behind --verbose
    # (or the existing --scaffold/--scaffold-all), and a BASELINE prints a
    # ONE-LINE pointer so the audit obligation is still announced rather
    # than silently dropped (run-37 item 7).
    want_scaffold = ("--scaffold" in sys.argv or "--scaffold-all" in sys.argv
                     or "--verbose" in sys.argv)
    if want_scaffold and source is not None:
        print_scaffold_census(source, full="--scaffold-all" in sys.argv)
    elif verdict.startswith("BASELINE") and source is not None:
        count = len(scaffold_rows(source.read_bytes().decode("latin-1")))
        if count:
            print(f"[{count} pragma/volatile scaffold row(s) in this TU —"
                  " re-audit each (is its original premise still live?):"
                  " rerun with --verbose, or --scaffold-all for every row]")

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
        kind = verdict.split()[0]
        bank_snapshot(unit, source,
                      baseline=verdict.startswith("BASELINE"),
                      verdict_kind=kind, fn=fn,
                      rebaseline="--rebaseline" in sys.argv)
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
        # Truncate through fndiff.truncate_ops so a dropped IMMEDIATE row
        # (which sits below the clusters) is announced, never silently cut —
        # a truncated ops view read as a frame collapse once (item 5).
        sys.path.insert(0, str(TOOLS))
        from fndiff import truncate_ops
        print(truncate_ops(ops_output, 16))
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
    # Non-zero ONLY for an unarbitrated CONFLICT (3). Every other verdict —
    # including REGRESSED — is a completed probe and exits 0.
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
