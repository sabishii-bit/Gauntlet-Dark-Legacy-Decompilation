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
    ours = _function_bytes(root / "build" / VERSION / "src" / f"{bare}.o")
    rows = {}
    for name, t_bytes in target.items():
        o_bytes = ours.get(name)
        if o_bytes is None:
            continue
        offsets = unabsorbed_offsets(o_bytes, t_bytes)
        if offsets is None:
            rows[name] = {"unabsorbed": None, "clusters": None,
                          "insns": len(t_bytes) // 4, "tier": None}
            continue
        rows[name] = {
            "unabsorbed": len(offsets),
            "clusters": len(clusters(offsets)),
            "insns": len(t_bytes) // 4,
            "tier": "A" if not offsets else "B",
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
                  " (size mismatch — outside every postprocessor class)")
        else:
            print(f"== {name}: {row['insns']}i  unabsorbed"
                  f" {row['unabsorbed']}u/{row['clusters']}c  tier"
                  f" {row['tier']}")
    tier_a = sum(1 for r in rows.values() if r["tier"] == "A")
    print(f"[tier A {tier_a} / {len(rows)}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
