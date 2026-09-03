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
  --discard [--function | --whole-file]
                     restore the TU to HEAD (the neutral-edit undo), then
                     REBUILD the object so object-reading tools stop
                     reporting the discarded probe (--no-rebuild skips)
  --revert-baseline  restore the SESSION's first banked baseline, then
                     rebuild the object for the same reason
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
                     probe cycles spent on pure pin plumbing.
                     BOTH ENDS of that A/B consume the bank: a KEEPING verdict
                     (BASELINE/IMPROVED/NEUTRAL/REBASED) drops it too, because
                     the re-derived pin is the one matching the state just
                     banked. Only the revert end used to, so a bank outlived
                     its A/B and the NEXT revert in the TU put pre-session pin
                     hashes back over a kept re-derivation (PC hand-deleted
                     the bank twice)
  --slots            force the slotdiff map even without a slot signal
  --no-slots         suppress the auto-invoked slot map (below)
  --rebaseline       deliberately MOVE the session baseline to the current
                     state. The baseline is created by the first bank on a
                     unit whatever verdict caused it, and is never
                     overwritten silently; this is the override
  --no-fuzzy-gate    skip the pre-bank fresh-fuzzy measurement (below).
                     Faster, and how the loop behaved before run 36 — but
                     a keep banked this way is unarbitrated
  --accept-fuzzy-loss  with --rebase-best ONLY: bank the keep even though
                     fresh fuzzy FELL below the anchor. Since run 40 the
                     fuzzy gate binds --rebase-best too, so a keep that
                     loses fuzzy is REBASE-REFUSED unless you say this;
                     the verdict then reads REBASED-FUZZY-LOSS with the
                     delta in the headline, for the record to quote
  --no-tu-gate       skip the pre-bank TU-SCOPE sibling cross-check
                     (below). Only ever runs at all when the diff changes
                     a file-scope declaration, storage class, qualifier or
                     pragma; say so in the record if you use it
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
the old all-or-nothing restore deliberately. --revert-baseline remains
whole-file by construction.

(2b) --discard is SCOPE-CHECKED since run 40. It still restores the whole
file, but it first asks what that would destroy: if any uncommitted hunk
in the TU lies outside the named function — a sibling function's
in-progress edit, or a file-scope declaration change — it REFUSES and
offers `--discard --function` (restore only this function's hunks) or
`--discard --whole-file` (the old behaviour, deliberately). When every
hunk is inside the named function the two are the same bytes and nothing
changes. Its success line always said "uncommitted work on other
functions in this TU is gone", which is a receipt, not a guard.

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
THE TU-SCOPE GATE RUNS BESIDE IT. The fuzzy gate closes "this function's
other metric disagrees"; it cannot close "this function is not the only
function in the object". A file-scope declaration, storage-class, pool
qualifier or pragma change moves SIBLING bytes, and real, the opcode
multiset, the slot map and fuzzy are all computed over ONE function's
.text. Measured: a one-word edit (`static void* potionicon_tab[5];` ->
external linkage) touching no function body scored IMPROVED real 840 ->
838 here, banked a new BEST, and cost NINE byte-exact functions TU-wide
(claim.law.PC_storage-class-of-a-same-tu-base-object-is-a-codegen-lever-
that-must-be-gated-tu-wide). probe now reads the file-scope items out of
the DIFF, and ONLY when they moved spends a build-free cross-check of the
whole TU against its `defake_gate` baseline:
  TU-SCOPE REGRESSED  a byte-exact SIBLING was demoted — best NOT updated
            and nothing banked. The siblings are named.
  TU-SCOPE UNGATED    the cross-check could not run (no TU baseline yet,
            or the baseline describes the edited bytes) — best NOT updated
            either. Fail-closed: a measurement nobody took is not evidence
            of no loss, and that is exactly how the nine were lost.
A body-only edit produces an identical file-scope item list and pays
nothing. BASELINE is exempt (it banks no improvement claim and is the
session's only revert point) and is annotated instead; --no-tu-gate opts
out.

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
function's fresh objdiff fuzzy, and computes NO verdict and does not move
the BEST anchor or the rolling revert point. Re-running the verdict on
bytes that were already scored is what made a CONFLICT re-read as
REGRESSED (see classify()'s BEST-anchored multiset comparison); an
arbitration readout must never be able to do that.

The ONE exception is the first probe on a unit: when NO snapshot exists,
--fuzzy banks the baseline from the state it just built and scored, so a
worker who reaches for --fuzzy first is not left with `--revert` saying
"no banked snapshot for this unit yet" over a build already paid for.
There is nothing to move in that case, which is exactly why it is safe;
once a snapshot exists --fuzzy never touches it. --no-bank opts out.

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

--arbitrate SWAPS THE PIN STATE IN STEP WITH THE SOURCE. webfrank.json is
global and pairs with exactly ONE source state, so on a TU whose pin was
re-derived with `--rederive-pin --transient` the banked half used to abort
in the WEBFRANK stage and the whole arbitration returned 1 — measured on
game/game/player with the do_exit permutation pin: `[current] real 870`
scored, then `BUILD FAILED (banked state)`. The pre-probe hashes are
already in the transient bank, so probe reads them out (without consuming
the bank), builds each half against the pin hashes that belong to it, and
restores BOTH files in the same `finally` as the source. Unpairable slots
WARN instead of silently measuring a partial swap.
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


def keep_consumes_transient_bank(argv):
    """Should a KEEP drop this TU's transient pin bank? (run-38 item 5)

    False on a revert invocation: --revert / --revert-baseline / --discard
    re-score through the same keep path, and their own consumer
    (wf_rederive_pin.restore_transient) deliberately KEEPS the bank when
    it emitted notes — "resolve these, then delete it by hand". Dropping
    it from the keep path would overrule that instruction.
    """
    return not any(flag in argv for flag in
                   ("--revert", "--revert-baseline", "--discard"))


def drop_transient_pins(unit, why):
    """Consume this TU's transient pin bank on a KEEP (run-38 item 5).

    `restore_transient` consumes the bank on a REVERT, because it
    describes one A/B. The other end of that A/B had no consumer at all:
    when a probe KEEPS the state, the re-derived pin is the one that
    matches the tree from here on, and the pre-probe hashes in the bank
    are stale. Left there, the next `--revert` / `--revert-baseline` /
    `--discard` in the same TU restores PRE-SESSION pin hashes over a pin
    that was deliberately re-derived and kept — a state PC had to
    hand-delete the bank to escape, twice.

    Returns True when a bank was dropped. Fail-soft like every other
    transient-pin path: a checkout where the postprocessor stack cannot
    import has no bank to drop.
    """
    module = _wf_rederive_module()
    if module is None:
        return False
    bank = Path(module.bank_path(unit))
    if not bank.exists():
        return False
    try:
        bank.unlink()
    except OSError as error:
        print(f"[transient pin bank NOT dropped: {error} — delete {bank} by"
              " hand, or a later revert will restore pre-session pin"
              " hashes]")
        return False
    print(f"[transient pin bank CONSUMED by this {why}: the re-derived pin"
          " hashes now match the banked state, so no later revert will put"
          " the pre-probe hashes back. Re-derive again if you revert the"
          " source by hand.]")
    return True


REDERIVE_HINT_RE = re.compile(
    r"probe\.py\s+(?P<unit>\S+)\s+(?P<fn>\S+)\s+--rederive-pin")


def pin_named_by_build(text):
    """The pin function webfrank's OWN repair hint names, or None.

    webfrank.rederive_hint() already prints
    `python tools/gdl/probe.py <unit> <pin> --rederive-pin` on the abort, so
    the failing pin's identity is in the build output every time. probe just
    never read it.
    """
    match = REDERIVE_HINT_RE.search(text or "")
    return match.group("fn") if match else None


def pin_functions(config_data, unit):
    """Every function this TU has a webfrank rule for, in file order."""
    if not isinstance(config_data, dict):
        return []
    return [rule.get("function")
            for rule in config_data.get("units", {}).get(unit, [])
            if isinstance(rule, dict) and rule.get("function")]


def read_pin_functions(unit):
    config = Path(f"config/{VERSION}/webfrank.json")
    if not config.exists():
        return []
    try:
        return pin_functions(
            json.loads(config.read_text(encoding="utf-8")), unit)
    except (OSError, json.JSONDecodeError):
        return []


def resolve_pin_target(requested, pins, failing=None):
    """(function_to_rederive, note) for --rederive-pin (run-39 item 3).

    THE DEFECT. `--rederive-pin` re-derived the function NAMED ON THE
    COMMAND LINE, which is the function the worker was probing — while the
    pin that aborts a build is a DOWNSTREAM one (a permutation pin whose
    window relocations moved because the upstream edit renumbered the
    pool). Reproduced at 0f45ae610:
    `probe.py game/game/player do_players --rederive-pin` printed
    "no webfrank rule for game/game/player::do_players" and then
    "rederive-pin ABORTED — a body hash moved (the edit changed codegen,
    not just the pool)". The first line is accurate and useless; the second
    is WRONG — no body hash moved, there is simply no rule — and it tells
    the worker their edit changed codegen when it did not. The pin that
    actually failed was do_exit, and webfrank's own abort text named it.

    Pure so every branch is decided without a build. Resolution order:
    the requested function if it really is a pin; else the pin the BUILD
    named; else the TU's only pin; else refuse and list the candidates,
    because guessing among several pins would paste hashes into the wrong
    rule.
    """
    if requested in pins:
        return requested, ""
    if not pins:
        return None, (
            f"{requested} has no webfrank rule, and neither does any other"
            " function in this TU — there is no pin here to re-derive."
            " Nothing was built or pasted. If a build is failing, it is not"
            " failing on a pin in this unit.")
    if failing and failing in pins:
        return failing, (
            f"[{requested} has no webfrank rule; the pin that ABORTED the"
            f" build is {failing}, which webfrank's own abort text names."
            f" Re-deriving {failing} instead — this is the downstream"
            " permutation pin your upstream edit shifted.]")
    if len(pins) == 1:
        return pins[0], (
            f"[{requested} has no webfrank rule. This TU has exactly one"
            f" pin, {pins[0]}, so that is the one being re-derived — a"
            " permutation pin aborts on the function it PINS, not on the"
            " function you edited.]")
    return None, (
        f"{requested} has no webfrank rule, and this TU has"
        f" {len(pins)} pins, so which one to re-derive cannot be inferred:"
        f" {', '.join(pins)}.\n"
        "  Nothing was built or pasted — pasting hashes into the wrong rule"
        " is not recoverable from the rule text alone. Re-run the failing"
        " build and read the pin named in webfrank's abort, then:\n"
        f"    python tools/gdl/probe.py <unit> <that pin> --rederive-pin")


def rederive_abort_reason(output, unit, fn):
    """Why wf_rederive_pin refused, read from ITS OWN output.

    The abort used to print ONE sentence for every failure: "a body hash
    moved (the edit changed codegen, not just the pool), or the rule has no
    instruction_permutation". On the missing-rule path that sentence is
    FALSE in its load-bearing half — no body hash moved, and the worker is
    told their edit changed codegen when it did not. Measured at 0f45ae610
    on `probe.py game/game/player do_players --rederive-pin`.

    Pure over the tool's text so each branch is tested without a build.
    """
    text = output or ""
    if "no webfrank rule" in text:
        return (f"rederive-pin ABORTED: {unit}::{fn} has no webfrank rule,"
                " so there is nothing to re-derive. NO body hash moved and"
                " nothing about your edit's codegen is implied by this."
                " A permutation pin aborts a build on the function it PINS,"
                " which is usually DOWNSTREAM of the one you edited — re-run"
                " the failing build and use the pin webfrank's abort names.")
    if "instruction_permutation" in text or "permutation" in text:
        return (f"rederive-pin ABORTED: {unit}::{fn} has a rule but no"
                " instruction_permutation window, so there are no relocation"
                " hashes to re-derive. Nothing was pasted.")
    return ("rederive-pin ABORTED — a body hash moved, so the edit changed"
            " CODEGEN, not just the anonymous pool. Nothing was pasted:"
            " re-derive the rule from scratch rather than pasting hashes"
            " over a body that is no longer the one the rule was proven on.")


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

    The function re-derived is RESOLVED, not assumed (run-39 item 3): the
    pin that aborts a build is the DOWNSTREAM one, not the function being
    probed, so a worker naming their own function used to get an accurate
    "no webfrank rule" followed by a FALSE "a body hash moved (the edit
    changed codegen)". See resolve_pin_target.
    """
    parts = unit.split("/")
    body = Path(f"build/{VERSION}/src/{'/'.join(parts[:-1])}"
                f"/.postprocess/body/{parts[-1]}.o")
    wf_tool = TOOLS / "composed_census" / "wf_rederive_pin.py"

    pins = read_pin_functions(unit)
    if fn not in pins:
        # Ask the BUILD which pin is failing before guessing. webfrank's
        # abort text names it; this is the only place that costs a build,
        # and only on the path that was previously guaranteed to fail.
        failing = None
        if len(pins) > 1:
            print(f"[{fn} has no webfrank rule in {unit} — building the"
                  " object to read which pin the WEBFRANK stage aborts on]")
            probe_build = subprocess.run(
                ["ninja", f"build/{VERSION}/src/{unit}.o"],
                capture_output=True, text=True)
            failing = pin_named_by_build(probe_build.stdout
                                         + probe_build.stderr)
        target, note = resolve_pin_target(fn, pins, failing)
        if note:
            print(note)
        if target is None:
            return 1
        fn = target

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
        print(rederive_abort_reason(r.stdout + r.stderr, unit, fn))
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


def raw_object_target(unit):
    """The ninja target for `unit`'s PRE-postprocess object.

    Run-39 item 10. `--raw` exists to score the compiler's own output for a
    TU whose functions are WebFrank-pinned, but probe built
    `build/<V>/src/<unit>.o` regardless — the POSTPROCESSED object, whose
    edge hash-asserts every pin. So the one situation --raw is for (a pin
    made stale by your own upstream edit) was exactly the situation in which
    it could not run: reproduced at 7688fc7df, where `probe --raw` on
    game/game/player::do_players died in the WEBFRANK stage on the do_exit
    pin. SY worked around it by building the body object and running fnasm
    by hand.

    Resolved through fnasm.raw_obj_path so the `frank` stage is preferred
    over `body` exactly as the reader does — frank runs BEFORE the object
    postprocessor when both are configured, so its output is what webfrank
    consumes. Falls back to the body path when nothing is staged yet,
    because a target that does not exist must still be BUILDABLE.

    Returned REPO-ROOT-RELATIVE with forward slashes: fnasm hands back an
    absolute path (it is a reader), and ninja rejects those outright —
    `ninja: error: unknown target 'W:\\...'`, measured on the first run of
    this function.
    """
    parts = unit.split("/")
    fallback = (f"build/{VERSION}/src/{'/'.join(parts[:-1])}"
                f"/.postprocess/body/{parts[-1]}.o")
    try:
        sys.path.insert(0, str(TOOLS))
        import fnasm as _fnasm
        staged = _fnasm.raw_obj_path(unit)
        if staged is not None:
            return Path(staged).resolve().relative_to(
                Path.cwd().resolve()).as_posix()
    except Exception:
        pass
    return fallback


def rebuild_after_restore(unit, why):
    """Rebuild ``unit``'s object after a restore that returns early.

    Run-39 item 11 / claim.law.MS_probe-discard-restores-source-but-not-
    objects-so-object-reading-tools-report-the-discarded-probe.20260902.v1.
    `--discard` and `--revert-baseline` restore the SOURCE and return
    without building, so every object-reading tool — wf_word_diff, fnasm,
    fndiff --no-build, regnorm, savedregs — keeps reporting the DISCARDED
    probe on a tree `git status` calls clean. MS nearly banked a word count
    that way (62 stale against 61 true).

    Reproduced at f1105b430 on game/game/player::DoPlayerTexMods: clean
    tree DIFFERING WORDS = 0; with a storage-class flip, 7; after
    `--discard` restored the source to HEAD, still 7.

    `--revert` never needed this — it falls through to main()'s own build
    and re-score. The two early-returning paths did.
    """
    build = subprocess.run(["ninja", f"build/{VERSION}/src/{unit}.o"],
                           capture_output=True, text=True)
    if build.returncode == 0:
        print(f"[object rebuilt after {why}: build/{VERSION}/src/{unit}.o now"
              " describes the restored source, so wf_word_diff / fnasm /"
              " fndiff --no-build / regnorm read the tree you are actually"
              " in. --no-rebuild skips this.]")
        return True
    print(f"WARNING: the object FAILED to rebuild after {why}. The source is"
          f" restored but build/{VERSION}/src/{unit}.o still holds the"
          " DISCARDED probe's bytes, and every object-reading tool"
          " (wf_word_diff, fnasm, fndiff --no-build, regnorm, savedregs)"
          " will report that state on a tree git calls clean — the defect"
          " claim.law.MS_probe-discard-restores-source-but-not-objects"
          " records. Do not quote a number from this tree until a build"
          " succeeds. Build output:")
    print((build.stdout + build.stderr).strip()[-1200:])
    return False


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


def readout_banks_baseline(snapshot_exists, has_source, no_bank):
    """Should a --fuzzy READOUT bank the session baseline? (run-38 item 4)

    True ONLY on the first probe of a unit. --fuzzy is an arbitration
    readout and must never move a revert point that already exists — that
    is the whole reason the branch banks nothing. But when NO snapshot
    exists there is nothing to move, and refusing to bank left `--revert`
    answering "no banked snapshot for this unit yet" on a function whose
    build had already been paid for by the readout itself.
    """
    return bool(has_source) and not snapshot_exists and not no_bank


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
    created_baseline = action in ("create", "overwrite")
    if created_baseline:
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
    # Whether the SESSION BASELINE file was (re)written here — the caller
    # records the baseline's scores in state on exactly that event, so
    # `baseline_clause` can print "where this session started" on later
    # negatives instead of leaving the worker to re-base mentally.
    return created_baseline


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


def _parity(insns):
    """(target, ours) from a "T<n>/O<n>" string, or None."""
    match = re.match(r"T(\d+)/O(\d+)$", insns or "")
    return (int(match.group(1)), int(match.group(2))) if match else None


def count_class_line(prev_insns, insns):
    """The CATEGORICAL verdict, printed before any real/fuzzy comparison.

    MV, run 39 (run-40 item 8): an instruction-COUNT change is not a
    quantity on the same scale as `real` or fuzzy — it is a CLASS change.
    claim.law.webfrank-cannot-close-a-count-asymmetric-residual and
    claim.law.webfrank-cannot-close-instruction-count-deltas both make the
    count the deciding fact: while T != O, NO postprocessor class can reach
    the function at all, so no rule, no permutation and no recolor is
    available however good the fuzzy looks. probe printed that only
    indirectly, as a `real`/multiset comparison and a count-distance
    predictor, so a probe that GAINED or LOST parity read as an ordinary
    numeric move and the class change went unremarked.

    Silent when parity did not change: this is a transition report, and
    saying "still asymmetric" on every probe of a long-asymmetric function
    is the kind of constant line readers learn to skip.
    """
    now = _parity(insns)
    before = _parity(prev_insns)
    if now is None or before is None:
        return ""
    was_equal = before[0] == before[1]
    is_equal = now[0] == now[1]
    if was_equal == is_equal:
        return ""
    if is_equal:
        return (f"COUNT-PARITY GAINED  insns {prev_insns} -> {insns}:"
                " ours and target now hold the SAME instruction count. This"
                " is a CLASS change, not a score change — a count-asymmetric"
                " residual is outside EVERY postprocessor class, and this"
                " function has just become eligible for one. Read it before"
                " the real/fuzzy line below.")
    return (f"COUNT-PARITY LOST  insns {prev_insns} -> {insns}:"
            f" ours and target now differ by {abs(now[0] - now[1])}"
            " instruction(s). This is a CLASS change, not a score change —"
            " while the counts differ NO postprocessor rule can close this"
            " function, so a fuzzy or real gain here buys a state no rule"
            " can finish. Read it before the real/fuzzy line below.")


def restore_scope_counts(base_text, cur_text, fn):
    """(inside, outside, entangled_spans) hunks between ``base`` and the tree.

    The counting half of `scoped_revert`, split out so `--discard` can ASK
    what a whole-file restore would destroy before doing it. Returns None
    when the function cannot be located on either side — the caller must
    then refuse to reason about scope rather than assume.
    """
    base_lines = split_lines(base_text, keepends=True)
    cur_lines = split_lines(cur_text, keepends=True)
    base_span = function_span(base_text, fn)
    cur_span = function_span(cur_text, fn)
    if base_span is None or cur_span is None:
        return None
    matcher = difflib.SequenceMatcher(None, base_lines, cur_lines,
                                      autojunk=False)
    inside = outside = 0
    entangled = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        in_cur = _inside(cur_span[0], cur_span[1], j1, j2)
        in_base = _inside(base_span[0], base_span[1], i1, i2)
        out_cur = _outside(cur_span[0], cur_span[1], j1, j2)
        out_base = _outside(base_span[0], base_span[1], i1, i2)
        if in_cur and in_base:
            inside += 1
        elif out_cur and out_base:
            outside += 1
            entangled.append(("outside", j1 + 1, j2 + 1))
        else:
            entangled.append(("straddling", j1 + 1, j2 + 1))
    return inside, outside, entangled


def discard_refusal(fn, unit, inside, outside, entangled):
    """The refusal text for a --discard that would destroy other work.

    MEASURED TWICE: `--discard` restores the WHOLE FILE to HEAD, and every
    multi-function TU session runs it. Its own success line has always said
    "uncommitted work on other functions in this TU is gone" — after the
    fact, which is not a guard, it is a receipt. `--revert` has been
    function-scoped since run 36 for exactly this reason; --discard was left
    whole-file "by construction".

    The refusal fires ONLY when there IS other work: with every hunk inside
    the named function a whole-file restore and a scoped one are the same
    bytes, and nothing changes.

    TWO CLASSES, REPORTED SEPARATELY (run-41 item 9). `restore_scope_counts`
    returns OUTSIDE and STRADDLING hunks in one list while counting only the
    outside ones, and this text used to print the outside COUNT against the
    whole LIST. On a hunk that crosses the boundary — which is what a
    declaration hoist out of a nested block produces when it moves the
    function's own bracing lines — it read, verbatim:

        0 uncommitted hunk(s) in this TU lie OUTSIDE alpha
        (straddling L12-L13)

    a count of zero followed by a list of one, and it then offered
    `--discard --function` as the remedy, which probe's own control flow
    refuses again for exactly that hunk. Both numbers now come from the list
    they describe, and only the remedies that can actually run are offered.
    """
    outside_spans = [row for row in entangled if row[0] == "outside"]
    straddling = [row for row in entangled if row[0] == "straddling"]

    def spans_of(rows):
        return ", ".join(f"L{a}-L{b}" for _kind, a, b in rows)

    lines = [f"REFUSED: --discard would restore ALL of {unit} to HEAD."]
    if outside_spans:
        lines.append(
            f"  {len(outside_spans)} uncommitted hunk(s) lie OUTSIDE {fn}"
            f" ({spans_of(outside_spans)}) — restoring the whole file"
            " destroys them, and that is the one thing this flag cannot"
            " undo.")
    if straddling:
        lines.append(
            f"  {len(straddling)} hunk(s) STRADDLE {fn}'s boundary"
            f" ({spans_of(straddling)}) — each contains lines both inside"
            " and outside the function, so no scoped restore can separate"
            " them.")
    if not outside_spans:
        lines.append(
            f"  No hunk lies wholly outside {fn}: every other differing line"
            " in this TU is inside it. --whole-file therefore destroys no"
            " sibling function's work here, only the straddling hunk(s)"
            " above.")
    if straddling:
        lines.append(
            "  --discard --function  WILL REFUSE while a straddling hunk"
            " exists; it cannot split one.")
    else:
        lines.append(
            f"  --discard --function   restore ONLY the hunks inside {fn},"
            " leaving the rest of the TU's uncommitted work alone")
    lines.append(
        "  --discard --whole-file  take the old all-or-nothing restore"
        " deliberately (run `git diff` first)")
    return "\n".join(lines)


def count_distance(text):
    """|target - ours| from a "T<n>/O<n>" insns string, or None."""
    match = re.match(r"T(\d+)/O(\d+)$", text or "")
    return abs(int(match.group(1)) - int(match.group(2))) if match else None


def object_digest(unit, fn, fn_stripped, objfile=None):
    """Raw-byte signature of the built function, or None if unavailable.

    ``objfile`` overrides the default postprocessed object so `--raw` hashes
    the bytes it actually scored; hashing the postprocessed object under
    --raw would make the re-score guard and NEUTRAL-IDENTICAL describe a
    different object than the verdict.
    """
    try:
        sys.path.insert(0, str(TOOLS))
        import fndiff as _fndiff
        objfile = Path(objfile or f"build/{VERSION}/src/{unit}.o")
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


def data_digest(unit, objfile=None):
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
        objfile = Path(objfile or f"build/{VERSION}/src/{unit}.o")
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

SNAPSHOT_ANCHOR = "snapshot_anchor"


def anchor_of(state):
    """The BEST anchor currently in ``state``, as a plain dict."""
    return {key: state.get(key) for key in BEST_KEYS}


def roll_back_anchor(state):
    """(new_state, note) — put the BEST anchor back to the one banked WITH
    the snapshot --revert just restored (run-38 item 9).

    THE DEFECT. classify() scores every verdict against ``best_real``, and
    probe.py persists the state UNCONDITIONALLY — while the SNAPSHOT bank
    sits behind ``--no-bank``, the flag documented for exactly the probes
    this bites ("DIAGNOSTIC probes ... that will be hand-reverted"). So a
    diagnostic edit moves the anchor onto itself and leaves the revert
    point behind, and the following --revert scores the RESTORED state
    against the edit it just discarded: "REGRESSED vs best 5 ... [revert
    advised]" on a revert that worked perfectly. AT measured it; T7's
    run-37 item-7 fix relabelled the neighbouring annotations but never
    touched this comparison.

    Fail-soft in both directions. No recorded anchor (a state banked
    before this, or a snapshot last moved by a probe of a DIFFERENT
    function in the same TU) leaves the anchor alone and SAYS so, rather
    than inventing a rollback target; an anchor equal to the current one
    is a no-op with no note.
    """
    banked = state.get(SNAPSHOT_ANCHOR)
    if not isinstance(banked, dict):
        return state, ("[no anchor was recorded with this snapshot, so the"
                       " verdict below is scored against the BEST state"
                       " seen this session — which, after a --no-bank"
                       " diagnostic, is the edit you just discarded. Probe"
                       " once more to re-anchor.]")
    current = anchor_of(state)
    if all(banked.get(key) == current.get(key) for key in BEST_KEYS):
        return state, ""
    state = dict(state)
    for key in BEST_KEYS:
        value = banked.get(key)
        if value is None:
            state.pop(key, None)
        else:
            state[key] = value
    return state, (
        f"[BEST anchor rolled back with the source: best_real"
        f" {current.get('best_real')} -> {banked.get('best_real')}. The"
        " restored state is scored against ITS OWN history, not against"
        " the edit this revert discarded.]")

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


def baseline_clause(state, real):
    """Where this session STARTED, for any verdict that quotes `best`.

    Run-39 criticism (MV, run-40 item 2): `REGRESSED vs best 852: real 852
    -> 864` names two numbers, neither of which is the one a worker needs —
    "am I above or below where this session began?" — so every negative got
    re-based mentally, against a `best` that may itself have been banked by
    a probe two edits ago. The session baseline real is already on disk (it
    is what `--revert-baseline` restores); it just was never printed.
    """
    base_real = state.get("baseline_real")
    if base_real is None:
        return ""
    delta = real - base_real
    direction = ("WORSE than" if delta > 0 else
                 "BETTER than" if delta < 0 else "EQUAL to")
    insns = state.get("baseline_insns")
    where = f", insns {insns}" if insns else ""
    return (f"\n[SESSION BASELINE real {base_real}{where} — this state is"
            f" {direction} the state this session started from"
            f" ({delta:+d} real). `best` is a rolling anchor; the baseline"
            " is the number that says whether the session is ahead.]")


def apply_fuzzy_bank_gate(verdict, state, prior_best, prior_best_fuzzy,
                          fuzzy, accept_fuzzy_loss=False):
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

    REBASED IS NOT EXEMPT ANY MORE (run-40 item 2). It used to be, on the
    reasoning that `--rebase-best` "IS the deliberate arbitrated keep" — but
    the flag records an INTENTION to arbitrate, not an arbitration, and
    nothing checked that the intention was discharged. Run 39, do_players
    probe A: a CONFLICT-shaped keep (real 840 -> 852 WORSE, multiset 14t ->
    12t BETTER) was banked as BEST at fresh fuzzy 96.8433 against the
    baseline's 97.2692, and the next two probes then scored as REGRESSED
    against that poisoned anchor — the exact failure the run-38 gate was
    built to stop, walking through the one door left open for it. Quoted in
    attempt.PC_do-players-linkage-axis-closed-from-step0-artifact-and-the-
    assignment-site-decides.20260903.v1. The keep can still be made, but it
    is now made EXPLICITLY, with the loss on the line, via
    `--accept-fuzzy-loss`.
    """
    if not banks_best(verdict):
        return verdict, state
    rebased = verdict.startswith("REBASED")
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
            " this as progress."
            + ("\nAND THIS IS A --rebase-best KEEP: the flag declares the"
               " keep ARBITRATED, and no arbitration happened. Nothing"
               " downstream will ever say so again — arbitrate NOW."
               if rebased else "")), state
    if prior_best_fuzzy is None or fuzzy >= prior_best_fuzzy - FUZZY_GATE_EPS:
        return verdict, state
    head = verdict.split("\n", 1)[0]
    delta = fuzzy - prior_best_fuzzy
    if rebased and accept_fuzzy_loss:
        # The keep proceeds, but the loss is now IN THE HEADLINE, so a
        # record quoting this verdict cannot omit it and a later probe's
        # negative can be traced to the anchor it was measured against.
        return (
            f"REBASED-FUZZY-LOSS  best banked at a fuzzy REGRESSION:"
            f" {prior_best_fuzzy:.4f}% -> {fuzzy:.4f}% ({delta:+.4f})"
            " — ACKNOWLEDGED via --accept-fuzzy-loss. Every later probe on"
            " this function is now anchored on a state the project scores"
            " WORSE; say so in the record, and re-derive any negative from"
            " the last COMMIT before recording it."
            f"\n[instruction-stream verdict: {head}]"), state
    state = dict(state)
    for key, value in prior_best.items():
        if value is None:
            state.pop(key, None)
        else:
            state[key] = value
    if rebased:
        gated = (
            f"REBASE-REFUSED  fresh objdiff fuzzy {prior_best_fuzzy:.4f}% ->"
            f" {fuzzy:.4f}% ({delta:+.4f}) — --rebase-best did NOT bank."
            " The flag declares an ARBITRATED keep; fuzzy from a fresh"
            " report is the arbiter, and it rejected this state. Run 39"
            " banked exactly this shape (real worse, multiset better, fuzzy"
            " 96.8433 vs 97.2692) and the next two probes read as REGRESSED"
            " against it. REVERT, or re-run with --accept-fuzzy-loss if the"
            " keep is deliberate — that spelling puts the loss in the"
            " headline for the record to quote."
            f"\n[instruction-stream verdict, SUPERSEDED by the gate: {head}]")
        return gated, state
    gated = (
        f"FUZZY-REGRESSED  fresh objdiff fuzzy {prior_best_fuzzy:.4f}% ->"
        f" {fuzzy:.4f}% ({delta:+.4f}) — best NOT updated"
        " and NOTHING banked, even though the instruction-stream metrics"
        " improved. real and the multiset are both computed over .text and"
        " both read register-color cascades; fuzzy from a fresh report is"
        " the arbiter when they disagree. REVERT, or arbitrate and bank the"
        " keep deliberately with --rebase-best --accept-fuzzy-loss."
        f"\n[instruction-stream verdict, SUPERSEDED by the gate: {head}]")
    return gated, state


# ---------------------------------------------------------------------------
# TU-SCOPE BANK GATE (run-39 item 1) — the nine-STRICT hazard.
#
# claim.law.PC_storage-class-of-a-same-tu-base-object-is-a-codegen-lever-
# that-must-be-gated-tu-wide: a ONE-WORD edit touching no function body
# (`static void* potionicon_tab[5];` -> external linkage) scored
# `IMPROVED real 840 -> 838 ... [best updated]` here and was banked as a
# new BEST, while `defake_gate check game/game/player` reported 13
# regressions of which NINE were byte-exact losses, and the full-image
# PROGRESS line fell STRICT 56.46% (2575) -> 56.19% (2566). Reproduced in
# this worktree at 0f45ae610 before the fix: the probe half printed
# `IMPROVED ... [best updated]`, the gate half named the same nine
# (AppendItemToLevel, setup_player_models, show_crystals byte-identical ->
# real N; DoPlayerTexMods, GetMaxPlayerModelSize, SetupPlayerTexMods,
# ShowRuneStones EXACT -> OPERAND_DIFF; SetPlayerWindows, del_player_blits
# EXACT -> STRUCTURAL).
#
# EVERY function-level instrument in this loop is blind to this by
# construction — real, the opcode multiset, the slot map, and fuzzy are
# all computed over ONE function's .text. The DATA column fires here (the
# extabindex move), but it explicitly says "This is NOT a revert order"
# and names non-text sections, not sibling .text losses; it is not this
# alarm and did not stop the bank.
#
# So: detect from the DIFF whether the edit reached TU scope at all, and
# only then spend the sibling cross-check. A body-only edit — the
# overwhelming majority of probes — pays nothing.
# ---------------------------------------------------------------------------

# Keywords that decide LINKAGE (which section an object lands in, and
# whether MWCC may address it off a same-TU base register web).
LINKAGE_KEYWORDS = ("static", "extern", "inline", "register")
# Keywords that decide POOL MEMBERSHIP (.rodata/.sdata2 vs .bss/.data/
# .sdata) without changing linkage. A `const` flip moves an object between
# pools and renumbers everything after it exactly as an added declaration
# does.
POOL_QUALIFIERS = ("const", "volatile")

_WS_RE = re.compile(r"\s+")


def _norm_decl(text):
    """Whitespace-collapsed declaration text; "" when there is nothing."""
    return _WS_RE.sub(" ", text).strip()


def file_scope_items(text):
    """Ordered [(kind, normalized_text)] for every FILE-SCOPE item.

    Three kinds, and they are exactly the three things that can move a
    sibling function's bytes without appearing in that sibling's source:

      decl    a file-scope declaration or definition (`static void* t[5];`,
              `const float k = 1.0f;`, `struct S { ... } s;`). Its presence,
              size, order, linkage and qualifiers all decide section layout
              and pool numbering TU-wide.
      fndef   the HEAD of a function definition (everything before its
              opening brace). Carries the linkage of the function itself —
              `static void f(void)` -> `void f(void)` is a TU-scope change
              that edits no declaration.
      pragma  a file-scope `#pragma`. AGENTS records the measured hazard
              directly: "NEVER unscoped #pragma peephole off mid-TU (poisons
              all downstream fns)".

    Function BODIES are discarded — that is the whole point. An edit that
    only rewrites statements inside braces produces an identical item list
    and costs this gate nothing.

    A depth-0 brace group is a FUNCTION BODY iff the text before its `{`
    ends in `)` (a declarator) or is empty (K&R parameter declarations
    already flushed at their own semicolons) — then the head is emitted as
    `fndef` and the body discarded. ANY other head means the braces are an
    aggregate body or an initializer list (`static struct {...} s;`,
    `static const int tab[] = {1,2,3};`), whose CONTENTS decide layout and
    pool bytes, so the braces stay in the buffer and the whole thing lands
    as one `decl` at its semicolon. Guessing from what FOLLOWS the `}`
    instead loses the identifier of every `struct {...} name;` (measured:
    the first form of this parser emitted `fndef "static struct"` plus
    `decl "gThing"`).

    Parsed over comment/literal-stripped text (so a `}` in a string cannot
    unbalance the depth), and preprocessor lines are skipped for brace
    counting entirely, backslash-continuations included: a multi-line macro
    body carrying an unmatched brace would otherwise desynchronise every
    item after it.

    A C++ LINKAGE-SPECIFICATION block (`extern "C" { ... }`) is
    TRANSPARENT: it opens no scope, and everything inside it is still file
    scope. This is not a nicety — `extern "C" {` wraps lines 44-2842 of
    src/game/movie/movieplayer.cpp and 73-400 of src/game/pb/pb_tree.cpp,
    so counting it as a scope would have made this gate itemize NOTHING in
    those TUs and silently pass every file-scope edit in them. (The first
    draft of this docstring asserted no such block existed in the tree;
    grepping refuted it.)
    """
    items = []
    depth = 0            # NON-transparent open braces only
    stack = []           # one bool per open brace: True = linkage spec
    buf = []
    body_head = None
    in_directive = False
    for line in split_lines(strip_noncode(text)):
        stripped = line.strip()
        if in_directive or stripped.startswith("#"):
            if depth == 0 and not in_directive and stripped.startswith(
                    "#pragma"):
                items.append(("pragma", _norm_decl(stripped)))
            in_directive = stripped.endswith("\\")
            continue
        for ch in line:
            if ch == "{":
                head = _norm_decl("".join(buf)) if depth == 0 else None
                # `extern "C"` normalizes to bare `extern`: strip_noncode
                # blanks the string literal but preserves its width.
                if depth == 0 and head == "extern":
                    stack.append(True)
                    buf = []
                    continue
                if depth == 0:
                    body_head = head
                stack.append(False)
                depth += 1
                buf.append(ch)
                continue
            if ch == "}":
                if stack and stack.pop():
                    buf = []
                    continue
                depth = max(depth - 1, 0)
                buf.append(ch)
                if depth == 0 and (body_head == ""
                                   or (body_head or "").endswith(")")):
                    # A function body: keep the head, drop the statements.
                    if body_head:
                        items.append(("fndef", body_head))
                    buf = []
                    body_head = None
                continue
            if ch == ";" and depth == 0:
                decl = _norm_decl("".join(buf))
                if decl:
                    items.append(("decl", decl))
                buf = []
                body_head = None
                continue
            buf.append(ch)
        buf.append(" ")
    return items


def _split_keywords(decl):
    """(frozenset(leading keywords), remainder) for a normalized decl."""
    words = decl.split(" ")
    keywords = set()
    index = 0
    while index < len(words) and words[index] in (
            LINKAGE_KEYWORDS + POOL_QUALIFIERS):
        keywords.add(words[index])
        index += 1
    return frozenset(keywords), " ".join(words[index:])


def _keyword_change(old, new):
    """A category name when `old`/`new` differ ONLY in leading storage-class
    or pool keywords, else None."""
    old_keywords, old_rest = _split_keywords(old)
    new_keywords, new_rest = _split_keywords(new)
    if old_rest != new_rest or old_keywords == new_keywords:
        return None
    moved = old_keywords ^ new_keywords
    if moved & set(LINKAGE_KEYWORDS):
        return "storage-class/linkage"
    return "pool qualifier"


def tu_scope_changes(head_text, cur_text):
    """[(category, description)] for every FILE-SCOPE difference, or [].

    `head_text is None` (the source is not committed) returns [] — there is
    no committed sibling to regress, exactly as stale_restore_refusal treats
    an uncommitted file. That is the one place this gate is deliberately
    silent, and it is silent because the hazard cannot exist there.

    Empty means the edit lives entirely inside function bodies and the
    sibling cross-check below is not worth a measurement.
    """
    if head_text is None or cur_text is None:
        return []
    old = file_scope_items(head_text)
    new = file_scope_items(cur_text)
    if old == new:
        return []
    changes = []
    matcher = difflib.SequenceMatcher(None, old, new, autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        if tag == "replace" and (i2 - i1) == (j2 - j1):
            for old_item, new_item in zip(old[i1:i2], new[j1:j2]):
                category = (_keyword_change(old_item[1], new_item[1])
                            if old_item[0] == new_item[0] else None)
                if category:
                    changes.append((category,
                                    f"{old_item[1]}  ->  {new_item[1]}"))
                else:
                    changes.append((f"{new_item[0]} changed",
                                    f"{old_item[1]}  ->  {new_item[1]}"))
            continue
        for kind, item in old[i1:i2]:
            changes.append((f"{kind} REMOVED", item))
        for kind, item in new[j1:j2]:
            changes.append((f"{kind} ADDED", item))
    return changes


# A REGRESSION whose reason names one of these is the loss of a BYTE-EXACT
# function — the STRICT column, and the only thing the project's progress
# number counts as matched. defake_gate.compare writes these three phrasings
# and no other for that class.
STRICT_LOSS_MARKERS = ("was byte-identical", "status EXACT ->",
                       "function vanished")


def sibling_strict_losses(verdicts, probed_names):
    """(strict, other) regression rows on functions OTHER than the probed one.

    `strict` are byte-exact losses — the nine of the law. `other` are
    ordinary real/structure regressions, reported but not fatal: a
    sibling's real moving while it was never exact is the ordinary cost of
    a TU-wide conversion and is arbitrated, not refused.

    Pure over defake_gate.compare's (name, verdict, why) triples so the
    split is tested without an object file.
    """
    strict, other = [], []
    for row in verdicts or []:
        name, verdict, why = row[0], row[1], row[2]
        if name in probed_names or name == "__sections__":
            continue
        if verdict != "REGRESSION":
            continue
        if any(marker in why for marker in STRICT_LOSS_MARKERS):
            strict.append((name, why))
        else:
            other.append((name, why))
    return strict, other


def tu_scope_refusal(scope_changes, strict, other, note, unit, fn):
    """The refusal text, or "" when the bank may proceed.

    Refuses on a STRICT sibling loss, and refuses just as hard when the
    cross-check COULD NOT RUN (`note` set, `strict` None). Fail-closed is
    the whole design: absence of the measurement is not evidence of no
    sibling loss, and this gate exists because a measurement nobody took
    cost nine byte-exact functions.
    """
    scope_lines = "\n".join(f"    [{category}] {what}"
                            for category, what in scope_changes)
    head = ("TU-SCOPE EDIT DETECTED — this diff changes FILE-SCOPE"
            f" declarations, not just {fn}'s body:\n{scope_lines}\n"
            "  Every score in this loop (real, the opcode multiset, the"
            " slot map, fuzzy) is computed over ONE function's .text and"
            " is blind to what this does to the TU's other functions"
            " (claim.law.PC_storage-class-of-a-same-tu-base-object-is-a-"
            "codegen-lever-that-must-be-gated-tu-wide).")
    if strict is None:
        return (f"TU-SCOPE UNGATED  best NOT updated and NOTHING banked.\n"
                f"{head}\n"
                f"  The sibling cross-check could not run: {note}.\n"
                "  Take the TU baseline and re-probe:\n"
                f"    python tools/gdl/defake_gate.py baseline {unit}"
                " --at-head\n"
                f"    python tools/gdl/probe.py {unit} {fn}\n"
                "  --no-tu-gate banks without the cross-check (say so in"
                " the record if you use it).")
    if strict:
        rows = "\n".join(f"    {name}  {why}" for name, why in strict)
        extra = ""
        if other:
            extra = ("\n  Plus"
                     f" {len(other)} non-exact sibling regression(s):"
                     " " + ", ".join(name for name, _ in other) + ".")
        return (f"TU-SCOPE REGRESSED  {len(strict)} BYTE-EXACT sibling(s)"
                " lost — best NOT updated and NOTHING banked, even though"
                f" the instruction-stream metrics improved for {fn}.\n"
                f"{head}\n"
                f"  Byte-exact siblings destroyed by this edit:\n{rows}"
                f"{extra}\n"
                "  A probed function's gain does not buy exact siblings."
                " Revert, or add the compensating pad in the vacated slot"
                " and re-probe; keep it only if a full `ninja` PROGRESS"
                " STRICT count does not fall. --no-tu-gate banks anyway.")
    passed = ("[TU-scope gate: file-scope change(s) detected and"
              " cross-checked against the TU baseline — no byte-exact"
              " sibling lost"
              + (f", {len(other)} non-exact sibling regression(s):"
                 " " + ", ".join(name for name, _ in other)
                 if other else "") + f". Baseline: {note}.]")
    return "\x00" + passed


def apply_tu_scope_gate(verdict, state, prior_best, scope_changes, strict,
                        other, note, unit, fn):
    """(verdict, state) — un-bank a new BEST that costs a byte-exact sibling.

    Mirrors apply_fuzzy_bank_gate: only the banking verdicts are gated,
    REBASED is exempt (it IS the deliberate arbitrated keep), and refusing
    restores the BEST_KEYS captured before classify() ran.

    BASELINE is exempt too, and for a different reason: it banks no
    IMPROVEMENT claim, it creates the only revert point the session has.
    Refusing it would leave a worker who edited before their first probe
    with no snapshot at all — strictly worse than the hazard, and the
    FIRST-BASELINE TRAP already has its own loud warning. A BASELINE over a
    TU-scope diff is ANNOTATED with the same evidence and banks normally.
    """
    if not banks_best(verdict) or verdict.startswith("REBASED"):
        return verdict, state
    if not scope_changes:
        return verdict, state
    if verdict.startswith("BASELINE"):
        scope_lines = "\n".join(f"    [{category}] {what}"
                                for category, what in scope_changes)
        return verdict + (
            "\n[TU-scope gate: this BASELINE is banked over file-scope"
            f" change(s) already in the tree:\n{scope_lines}\n"
            "  The bank proceeds — a baseline claims no improvement and is"
            " the session's only revert point — but every later verdict on"
            " this unit is measured against a tree that has already moved"
            " its siblings. Take `defake_gate.py baseline"
            f" {unit} --at-head` if you need the committed comparison.]"
        ), state
    message = tu_scope_refusal(scope_changes, strict, other, note, unit, fn)
    if not message:
        return verdict, state
    if message.startswith("\x00"):
        return verdict + "\n" + message[1:], state
    state = dict(state)
    for key, value in prior_best.items():
        if value is None:
            state.pop(key, None)
        else:
            state[key] = value
    head = verdict.split("\n", 1)[0]
    return (message + "\n[instruction-stream verdict, SUPERSEDED by the"
            f" gate: {head}]"), state


def _defake_gate_module():
    """The defake_gate module, or None. Fail-soft import, fail-CLOSED use:
    the caller turns None into a refusal, not into a pass."""
    try:
        if str(TOOLS) not in sys.path:
            sys.path.insert(0, str(TOOLS))
        import defake_gate
        return defake_gate
    except Exception:
        return None


def tu_sibling_regressions(unit):
    """(verdicts, note) from the TU's defake_gate baseline, or (None, why).

    Takes NO build: probe has already built this object, and measure_unit
    only reads it. The baseline is whatever `defake_gate.py baseline` last
    banked for the unit; a baseline whose `source_sha1` equals the CURRENT
    source describes the EDITED tree and can only report "no change", so it
    is refused rather than believed.
    """
    module = _defake_gate_module()
    if module is None:
        return None, ("tools/gdl/defake_gate.py could not be imported")
    try:
        path = module.gate_path(unit)
        if not path.exists():
            return None, f"no TU baseline at {path}"
        baseline, meta = module.load_baseline(path)
        if meta.get("source_sha1") and meta["source_sha1"] == (
                module.source_digest(unit)):
            return None, (
                f"the baseline at {path} was taken from the SOURCE BYTES"
                " now in the working tree, so it describes the edited state"
                " and can only report 'no change'")
        snap, _fuzzy_note = module.measure_unit(unit)
        verdicts = module.compare(
            baseline, snap, resolve=module.resolve_symbol,
            target_relocs=module.target_relocation_symbols(unit))
        anchor = (meta.get("head") or "?")[:9]
        head = git_head()
        drift = ("" if head and meta.get("head") == head else
                 " — NOT the current HEAD, so a row here may predate this"
                 " edit; re-take with --at-head to be sure")
        return verdicts, f"{path} anchored at {anchor}{drift}"
    except Exception as error:
        return None, f"the TU cross-check raised {type(error).__name__}: {error}"


def classify(state, real, insns, multiset_tokens, rebase_best=False,
             digest=None, source_changed=True, fuzzy=None,
             accept_fuzzy_loss=False):
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
            verdict += baseline_clause(state, real)
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
            verdict += baseline_clause(state, real)
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
                                           best_fuzzy, fuzzy,
                                           accept_fuzzy_loss)
    # THE CATEGORICAL LINE (run-40 item 8) is carried OUT OF BAND, in
    # `count_class`, and printed by main() ABOVE the verdict. It is not
    # prepended to `verdict` on purpose: every downstream decision in this
    # tool — banks_best, the snapshot bank, conflict_gate, the NEUTRAL
    # annotators — dispatches on verdict.startswith(), so a prefix would
    # silently disable banking. It changes what the verdict MEANS, never
    # what it IS.
    state["count_class"] = count_class_line(state.get("last_insns"), insns)
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


def transient_pin_texts(unit):
    """(wf_for_working, wf_for_banked, notes) — the webfrank.json text that
    pairs with EACH source state — or None when there is nothing to pair.

    Run-39 item 2, reproduced live before this was written. `--arbitrate`
    swaps the SOURCE between the working tree and the banked snapshot and
    builds each, but webfrank.json is global state that pairs with exactly
    ONE of them: after a `--rederive-pin --transient`, the config holds the
    re-derived hashes for the WORKING source, so building the BANKED source
    aborts in the WEBFRANK stage and the whole arbitration returns 1.
    Measured on game/game/player at 0f45ae610 with the do_exit permutation
    pin: `[current] real 870` scored, then `BUILD FAILED (banked state)`.
    That is the one A/B a pinned TU most needs and the one it could not run.

    The pre-probe hashes are already banked — `wf_rederive_pin` wrote them
    for exactly this reason — so this reads them out WITHOUT consuming the
    bank: restore_transient is run for its text, both files are put straight
    back, and the caller gets two texts it can pair with two source states.

    Returns None (arbitrate behaves exactly as before) when there is no
    bank, no postprocessor stack, or nothing actually moved. Returns notes
    instead of a pairing when restore_transient could not pair the slots —
    a partial swap must WARN, never be presented as a measurement.
    """
    module = _wf_rederive_module()
    if module is None:
        return None
    config = Path(f"config/{VERSION}/webfrank.json")
    bank = Path(module.bank_path(unit))
    if not bank.exists() or not config.exists():
        return None
    bank_bytes = bank.read_bytes()
    working = config.read_bytes()
    restored, notes = [], []
    try:
        restored, notes = module.restore_transient(
            unit, str(config), str(bank))
        banked = config.read_bytes()
    except Exception as error:                       # pragma: no cover
        banked = working
        notes = [f"restore_transient raised {type(error).__name__}: {error}"]
    finally:
        # Both files go back exactly as found, whatever happened: this
        # function is a READ of the bank, not a use of it.
        config.write_bytes(working)
        if not bank.exists():
            bank.write_bytes(bank_bytes)
    if notes:
        return working, None, notes
    if not restored or banked == working:
        return None
    return working, banked, []


def run_arbitrate(unit, fn, fn_stripped, source, raw_flag=(),
                  vs_baseline=False):
    """Build+score BOTH the banked and the working state, then restore.

    The restore is in a `finally`: a failed build, a KeyboardInterrupt or an
    exception must never leave the snapshot's bytes sitting in the worker's
    source file, which is the one way this mode could destroy an edit. The
    same `finally` restores config/GUNE5D/webfrank.json, which this mode now
    swaps in step with the source (see transient_pin_texts).
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

    # PIN STATE PAIRS WITH SOURCE STATE (run-39 item 2). webfrank.json is
    # global and matches exactly one of the two source states; without this
    # the banked half aborts in the WEBFRANK stage on any TU whose pin was
    # re-derived with --transient. The config is an explicit `build.ninja`
    # input to the webfrank edge, so rewriting it re-triggers the stage on
    # its own — no configure.py, because no NEW edge is created.
    config = Path(f"config/{VERSION}/webfrank.json")
    config_bytes = config.read_bytes() if config.exists() else None
    pins = transient_pin_texts(unit)
    wf_working = wf_banked = None
    if pins is not None:
        wf_working, wf_banked, notes = pins
        if wf_banked is None:
            print("WARNING: this TU has a TRANSIENT pin bank that could not"
                  " be paired with the banked source state — "
                  + "; ".join(notes) + ". The banked half of this"
                  " arbitration will very likely abort in the WEBFRANK"
                  " stage. Resolve the note(s) above first.")
        else:
            print("[transient pin bank found: webfrank.json will be swapped"
                  " IN STEP with the source so each half builds against the"
                  " pin hashes that belong to it; both files are restored"
                  " afterwards]")

    def measure(which, wf_text=None):
        if wf_text is not None and config_bytes is not None:
            config.write_bytes(wf_text)
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
        current = measure("current", wf_working)
        if current is None:
            return 1
        source.write_bytes(banked_bytes)
        banked = measure("banked", wf_banked)
        if banked is None:
            return 1
    finally:
        # Unconditional: the working tree leaves this call exactly as it
        # arrived, whatever happened in between — source AND pin state.
        if source.read_bytes() != current_bytes:
            source.write_bytes(current_bytes)
            print("[working tree restored to your edited state]")
        if config_bytes is not None and config.read_bytes() != config_bytes:
            config.write_bytes(config_bytes)
            print("[config/GUNE5D/webfrank.json restored to its pre-"
                  "arbitration state; the transient pin bank is untouched]")
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


# The direction the hint used to prescribe unconditionally, kept as the
# SLOT-CLASS counter-example rather than deleted: on a frame/local-area
# residual it is backwards, and it was measured backwards four times in one
# lane. attempt.CL_mbcameraupdate-derived-iv-order-closes-the-scratch-band-
# and-costs-8-frame-bytes.20260903.v1 probed MBCameraUpdate's 8-byte frame
# surplus with FOUR declaration-class levers — hoisting the loop locals to
# the enclosing block, deleting the block and moving all three ints to
# function scope, deleting a local by reusing another, and eliminating a
# named variable in favour of an inline expression — and all four returned
# NEUTRAL-IDENTICAL against the same object digest 92a95ba06d65. The two
# probes that DID reach codegen were STATEMENT-shape (an accumulator
# `dstOffset += 16` at the loop end versus a derived `dstOffset = row * 16`
# at its top), and the kept win — real 34 -> 10 — was a statement-shape one.
# Its predecessor record adds two more: CT's probes B and D were both
# declaration hoists and both NEUTRAL-IDENTICAL at digest 0ee73392d393.
_SLOT_CLASS_REDIRECT = (
    " ON THIS FUNCTION THE USUAL ADVICE IS BACKWARDS. The slot arbiter"
    " fired, so the residual is frame- or slot-shaped, and declaration,"
    " scope and local-COUNT levers are exactly the ones measured to fold"
    " away here: MBCameraUpdate's 8-byte frame surplus took four of them"
    " (hoist the loop locals to the enclosing block; delete the block and"
    " move the ints to function scope; delete a local by reusing another;"
    " replace a named variable with an inline expression) and returned the"
    " SAME object digest 92a95ba06d65 every time, while both probes that"
    " reached codegen were STATEMENT-shape — an accumulator against a"
    " derived induction variable — and the statement-shape form is what"
    " bought real 34 -> 10"
    " (attempt.CL_mbcameraupdate-derived-iv-order-closes-the-scratch-band-"
    "and-costs-8-frame-bytes.20260903.v1). Try the statement shape that"
    " changes WHAT IS LIVE ACROSS THE LOOP, and arbitrate on the slot map"
    " printed below, not on `real`.")

_LEVER_QUESTION = (
    " Change the CONSTRUCT CLASS, not the spelling. Which class reaches"
    " codegen is a fact about this function, not a rule: declaration, type"
    " and order levers reach it in some functions and fold in others, and"
    " so do statement-shape levers — pick the class whose OUTPUT you can"
    " name in the target's aligned view, and `gdlmem laws --query <your"
    " residual signature>` first.")


def replan_hint(streak, slot_class=False):
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

    CLASS-CONDITIONAL FROM RUN 42 (item 5). The digest FACT above is what
    the banner is for and it is unchanged; the PRESCRIPTION that rode along
    with it — "a declaration/type/order change rather than a statement
    respell" — was measured pointing the wrong way six times on one
    function, because on a frame/slot residual the declaration class is
    precisely the one that folds. `slot_class` is the slot arbiter's own
    signal, already computed for the map probe prints under a slot-shaped
    residual, so the redirect costs no extra measurement.
    """
    if streak < 1:
        return None
    tail = _SLOT_CLASS_REDIRECT if slot_class else _LEVER_QUESTION
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
            " survive to the object? does the target even differ here?)."
            + tail
        )
    return (
        f"RE-PLAN THE AXIS CLASS: {streak} consecutive NEUTRAL-IDENTICAL"
        " probes — every one of those edits folded away before codegen and"
        " the object bytes never moved. That is a fact about the AXIS, not"
        " about the spellings: this construct does not reach the compiler's"
        " decision point at all, so a further spelling of it cannot either."
        " Record the axis as measured-dead with these"
        f" {streak} probed forms, or move to a different mechanism or"
        " function boundary." + tail
    )


def neutral_identical_proof_line(unit, fn, digest, real, insns,
                                 multiset_tokens, head=None):
    """The machine-readable NEUTRAL-IDENTICAL line a record can cite.

    Run-39 item 6. A deliberate A/B that returns a BYTE-IDENTICAL object is
    POSITIVE evidence — it proves the two constructs sit in different
    allocator classes, because a within-class reorder always moves
    something. MV used two of them as the proofs behind
    claim.law.MV_callee-saved-numbering-has-a-width-class-ahead-of-
    declaration-order.20260902.v1, and had to transcribe the finding as
    prose because probe emitted nothing quotable: the annotation named no
    unit, no function, no object digest and no commit, so a reader could not
    tell WHICH bytes were identical or WHERE.

    Deliberately has no timestamp. Every field is reproducible from the
    named commit, so two runs of the same A/B emit the same line and a
    record citing it can be re-verified rather than merely believed.
    """
    fields = [
        f"unit={unit}", f"fn={fn}",
        f"bytes={digest or 'unmeasured'}",
        f"real={real}",
        f"insns={insns or 'unmeasured'}",
        f"multiset={multiset_tokens}t" if multiset_tokens is not None
        else "multiset=unmeasured",
        f"head={head[:9] if head else 'unknown'}",
    ]
    return "NEUTRAL-IDENTICAL-PROOF " + " ".join(fields)


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
                            " compiler's decision point."
                            " FOR A DELIBERATE A/B IT IS POSITIVE EVIDENCE:"
                            " if you REORDERED or RETYPED two declarations"
                            " and the object did not move, the two are in"
                            " different allocator classes and no further"
                            " reordering will ever move them past each other"
                            " — that is a proved class BOUNDARY, and MV"
                            " recorded two of them as the proofs behind"
                            " claim.law.MV_callee-saved-numbering-has-a-"
                            "width-class-ahead-of-declaration-order"
                            ".20260902.v1. The PROOF line below is emitted"
                            " for citation]")
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
        head_bytes_now = shown.stdout
        whole_file = "--whole-file" in sys.argv
        scoped = "--function" in sys.argv
        counts = None
        if not whole_file:
            try:
                counts = restore_scope_counts(
                    head_bytes_now.decode("latin-1"),
                    source.read_bytes().decode("latin-1"), fn)
            except ValueError:
                counts = None
        if counts is None and not whole_file:
            print(f"[--discard: cannot locate {fn} on both sides, so the"
                  " scope of this restore is UNMEASURED — falling back to"
                  " whole-file. Run `git diff` first if other functions in"
                  " this TU carry uncommitted work.]")
        elif counts is not None:
            inside, outside, entangled = counts
            if entangled and not scoped:
                print(discard_refusal(fn, unit, inside, outside, entangled))
                return 1
            if scoped:
                straddling = [row for row in entangled
                              if row[0] == "straddling"]
                if straddling:
                    print(discard_refusal(fn, unit, inside, outside,
                                          entangled))
                    return 1
                try:
                    new_text, notes = scoped_revert(
                        head_bytes_now.decode("latin-1"),
                        source.read_bytes().decode("latin-1"), fn)
                except ValueError as error:
                    print(f"cannot discard --function: {error}")
                    return 1
                source.write_bytes(new_text.encode("latin-1"))
                print(f"discarded (function-scoped): {source} — {notes}")
                restore_transient_pins(unit)
                warn_outside_edits(source, None)
                if "--no-rebuild" not in sys.argv:
                    rebuild_after_restore(unit, "--discard --function")
                return 0
        source.write_bytes(head_bytes_now)
        print(f"discarded: {source} restored to HEAD (whole file —"
              " uncommitted work on other functions in this TU is gone)")
        restore_transient_pins(unit)
        # Even a whole-file discard leaves HEADER edits live (run 34 item 3).
        warn_outside_edits(source, None)
        if "--no-rebuild" not in sys.argv:
            rebuild_after_restore(unit, "--discard")
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
        if "--no-rebuild" not in sys.argv:
            rebuild_after_restore(unit, "--revert-baseline")
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
        # Roll the BEST anchor back with the source (run-38 item 9), before
        # the re-score below reads the state file. Without it the restored
        # state is scored against the session's best — which after a
        # --no-bank diagnostic is the edit this revert just discarded, and
        # the successful revert prints "[revert advised]".
        if state_file.exists():
            try:
                reverted_state = json.loads(
                    state_file.read_text(encoding="utf-8"))
            except ValueError:
                reverted_state = None
            if isinstance(reverted_state, dict):
                reverted_state, note = roll_back_anchor(reverted_state)
                if note:
                    print(note)
                state_file.write_text(json.dumps(reverted_state),
                                      encoding="utf-8")
        # A FUNCTION-SCOPED revert reaches only hunks inside `fn`. Cross-check
        # the whole tree and name what it could not reach — MV's
        # volatile-in-a-macro header edit survived a revert and stayed live.
        warn_outside_edits(source, None if "--whole-file" in sys.argv else fn)

    # --raw BUILDS THE RAW OBJECT (run-39 item 10). Building the
    # postprocessed object here made --raw unusable in the one case it
    # exists for: a pin your own upstream edit made stale aborts the
    # WEBFRANK edge, so the escape hatch died on the thing it was escaping.
    raw = "--raw" in sys.argv
    object_target = (raw_object_target(unit) if raw
                     else f"build/{VERSION}/src/{unit}.o")
    if raw:
        print(f"[--raw: building {object_target} — the compiler's own"
              " output, WITHOUT driving the WEBFRANK edge, so a stale pin"
              " cannot block this score]")
    build = subprocess.run(
        ["ninja", object_target], capture_output=True, text=True,
    )
    if build.returncode != 0:
        print("BUILD FAILED:")
        print((build.stdout + build.stderr).strip()[-1500:])
        return 1

    raw_flag = ["--raw"] if raw else []
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
    # Both digests read the object that was actually SCORED, so under --raw
    # the re-score guard and NEUTRAL-IDENTICAL describe the same bytes the
    # verdict does.
    scored_object = object_target if raw else None
    digest = object_digest(unit, fn, fn_stripped, scored_object)
    prev_data = state.get("last_data")
    data = data_digest(unit, scored_object)
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
        # PURE READOUT. No verdict, and no state mutation beyond the fuzzy
        # number itself. Arbitration must be able to re-read a state the
        # loop has already scored without the verdict changing underneath
        # it (a CONFLICT re-read as REGRESSED that way, on bytes that had
        # not moved).
        #
        # FIRST-BASELINE TRAP, --fuzzy edition (run-38 item 4). Banking
        # nothing is right whenever a revert point already EXISTS — moving
        # it under an arbitration is the hazard this branch was built to
        # avoid. On a function nobody has probed yet there is nothing to
        # move: the state was just built and scored, the pristine bytes
        # are in front of us, and refusing to bank left `--revert`
        # answering "no banked snapshot for this unit yet" over a build
        # already paid for. Bank ONLY in that case; still no verdict, and
        # the BEST anchor is still untouched.
        first_bank = snap is not None and readout_banks_baseline(
            snap.exists(), source is not None, "--no-bank" in sys.argv)
        tok = (f", multiset {multiset_tokens}t"
               if multiset_tokens is not None else "")
        banked_note = ("no verdict computed; no revert point existed, so"
                       " this readout banked one" if first_bank else
                       "no verdict computed, no revert point banked")
        print(f"READOUT   real {real} (insns {insns}{tok})"
              f"  [--fuzzy: {banked_note}]")
        standing = state.get("last_verdict")
        if standing:
            print(f"[standing verdict, unchanged by this readout]"
                  f"\n{standing}")
        if first_bank:
            if bank_snapshot(unit, source, baseline=True,
                             verdict_kind="BASELINE", fn=fn):
                state["baseline_real"] = real
                state["baseline_insns"] = insns
                if multiset_tokens is not None:
                    state["baseline_multiset"] = multiset_tokens
            print("[banked from the CURRENT state — still no verdict and no"
                  " BEST anchor. --revert / --revert-baseline now restore"
                  " THIS state; --no-bank opts out.]")
        fuzzy_readout(unit, fn, fn_stripped, state, state_file, digest=digest)
        return 0

    rebase_best = "--rebase-best" in sys.argv
    accept_fuzzy_loss = "--accept-fuzzy-loss" in sys.argv
    # The pre-verdict state, kept so the fuzzy gate below can re-run the
    # classification from the SAME inputs. Re-classifying from the state
    # classify() already returned would compare `real` against a best it
    # had just banked and read every improvement as NEUTRAL.
    state_before = dict(state)
    verdict, state = classify(state, real, insns, multiset_tokens,
                              rebase_best=rebase_best,
                              digest=digest, source_changed=source_changed,
                              fuzzy=cached_fuzzy,
                              accept_fuzzy_loss=accept_fuzzy_loss)
    # FRESH FUZZY BEFORE THE BANK (run-35 item 1). The verdict above is
    # provisional whenever it would move the BEST anchor: a real+multiset
    # win can still be a fuzzy LOSS, and banking one poisons every later
    # verdict on the function. Spend the report build here — on the
    # verdicts that actually change the high-water mark — rather than
    # discovering it two probes later. Only --no-fuzzy-gate opts out.
    #
    # --rebase-best USED TO BE EXEMPT HERE (run-40 item 2). It was the one
    # door left open, and run 39 walked through it: a CONFLICT-shaped keep
    # banked at fuzzy 96.8433 against a 97.2692 baseline, with no fuzzy
    # measured at all, and the two probes after it read as REGRESSED. A
    # flag that declares an arbitration is not an arbitration.
    if (banks_best(verdict) and cached_fuzzy is None
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
                                  rebase_best=rebase_best, digest=digest,
                                  source_changed=source_changed,
                                  fuzzy=fresh,
                                  accept_fuzzy_loss=accept_fuzzy_loss)
        if fresh is not None:
            cached_fuzzy = fresh
            state["last_fuzzy"] = fresh
            if digest is not None:
                state["last_fuzzy_bytes"] = digest
    # THE TU-SCOPE BANK GATE (run-39 item 1). The fuzzy gate above closed the
    # "this function's other metric disagrees" hole; this closes the
    # "this function is not the only function in the object" hole. A
    # file-scope declaration, storage class, extern or pool change moves
    # SIBLING bytes, and every instrument in this loop scores one function's
    # .text. Measured on the reproduction above: `IMPROVED real 840 -> 838
    # [best updated]` on a one-word edit that demoted nine byte-exact
    # siblings. Detected from the DIFF first, so a body-only edit — nearly
    # every probe — pays nothing at all.
    if (banks_best(verdict) and not rebase_best
            and "--no-tu-gate" not in sys.argv and source is not None):
        committed = head_bytes(source)
        scope_changes = tu_scope_changes(
            None if committed is None else committed.decode("latin-1"),
            source.read_bytes().decode("latin-1"))
        if scope_changes:
            print(f"[TU-scope gate: {len(scope_changes)} file-scope"
                  " change(s) in this diff — cross-checking the whole TU"
                  " against its defake_gate baseline (no build;"
                  " --no-tu-gate skips it)]")
            verdicts, note = tu_sibling_regressions(unit)
            strict, other = (None, None)
            if verdicts is not None:
                strict, other = sibling_strict_losses(
                    verdicts, {fn, fn_stripped})
            verdict, state = apply_tu_scope_gate(
                verdict, state, {key: state_before.get(key)
                                 for key in BEST_KEYS},
                scope_changes, strict, other, note, unit, fn)
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
    # The slot arbiter's signal is needed BEFORE the replan hint (run-42
    # item 5): the hint's prescription is class-conditional and the
    # frame/slot class is where the old unconditional one pointed the wrong
    # way. Computed once here on exactly the condition the map itself is
    # gated on, then reused for printing further down — no second slotdiff.
    slots_output, slots_fire, slots_reason = None, False, ""
    if real > 0 and "--no-slots" not in sys.argv:
        slots_output = run_slot_arbiter(unit, fn)
        slots_fire, slots_reason = slot_arbiter_signal(slots_output)
    hint = replan_hint(streak, slot_class=slots_fire)
    if hint:
        verdict += "\n" + hint
        state["last_verdict"] = verdict
    state_file.write_text(json.dumps(state), encoding="utf-8")
    # CATEGORICAL FIRST (run-40 item 8). A count-parity change decides which
    # postprocessor classes exist for this function at all; `real` and fuzzy
    # cannot express that, and printing it under them let a class change
    # read as an ordinary numeric move.
    if state.get("count_class"):
        print(state["count_class"])
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
    # The citable half of a byte-identical A/B (run-39 item 6). A deliberate
    # reorder/retype that leaves the object unchanged PROVES a class
    # boundary, and the prose annotation named neither the bytes nor the
    # commit, so MV had to transcribe its two proofs by hand.
    if "NEUTRAL-IDENTICAL" in verdict:
        print(neutral_identical_proof_line(
            unit, fn, digest, real, insns, multiset_tokens, git_head()))
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
    if slots_fire or ("--slots" in sys.argv and slots_output):
        print(slot_arbiter_header(
            slots_reason or "requested with --slots (no decisive slot"
                            " signal)"))
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
        created_baseline = bank_snapshot(
            unit, source,
            baseline=verdict.startswith("BASELINE"),
            verdict_kind=kind, fn=fn,
            rebaseline="--rebaseline" in sys.argv)
        # The SESSION BASELINE's own scores, banked at exactly the moment
        # the baseline FILE is written, so later negatives can print where
        # the session started instead of only where the rolling `best` is
        # (run-40 item 2). Nothing else writes these keys, so --rebaseline
        # moving the file moves the numbers with it.
        if created_baseline:
            state["baseline_real"] = real
            state["baseline_insns"] = insns
            if multiset_tokens is not None:
                state["baseline_multiset"] = multiset_tokens
            if cached_fuzzy is not None:
                state["baseline_fuzzy"] = cached_fuzzy
        # Name WHICH verdict banked: the discard path differs (a banked
        # NEUTRAL means --revert restores the PROBE, so discarding it
        # needs git), and the identical banner hid that twice.
        print(f"[revert point banked ({kind}): probe.py {unit} {fn}"
              " --revert restores THIS state"
              + ("; to discard this neutral edit use git, not --revert"
                 if kind == "NEUTRAL" else "") + "]")
        # The KEEP end of the transient-pin A/B (run-38 item 5). The revert
        # end already consumes the bank; this one did not, so a bank
        # outlived the A/B it described and the NEXT revert in the TU
        # restored pre-session pin hashes over a kept re-derivation.
        #
        # NOT on a revert invocation: --revert/--revert-baseline/--discard
        # re-score through this same path, and their own consumer
        # (restore_transient) deliberately KEEPS the bank when it emitted
        # notes ("resolve these, then delete it"). Dropping it here would
        # overrule that instruction.
        if keep_consumes_transient_bank(sys.argv):
            drop_transient_pins(unit, f"{kind} keep")
        # Record the BEST anchor that goes WITH these bytes (run-38 item 9).
        # --revert restores the source; without this it cannot restore the
        # anchor, and the restored state gets scored against whatever the
        # session's best was — after a --no-bank diagnostic, the very edit
        # the revert discarded.
        state[SNAPSHOT_ANCHOR] = anchor_of(state)
        state_file.write_text(json.dumps(state), encoding="utf-8")
    elif source is not None and "--no-bank" in sys.argv:
        print("[--no-bank: snapshot NOT updated — hand-revert this edit]")

    # A failed probe almost always needs the ops view next — print it
    # unasked (the multiset pass above already fetched it).
    printed_ops = False
    if (verdict.startswith(("REGRESSED", "CONFLICT", "FUZZY-REGRESSED",
                            "REBASE-REFUSED"))
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
