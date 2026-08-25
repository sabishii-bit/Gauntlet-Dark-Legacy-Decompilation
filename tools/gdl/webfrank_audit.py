#!/usr/bin/env python3
"""Audit all REGISTER_ONLY residuals for fail-closed webfrank eligibility.

This does not modify normal build objects or configuration.  It proves that
each candidate has identical instruction/relocation shape, then checks that
every raw difference occupies one of PowerPC's four five-bit register slots.
The emitted JSON contains semantic register-field edits plus complete function
hashes and can be reviewed before merging into ``webfrank.json``.

Usage:
  python tools/gdl/webfrank_audit.py
  python tools/gdl/webfrank_audit.py --min-insns 50 --grep game/enemy
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from fndiff import classify_function, instruction_lines, parse  # noqa: E402
from webfrank import Section, Symbol, _sections, _symbols  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"
REGISTER_SHIFTS = (6, 11, 16, 21)
REGISTER_MASK = sum(0x1F << shift for shift in REGISTER_SHIFTS)


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def _normalized_symbol(data: bytes, sections: list[Section], name: str) -> Symbol:
    symbols = _symbols(data, sections)
    exact = [symbol for symbol in symbols if symbol.name == name and symbol.size]
    if exact:
        return exact[0]
    suffix = re.compile(rf"^{re.escape(name)}_80[0-9A-Fa-f]{{6}}$")
    matches = [symbol for symbol in symbols if suffix.match(symbol.name) and symbol.size]
    if len(matches) == 1:
        return matches[0]
    raise KeyError(f"symbol {name!r} not found")


def _function_bytes(path: Path, name: str, *, target: bool) -> tuple[Symbol, bytes]:
    data = path.read_bytes()
    sections = _sections(data)
    symbol = _normalized_symbol(data, sections, name)
    section = sections[symbol.section_index]
    start = section.offset + symbol.value
    return symbol, data[start:start + symbol.size]


def _field_edits(base: bytes, target: bytes) -> list[dict]:
    if len(base) != len(target) or len(base) % 4:
        raise ValueError("function sizes are not equal, aligned words")
    edits = []
    probe = bytearray(base)
    for offset in range(0, len(base), 4):
        current = struct.unpack_from(">I", base, offset)[0]
        wanted = struct.unpack_from(">I", target, offset)[0]
        difference = current ^ wanted
        if not difference:
            continue
        if difference & ~REGISTER_MASK:
            raise ValueError(
                f"non-register bits differ at +0x{offset:x}: 0x{difference:08x}"
            )
        fields = {}
        recolored = current
        for shift in REGISTER_SHIFTS:
            mask = 0x1F << shift
            if difference & mask:
                value = (wanted >> shift) & 0x1F
                fields[str(shift)] = value
                recolored = (recolored & ~mask) | (value << shift)
        if recolored != wanted:
            raise ValueError(f"register fields did not reconstruct +0x{offset:x}")
        struct.pack_into(">I", probe, offset, recolored)
        edits.append({"at": f"0x{offset:x}", "set": fields})
    if probe != target:
        raise ValueError("recolored bytes do not match target")
    return edits


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grep", help="only units containing this text")
    parser.add_argument("--min-insns", type=int, default=0)
    parser.add_argument(
        "--compact", action="store_true",
        help="emit target-backed copy_register_fields rules instead of every field",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO / "build" / VERSION / "webfrank-audit.json",
    )
    args = parser.parse_args()

    commands = json.loads((REPO / "compile_commands.json").read_text())
    units: dict[str, list[dict]] = {}
    rejected = []
    register_only = 0
    total_bytes = 0

    for command in commands:
        source = Path(command["file"])
        relative = source.relative_to(REPO / "src")
        unit = str(relative.with_suffix("")).replace("\\", "/")
        if args.grep and args.grep not in unit:
            continue
        target_path = REPO / "build" / VERSION / "obj" / relative.with_suffix(".o")
        base_path = Path(command["output"])
        if not target_path.is_file() or not base_path.is_file():
            continue
        target_functions = parse(target_path)
        base_functions = parse(base_path)
        for function, target_lines in target_functions.items():
            base_lines = base_functions.get(function)
            if base_lines is None:
                continue
            if classify_function(target_lines, base_lines) != "REGISTER_ONLY":
                continue
            register_only += 1
            instruction_count = len(instruction_lines(target_lines))
            if instruction_count < args.min_insns:
                continue
            try:
                base_symbol, base_bytes = _function_bytes(
                    base_path, function, target=False
                )
                _, target_bytes = _function_bytes(target_path, function, target=True)
                edits = _field_edits(base_bytes, target_bytes)
                patch = {
                    "function": base_symbol.name,
                    "before_sha256": _sha256(base_bytes),
                    "after_sha256": _sha256(target_bytes),
                    "audit": {
                        "classification": "REGISTER_ONLY",
                        "instructions": instruction_count,
                    },
                }
                if args.compact:
                    patch["copy_register_fields"] = True
                else:
                    patch["register_fields"] = edits
                units.setdefault(unit, []).append(patch)
                total_bytes += len(target_bytes)
            except Exception as error:
                rejected.append(
                    {
                        "unit": unit,
                        "function": function,
                        "instructions": instruction_count,
                        "reason": str(error),
                    }
                )

    output = {
        "version": 1,
        "units": units,
        "audit": {
            "register_only_functions": register_only,
            "eligible_functions": sum(len(patches) for patches in units.values()),
            "eligible_units": len(units),
            "eligible_code_bytes": total_bytes,
            "rejected": rejected,
        },
    }
    output_path = args.output if args.output.is_absolute() else REPO / args.output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(
        f"WEBFRANK AUDIT: {register_only} register-only; "
        f"{output['audit']['eligible_functions']} eligible in {len(units)} TUs; "
        f"{total_bytes} code bytes; {len(rejected)} rejected"
    )
    for item in rejected:
        print(
            f"  REJECT {item['unit']}::{item['function']} "
            f"({item['instructions']} insns): {item['reason']}"
        )
    print(f"report: {output_path.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
