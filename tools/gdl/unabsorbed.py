#!/usr/bin/env python3
"""UNABSORBED words: the postprocessor-closability metric, made shareable.

A word of our compiled function is ABSORBED when the difference against the
target word is explainable by the WebFrank classes -- identical, a pure
register-field respell, or one of the copy-form families. Everything else
classifies as `other` and is UNABSORBED: it is the residue no recolor or
form rewrite can reach, and it is what actually decides whether a function
is postprocessor-closable.

  unabsorbed == 0  ("tier A")  the register-field stage alone reproduces
                               the target; only the recolor proof remains
  unabsorbed  > 0  ("tier B")  a permutation or real source work is needed
                               for those words first

THE COUNT IS NOT THE CLASS.  Run-34 criticism (MV): two probes reported an
identical unabsorbed count of 4 while the residual moved from a class no
postprocessor can reach to one a permutation window can -- the number was
flat and the eligibility was not, so the roster reported "no change" on a
real change.  Every row therefore carries a residual CLASS alongside the
count, decided over the unabsorbed offsets only:

  allocator         0 unabsorbed words: the register-field stage alone
                    reproduces the target (tier A).
  schedule          our words at the unabsorbed offsets are a PERMUTATION of
                    the target's (identical word multiset) -- a permutation
                    candidate, subject to a legal window existing.
  operand           same opcodes, different non-register operand fields
                    (displacements, immediates, addends): neither a recolor
                    nor a permutation reaches these.
  source-structural the opcode multisets themselves differ -- an opcode one
                    side emits and the other does not.  Provably outside
                    every postprocessor class; this is source work.
  count-asymmetric  unequal function sizes, so the metric is undefined and
                    no postprocessor class can apply (the count asymmetry
                    laws).
  compiler-exact    the RAW compiler output already equals the target: zero
                    differing words, nothing to screen.
  rule-served       a WebFrank rule for this function is already shipped in
                    config/GUNE5D/webfrank.json; it is closed, not open.

READ THE RAW OBJECT, NOT THE POSTPROCESSED ONE.  Run-36 criticism (MC): this
census loaded `ours` from build/GUNE5D/src/<tu>.o -- the WEBFRANK stage's
OUTPUT -- so every function a shipped rule already closed read 0 unabsorbed /
tier A / class `allocator` BY CONSTRUCTION, exactly like a function the
compiler emits byte-exactly.  That is the fndiff pin-masking law generalised
from a SCORE to a CLASSIFIER, and it is worse there: a score of 0 announces
itself as suspicious, while a class of `allocator` reads as a work item.  A
lane was dispatched to screen "24 allocator-class functions" for
postprocessor candidacy; 20 had zero differing words, 3 were already
rule-served, and exactly ONE was genuinely open -- a 24:1 sizing error
(claim.law.MC_the-unabsorbed-census-scores-the-postprocessed-object...).
`ours` now comes from the raw .postprocess/body object where one exists, as
wf_word_diff.py already did, and the two already-closed populations get their
own classes so no roster can fold them into the open one again.

The metric was derived in the HV lane and lived only in
tools/gdl/composed_census/hv_perm.py, so every ROSTER -- the regnorm census,
the memory graph's `brief` -- ranked functions by size and by residual
counts that say nothing about closability. This module is the shared home;
the classifier itself is REUSED from ha_close, never re-derived (a prior
census that re-derived it dropped the copy->copy arrow and mis-TIERED rows
rather than merely miscounting them).

Usage:
  python tools/gdl/unabsorbed.py game/sys/memcard        # one TU
  python tools/gdl/unabsorbed.py game/sys/memcard --json

Only EQUAL-SIZE function pairs have the metric: a size mismatch is a count
asymmetry, which is outside every postprocessor class by construction, and
those rows report None rather than a misleading number.
"""

import json
import os
import re
import struct
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent.parent
VERSION = "GUNE5D"
CLUSTER_GAP = 16


def _load_backend():
    """(webfrank, ha_close) or (None, None) when unavailable.

    Kept lazy and fail-soft: `brief` and the census must degrade to "no
    number" rather than break when the postprocessor stack cannot import.
    """
    for path in (str(TOOLS), str(TOOLS / "composed_census")):
        if path not in sys.path:
            sys.path.insert(0, path)
    try:
        import webfrank as wf
        import ha_close as ha
        return wf, ha
    except Exception:
        return None, None


def unabsorbed_offsets(ours: bytes, target: bytes):
    """Byte offsets of words that classify as `other`, or None.

    None means the metric is undefined here (unequal sizes, or the
    postprocessor backend is unavailable) -- never an empty list, because
    an empty list reads as "fully absorbed" and that is the one wrong
    answer a roster must not be given.
    """
    wf, ha = _load_backend()
    if wf is None or len(ours) != len(target) or not ours:
        return None
    out = []
    for off in range(0, len(ours), 4):
        try:
            kind = ha.classify(wf._u32(ours, off), wf._u32(target, off))[0]
        except Exception:
            return None
        if kind == "other":
            out.append(off)
    return out


# Primary opcodes whose identity lives in an EXTENDED field as well.
_XO10_PRIMARIES = (19, 31)
# Floating/paired-single primaries that carry BOTH an X-form 10-bit XO and an
# A-form 5-bit XO. Every A-form XO on this core is >= 16 and every X-form XO
# has low 5 bits < 16, so the low bits decide the width. Reading an A-form op
# with the 10-bit field would fold its FRC REGISTER into the opcode identity
# and split `fmadds fr1,..,fr4,..` from `fmadds fr1,..,fr5,..` as if they
# were different instructions — which would report a pure recolor as
# source-structural, the exact misclassification this column exists to stop.
_AB_PRIMARIES = (4, 59, 63)


def opcode_key(word: int):
    """(primary, extended) opcode identity, register/immediate fields masked.

    Deliberately NOT a mnemonic: this is only ever compared against another
    key, never printed as a claim about an instruction.
    """
    primary = word >> 26
    if primary in _XO10_PRIMARIES:
        return (primary, (word >> 1) & 0x3FF)
    if primary in _AB_PRIMARIES:
        short = (word >> 1) & 0x1F
        return (primary, short if short >= 16 else (word >> 1) & 0x3FF)
    return (primary, None)


def _words(data: bytes, offsets):
    return [struct.unpack_from(">I", data, off)[0] for off in offsets]


def residual_class(ours: bytes, target: bytes, offsets):
    """Name the residual CLASS over the unabsorbed offsets.

    ``offsets`` is exactly what ``unabsorbed_offsets`` returned: None (the
    metric is undefined) -> "count-asymmetric"; empty -> "allocator". The
    remaining three classes are decided locally, over those offsets only,
    because absorbed words differ by a register respell and comparing them
    would conflate the recolor stage with everything else.

    The order matters and is from strongest evidence to weakest: a differing
    OPCODE multiset is provably outside every postprocessor class, so it
    outranks the permutation test; an identical WORD multiset means our words
    ARE the target's words in another order, which is the permutation
    candidate; anything left shares opcodes but not operands.
    """
    if offsets is None:
        return "count-asymmetric"
    if not offsets:
        return "allocator"
    our_words = _words(ours, offsets)
    tgt_words = _words(target, offsets)
    our_ops = sorted(opcode_key(word) for word in our_words)
    tgt_ops = sorted(opcode_key(word) for word in tgt_words)
    if our_ops != tgt_ops:
        return "source-structural"
    if sorted(our_words) == sorted(tgt_words):
        return "schedule"
    return "operand"


# Which classes a postprocessor class can reach at all, for the roster's
# summary line. `schedule` is a CANDIDATE (a legal permutation window must
# also exist); the other two are not reachable by any shipped class.
ELIGIBLE_CLASSES = ("allocator", "schedule")

# Already closed, by two different routes. These are NOT work items, and
# folding them into `allocator` is what produced the 24:1 sizing error.
CLOSED_CLASSES = ("compiler-exact", "rule-served")


def rule_served_functions(unit: str, root: Path | None = None):
    """Function names carrying a shipped WebFrank rule for `unit`.

    The join the MC law prescribes: it is what separates `rule-served` from
    `open`, and no amount of reading the census alone reveals it.
    """
    root = Path(root) if root is not None else ROOT
    config = root / "config" / VERSION / "webfrank.json"
    try:
        data = json.loads(config.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set()
    return {rule.get("function")
            for rule in data.get("units", {}).get(unit, [])
            if rule.get("function")}


def raw_object_path(root: Path, bare: str):
    """Our object for `bare`, preferring the RAW pre-WebFrank body.

    build/GUNE5D/src/<tu>.o is the postprocessor's OUTPUT; comparing it to
    the target hides every shipped rule. The .postprocess/body object is
    the compiler's own output, which is what a closability metric must
    score. Falls back to the plain path for units with no postprocess step
    (the SDK libraries), where the two are the same bytes anyway.
    """
    parts = bare.split("/")
    raw = (root / "build" / VERSION / "src" / Path(*parts[:-1])
           / ".postprocess" / "body" / (parts[-1] + ".o"))
    return raw if raw.exists() else root / "build" / VERSION / "src" / f"{bare}.o"


def differing_words(ours: bytes, target: bytes):
    """Count of 4-byte words that differ, or None when sizes disagree."""
    if len(ours) != len(target):
        return None
    return sum(1 for off in range(0, len(ours), 4)
               if ours[off:off + 4] != target[off:off + 4])


def clusters(offsets, gap: int = CLUSTER_GAP):
    """Contiguous runs of unabsorbed offsets, merged across `gap` bytes."""
    runs: list[list[int]] = []
    for off in sorted(offsets):
        if runs and off - runs[-1][1] <= gap:
            runs[-1][1] = off
        else:
            runs.append([off, off])
    return [(lo, hi + 4) for lo, hi in runs]


def _function_bytes(path: Path):
    """{name: bytes} for every sized function symbol in an object."""
    wf, _ha = _load_backend()
    if wf is None or not path.exists():
        return {}
    data = bytearray(path.read_bytes())
    sections = wf._sections(data)
    out = {}
    for sym in _symbols(wf, data, sections):
        if not sym.size:
            continue
        sec = sections[sym.section_index]
        start = sec.offset + sym.value
        out[sym.name] = bytes(data[start:start + sym.size])
    return out


def _symbols(wf, data, sections):
    """Function symbols, via cn_census when present, else webfrank."""
    try:
        import cn_census
        return cn_census.functions(data, sections)
    except Exception:
        return [s for s in wf._symbols(data, sections)
                if getattr(s, "kind", None) == "function"]


def unit_rows(unit: str, root: Path | None = None):
    """{function: {unabsorbed, clusters, insns, tier}} for one TU.

    Rows whose metric is undefined carry unabsorbed=None and tier=None.
    """
    root = Path(root) if root is not None else ROOT
    bare = re.sub(r"\.(c|cpp)$", "",
                  unit.replace("\\", "/").removeprefix("src/"))
    target = _function_bytes(root / "build" / VERSION / "obj" / f"{bare}.o")
    # RAW compiler output, never the WebFrank stage's output (MC law).
    ours = _function_bytes(raw_object_path(root, bare))
    served = rule_served_functions(bare, root=root)
    rows = {}
    for name, t_bytes in target.items():
        o_bytes = ours.get(name)
        if o_bytes is None:
            continue
        offsets = unabsorbed_offsets(o_bytes, t_bytes)
        if offsets is None:
            rows[name] = {"unabsorbed": None, "clusters": None,
                          "insns": len(t_bytes) // 4, "tier": None,
                          "differing_words": None,
                          "rule_served": name in served,
                          "residual_class": ("rule-served" if name in served
                                             else "count-asymmetric")}
            continue
        differing = differing_words(o_bytes, t_bytes)
        # Precedence: a shipped rule and a byte-exact compiler output are
        # both facts about whether this row is OPEN at all, and they
        # outrank the shape of a residual that is already closed.
        if name in served:
            klass = "rule-served"
        elif differing == 0:
            klass = "compiler-exact"
        else:
            # THE COUNT IS NOT THE CLASS (run 34 item 5).
            klass = residual_class(o_bytes, t_bytes, offsets)
        rows[name] = {
            "unabsorbed": len(offsets),
            "clusters": len(clusters(offsets)),
            "insns": len(t_bytes) // 4,
            "tier": "A" if not offsets else "B",
            "differing_words": differing,
            "rule_served": name in served,
            "residual_class": klass,
        }
    return rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 1:
        print(__doc__)
        return 2
    rows = unit_rows(args[0])
    if not rows:
        print(f"no paired functions for {args[0]!r} — run ninja first")
        return 1
    if "--json" in sys.argv:
        print(json.dumps(rows, indent=1, sort_keys=True))
        return 0
    print(f"-- unabsorbed census: {args[0]} ({len(rows)} paired functions;"
          " tier A = 0 unabsorbed words = the register-field stage alone"
          " reproduces the target) --")
    for name in sorted(rows, key=lambda n: (
            rows[n]["unabsorbed"] is None,
            rows[n]["unabsorbed"] or 0, rows[n]["insns"])):
        row = rows[name]
        if row["unabsorbed"] is None:
            print(f"== {name}: {row['insns']}i  unabsorbed n/a"
                  " (size mismatch — outside every postprocessor class)"
                  f"  class {row['residual_class']}")
        else:
            print(f"== {name}: {row['insns']}i  unabsorbed"
                  f" {row['unabsorbed']}u/{row['clusters']}c  tier"
                  f" {row['tier']}  raw-diff {row['differing_words']}w"
                  f"  class {row['residual_class']}")
    tier_a = sum(1 for r in rows.values() if r["tier"] == "A")
    counts: dict[str, int] = {}
    for row in rows.values():
        counts[row["residual_class"]] = counts.get(row["residual_class"], 0) + 1
    breakdown = ", ".join(f"{name} {counts[name]}" for name in sorted(counts))
    eligible = sum(counts.get(name, 0) for name in ELIGIBLE_CLASSES)
    closed = sum(counts.get(name, 0) for name in CLOSED_CLASSES)
    print(f"[tier A {tier_a} / {len(rows)}]")
    print(f"[class: {breakdown}]")
    print(f"[already closed {closed} / {len(rows)}"
          f" (compiler-exact {counts.get('compiler-exact', 0)} +"
          f" rule-served {counts.get('rule-served', 0)}) — NOT work items."
          " Scoring the POSTPROCESSED object used to fold both into"
          " `allocator` and oversized one work order 24:1]")
    print(f"[OPEN postprocessor-reachable {eligible} / {len(rows)}"
          " (allocator + schedule; schedule still needs a legal permutation"
          " window). THE COUNT IS NOT THE CLASS: an unchanged unabsorbed"
          " count can hide a move between these classes — compare this line"
          " across probes, not the counts alone]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
