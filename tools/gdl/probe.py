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

Escape hatches (a worker concluded --discard "does not exist" because this
docstring omitted it — the flags below all work):
  --discard          restore the TU to HEAD (the neutral-edit undo)
  --revert-baseline  restore the SESSION's first banked baseline
  --no-bank          score without banking (diagnostic probes)
  --raw              score the pre-webfrank compiler output (pinned TUs)
  --stateless        sweep mode: score only — no state, bank, or verdict
  --scaffold         print the pragma/volatile scaffold census on ANY
                     probe, not only a BASELINE
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

--fuzzy is a PURE READOUT: it builds, prints the scores and this
function's fresh objdiff fuzzy, and computes NO verdict and banks NO
snapshot. Re-running the verdict on bytes that were already scored is
what made a CONFLICT re-read as REGRESSED (see classify()'s BEST-anchored
multiset comparison); an arbitration readout must never be able to do
that. Score and bank with a plain probe, then arbitrate with --fuzzy.
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


def classify(state, real, insns, multiset_tokens, rebase_best=False,
             digest=None, source_changed=True):
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

    def bank_best():
        state["best_real"] = real
        state["best_multiset"] = multiset_tokens
        state["best_insns"] = insns

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
                    verdict += (f"\nCOUNT DISTANCE {prev_dist} ->"
                                f" {cur_dist} but the multiset moved — the"
                                " predictor is NOT valid here; arbitrate on"
                                " fresh fuzzy only")
        else:
            # The arrow is previous->current; the CLASSIFICATION is vs
            # best. Printing both without labels read as a contradiction
            # ("REGRESSED 244 -> 234") and cost two re-reads in the field.
            verdict = (f"REGRESSED vs best {best}: real"
                       f" {state.get('last_real', best)} -> {real}"
                       f" (prev -> current; insns {insns}{tok})"
                       "  [revert advised]")
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


def fuzzy_readout(unit, fn, fn_stripped, state, state_file):
    """Build the report and print this function's fresh objdiff fuzzy.

    CONFLICT arbitration used to cost two manual builds per keep; this is
    that loop, made durable. The report build is a full link — expect it
    to take as long as ninja.
    """
    rep = subprocess.run(["ninja", f"build/{VERSION}/report.json"],
                         capture_output=True, text=True)
    if rep.returncode != 0:
        print("[--fuzzy: report build FAILED — no fuzzy readout]")
        return
    try:
        report = json.loads(
            Path(f"build/{VERSION}/report.json").read_text(encoding="utf-8"))
        bare = re.sub(r"\.(c|cpp)$", "", unit)
        val = None
        for entry in report.get("units", []):
            if entry.get("name", "").endswith(bare):
                for func in entry.get("functions", []):
                    if func["name"] in (fn, fn_stripped) or \
                            func["name"].startswith(fn + "_80"):
                        val = float(func.get("fuzzy_match_percent", 0.0))
        prev_fz = state.get("last_fuzzy")
        if val is not None:
            arrow = (f" (prev {prev_fz:.4f})"
                     if isinstance(prev_fz, float) else "")
            print(f"FUZZY (fresh report): {val:.4f}%{arrow}")
            state["last_fuzzy"] = val
            state_file.write_text(json.dumps(state), encoding="utf-8")
        else:
            print("[--fuzzy: function not found in report]")
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
    """Advice after a run of edits that never reached codegen at all.

    NEUTRAL-IDENTICAL means the object bytes did not move: the edit folded
    away BEFORE codegen, so the source text never reached the compiler's
    decision point. One is a strong negative on that spelling. Three in a
    row is no longer evidence about spellings — it is evidence about the
    AXIS CLASS, because three different source constructs all failed to
    reach the same decision point. The loop used to say nothing, which
    invites a fourth spelling of a lever already proven unreachable.
    """
    if streak < REPLAN_AT:
        return None
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
                     prev_data=None, data=None, source_changed=True):
    """Byte-identity + structural-drift annotations for a NEUTRAL verdict.

    Split out of classify() so the verdict table stays pure and testable;
    this half only formats what the caller already measured.
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
        if not bytes_identical:
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

    build = subprocess.run(
        ["ninja", f"build/{VERSION}/src/{unit}.o"],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        print("BUILD FAILED:")
        print((build.stdout + build.stderr).strip()[-1500:])
        return 1

    raw_flag = ["--raw"] if "--raw" in sys.argv else []
    count = subprocess.run(
        [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
         "--count", "--no-build", *raw_flag],
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
        fuzzy_readout(unit, fn, fn_stripped, state, state_file)
        return 0

    verdict, state = classify(state, real, insns, multiset_tokens,
                              rebase_best="--rebase-best" in sys.argv,
                              digest=digest, source_changed=source_changed)
    if verdict.startswith("NEUTRAL"):
        verdict = annotate_neutral(verdict, real, insns, multiset_tokens,
                                   prev_tokens, prev_insns, prev_digest,
                                   digest, prev_data=prev_data, data=data,
                                   source_changed=source_changed)
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

    want_scaffold = ("--scaffold" in sys.argv or "--scaffold-all" in sys.argv
                     or verdict.startswith("BASELINE"))
    if want_scaffold and source is not None:
        print_scaffold_census(source, full="--scaffold-all" in sys.argv)

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
