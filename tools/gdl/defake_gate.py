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
  python tools/gdl/defake_gate.py check game/enemy/enemy.c --arbiter fuzzy
      (measure the fuzzy a real-growth REGRESSION would override when the
      genuine structural rows are FLAT — see the FUZZY ARBITER below)
  python tools/gdl/defake_gate.py check game/x/y.c --bank-arbitrated=<fn>
      (accept a fuzzy-arbitrated keep for ONE function without
      re-anchoring any sibling — the mandate-correct way to bank a
      keep that `real` reads as a regression)
  python tools/gdl/defake_gate.py check game/audio/sndfx.c,game/ui/attract.c --rebuild
  python tools/gdl/defake_gate.py arbitrations [game/enemy/enemy.c]
      (counts and the override RATE from the arbitration log)
  python tools/gdl/defake_gate.py roster game/enemy/critter [--rebuild]
      (the whole per-function sweep in ONE call: status, insn counts,
      `real` with its delta against the gate baseline, genuine structural
      rows, fndiff --clean's verdict, fuzzy, the SLOT shape, and which rows
      are WebFrank-PINNED. Every one of those numbers was already computed
      here to take a baseline and reachable no other way, so a lane
      wanting the per-function view ran fndiff once per function instead
      — 15 subprocess calls for one mandated sweep. Rows sort by `real`
      descending with pinned rows last, because a pinned function reads
      real 0 by construction and is not open work. The SLOT column names a
      save-set, frame-size or exclusive-slot delta and costs two object
      parses for the WHOLE unit; `real` actively FIGHTS those rows, so they
      are arbitrated on `slotdiff.py`, not on the roster's own ranking.
      Calibrated 2026-09-03 over 256 units / 3032 functions: 58 of 353 open
      rows flagged (16.4%) and zero closed rows.)

ONE DECISION, TWO SPELLINGS (run-43 item 6). `--arbitrate` here is the
ACCEPT: it takes a checked CONFLICT and passes the gate. probe.py's accept
word is `--rebase-best`, and this gate now takes that spelling too, so a
lane alternating between the two tools inside one loop cannot type the
wrong one. WATCH THE OTHER DIRECTION: `probe.py --arbitrate` is NOT an
accept at all — it is a MEASUREMENT that builds both states, prints their
(real, fuzzy) pairs and banks nothing. One word, two meanings; probe's
readout now says so in its own header, and probe does NOT alias
`--arbitrate` to its accept.

A CONFLICT this call accepts prints as `ARBITRATED`, not as `CONFLICT ...
(pass --arbitrate to accept)` above a "GATE OK (arbitrated)" line telling
you it was accepted. The verdict tuples are unchanged.

IMPORTABLE CORE: snapshot, compare, verdict_row, arbitrate_regressions,
parse_clean, roster_rows, format_roster, load_baseline, normalize_unit,
arbitration_event, summarize_arbitrations — pure over parsed tool output
and baseline dicts; no build and no printing at import (run-43 item 10;
the convention is documented in AGENTS.md).

EVERY ARBITRATION IS LOGGED. `--arbitrate` keeps, `--bank-arbitrated` row
re-anchors, and refused CONFLICTs all append one json line to
build/GUNE5D/gate/arbitrations.jsonl carrying the unit, the commit, the
arbiter used and the full verdict detail (real delta, genuine rows, fuzzy
note) behind the decision. Refusals are logged as well as keeps ON PURPOSE:
with keeps alone the log counts overrides but cannot form a rate, and the
override ratios this project quotes about itself were previously asserted
from memory with nothing on disk to check them against.

A comma-separated unit list gates every named TU in one call (exit code =
worst) — use it for paired fixes (a signature change plus its callers) so
the second TU can never be forgotten.

RELOCATION CHANGES CARRY A DIRECTION. A changed relocation symbol at
unchanged instruction words is a semantic change and stays loud, but the
check no longer scores it OURS-VS-OURS: the new symbol is resolved against
the TARGET object's relocation at that instruction, and the verdict is
RELOC-TOWARD-TARGET (a repair — passes the gate, bank it with
--update-improved) or REGRESSION/MOVED-AWAY-FROM-TARGET. Without a target
object, or when neither symbol matches the target's, it is
REGRESSION/DIRECTION-UNKNOWN — fail-closed, as before. The old check printed
one "revert or fix before committing" for both directions and told three
run-37 lanes to revert genuine fixes
(claim.law.RS_defake-gate-wrong-callee-check-is-ours-vs-ours-...20260902.v1).

DATUMS ARE RESOLVED BEFORE NAMES (run-48 item 2). That direction check
compared SYMBOL NAMES, and an MWCC anonymous pool entry (`@433`, `@1058`) is
not in config/GUNE5D/symbols.txt, so it resolved to nothing and every
named -> anonymous row read as MOVED-AWAY-FROM-TARGET — for a relocation
pointing at the SAME CONSTANT under a compiler-private name. Measured on
game/world/items::DistanceToClosestPlayer, where `fndiff --relocs` printed
`relocation sets IDENTICAL (16 reloc(s), addresses resolved)` and exited 0
against the gate's exit 1 (claim.law.PR_defake-gate-compares-relocation-
names-while-fndiff-relocs-resolves-them.20260903.v1), and the integrator had
to rule the keep in by hand. A named-versus-anonymous row is now decided by
what the two entries HOLD, using fndiff's own kind-equal-value classifier:
equal bytes -> RELOC-DATUM-EQUAL (passes, bank with --update-improved);
DIFFERING bytes -> REGRESSION, which the name comparison could only call
DIRECTION-UNKNOWN. Two CONCRETE addresses are still compared by address —
same value at two addresses is two different data objects.

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

`--at-head` RESTORES THE SOURCE ONLY — it does NOT restore
config/GUNE5D/webfrank.json, which is global state pairing with exactly one
source state. Taken while this unit's rules are uncommitted, the baseline is
HEAD's source measured against the TREE's pins (no commit's state), and when
the drift is a re-derived permutation pin the WEBFRANK stage hash-asserts and
the build aborts naming the PIN rather than the flag: WS spent two builds
there. It now SAYS SO before spending the build, scoped to this unit's rules.
Restoring the config was rejected deliberately — it is shared with every
concurrent lane, so a write would race them, and a rule change needs
`python configure.py` re-run before its build edge exists at all.

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

The third route is the FUZZY ARBITER (`--arbiter fuzzy`), for the case the
structure arbiter is silent about: real rose while the genuine structural
rows held FLAT. Flat rows mean regnorm sees the same amount of genuine
structure, so `real` alone decides -- and every keep of that shape had to be
overridden by hand against a fuzzy the gate never printed. --arbiter fuzzy
regenerates build/GUNE5D/report.json (the discipline-3 "fresh report" rule,
never a stale read) and reports the per-function fuzzy delta the REGRESSION
would override: a RISING fuzzy becomes a CONFLICT, a falling or flat one
stays a REGRESSION with real and fuzzy explicitly agreeing. The delta is
PRINTED in every arbitrated case, so the number behind a manual override is
in the gate output instead of a separate command. Baselines taken before
this item carry no fuzzy anchor and say so.
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
import slotdiff  # noqa: E402  (the roster's SLOT column: frame/slot shape)

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


SECTION_HEAD_RE = re.compile(r"^Contents of section (\S+):$")
# dtk/MWCC name the EABI exception tables WITHOUT a leading dot in these
# objects (`extab`/`extabindex`, confirmed by `objdump -s` on memcard.o);
# the dotted spellings are kept for portability across other toolchains.
EH_SECTIONS = ("extab", "extabindex", ".extab", ".extabindex",
               ".eh_frame", ".gcc_except_table")


def parse_section_digests(dump):
    """{section: sha1[:12]} over an `objdump -s` dump, .text* excluded.

    Pure text -> dict so the classification is testable without an object
    file or a toolchain.
    """
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
    return {n: h for n, h in out.items() if not n.startswith(".text")}


# The chunked form datadiff.py has used since run 34, not a looser one: the
# ASCII gutter objdump prints after the hex can itself look like hex ("abcd"),
# and only the fixed 8-or-9-char chunking keeps it out of the bytes.
HEX_ROW_RE = re.compile(r"^ ([0-9a-f]+) ((?:[0-9a-f ]{8,9}){1,4})")


def parse_section_bytes(dump):
    """{section: bytes} over an `objdump -s` dump, .text* excluded.

    Pure text -> dict, so the byte accounting below is testable without an
    object file or a toolchain.
    """
    out, cur = {}, None
    for line in dump.splitlines():
        head = SECTION_HEAD_RE.match(line.strip())
        if head:
            cur = head.group(1)
            out[cur] = bytearray()
            continue
        if cur is None:
            continue
        row = HEX_ROW_RE.match(line)
        if row:
            hexpart = row.group(2).replace(" ", "")
            if len(hexpart) % 2:
                hexpart = hexpart[:-1]
            out[cur] += bytes.fromhex(hexpart)
    return {n: bytes(b) for n, b in out.items() if not n.startswith(".text")}


# Sections that are not MATCHED DATA and must stay out of the accounting.
# `.comment` is the toolchain's own version string and `.note.split` is dtk's
# split annotation, present only on the target side: measured on
# zlib/inflate.c they contributed 292/316 and 0/244 to an otherwise 495/500
# picture, which is the kind of noise that trains a reader to ignore a number.
def is_scored_data(name):
    return not (name == ".comment" or name.startswith(".note"))


def matched_data_bytes(ours, target):
    """POSITIONAL byte agreement per section: {section: (matched, size)}.

    `size` is the TARGET section's length, so a section our object does not
    emit at all counts its whole size as lost rather than vanishing from the
    accounting. Sections only WE emit contribute 0/0 and show up as an
    unclaimed-bytes row instead of silently scoring 100%.

    THIS IS THE DIAGNOSTIC NUMBER, NOT THE SCORE (run-48 item 3). It counts
    every byte that happens to agree, and the image's Data measure does not
    work that way — see `image_matched_data_bytes`, which is what the gate
    prices against.
    """
    out = {}
    for name in sorted(set(ours) | set(target)):
        a, b = ours.get(name, b""), target.get(name, b"")
        out[name] = (sum(1 for x, y in zip(a, b) if x == y), len(b))
    return out


def image_matched_data_bytes(ours, target):
    """The IMAGE's Data accounting: {section: (matched, size)}, ALL-OR-NOTHING.

    THE DEFECT (run-48 item 3, the second DATA blindness). The gate priced a
    data change by NETTING per-section byte agreement, and the project's Data
    measure — the PROGRESS `Data:` line, read from build/GUNE5D/report.json —
    counts a data section as matched only when it matches ENTIRELY. A section
    that is one byte wrong contributes its FULL size here and ZERO there, so a
    keep that breaks a section reads NET +0 / GATE OK while the image loses
    the whole section.

    Reproduced at cdfff02e2 on game/ui/btext, whose extabindex is one byte off:

        GATE per-byte accounting     619/620
        report.json main/game/ui/btext   matched_data=248  total_data=620
        all-or-nothing recomputation 248/620   <- agrees with report.json

    So the gate's own baseline printout overstated matched Data by 371 of 620
    bytes on that one TU, and a change that destroyed the 248-byte extab match
    while repairing a single byte of extabindex would have netted +1 here
    against −248 in the image.

    CALIBRATED TWO-SIDED at cdfff02e2 over all 168 unit pairs this gate can
    price (T18_scratch/t18_calib_item3.py):
      POSITIVES  91 units / 134 sections where the two accountings DISAGREE
                 — the per-byte form overstated matched Data by 30,920 bytes
                 in total, led by game/sound/sounds_evt (2,173 -> 0),
                 game/anim/action (2,340 -> 220) and game/game/player
                 (1,386 -> 0).
      NEGATIVES  77 units / 326 sections where the two are identical, so the
                 change is inert for them.
    VALIDATED against report.json's own `matched_data`, restricted to the 129
    units whose section SET this gate fully covers (objdump -s cannot dump a
    bss-family section, and the report does not count `.init` as data — the
    other 39 units are excluded because that measures COVERAGE, not
    accounting):
      110 units  reproduce report.json EXACTLY
       18 units  UNDER-count it (objdiff credits data at symbol granularity
                 and resolves relocations, so a section holding a pointer
                 table can be credited there while its raw bytes differ here)
        1 unit   OVER-counts it, Runtime.PPCEABI.H/NMWException, by 16 bytes
    So this is a LOWER BOUND on the image's measure, not a reproduction of
    it, and that is the direction a gate needs: it under-credits rather than
    manufacturing a pass. The per-byte form is an UPPER bound and did exactly
    the opposite.

    Pure over two {section: bytes} maps, like the positional form beside it.
    A section we do not emit at all, or emit at a different length, cannot
    match: equality is over the WHOLE target section.
    """
    out = {}
    for name in sorted(set(ours) | set(target)):
        a, b = ours.get(name, b""), target.get(name, b"")
        out[name] = (len(b) if b and a == b else 0, len(b))
    return out


def data_section_digests(objfile, targetfile=None):
    """Per-section state for the DATA baseline: sha, size, matched bytes.

    The per-TU DATA baseline (run 34 item 1). defake_gate scores only .text
    (fndiff over the instruction stream), so a source change that widens a
    function's callee-saved save area silently DESTROYS its TU's .extab
    match while every per-function verdict IMPROVES
    (claim.law.WS_frame-widening-silently-breaks-the-tus-extab-match). None
    when the object or objdump is unavailable — a missing measurement never
    manufactures a verdict.

    Run-46 item 3. The run-34 form banked a DIGEST per section, so `check`
    could say "something moved" and then hand the lane a prose instruction to
    go arbitrate by hand. A keep that destroyed 144 bytes of matched Data
    passed every text arbiter that way. A digest cannot be subtracted; a byte
    count can, so the baseline now banks the count too and `check` prints the
    DELTA. The digest is kept for change DETECTION (it catches a permutation
    that preserves the matched-byte count).
    """
    if not objfile.exists():
        return None
    dump = subprocess.run([str(fndiff.OBJDUMP), "-s", str(objfile)],
                          capture_output=True, text=True)
    if dump.returncode != 0:
        return None
    digests = {n: d for n, d in parse_section_digests(dump.stdout).items()
               if is_scored_data(n)}
    ours = {n: b for n, b in parse_section_bytes(dump.stdout).items()
            if is_scored_data(n)}
    target = {}
    if targetfile is not None and Path(targetfile).exists():
        tdump = subprocess.run([str(fndiff.OBJDUMP), "-s", str(targetfile)],
                               capture_output=True, text=True)
        if tdump.returncode == 0:
            target = {n: b for n, b in
                      parse_section_bytes(tdump.stdout).items()
                      if is_scored_data(n)}
    # TWO accountings, banked side by side (run-48 item 3). `matched_image`
    # is the one the gate PRICES on, because it is the one the project's
    # Data measure uses; `matched` stays the positional diagnostic, and
    # keeping the old key name is what lets a pre-run-48 baseline still be
    # recognised — and LABELLED — as positional rather than silently
    # subtracted from an image number.
    scores = matched_data_bytes(ours, target) if target else {}
    image = image_matched_data_bytes(ours, target) if target else {}
    out = {}
    for name, sha in digests.items():
        row = {"sha": sha, "size": len(ours.get(name, b""))}
        if name in scores:
            row["matched"], row["target_size"] = scores[name]
        if name in image:
            row["matched_image"] = image[name][0]
        out[name] = row
    for name, (matched, size) in scores.items():
        if name not in out and size:
            out[name] = {"sha": None, "size": 0,
                         "matched": matched, "target_size": size,
                         "matched_image": image.get(name, (0, size))[0]}
    return out


def _section_row(entry):
    """(sha, matched, target_size, basis) from any baseline format.

    Pre-run-46 baselines banked a bare digest string; those still detect a
    change, they just cannot price it.

    `basis` (run-48 item 3) says WHICH accounting `matched` is in:
    ``"image"`` = all-or-nothing per section, the same rule the PROGRESS
    `Data:` line uses, and the only one a NET may be quoted from;
    ``"positional"`` = the per-byte count a pre-run-48 baseline banked, which
    OVERSTATES matched Data (619/620 versus the image's 248/620 on
    game/ui/btext) and must never be silently subtracted from an image
    number. ``None`` = unpriced.
    """
    if isinstance(entry, str):
        return entry, None, None, None
    if isinstance(entry, dict):
        if entry.get("matched_image") is not None:
            return (entry.get("sha"), entry.get("matched_image"),
                    entry.get("target_size"), "image")
        if entry.get("matched") is not None:
            return (entry.get("sha"), entry.get("matched"),
                    entry.get("target_size"), "positional")
        return entry.get("sha"), None, entry.get("target_size"), None
    return None, None, None, None


def _positional_matched(entry):
    """The diagnostic per-byte count banked beside the image one, or None."""
    if isinstance(entry, dict):
        return entry.get("matched")
    return None


def data_section_verdicts(base_entry, cur_entry):
    """DATA-CHANGED verdict rows for moved non-text sections, or [].

    Its own verdict class: score-invisible to every per-function row, so it
    neither passes nor fails the gate on its own. When BOTH sides carry the
    run-46 byte accounting it now prices the change in matched DATA BYTES
    per section plus a net — the number the lane would otherwise have gone
    to a full `ninja` PROGRESS 'Data:' line to get. None on either side (a
    baseline taken before this feature, or an unmeasured object) yields no
    row rather than a false alarm.
    """
    base = (base_entry or {}).get("data")
    cur = (cur_entry or {}).get("data")
    if not isinstance(base, dict) or not isinstance(cur, dict):
        return []
    # DETECTION stays strictly more sensitive than PRICING: the digest, the
    # image count and the positional count are all compared, so a change
    # that moves only one of the three still produces a row.
    moved = sorted(n for n in set(base) | set(cur)
                   if _section_row(base.get(n))[0]
                   != _section_row(cur.get(n))[0]
                   or _section_row(base.get(n))[1]
                   != _section_row(cur.get(n))[1]
                   or _positional_matched(base.get(n))
                   != _positional_matched(cur.get(n)))
    if not moved:
        return []
    eh = [n for n in moved if n in EH_SECTIONS]
    priced, net, unpriced, positional = [], 0, [], []
    for name in moved:
        _, bm, bt, b_basis = _section_row(base.get(name))
        _, cm, ct, c_basis = _section_row(cur.get(name))
        if bm is None or cm is None:
            unpriced.append(name)
            continue
        if "positional" in (b_basis, c_basis):
            positional.append(name)
        delta = cm - bm
        net += delta
        row = (f"{name} {bm}->{cm} of {ct if ct is not None else bt}"
               f" ({delta:+d} B)")
        # The per-byte numbers, as a SECOND column when they disagree with
        # the image's: a section can lose its whole match while nearly every
        # byte still agrees, and that is the shape a lane must recognise.
        bp, cp = _positional_matched(base.get(name)), \
            _positional_matched(cur.get(name))
        if (bp is not None and cp is not None
                and (bp, cp) != (bm, cm)):
            row += f" [per-byte {bp}->{cp}, diagnostic only]"
        priced.append(row)
    if priced:
        detail = ("matched DATA bytes: " + "; ".join(priced)
                  + f". NET {net:+d} B of matched Data")
        if net < 0:
            detail += (" — THIS KEEP DESTROYS MATCHED DATA and every"
                       " per-function verdict here scores .text ONLY")
        elif net > 0:
            detail += " gained (invisible to every per-function verdict)"
        if positional:
            # RUN-48 ITEM 3. Mixing the two accountings is the defect, so a
            # NET computed from either side's positional number is LABELLED
            # rather than quoted as the image's.
            detail += ("; ACCOUNTING IS POSITIONAL for " + ", ".join(positional)
                       + " (a pre-run-48 baseline banked per-BYTE agreement,"
                         " which overstates matched Data — 619/620 against"
                         " the image's 248/620 on game/ui/btext — because the"
                         " PROGRESS `Data:` line counts a section as matched"
                         " only when it matches ENTIRELY). This NET is NOT"
                         " the image's number; re-take the baseline"
                         " (`defake_gate.py baseline <unit> --at-head`) to"
                         " price it all-or-nothing")
        else:
            detail += ("; accounting is ALL-OR-NOTHING per section, the same"
                       " rule the PROGRESS `Data:` line uses")
    else:
        detail = ("non-text section(s) " + ", ".join(moved) + " changed — every"
                  " per-function verdict here scores .text ONLY and is blind to"
                  " these bytes")
    if unpriced:
        detail += ("; unpriced section(s) " + ", ".join(unpriced)
                   + " (baseline predates the byte accounting — re-take"
                   " it to price them)")
    if eh:
        detail += ("; exception-table section(s) " + ", ".join(eh)
                   + " moved (the all-or-nothing extab-loss signature — a"
                   " widened callee-saved save area is the usual cause)")
    if not priced:
        detail += (". Arbitrate with a full `ninja` PROGRESS 'Data:'"
                   " comparison (or `datadiff.py <unit> --sections`) before"
                   " committing")
    return [("__sections__", "DATA-CHANGED", detail)]


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


_SYMBOL_TEXT_RE = re.compile(
    r"([A-Za-z_@.$][\w.$@]*)([+-]0x[0-9a-fA-F]+|[+-]\d+)?$")


def resolve_symbol(symbol):
    """Absolute address for a relocation symbol text, or None.

    fndiff.symbol_addresses() already registers the dtk `_80XXXXXX`
    address-suffix aliases, which is exactly the benign naming convention
    (`get_attn_pos` vs `get_attn_pos_8002C9A8`) this check must keep
    passing.
    """
    head = _SYMBOL_TEXT_RE.match((symbol or "").strip())
    if not head:
        return None
    base = fndiff.symbol_addresses().get(head.group(1))
    if base is None:
        return None
    try:
        return base + int(head.group(2) or "0", 0)
    except ValueError:
        return None


def naming_drift_is_benign(base_relocs, cur_relocs, resolve=None):
    """(benign, reason) for a relocation-symbol-only change.

    UNSOUND PREDECESSOR (claim.law.HV_defake-gate-naming-drift-is-a-false-
    benign-on-a-wrong-callee.20260901.v1): the gate called a change benign
    whenever the instruction WORDS were unchanged. In an unlinked object a
    REL24 `bl` word carries no target at all — the callee lives entirely in
    the relocation symbol — so that test is trivially true for ANY callee
    substitution. It passed a real wrong-callee bug in both directions:
    it would not have caught the bug going in, and it called the fix
    cosmetic.

    The distinguishing evidence is cheap and already on disk: a symbol
    change is benign only if both names RESOLVE TO THE SAME ADDRESS in
    config/GUNE5D/symbols.txt. Anything else — a differing address, an
    unresolvable name, a changed relocation type or count, or a baseline
    too old to carry the symbols — fails closed.
    """
    resolve = resolve or resolve_symbol
    if base_relocs is None or cur_relocs is None:
        return False, ("this baseline carries no relocation symbols (taken"
                       " before the wrong-callee fix), so a symbol change"
                       " cannot be proved benign — re-take the baseline")
    if len(base_relocs) != len(cur_relocs):
        return False, (f"relocation count {len(base_relocs)} ->"
                       f" {len(cur_relocs)}: a relocation was added or"
                       " removed, which is never a rename")
    for (base_type, base_sym), (cur_type, cur_sym) in zip(
            base_relocs, cur_relocs):
        if base_type != cur_type:
            return False, (f"relocation type {base_type} -> {cur_type} on"
                           f" {base_sym!r}: not a rename")
        if base_sym == cur_sym:
            continue
        base_at, cur_at = resolve(base_sym), resolve(cur_sym)
        if base_at is None or cur_at is None:
            unknown = base_sym if base_at is None else cur_sym
            return False, (f"relocation symbol {base_sym!r} -> {cur_sym!r}:"
                           f" {unknown!r} does not resolve in symbols.txt,"
                           " so identity cannot be established")
        if base_at != cur_at:
            return False, (f"relocation symbol {base_sym!r} (0x{base_at:08X})"
                           f" -> {cur_sym!r} (0x{cur_at:08X}) — DIFFERENT"
                           " addresses, i.e. a different callee/datum, not a"
                           " rename; this is a semantic change")
    return True, "every changed relocation symbol resolves to one address"


def target_object(unit):
    return Path(f"build/{VERSION}/obj/{re.sub(r'[.](c|cpp)$', '', unit)}.o")


def target_relocation_symbols(unit):
    """{fn: [(reloc_type, symbol), ...]} from the TARGET object, or {}.

    The gate scores our object against OUR OWN earlier baseline, which is
    correct for every other verdict and backwards for exactly one: a
    relocation change has a DIRECTION, and the truth about which direction
    is right has been sitting unread in build/GUNE5D/obj/ the whole time
    (claim.law.RS_defake-gate-wrong-callee-check-is-ours-vs-ours-so-a-
    correction-toward-the-target-reads-as-a-regression.20260902.v1).
    Missing object -> {} -> the check keeps its old fail-closed behaviour;
    a missing measurement never manufactures a verdict.
    """
    obj = target_object(unit)
    if not obj.exists():
        return {}
    try:
        return fndiff.relocation_symbols(obj)
    except Exception:
        return {}


def _resolved_counts(rows, resolve):
    from collections import Counter
    return Counter(
        addr for addr in (resolve(sym) for _type, sym in (rows or []))
        if addr is not None)


def our_object(unit):
    """The object this gate scores — the same one `measure_unit` reads."""
    return Path(f"build/{VERSION}/src/{re.sub(r'[.](c|cpp)$', '', unit)}.o")


# "not supplied" must be distinguishable from "read, and there was nothing
# there": None is a REAL answer from both byte readers (an uninitialized
# section, an unknown symbol), and a default of None made the two the same
# argument — a test asserting the unreadable case silently exercised a live
# symbols.txt lookup instead.
UNREAD = object()


def pool_datum_direction(target_sym, cur_sym, ours_object,
                         target_bytes=UNREAD, ours_bytes=UNREAD):
    """(direction, detail) under the KIND-EQUAL-VALUE rule, or None.

    THE DEFECT (run-48 item 2, claim.law.PR_defake-gate-compares-relocation-
    names-while-fndiff-relocs-resolves-them.20260903.v1). `resolve_symbol`
    reads config/GUNE5D/symbols.txt, and an MWCC anonymous pool entry
    (`@433`, `@1058`) is not in it, so it resolves to None. The name
    comparison below then reads "our OLD symbol matched the target's, the new
    one does not" and returns `away` — a REGRESSION — for a relocation that
    points at the SAME CONSTANT under a compiler-private name. Reproduced at
    c8cdf216d by driving this function with the record's own symbols:

      _row_direction(13, 'sArrowFloorYOffset', '@433', ...) -> away
        "target reloc[13] is 'sArrowFloorYOffset' (0x80346FB0) — our OLD
         symbol matched it, the new one does not"

    while `fndiff --relocs` on the same body prints `relocation sets
    IDENTICAL (16 reloc(s), addresses resolved)` and exits 0, and
    `fndiff.target_datum_bytes('sArrowFloorYOffset')` is `3fe0000000000000`
    — the f64 0.5 our `@433` also holds. The integrator RULED that keep
    approved under the kind-equal-value rule (work_claim.apply-rulings
    .20260903.v1).

    THE RULE IS fndiff's, not a new one: `pool_row_findings` already
    classifies a named-vs-anonymous row by its BYTES — POOL-KIND-EQUAL when
    they agree (benign), WRONG-POOL-VALUE when they do not — and
    `_datum_prefix_equal` handles the granularity mismatch (dtk names a whole
    contiguous run with one `lbl_ADDR` symbol while we emit each literal as
    its own `@N`). This applies the same classifier to the direction check.

    SCOPE, deliberately narrow. Returns None — the name comparison decides
    exactly as before — unless at least one side is an ANONYMOUS pool entry.
    Two CONCRETE addresses holding the same value are still two different
    data objects (fndiff calls that WRONG-POOL-DATUM), and a bytes-equal
    escape there would let a wrong-datum bug through. Returns None as well
    when either side's bytes could not be read: that is fndiff's
    POOL-KIND-UNDECIDED, and an unread measurement must never manufacture a
    pass.

    CALIBRATED TWO-SIDED at c8cdf216d over all 257 built unit pairs, by
    running fndiff's own classifier (`pool_row_findings`) across the image —
    this is the class CENSUS, i.e. the population the rule is defined over,
    not a firing count (the direction check only runs when a symbol changed
    against the gate's baseline at unchanged instruction words):
      POOL-KIND-EQUAL       3,317 rows in 634 functions  -> now PASSES
      WRONG-POOL-VALUE         81 rows in  32 functions  -> still FAILS, now
                                                            with a VALUE
                                                            reason instead of
                                                            DIRECTION-UNKNOWN
      POOL-KIND-UNDECIDED       0 rows                   -> unchanged
      RENAME                   81 rows in  52 functions  -> untouched
      WRONG-POOL-DATUM         36 rows in  18 functions  -> untouched
      POOL-RENUMBER            24 rows in  10 functions  -> untouched
    The positive class reaches into the 100%-matched SDK — three of the first
    rows are dolphin/demo/DEMOInit::LoadMemInfo, which is byte-identical at
    real 0 — which is how thoroughly a NAME comparison misreads this shape.

    Pure over the two byte strings when they are supplied, so both verdicts
    are decided in a test without an object or a retail image.
    """
    if fndiff._symbol_kind(target_sym) == fndiff._symbol_kind(cur_sym):
        return None
    if "anon" not in (fndiff._symbol_kind(target_sym),
                      fndiff._symbol_kind(cur_sym)):
        return None
    if target_bytes is UNREAD:
        target_bytes = fndiff.target_datum_bytes(target_sym)
    if ours_bytes is UNREAD:
        ours_bytes = fndiff.ours_datum_bytes(cur_sym, ours_object)
    if not target_bytes or not ours_bytes:
        return None
    if fndiff._datum_prefix_equal(target_bytes, ours_bytes):
        return "datum-equal", (
            f"the target's {target_sym!r} and our {cur_sym!r} hold the SAME"
            f" datum ({fndiff._render_value(target_bytes)}) — a named-versus-"
            "anonymous pool spelling, not a moved relocation. `@N` is an MWCC"
            " compiler-private pool entry and is absent from symbols.txt, so"
            " the NAME comparison can only read it as a loss; the kind-equal-"
            "value rule (fndiff pool_row_findings: POOL-KIND-EQUAL) decides"
            " it by VALUE")
    return "away", (
        f"the target's {target_sym!r} holds"
        f" {fndiff._render_value(target_bytes)} while our {cur_sym!r} holds"
        f" {fndiff._render_value(ours_bytes)} — a named-versus-anonymous pool"
        " row whose BYTES DISAGREE (fndiff's WRONG-POOL-VALUE class). Our"
        " pool entry carries a different constant; this is a value defect,"
        " which the name comparison could only report as DIRECTION-UNKNOWN")


def _row_direction(index, base_sym, cur_sym, cur_relocs, target_relocs,
                   resolve, ours_object=None):
    """(direction, detail) for ONE changed relocation, judged vs target.

    Positional pairing first: `real 0` on both sides means the instruction
    stream already agrees with target, so relocation i on our side is
    relocation i on the target's. When the lists do not line up (differing
    length or type at i) it falls back to how many times each address is
    relocated in the target function at all — weaker, but still a fact
    about the TARGET rather than about our own history.
    """
    base_at, cur_at = resolve(base_sym), resolve(cur_sym)
    aligned = (len(target_relocs) == len(cur_relocs)
               and index < len(target_relocs)
               and target_relocs[index][0] == cur_relocs[index][0])
    if aligned:
        target_sym = target_relocs[index][1]
        target_at = resolve(target_sym)
        where = (f"target reloc[{index}] is {target_sym!r}"
                 + (f" (0x{target_at:08X})" if target_at is not None else
                    " (unresolvable)"))
        # DATUMS BEFORE NAMES (run-48 item 2). A named-versus-anonymous row
        # is decided by what the two entries HOLD; the name comparison
        # below cannot resolve an `@N` at all and reads every one of them
        # as a loss.
        by_value = pool_datum_direction(target_sym, cur_sym, ours_object)
        if by_value is not None:
            direction, detail = by_value
            return direction, f"{where} — {detail}"
        if target_at is None:
            return "unknown", (f"{where} — the target's own symbol does not"
                               " resolve, so direction is undecidable")
        if cur_at == target_at and base_at != target_at:
            return "toward", f"{where} — our new symbol MATCHES it"
        if base_at == target_at and cur_at != target_at:
            return "away", f"{where} — our OLD symbol matched it, the new one"\
                           " does not"
        return "unknown", (f"{where} — neither the old nor the new symbol"
                           " matches it")
    counts = _resolved_counts(target_relocs, resolve)
    n_base, n_cur = counts.get(base_at, 0), counts.get(cur_at, 0)
    where = (f"relocation lists do not line up positionally (target"
             f" {len(target_relocs)} vs ours {len(cur_relocs)}), so the"
             f" target's relocated-address counts decide: old address"
             f" {n_base}x, new address {n_cur}x in the target function")
    if n_cur > n_base:
        return "toward", where
    if n_cur < n_base:
        return "away", where
    return "unknown", where


def relocation_change_direction(base_relocs, cur_relocs, target_relocs,
                                resolve=None, ours_object=None):
    """('toward'|'datum-equal'|'away'|'unknown', detail) for a symbol change.

    Called only for a change `naming_drift_is_benign` already refused as a
    rename, i.e. one that genuinely re-points a call or a datum. `away`
    dominates: if any single relocation moved away from the target the
    whole change fails, no matter what the others did.

    `datum-equal` (run-48 item 2) is a PASSING direction: every changed row
    was decided by VALUE and none of them moved. It ranks below `toward`
    only in that a run containing both is reported as `toward` — a repair
    plus a re-spelling is a repair.
    """
    resolve = resolve or resolve_symbol
    if not target_relocs:
        return "unknown", ("no target relocation list available (build"
                           " build/GUNE5D/obj/<unit>.o), so the direction of"
                           " this change cannot be judged")
    if base_relocs is None or cur_relocs is None:
        return "unknown", "no baseline relocation symbols to compare"
    rows = []
    for index, ((base_type, base_sym), (_cur_type, cur_sym)) in enumerate(
            zip(base_relocs, cur_relocs)):
        if base_sym == cur_sym:
            continue
        direction, detail = _row_direction(
            index, base_sym, cur_sym, cur_relocs, target_relocs, resolve,
            ours_object=ours_object)
        rows.append((direction, f"{base_sym!r} -> {cur_sym!r}: {detail}"))
    if not rows:
        return "unknown", "no changed relocation symbol to judge"
    joined = "; ".join(detail for _direction, detail in rows)
    directions = {direction for direction, _detail in rows}
    if "away" in directions:
        return "away", joined
    if "toward" in directions and directions <= {"toward", "datum-equal"}:
        return "toward", joined
    if directions == {"datum-equal"}:
        return "datum-equal", joined
    return "unknown", joined


def compare(baseline, current, renames=None, resolve=None,
            target_relocs=None, ours_object=None):
    """Verdicts per function; regression = matched fell or real grew.

    ``renames`` maps old baseline names to new current names (--rename
    old=new): a deliberate symbol rename otherwise reads as vanished+new
    and fails the gate on a change that may have added exacts — a worker
    had to arbitrate a 3-exact rename by hand.

    ``target_relocs`` is {fn: [(reloc_type, symbol)]} from the TARGET
    object. With it, a relocation change that is not a rename is reported
    with its DIRECTION — RELOC-TOWARD-TARGET (a repair, which passes) or
    REGRESSION/MOVED-AWAY-FROM-TARGET — instead of one verdict for both.
    """
    renames = renames or {}
    target_relocs = target_relocs or {}
    verdicts = []
    # Per-TU DATA verdict class (run 34 item 1): a moved non-text section is
    # score-invisible to every per-function row below, so it is compared
    # separately and its reserved key skipped in the function loop.
    verdicts.extend(data_section_verdicts(baseline.get("__sections__"),
                                          current.get("__sections__")))
    for name, base in sorted(baseline.items()):
        if name == "__sections__":
            continue
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
                # Unchanged instruction words do NOT prove a rename: a
                # REL24 `bl` carries no target in an unlinked object. The
                # symbols must resolve to ONE address.
                ok, why = naming_drift_is_benign(
                    base.get("relocs"), cur.get("relocs"), resolve=resolve)
                if ok:
                    verdicts.append(
                        (name, "NAMING-DRIFT",
                         f"reloc lines renamed, instruction words unchanged"
                         f" and {why} — benign; re-baseline with"
                         " --update-improved when done")
                    )
                else:
                    # DIRECTION, against the target object. The detection
                    # above is right and valuable; what it could not do was
                    # tell a repair from a defect, so it printed
                    # "revert or fix" over three genuine run-37 fixes.
                    cur_name = renames.get(name, name)
                    direction, dwhy = relocation_change_direction(
                        base.get("relocs"), cur.get("relocs"),
                        target_relocs.get(cur_name), resolve=resolve,
                        ours_object=ours_object)
                    head = ("relocation symbols changed at unchanged"
                            f" instruction words — {why}.")
                    if direction == "toward":
                        verdicts.append(
                            (name, "RELOC-TOWARD-TARGET",
                             head + " MOVED-TOWARD-TARGET: " + dwhy
                             + " — this is a relocation REPAIR, not a"
                               " regression; keep it and re-anchor with"
                               " --update-improved")
                        )
                    elif direction == "datum-equal":
                        # RUN-48 ITEM 2. Every changed row was decided by
                        # VALUE and none of them moved: our relocation names
                        # an MWCC anonymous pool entry holding exactly the
                        # datum the target's symbol holds. This used to be a
                        # REGRESSION and an exit 1, and the integrator had to
                        # rule over it by hand.
                        verdicts.append(
                            (name, "RELOC-DATUM-EQUAL",
                             head + " KIND-EQUAL-VALUE: " + dwhy
                             + " — the relocation did NOT move; only the"
                               " pool SPELLING did. Not a regression; keep it"
                               " and re-anchor with --update-improved")
                        )
                    elif direction == "away":
                        verdicts.append(
                            (name, "REGRESSION",
                             head + " MOVED-AWAY-FROM-TARGET: " + dwhy
                             + " — revert or fix before committing")
                        )
                    else:
                        verdicts.append(
                            (name, "REGRESSION",
                             head + " DIRECTION-UNKNOWN: " + dwhy
                             + ". A REL24 callee lives ENTIRELY in its"
                               " relocation, so 'words unchanged' proves"
                               " nothing here — arbitrate against the"
                               " target (fndiff --relocs) before keeping")
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
    for name in sorted(set(current) - set(baseline) - renamed_targets
                       - {"__sections__"}):
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


REPORT = Path(f"build/{VERSION}/report.json")


def read_report_fuzzy(unit, report=None):
    """{fn: fuzzy%} for `unit` from an objdiff report, {} if unreadable.

    Read-only and freshness-blind by itself: `fresh_fuzzy` is what enforces
    the discipline-3 rule that a quoted fuzzy comes from a report generated
    AFTER the object under test.
    """
    path = Path(report) if report is not None else REPORT
    bare = re.sub(r"\.(c|cpp)$", "", str(unit).replace("\\", "/").strip("/"))
    if bare.startswith("src/"):
        bare = bare[len("src/"):]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    for entry in data.get("units", []):
        if entry.get("name", "").endswith(bare):
            return {fn["name"]: float(fn.get("fuzzy_match_percent", 0.0))
                    for fn in entry.get("functions", [])
                    if fn.get("name")}
    return {}


def report_is_fresh(unit, report=None):
    """True when the report is at least as new as the unit's object."""
    path = Path(report) if report is not None else REPORT
    obj = Path(f"build/{VERSION}/src/{re.sub(r'[.](c|cpp)$', '', unit)}.o")
    try:
        return path.stat().st_mtime >= obj.stat().st_mtime
    except OSError:
        return False


def fresh_fuzzy(bare_unit, _names):
    """Per-function fuzzy for `bare_unit` from a FRESHLY built report.

    Regenerates build/GUNE5D/report.json before reading it. Reading the
    report a previous command left behind is the recorded way a 0.04-off
    number nearly got banked, so this never reads a report it did not just
    cause to be written.
    """
    build = subprocess.run(["ninja", str(REPORT).replace("\\", "/")],
                           capture_output=True, text=True)
    if build.returncode != 0:
        print("[--arbiter fuzzy: report build FAILED, fuzzy unavailable]")
        print((build.stdout + build.stderr).strip()[-600:])
        return {}
    return read_report_fuzzy(bare_unit)


def _fuzzy_note(was, now):
    """'fuzzy A -> B (+D)' or a reason the delta could not be formed."""
    if now is None:
        return None, ("fuzzy delta UNMEASURED — pass `--arbiter fuzzy` to"
                      " measure the fuzzy this REGRESSION would override")
    if was is None:
        return None, (f"fresh fuzzy is {now:.4f} but this baseline carries no"
                      " fuzzy anchor to compare it against — re-take the"
                      " baseline to enable the fuzzy arbiter")
    delta = now - was
    return delta, f"fuzzy {was:.4f} -> {now:.4f} ({delta:+.4f})"


def verdict_row(verdict, detail, accepted):
    """(label, detail) for one printed row — run-43 item 6.

    A CONFLICT the SAME call is about to accept used to print as
    "CONFLICT ... arbitrate ... (pass --arbitrate to accept)" directly above
    "GATE OK (arbitrated: N CONFLICT accepted)": the row instructed the
    reader to do what they had just done. Only the printed LABEL moves —
    the verdict tuples this function reads are untouched, so nothing that
    parses verdicts by name is affected.
    """
    if accepted and verdict == "CONFLICT":
        return ("ARBITRATED",
                detail.split("; arbitrate")[0]
                + "  — ACCEPTED by --arbitrate on this call")
    return verdict, detail


def arbitrate_regressions(verdicts, unit, baseline=None, genuine_fn=None,
                          ops_fn=None, fuzzy_fn=None, arbiter=None):
    """Downgrade real-growth REGRESSIONs to CONFLICT when the current state
    is structurally target-identical (equal insn counts, IDENTICAL opcode
    multiset): the growth can be pure naming churn invisible to `real`.
    Never applies to functions that were byte-exact at baseline.

    ``arbiter='fuzzy'`` additionally measures the FRESH fuzzy of every
    disputed function, so the flat-genuine-rows case — where the structure
    arbiter has nothing to say and every keep was overridden by hand —
    reports the delta it would be overriding, and a RISING fuzzy becomes a
    CONFLICT instead of a bare REGRESSION.
    """
    bare_unit = re.sub(r"\.(c|cpp)$", "", unit)
    baseline = baseline or {}
    genuine_fn = genuine_fn or genuine_counts
    ops_fn = ops_fn or ops_text
    disputed = [name for name, verdict, detail in verdicts
                if verdict == "REGRESSION"
                and re.match(r"real (\d+) -> (\d+)$", detail)
                and not detail.startswith("real 0 ")]
    genuine_now = genuine_fn(bare_unit, disputed) if disputed else {}
    fuzzy_now = {}
    if disputed and arbiter == "fuzzy":
        fuzzy_now = (fuzzy_fn or fresh_fuzzy)(bare_unit, disputed) or {}
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
        delta, note = _fuzzy_note(baseline.get(name, {}).get("fuzzy"),
                                  fuzzy_now.get(name))
        if now < was:
            out.append((name, "CONFLICT",
                        detail + f" BUT genuine structural rows {was} ->"
                        f" {now} FELL — the residual moved nearer target"
                        " and real is reading the re-alignment; arbitrate"
                        " on fuzzy from a fresh report, do NOT auto-revert"
                        " (pass --arbitrate to accept)"
                        + (f" [{note}]" if delta is not None else "")))
        elif now > was:
            out.append((name, verdict,
                        detail + f" (genuine structural rows {was} -> {now}"
                        " ROSE — structure agrees with real)"))
        elif delta is not None and delta > 0:
            # FUZZY ARBITER: rows flat, so the structure arbiter is silent
            # and `real` alone was deciding. A rising fresh fuzzy says the
            # finer metric disagrees with real — the exact shape that was
            # being overridden by hand.
            out.append((name, "CONFLICT",
                        detail + f" BUT genuine structural rows FLAT {was} ->"
                        f" {now} while {note} ROSE — the finer metric"
                        " disagrees with real; arbitrate, do NOT auto-revert"
                        " (pass --arbitrate to accept)"))
        elif delta is not None:
            out.append((name, verdict,
                        detail + f" (genuine structural rows FLAT {was} ->"
                        f" {now}; {note} — real and fuzzy AGREE)"))
        else:
            out.append((name, verdict,
                        detail + f" (genuine structural rows {was} -> {now};"
                        f" {note})"))
    return out


ARBITRATION_LOG = Path(f"build/{VERSION}/gate/arbitrations.jsonl")


def bare_unit(unit):
    """`game/sys/memcard` from any accepted spelling of that unit.

    The log is keyed on the EXTENSIONLESS form. `check` is invoked as
    `game/sys/memcard.c` and `game/sys/memcard` interchangeably across
    this project's tools, and keying on the raw string made
    `arbitrations game/sys/memcard` report zero events for a unit that had
    just logged one — a scoping filter that silently answers "none" is
    worse than no filter at all.
    """
    return re.sub(r"\.(c|cpp)$", "", normalize_unit(unit))


def arbitration_event(unit, action, rows, arbiter=None, reanchored=False,
                      head=None, at=None):
    """One append-only record of an arbitration decision.

    ``rows`` is the list of (name, verdict, detail) triples the decision
    covered; the detail strings already carry the real delta, the genuine
    structural rows and the fuzzy note, so the log preserves the EVIDENCE
    behind an override, not merely a counter.
    """
    return {
        "at": at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "unit": bare_unit(unit),
        "action": action,
        "arbiter": arbiter,
        "reanchored": bool(reanchored),
        "head": head,
        "functions": [{"name": n, "verdict": v, "detail": d}
                      for n, v, d in rows],
    }


def log_arbitration(event, path=None):
    """Append one event to the gate's arbitration log.

    The filename is deliberately NOT lane-prefixed: build/GUNE5D/gate/ is
    per-worktree build output, so there is no cross-lane collision to
    avoid, and a per-lane name would defeat the only thing the log is for
    — aggregating a rate across a lane's whole pass.

    Logging never fails a gate: an unwritable log is reported and
    swallowed, because losing an audit line must not turn a passing check
    into a failing one.
    """
    path = Path(path) if path is not None else ARBITRATION_LOG
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(event, sort_keys=True) + "\n")
    except OSError as err:
        print(f"[arbitration log NOT written to {path}: {err}]")
        return False
    return True


def read_arbitrations(path=None):
    """Every well-formed event in the log, malformed lines skipped."""
    path = Path(path) if path is not None else ARBITRATION_LOG
    events = []
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return events
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            event = json.loads(line)
        except ValueError:
            continue
        if isinstance(event, dict) and event.get("action"):
            events.append(event)
    return events


def summarize_arbitrations(events, unit=None):
    """Counts and the override RATE the '1/3 of keeps' claim needs.

    ACCEPTED and REFUSED are the two halves of the same decision, so the
    rate is well-defined: of every CONFLICT a check raised, what fraction
    was overridden into a keep? Nothing in this tool used to persist
    either half, which is why that ratio has only ever been asserted.
    BANKED (--bank-arbitrated) is counted separately: it re-anchors one
    row without the gate having raised a CONFLICT in the same call.
    """
    if unit is not None:
        bare = bare_unit(unit)
        events = [e for e in events if e.get("unit") == bare]
    counts = {"accepted": 0, "refused": 0, "banked": 0}
    functions = 0
    for event in events:
        action = event.get("action")
        if action in counts:
            counts[action] += 1
        functions += len(event.get("functions") or [])
    decided = counts["accepted"] + counts["refused"]
    rate = counts["accepted"] / decided if decided else None
    return {"events": len(events), "functions": functions,
            "decided": decided, "rate": rate, **counts}


def format_arbitrations(summary, unit=None):
    scope = f" for {unit}" if unit else " (all units)"
    lines = [f"arbitration log{scope}: {summary['events']} event(s),"
             f" {summary['functions']} function row(s)"]
    lines.append(f"  accepted (--arbitrate keep)   {summary['accepted']}")
    lines.append(f"  refused  (gate failed)        {summary['refused']}")
    lines.append(f"  banked   (--bank-arbitrated)  {summary['banked']}")
    if summary["rate"] is None:
        lines.append("  override rate: UNDEFINED — no CONFLICT has been"
                     " raised through a logging gate yet")
    else:
        lines.append(f"  override rate: {summary['rate']:.1%}"
                     f" ({summary['accepted']}/{summary['decided']} raised"
                     " CONFLICTs kept)")
    return "\n".join(lines)


CLEAN_RE = re.compile(r"^==\s+(\S+?):\s+(.+?),\s+(\d+)\s+real diff lines")


def parse_clean(text):
    """{fn: (clean_status, clean_real)} from a whole-TU `fndiff --clean`.

    --clean already covers every function in ONE call; what was missing
    was anything that read it alongside `real` and the genuine-row count.
    """
    out = {}
    for line in text.splitlines():
        match = CLEAN_RE.match(line.strip())
        if match:
            name, status, real = match.groups()
            out[name] = (status, int(real))
    return out


def webfrank_pins(unit):
    """Function names this TU has a WebFrank rule for, or set().

    AGENTS.md discipline 10: a pinned function reads real 0 BY
    CONSTRUCTION (the gate scores the postprocessed object), so any
    roster that ranks by `real` must say which rows are pinned or it
    silently promises work that is already closed.
    """
    path = Path(f"config/{VERSION}/webfrank.json")
    if not path.exists():
        return set()
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set()
    return {rule.get("function")
            for rule in (data.get("units") or {}).get(unit, [])
            if rule.get("function")}


def slot_column(unit):
    """{function: short slot verdict} for a whole TU, in two object parses.

    Run-42 item 7. Both of CL's slot wins on game/mb/mb_camera were visible
    in probe's BASELINE banner before any source was read — "frame size
    target 40 vs ours 48; slots differ (4t/4o exclusive)" — but nothing in
    the ROSTER carried it, so a lane sweeping a TU could not see which rows
    were frame-shaped without probing each one. `real` is the wrong arbiter
    for exactly those rows
    (claim.law.real-can-underweight-a-large-alignment-gain-so-arbitrate-
    conflicts-on-fuzzy.20260831.v1), so a roster ranked on `real` alone
    points a frame residual at the metric that fights it.

    Costs two `fndiff.parse` calls for the WHOLE unit — the same two the
    roster's other columns already pay for — where the alternative is one
    `slotdiff.py` subprocess per function.

    The verdict is deliberately SHORT and ranked most-decisive first: a
    save-set delta is an unallocated callee-saved register rather than a
    local slot, and a worker mis-modelled a session by reading it as one.
    """
    try:
        target = fndiff.parse(Path(f"build/{VERSION}/obj/{unit}.o"))
        ours = fndiff.parse(Path(f"build/{VERSION}/src/{unit}.o"))
    except (OSError, ValueError):
        return {}
    out = {}
    for name, our_lines in ours.items():
        target_lines = target.get(name)
        if target_lines is None:
            continue
        t_slots, o_slots = (slotdiff.slot_map(target_lines),
                            slotdiff.slot_map(our_lines))
        t_save, o_save = (slotdiff.save_set(target_lines),
                          slotdiff.save_set(our_lines))
        t_frame, o_frame = (fndiff.frame_size(target_lines),
                            fndiff.frame_size(our_lines))
        parts = []
        if t_save != o_save:
            parts.append(f"save {t_save}/{o_save}")
        if t_frame != o_frame:
            parts.append(f"frame {t_frame}/{o_frame}")
        exclusive_t = len(set(t_slots) - set(o_slots))
        exclusive_o = len(set(o_slots) - set(t_slots))
        if exclusive_t or exclusive_o:
            parts.append(f"{exclusive_t}T/{exclusive_o}O")
        out[name] = ",".join(parts) if parts else "-"
    return out


def roster_rows(snap, clean, pins, baseline=None, slots=None):
    """Sorted roster rows: (name, status, insns, real, genuine, clean,
    clean_real, fuzzy, pinned, delta, slot).

    Ordered by the work each row represents — `real` descending, then
    name — so the roster reads as a queue. Pinned rows sort last whatever
    their score, because their real is not a measurement of open work.
    """
    baseline = baseline or {}
    slots = slots or {}
    rows = []
    for name, entry in snap.items():
        if name == "__sections__":
            continue
        pinned = name in pins
        was = (baseline.get(name) or {}).get("real")
        real = entry.get("real")
        delta = (None if was is None or real is None or was == real
                 else f"{was}->{real}")
        clean_status, clean_real = clean.get(name, ("?", None))
        ti, bi = entry.get("ti"), entry.get("bi")
        # A function with no DIFF line carries no counts at all; printing
        # "None/None" for the exact rows made the roster look broken.
        insns = "-" if ti is None and bi is None else f"{ti}/{bi}"
        rows.append((name, entry.get("status"), insns,
                     real, entry.get("genuine"), clean_status, clean_real,
                     entry.get("fuzzy"), pinned, delta,
                     slots.get(name, "-")))
    rows.sort(key=lambda row: (row[8], -(row[3] or 0), row[0]))
    return rows


def format_roster(unit, rows, has_baseline):
    lines = [f"ROSTER {unit}: {len(rows)} function(s)"
             f"  [real | genuine structural rows | fndiff --clean | fuzzy"
             f" | SLOT]"]
    open_rows = [r for r in rows if (r[3] or 0) > 0 and not r[8]]
    exact = sum(1 for r in rows if r[3] == 0 and not r[8])
    pinned = sum(1 for r in rows if r[8])
    lines.append(f"  {exact} at real 0, {len(open_rows)} with a residual,"
                 f" {pinned} WebFrank-PINNED (real 0 by construction —"
                 " not open work)")
    header = (f"  {'FUNCTION':<38} {'STATUS':<18} {'INSNS':>9} {'REAL':>6}"
              f" {'GEN':>5} {'FUZZY':>8} {'SLOT':<22}  CLEAN")
    lines.append(header)
    for (name, status, insns, real, genuine, clean_status, clean_real,
         fuzzy, pinned, delta, slot) in rows:
        mark = " [PINNED]" if pinned else ""
        show_real = "-" if real is None else str(real)
        if delta:
            show_real += f" ({delta})"
        lines.append(
            f"  {name:<38.38} {str(status):<18.18} {insns:>9}"
            f" {show_real:>6} {('-' if genuine is None else genuine):>5}"
            f" {('-' if fuzzy is None else f'{fuzzy:.2f}'):>8}"
            f" {slot:<22.22}  {clean_status}"
            + (f" [{clean_real}]" if clean_real not in (None, real) else "")
            + mark)
    slot_rows = [r for r in rows
                 if r[10] not in ("-", "") and (r[3] or 0) > 0 and not r[8]]
    if slot_rows:
        lines.append(
            f"  SLOT COLUMN: {len(slot_rows)} open row(s) carry a frame,"
            " save-set or exclusive-slot delta —"
            " `save tgt/ours` is an unallocated CALLEE-SAVED register (not a"
            " local slot), `frame tgt/ours` the frame size, `NT/MO` the"
            " exclusive slots each side holds. ARBITRATE THESE ON THE SLOT"
            " MAP (`slotdiff.py <unit> <fn>`), NOT ON `real`, which actively"
            " fights frame work"
            " (claim.law.real-can-underweight-a-large-alignment-gain-so-"
            "arbitrate-conflicts-on-fuzzy.20260831.v1). Both of run-41 CL's"
            " slot wins were visible in this data before any source was"
            " read, and the roster did not carry it.")
    if not has_baseline:
        lines.append("  (no gate baseline for this unit, so no REAL delta is"
                     " shown — take one with `defake_gate.py baseline"
                     f" {unit}`)")
    return "\n".join(lines)


def run_fndiff(unit, flag):
    result = subprocess.run(
        [sys.executable, str(FNDIFF), unit, flag],
        capture_output=True, text=True,
    )
    if result.returncode != 0 and "missing:" in (result.stdout + result.stderr):
        raise SystemExit(f"fndiff failed for {unit}:\n{result.stdout}{result.stderr}")
    return result.stdout


WEBFRANK_CONFIG = f"config/{VERSION}/webfrank.json"


def _unit_rules(text, unit):
    """This unit's webfrank rules out of a config blob, or None.

    None means "cannot tell" — no config, or JSON that will not parse — and
    is never rendered as "no rules": a manufactured all-clear on the
    question of whether the pins pair with the source is exactly the defect
    below.
    """
    if text is None:
        return None
    try:
        data = json.loads(text)
    except (ValueError, TypeError):
        return None
    if not isinstance(data, dict):
        return None
    return (data.get("units") or {}).get(unit, [])


def webfrank_unit_drift(head_text, working_text, unit):
    """(drifted, detail) for `unit`'s rules between HEAD and the tree.

    Run-44 item 8, from WS. `--at-head` sets the working tree's SOURCE
    aside and writes HEAD's bytes back, and it does NOT touch
    config/GUNE5D/webfrank.json — which is global state that pairs with
    exactly ONE source state. So an at-head baseline taken while this
    unit's rules are uncommitted is built from HEAD's source against the
    TREE's pins, which is no commit's state at all; when the drift is a
    re-derived permutation pin the WEBFRANK stage hash-asserts and the
    build simply aborts, naming the pin rather than the flag that caused
    it. WS spent two builds on that.

    Restoring the config here was rejected deliberately: webfrank.json is
    shared across every concurrent lane, a write would race them, and a
    rule change needs `configure.py` re-run before the WEBFRANK edge even
    materialises (AGENTS.md first-five-minutes trap 6). Naming the drift
    before the build is spent costs nothing and cannot corrupt anyone.

    Pure over two texts so both sides are decided without a git tree.
    Scoped to THIS UNIT: another lane's rules cannot reach this unit's
    object, and warning about them would make the notice constant noise in
    a fleet where webfrank.json is almost always dirty somewhere.
    """
    head_rules = _unit_rules(head_text, unit)
    tree_rules = _unit_rules(working_text, unit)
    if head_rules is None or tree_rules is None:
        if head_text is None and working_text is None:
            return False, ""            # no postprocessor config at all
        return True, ("this checkout's webfrank.json could not be read on"
                      " one side, so whether the pins pair with HEAD's"
                      " source is UNMEASURED")
    if head_rules == tree_rules:
        return False, ""
    head_names = [r.get("function") for r in head_rules if isinstance(r, dict)]
    tree_names = [r.get("function") for r in tree_rules if isinstance(r, dict)]
    if head_names != tree_names:
        return True, (f"the rule SET moved: HEAD has {head_names or 'none'},"
                      f" the working tree has {tree_names or 'none'}")
    changed = [name for head_rule, tree_rule, name
               in zip(head_rules, tree_rules, tree_names)
               if head_rule != tree_rule]
    return True, (f"rule BODIES differ for {', '.join(str(n) for n in changed)}"
                  " (a re-derived pin looks exactly like this)")


def webfrank_drift_warning(unit, detail):
    """What to print when an --at-head baseline would mix source and pins."""
    return (
        "WARNING: --at-head restores the SOURCE ONLY, and this unit's"
        f" webfrank rules are NOT HEAD's — {detail}.\n"
        "  The baseline below is therefore HEAD's source measured against"
        " the WORKING TREE's pins, which is not any commit's state. If the"
        " drift is a re-derived permutation pin the WEBFRANK stage will"
        " hash-assert and the build will abort naming the PIN, not this"
        " flag (WS spent two builds there).\n"
        f"  This tool does not restore {WEBFRANK_CONFIG}: it is global,"
        " shared with every concurrent lane, and a rule change needs"
        " `python configure.py` re-run before its build edge even exists."
        " Commit or revert the rule change first, or re-derive the pin"
        " against HEAD's source with"
        " `tools/gdl/composed_census/wf_rederive_pin.py`.")


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


def progress_split():
    """The live STRICT/EQUIVALENT split, or None when it cannot be read.

    Delegates to `progress.postprocessor_split` — one implementation, so this
    tool and `progress.py --split` can never quote two different numbers for
    the same question (the second-copy hazard that put two figures for one
    discriminator in front of two lanes).
    """
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import progress
        return progress.postprocessor_split()
    except Exception:
        return None


def _progress_module_format(split):
    """`progress.format_split`, or a plain fallback if the import fails."""
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import progress
        return progress.format_split(split)
    except Exception:
        return (f"STRICT {split.get('strict_percent', 0):.2f}%"
                f" + EQUIVALENT {split.get('equivalent_percent', 0):.2f}%")


def progress_delta_line(baseline_split, current_split):
    """The MEASURED STRICT/EQUIVALENT delta between two stamped splits.

    Returns None when either end is missing — a baseline taken before run 47
    carries no split, and an absent number must read as absent, never as
    zero movement.
    """
    if not baseline_split or not current_split:
        return None
    strict = (current_split["strict_percent"]
              - baseline_split["strict_percent"])
    equivalent = (current_split["equivalent_percent"]
                  - baseline_split["equivalent_percent"])
    fns = (current_split["strict_functions"]
           - baseline_split["strict_functions"])
    eq_fns = (current_split["equivalent_functions"]
              - baseline_split["equivalent_functions"])
    return (f"  PROGRESS SPLIT since this baseline: STRICT"
            f" {baseline_split['strict_percent']:.2f}% ->"
            f" {current_split['strict_percent']:.2f}% ({strict:+.2f},"
            f" {fns:+d} fns) | EQUIVALENT"
            f" {baseline_split['equivalent_percent']:.2f}% ->"
            f" {current_split['equivalent_percent']:.2f}%"
            f" ({equivalent:+.2f}, {eq_fns:+d} fns)."
            " IMAGE-WIDE, not this TU's: every lane's landed work moves it,"
            " so this is what your lane's delta must be measured AGAINST,"
            " not attributed to.")


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
    # THE PROGRESS SPLIT, stamped at the moment the baseline is taken
    # (run-47 item 6). Mandatory policy makes the STRICT/EQUIVALENT split
    # the reportable figure, and the computation lived only INSIDE
    # `configure.py progress`, so nothing could store one: a lane reporting
    # its own STRICT delta had to ATTRIBUTE it — read two printed
    # percentages from two different moments and assert the difference was
    # its own. With both ends stamped, `check` prints a MEASUREMENT.
    split = progress_split()
    if split is not None:
        meta["progress"] = split
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
    # ONE DECISION, TWO SPELLINGS (run-43 item 6). probe.py spells "accept
    # this arbitrated keep and move the anchor onto it" `--rebase-best`;
    # this gate spells it `--arbitrate`. A lane alternates between the two
    # tools inside one loop, so each now accepts the other's word. Neither
    # is deprecated: `--rebase-best` names what happens to the anchor and
    # `--arbitrate` names the decision, and both are in muscle memory.
    arbitrate = "--arbitrate" in sys.argv or "--rebase-best" in sys.argv
    at_head = "--at-head" in sys.argv
    # `--arbiter fuzzy` and `--arbiter=fuzzy` both work; the space form has
    # to pull its value back out of the positional list.
    arbiter = next((a.split("=", 1)[1] for a in sys.argv
                    if a.startswith("--arbiter=")), None)
    if arbiter is None and "--arbiter" in sys.argv:
        idx = sys.argv.index("--arbiter")
        if idx + 1 < len(sys.argv) and not sys.argv[idx + 1].startswith("--"):
            arbiter = sys.argv[idx + 1]
            if arbiter in args:
                args.remove(arbiter)
    if arbiter not in (None, "fuzzy"):
        print(f"unknown --arbiter {arbiter!r}; the only mode is `fuzzy`")
        return 2
    renames = {}
    for arg in sys.argv[1:]:
        if arg.startswith("--rename="):
            old, _, new = arg[len("--rename="):].partition("=")
            if old and new:
                renames[old] = new
    bank_arbitrated = next(
        (a.split("=", 1)[1] for a in sys.argv
         if a.startswith("--bank-arbitrated=")), None)
    if args[:1] == ["arbitrations"]:
        unit = args[1] if len(args) > 1 else None
        print(format_arbitrations(
            summarize_arbitrations(read_arbitrations(), unit), unit))
        return 0
    if args[:1] == ["roster"] and len(args) == 2:
        worst = 0
        for one in args[1].split(","):
            one = one.strip()
            if not one:
                continue
            worst = max(worst, run_roster(one, rebuild, arbiter))
        return worst
    if len(args) != 2 or args[0] not in ("baseline", "check"):
        print(__doc__)
        return 2
    mode, unit = args
    # Cross-lane ownership screen (run-46 item 1) — the same one probe.py
    # runs, on the machine-readable attributes.owned_units channel only.
    try:
        import claimscope
        for one in unit.split(","):
            rc = claimscope.warn_or_refuse(
                one.strip(), "defake_gate",
                enforce="--ignore-claim" not in sys.argv)
            if rc:
                return rc
    except ImportError:
        pass
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
                              renames, bank_arbitrated, at_head, arbiter)
            worst = max(worst, code)
        return worst
    return run_single(mode, unit, rebuild, update_improved, arbitrate,
                      renames, bank_arbitrated, at_head, arbiter)


def measure_unit(unit, arbiter=None):
    """(snapshot, fuzzy_note) — every per-function measurement this tool
    takes, in ONE pass over the unit.

    Extracted from run_single so `roster` can reuse it (run-38 item 7):
    the numbers a sweep needs — status, insn counts, real, genuine
    structural rows, fuzzy — were all computed here already and reachable
    only by taking a gate baseline, so a lane wanting a per-function view
    ran fndiff once per function instead (UC: 15 subprocess calls).
    """
    snap = snapshot(run_fndiff(unit, "--classify"),
                    run_fndiff(unit, "--count"))
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
        # Relocation SYMBOLS, kept as names rather than a hash: the
        # NAMING-DRIFT check has to resolve two spellings to addresses to
        # tell a rename from a different callee, and a hash cannot.
        for name, rows in fndiff.relocation_symbols(objfile).items():
            if name in snap:
                snap[name]["relocs"] = [list(row) for row in rows]
        # Per-TU DATA baseline (run 34 item 1): the object's non-text
        # sections, banked under a reserved key so `compare` can flag a
        # score-invisible data-section change (a lost .extab match) as its
        # own verdict class. Kept out of every per-function loop above.
        targetfile = Path(
            f"build/{VERSION}/obj/{re.sub(r'[.](c|cpp)$', '', unit)}.o")
        data_sections = data_section_digests(objfile, targetfile)
        if data_sections is not None:
            snap["__sections__"] = {"data": data_sections}
    # Genuine structural rows for every function `real` calls imperfect —
    # the structure arbiter's baseline half. Byte-exact rows can never be
    # disputed, so they are skipped and the count stays cheap.
    mismatching = [name for name, row in snap.items() if row.get("real")]
    if mismatching:
        for name, count in genuine_counts(unit, mismatching).items():
            if name in snap:
                snap[name]["genuine"] = count
    # Fuzzy anchor for the fuzzy arbiter. Only ever taken from a report at
    # least as new as the object it describes: an anchor read from a stale
    # report is worse than no anchor, because `check` would silently
    # compare a fresh number against a number for different bytes.
    bare = re.sub(r"\.(c|cpp)$", "", unit)
    fuzzy_rows, fuzzy_note = {}, "no fuzzy anchor (report older than object)"
    if arbiter == "fuzzy":
        fuzzy_rows = fresh_fuzzy(bare, None)
        fuzzy_note = "fuzzy anchor from a freshly built report"
    elif report_is_fresh(unit):
        fuzzy_rows = read_report_fuzzy(bare)
        fuzzy_note = "fuzzy anchor from the current report"
    for name, value in fuzzy_rows.items():
        if name in snap:
            snap[name]["fuzzy"] = value
    if not fuzzy_rows:
        fuzzy_note = ("no fuzzy anchor — re-take with `--arbiter fuzzy` to"
                      " enable the fuzzy arbiter")
    return snap, fuzzy_note


def run_roster(unit, rebuild, arbiter=None):
    """The whole per-function sweep in ONE call (run-38 item 7)."""
    unit = normalize_unit(unit)
    if rebuild:
        obj = re.sub(r"\.(c|cpp)$", "", unit)
        build = subprocess.run(["ninja", f"build/{VERSION}/src/{obj}.o"],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("BUILD FAILED (roster not run):")
            print((build.stdout + build.stderr).strip()[-1500:])
            return 1
    snap, fuzzy_note = measure_unit(unit, arbiter)
    clean = parse_clean(run_fndiff(unit, "--clean"))
    baseline = {}
    path = gate_path(unit)
    if path.exists():
        baseline, _meta = load_baseline(path)
    rows = roster_rows(snap, clean, webfrank_pins(unit), baseline,
                       slot_column(unit))
    print(format_roster(unit, rows, bool(baseline)))
    print(f"  {fuzzy_note}")
    return 0


def run_single(mode, unit, rebuild, update_improved, arbitrate, renames=None,
               bank_arbitrated=None, at_head=False, arbiter=None):
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
        # SAY SO BEFORE THE BUILD IS SPENT (run-44 item 8). --at-head moves
        # the source and nothing else; webfrank.json pairs with exactly one
        # source state.
        config = Path(WEBFRANK_CONFIG)
        head_config = subprocess.run(
            ["git", "show", f"HEAD:{WEBFRANK_CONFIG}"], capture_output=True)
        drifted, detail = webfrank_unit_drift(
            head_config.stdout.decode("utf-8", "replace")
            if head_config.returncode == 0 else None,
            config.read_text(encoding="utf-8", errors="replace")
            if config.exists() else None,
            re.sub(r"\.(c|cpp)$", "", unit))
        if drifted:
            print(webfrank_drift_warning(unit, detail))
        try:
            if dirty:
                src.write_bytes(shown.stdout)
            print(f"--at-head: baselining {unit} from commit"
                  f" {(git_head() or '?')[:9]}"
                  + (" (working-tree edits temporarily set aside)"
                     if dirty else " (working tree already matches HEAD)"))
            return run_single(mode, unit, True, update_improved, arbitrate,
                              renames, bank_arbitrated, at_head=False,
                              arbiter=arbiter)
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
    snap, fuzzy_note = measure_unit(unit, arbiter)
    path = gate_path(unit)
    if mode == "baseline":
        meta = save_baseline(path, snap, unit)
        exact = sum(1 for row in snap.values() if row.get("real") == 0)
        nfns = sum(1 for name in snap if name != "__sections__")
        print(f"baseline: {nfns} functions ({exact} at real 0) -> {path}")
        secs = (snap.get("__sections__") or {}).get("data")
        if secs:
            priced = {n: _section_row(e) for n, e in secs.items()}
            have = {n: r for n, r in priced.items() if r[1] is not None}
            if have:
                total_m = sum(r[1] for r in have.values())
                total_t = sum(r[2] or 0 for r in have.values())
                basis = ("all-or-nothing per section, as the PROGRESS `Data:`"
                         " line counts it"
                         if all(r[3] == "image" for r in have.values())
                         else "POSITIONAL per-byte — this OVERSTATES matched"
                              " Data; re-take with --at-head")
                print(f"  DATA baseline: {len(secs)} non-text section(s),"
                      f" {total_m}/{total_t} matched bytes ({basis})"
                      + "".join(f"; {n} {r[1]}/{r[2]}"
                                for n, r in sorted(have.items())))
            else:
                print(f"  DATA baseline: {len(secs)} non-text section(s)"
                      f" digested ({', '.join(sorted(secs))}) — NOT priced"
                      " (no target object to compare against, so `check` can"
                      " only report that they moved)")
        print(f"  {fuzzy_note}")
        if meta.get("progress"):
            print("  PROGRESS SPLIT stamped: "
                  + _progress_module_format(meta["progress"])
                  + " — `check` reports the MEASURED delta against this")
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
        # The MEASURED progress delta (run-47 item 6). Silent when the
        # baseline predates the stamp: an absent number must read as absent,
        # never as "no movement".
        delta = progress_delta_line(meta.get("progress"), progress_split())
        if delta:
            print(delta)
        elif meta.get("progress") is None:
            print("  [no PROGRESS SPLIT stamped in this baseline (taken"
                  " before run 47) — a STRICT delta quoted against it would"
                  " be ATTRIBUTED, not measured. Re-take with"
                  f" `defake_gate.py baseline {unit} --at-head`.]")
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
        before = baseline.get(target_row) or {}
        baseline[target_row] = snap[target_row]
        save_baseline(path, baseline, unit)
        log_arbitration(arbitration_event(
            unit, "banked",
            [(target_row, "BANKED",
              f"real {before.get('real')} -> {snap[target_row].get('real')}"
              f" (fuzzy {before.get('fuzzy')} ->"
              f" {snap[target_row].get('fuzzy')})")],
            arbiter=arbiter, reanchored=True, head=git_head()))
        print(f"banked arbitrated keep for {target_row} (that row only —"
              " every sibling still gates against its original anchor;"
              " record the arbitration + its fuzzy in the attempt record)")
    verdicts = compare(baseline, snap, renames,
                       target_relocs=target_relocation_symbols(unit),
                       ours_object=our_object(unit))
    verdicts = arbitrate_regressions(verdicts, unit, baseline,
                                     arbiter=arbiter)
    conflicts = [v for v in verdicts if v[1] == "CONFLICT"]
    regressions = [v for v in verdicts if v[1] == "REGRESSION"]
    # Run-43 item 6. A CONFLICT this very call is about to ACCEPT was still
    # printed as a bare "CONFLICT ... (pass --arbitrate to accept)" above the
    # "GATE OK (arbitrated: N CONFLICT accepted)" line — the row told the
    # reader to do the thing they had just done. The decision is known here,
    # so the row says which way it went. The verdict TUPLES are untouched:
    # only the printed label changes, so nothing that parses this output by
    # verdict name moves.
    accepted = bool(conflicts) and not regressions and arbitrate
    for name, verdict, detail in verdicts:
        label, text = verdict_row(verdict, detail, accepted)
        print(f"{label:10} {name}  {text}")
    data_changed = [v for v in verdicts if v[1] == "DATA-CHANGED"]
    if data_changed:
        priced = any("NET " in v[2] for v in data_changed)
        if priced:
            # Run-46 item 3: the row above now carries the byte delta, so
            # this note says what to DO with it instead of ordering the
            # measurement the row already made.
            print("NOTE: DATA-CHANGED — the byte delta above is matched DATA"
                  " this keep gains or destroys, and NO per-function verdict"
                  " here can see it (they score .text only). A negative net"
                  " is a regression even when every .text arbiter improves"
                  " (claim.law.WS_frame-widening-silently-breaks-the-tus-"
                  "extab-match). The delta is ALL-OR-NOTHING per section, the"
                  " same rule the PROGRESS `Data:` line uses — unless the row"
                  " says ACCOUNTING IS POSITIONAL, in which case it came from"
                  " a pre-run-48 baseline and OVERSTATES matched Data"
                  " (619/620 against the image's 248/620 on game/ui/btext).")
        else:
            print("NOTE: DATA-CHANGED — a non-text section moved; NO"
                  " per-function verdict here can see it. A frame-widening"
                  " keep can improve every .text arbiter while destroying its"
                  " TU's .extab match. This baseline predates the run-46 byte"
                  " accounting, so the change is UNPRICED: re-take the"
                  " baseline to get the delta, or arbitrate with a full"
                  " `ninja` PROGRESS 'Data:' comparison before committing.")
    if conflicts and not regressions and arbitrate:
        # Log BEFORE the two return paths below so an accepted override is
        # recorded whether or not the baseline is re-anchored.
        log_arbitration(arbitration_event(
            unit, "accepted", conflicts, arbiter=arbiter,
            reanchored=update_improved, head=git_head()))
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
        # The DENOMINATOR. Without recording refusals too, the log could
        # only ever count keeps, and an override RATE would stay exactly
        # as unfalsifiable as it was before.
        log_arbitration(arbitration_event(
            unit, "refused", conflicts, arbiter=arbiter, head=git_head()))
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
            # Announce any IMMEDIATE row dropped by the cut (item 5): they
            # sit below the clusters and a silent truncation hid the one
            # changed literal that was the whole residual.
            print(fndiff.truncate_ops(ops, 14))
        return 1
    # RELOC-TOWARD-TARGET and NAMING-DRIFT both leave the baseline holding
    # the OLD relocation symbols, so without re-anchoring they re-report on
    # every later check. Both details already tell the worker to re-anchor
    # with --update-improved; before this they were not in the bankable set,
    # so that instruction did nothing.
    improved = [v for v in verdicts
                if v[1] in ("IMPROVED", "RELOC-TOWARD-TARGET",
                            "RELOC-DATUM-EQUAL", "NAMING-DRIFT")]
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
