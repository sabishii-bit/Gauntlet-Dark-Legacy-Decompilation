#!/usr/bin/env python3
"""Image-wide census of the RANGE-PROOF class (redundant rlwinm mask bits).

For every function our object and the extracted target both define at the
same size, find every differing word that is a member of the class -- two
`rlwinm`s agreeing in opcode, rA, rS, SH and Rc and differing only in the
MB/ME mask field -- and run the class's own prover over it.  Nothing is
rewritten and nothing is written to the tree: this reports candidates so a
lane can decide which functions the class actually unblocks.

Run from the repository root after a completed `ninja`:

    python tools/gdl/composed_census/wf_range_proof_census.py [--json PATH]

The `remaining` column is what decides payoff.  A function whose only
differing words are proved mask sites is CLOSED BY THIS CLASS ALONE; one
with other differing words needs the recolor/permutation stages too, and is
reported so the composition can be attempted deliberately rather than
discovered by a failing build.

TWO LIMITS, both measured, neither of which the numbers announce on their
own:

1. POSITIONAL PAIRING OVER-REPORTS.  Words are compared at the same byte
   offset, so on a SCHEDULE-class function the pairing invents rows out of
   two differently-ordered streams.  The founding run's single ADJACENT hit,
   game/mb/mb_model::MBOX_AllocModelMem+0x2c, is exactly that: `fndiff --ops`
   reports its opcode multiset IDENTICAL (102/102, pure reorder), and the
   target's `clrrwi r25,r0,4` at +0x2c pairs with OUR `clrrwi r24,r7,4` at
   +0x30, not with the word sitting at +0x2c.  Confirm every hit with
   `fndiff --ops` before believing it, exactly as regnorm rows require.
2. POSITIONAL PAIRING ALSO UNDER-REPORTS.  A genuine mask site DISPLACED by
   a schedule difference is compared against whatever word landed at its
   offset and is never recognised.  So a zero here is "no member at a fixed
   offset", never "no member".
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    _function_text_relocations, _jumptable_targets, _sections,
    _successors, _symbols, _u32, decode_rlwinm, prove_zero_bits,
    redundant_mask_source_bits,
)

BUILD = ROOT / "build" / "GUNE5D"

# Sites that would need a mask rewrite AND a recolor on the SAME word.  This
# class refuses them by name (writing the target word there would be a
# recolor nothing had proved), exactly as the copy-form class refuses its own
# combined population.  Counted here so the payoff of a combined mode is a
# measured number and not an argument.
ADJACENT: list[str] = []


def our_object(unit_relative: Path) -> Path | None:
    """The RAW compiler output for a unit, preferring the pre-WebFrank body."""
    body = (BUILD / "src" / unit_relative.parent / ".postprocess" / "body"
            / unit_relative.name)
    if body.exists():
        return body
    plain = BUILD / "src" / unit_relative
    return plain if plain.exists() else None


def function_symbols(data: bytearray):
    sections = _sections(data)
    found = {}
    for symbol in _symbols(data, sections):
        if not symbol.name or not symbol.size:
            continue
        if not 0 <= symbol.section_index < len(sections):
            continue
        if sections[symbol.section_index].name != ".text":
            continue
        found.setdefault(symbol.name, symbol)
    return sections, found


def body_of(data, sections, symbol) -> bytes:
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return bytes(data[start:start + symbol.size])


def screen_unit(target_path: Path, unit: str, rows: list) -> None:
    relative = target_path.relative_to(BUILD / "obj")
    ours_path = our_object(relative)
    if ours_path is None:
        return
    target_data = bytearray(target_path.read_bytes())
    our_data = bytearray(ours_path.read_bytes())
    target_sections, target_functions = function_symbols(target_data)
    our_sections, our_functions = function_symbols(our_data)

    for name, our_symbol in our_functions.items():
        target_symbol = target_functions.get(name)
        if target_symbol is None or target_symbol.size != our_symbol.size:
            continue
        ours = body_of(our_data, our_sections, our_symbol)
        theirs = body_of(target_data, target_sections, target_symbol)
        if ours == theirs:
            continue
        differing = [
            offset for offset in range(0, len(ours), 4)
            if _u32(ours, offset) != _u32(theirs, offset)
        ]
        candidates = []
        for offset in differing:
            word, wanted = _u32(ours, offset), _u32(theirs, offset)
            mine, yours = decode_rlwinm(word), decode_rlwinm(wanted)
            if mine is None or yours is None:
                continue
            if mine[2] != yours[2] or mine[5] != yours[5]:
                continue                    # SH/Rc differ: not a mask pair
            if (mine[3], mine[4]) == (yours[3], yours[4]):
                continue                    # a pure recolor, not a mask pair
            if (mine[0], mine[1]) != (yours[0], yours[1]):
                # A site needing BOTH a mask rewrite and a recolor.  Outside
                # this class by construction, counted so the payoff of a
                # combined mode is a measured number rather than a guess.
                ADJACENT.append(f"{unit}::{name}+0x{offset:x}")
                continue
            try:
                required = redundant_mask_source_bits(word, wanted)
            except ValueError:
                continue
            candidates.append((offset, word, wanted, required))
        if not candidates:
            continue

        try:
            our_relocations = _function_text_relocations(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            jump = _jumptable_targets(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            words = [_u32(ours, at) for at in range(0, len(ours), 4)]
            relocated = {at // 4 for at in our_relocations}
            types = {at // 4: kind
                     for at, (kind, _n) in our_relocations.items()}
            successors, calls = _successors(
                words, relocated, {at // 4 for at in jump})
        except ValueError as failure:
            rows.append({
                "unit": unit, "function": name, "insns": len(ours) // 4,
                "sites": len(candidates), "proved": 0,
                "remaining": len(differing) - len(candidates),
                "status": f"cfg-refused: {failure}",
            })
            continue

        proved = []
        refused = []
        for offset, word, _wanted, required in candidates:
            try:
                prove_zero_bits(
                    words, offset // 4, decode_rlwinm(word)[1], required,
                    successors, calls, relocated, relocation_types=types)
                proved.append(offset)
            except ValueError as failure:
                refused.append((offset, str(failure)))

        remaining = len(differing) - len(proved)
        rows.append({
            "unit": unit,
            "function": name,
            "insns": len(ours) // 4,
            "sites": len(candidates),
            "proved": len(proved),
            "proved_at": [f"0x{at:x}" for at in proved],
            "obligations": [f"0x{req:08x}" for *_r, req in candidates],
            "remaining": remaining,
            "status": ("CLOSED BY THIS CLASS ALONE" if remaining == 0
                       else "needs composition" if proved
                       else "no site proves"),
            "refusals": [f"+0x{at:x}: {why}" for at, why in refused],
        })


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", type=Path, default=None)
    args = parser.parse_args()

    objects = sorted((BUILD / "obj").rglob("*.o"))
    if not objects:
        print("no target objects under build/GUNE5D/obj — run ninja first")
        return 1
    rows: list = []
    for target_path in objects:
        unit = str(target_path.relative_to(BUILD / "obj")).replace("\\", "/")
        unit = unit[:-2] if unit.endswith(".o") else unit
        try:
            screen_unit(target_path, unit, rows)
        except (ValueError, KeyError, IndexError, struct_error()) as failure:
            print(f"  ! {unit}: {failure}")

    rows.sort(key=lambda row: (-row["proved"], row["remaining"]))
    print(f"scanned {len(objects)} target objects; "
          f"{len(rows)} function(s) carry a range-proof-class candidate\n")
    header = f"{'proved':>6} {'sites':>5} {'remain':>6} {'insns':>6}  function"
    print(header)
    print("-" * len(header))
    for row in rows:
        print(f"{row['proved']:>6} {row['sites']:>5} {row['remaining']:>6} "
              f"{row['insns']:>6}  {row['unit']}::{row['function']}"
              f"  [{row['status']}]")
    closed = [row for row in rows if row["remaining"] == 0 and row["proved"]]
    print(f"\nclosable by the class ALONE: {len(closed)}")
    for row in closed:
        print(f"  {row['unit']}::{row['function']} "
              f"at {','.join(row['proved_at'])} "
              f"obligations {','.join(row['obligations'])}")
    print(f"\nADJACENT population (mask AND recolor on the same word, "
          f"refused by this class by name): {len(ADJACENT)}")
    for site in ADJACENT[:40]:
        print(f"  {site}")
    if len(ADJACENT) > 40:
        print(f"  ... and {len(ADJACENT) - 40} more")
    if args.json:
        args.json.write_text(json.dumps(rows, indent=1), encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


def struct_error():
    import struct
    return struct.error


if __name__ == "__main__":
    raise SystemExit(main())
