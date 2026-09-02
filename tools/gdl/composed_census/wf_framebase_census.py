#!/usr/bin/env python3
"""Image-wide census of the UNIFORM FRAME-BASE DELTA class.

Motivating measurement (run 34): DrawPsysSub, MBDrawPsys (game/mb/mb_particle)
and fn_8005F0F4 (game/world/gauntworld) each carry a residual whose differing
words are r1-relative displacements that all differ from the target by ONE
constant (+8).  Frame SIZE matches; only the base of the locals block moves.
Discipline 2 says count a shape seen twice before probing it, so this counts
the shape image-wide.

A row is a FRAME-SLOT row when our word and the target's word at the same
offset agree in opcode, in the destination/source register field and in the
base register field, that base register is r1, and only the 16-bit signed
displacement differs.  delta = target_displacement - our_displacement.

Per function the census reports the distinct deltas.  A function whose
frame-slot rows all share ONE nonzero delta and which has NO other differing
word is closed entirely by fixing the frame base; one with other differing
words needs that fix plus whatever else remains, and is reported separately
so the payoff is a measured number rather than an argument.

LIMIT, inherited from every offset-paired census in this project and stated
because the number does not announce it: words are compared at the SAME BYTE
OFFSET, so on a schedule-class function the pairing invents rows out of two
differently ordered streams, and a genuine frame row displaced by a schedule
difference is never recognised.  Confirm every hit with `fndiff --ops`
before believing it.

Run from the repository root after a completed `ninja`:

    python tools/gdl/composed_census/wf_framebase_census.py [--json PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import _sections, _symbols, _u32  # noqa: E402

BUILD = ROOT / "build" / "GUNE5D"

# D-form memory opcodes (lwz..stfdu) plus addi, the two ways a frame slot is
# named.  Anything else cannot carry an r1 displacement in the D field.
_MEM_OPCODES = set(range(32, 56))
_ADDI = 14
_FRAME_OPCODES = _MEM_OPCODES | {_ADDI}


def _decode_dform(word: int):
    """Return (opcode, rD, rA, signed displacement) for a D-form word."""
    opcode = word >> 26
    if opcode not in _FRAME_OPCODES:
        return None
    displacement = word & 0xFFFF
    if displacement >= 0x8000:
        displacement -= 0x10000
    return opcode, (word >> 21) & 0x1F, (word >> 16) & 0x1F, displacement


def our_object(unit_relative: Path) -> Path | None:
    """The RAW compiler output for a unit, preferring the pre-WebFrank body.

    Scoring the postprocessed object would hide exactly the words a rule
    already rewrites, so the raw body is the honest input.
    """
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

        deltas: Counter = Counter()
        other = 0
        for offset in range(0, len(ours), 4):
            word, wanted = _u32(ours, offset), _u32(theirs, offset)
            if word == wanted:
                continue
            mine, yours = _decode_dform(word), _decode_dform(wanted)
            if (
                mine is None or yours is None
                or mine[0] != yours[0]          # different opcode
                or mine[1] != yours[1]          # different rD/rS
                or mine[2] != yours[2]          # different base register
                or mine[2] != 1                 # base is not r1
            ):
                other += 1
                continue
            deltas[yours[3] - mine[3]] += 1

        if not deltas:
            continue
        rows.append({
            "unit": unit,
            "function": name,
            "insns": len(ours) // 4,
            "frame_rows": sum(deltas.values()),
            "deltas": {str(k): v for k, v in sorted(deltas.items())},
            "uniform_delta": (
                next(iter(deltas)) if len(deltas) == 1 else None
            ),
            "other_differing_words": other,
        })


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--json", default=str(BUILD / "wf_framebase_census.json"),
        help="where to write the full result (default under build/GUNE5D)",
    )
    args = parser.parse_args()

    rows: list = []
    for target_path in sorted((BUILD / "obj").rglob("*.o")):
        unit = str(target_path.relative_to(BUILD / "obj")).replace("\\", "/")
        try:
            screen_unit(target_path, unit, rows)
        except Exception as error:                       # noqa: BLE001
            print(f"skip {unit}: {error}")

    uniform = [r for r in rows if r["uniform_delta"] not in (None, 0)]
    pure = [r for r in uniform if r["other_differing_words"] == 0]
    uniform.sort(key=lambda r: -r["frame_rows"])

    print(f"functions with any frame-slot displacement row : {len(rows)}")
    print(f"  of those, ONE uniform nonzero delta          : {len(uniform)}")
    print(f"  of those, NO other differing word (closable) : {len(pure)}")
    spread: Counter = Counter(r["uniform_delta"] for r in uniform)
    print("\ndelta histogram (uniform-delta functions):")
    for delta, count in sorted(spread.items(), key=lambda kv: -kv[1]):
        print(f"  delta {delta:+5d} : {count} function(s)")

    print("\ntop uniform-delta functions by frame rows:")
    for row in uniform[:25]:
        print(
            f"  {row['unit']}::{row['function']} "
            f"({row['insns']}i) delta {row['uniform_delta']:+d} "
            f"rows {row['frame_rows']} other {row['other_differing_words']}"
        )

    if pure:
        print("\nCLOSABLE BY THE FRAME BASE ALONE (no other differing word):")
        for row in sorted(pure, key=lambda r: -r["frame_rows"]):
            print(
                f"  {row['unit']}::{row['function']} "
                f"({row['insns']}i) delta {row['uniform_delta']:+d} "
                f"rows {row['frame_rows']}"
            )

    Path(args.json).write_text(json.dumps(rows, indent=2), encoding="utf-8")
    print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
