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
from webfrank import (  # noqa: E402
    Section,
    Symbol,
    _function_text_relocations,
    _jumptable_targets,
    _sections,
    _symbols,
    register_slot_mask,
    verify_consistent_recolor,
)


REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"
REGISTER_SHIFTS = (6, 11, 16, 21)


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


# verify_consistent_recolor's refusal: "+0xNN: use of gA does not correspond
# to gB under the running renaming" (apply_patch parenthesises it).
_RECOLOR_REFUSAL = re.compile(
    r"\(?\+0x([0-9a-f]+): use of \w+ does not correspond to \w+"
    r" under the running renaming"
)
REPAIR_TOOL = "tools/gdl/composed_census/hv_repair.py"


def classify_rejection(unit: str, function: str, reason: str) -> dict:
    """Annotate a rejection with its repair route, if it has one.

    A verify_consistent_recolor refusal is a POINTER, NOT A VERDICT: it
    names the merge where a two-valued binding was observed, and the repair
    is usually a transposition of the pair of definitions UPSTREAM of it —
    a window that contributes ZERO unabsorbed words, so no census proposes
    it and no reader of this message would think to look. hv_repair
    searches exactly that space. Every rejection this scanner emitted when
    the cross-reference was added (11 of 11) was of this class, and not one
    of them said so.

    AGENTS.md discipline 14: a guard's refusal is a measurement of the
    GUARD, not only of the function.
    """
    match = _RECOLOR_REFUSAL.search(reason or "")
    if not match:
        return {"repair_candidate": False}
    return {
        "repair_candidate": True,
        "refusal_offset": f"0x{match.group(1)}",
        "repair_hint": (
            "verify_consistent_recolor refusal — a POINTER to a two-valued"
            " binding, not proof the function is ineligible. The repairing"
            " transposition lives UPSTREAM of this offset and contributes no"
            " unabsorbed words, so no census will propose it."),
        "next": f"python {REPAIR_TOOL} {unit} {function}",
    }


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
        allowed = register_slot_mask(current)
        if difference & ~allowed:
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
    parser.add_argument("--grep", help="only UNITS whose path contains this"
                                       " text (game/enemy, dolphin/card) —"
                                       " NOT a function name; use --function"
                                       " for that")
    parser.add_argument("--function", help="only functions with this NAME"
                                           " (exact, else substring), across"
                                           " every unit --grep leaves")
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
    units_total = 0
    units_selected = 0
    units_parsed = 0
    functions_selected = 0

    for command in commands:
        source = Path(command["file"])
        relative = source.relative_to(REPO / "src")
        unit = str(relative.with_suffix("")).replace("\\", "/")
        units_total += 1
        if args.grep and args.grep not in unit:
            continue
        units_selected += 1
        target_path = REPO / "build" / VERSION / "obj" / relative.with_suffix(".o")
        base_path = Path(command["output"])
        # For units that already have a postprocessor, command output is the
        # rewritten object; audit the raw compiler output instead.
        raw_body = base_path.parent / ".postprocess" / "body" / base_path.name
        if raw_body.is_file():
            base_path = raw_body
        if not target_path.is_file() or not base_path.is_file():
            continue
        units_parsed += 1
        target_functions = parse(target_path)
        base_functions = parse(base_path)
        for function, target_lines in target_functions.items():
            base_lines = base_functions.get(function)
            if base_lines is None:
                continue
            if args.function and not (args.function == function
                                      or args.function in function):
                continue
            functions_selected += 1
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
                # A byte-confined register diff is not yet proof of a recolor:
                # demand the position-consistent renaming bisimulation, so a
                # reorder or crossed value web is surfaced instead of emitted.
                base_data = base_path.read_bytes()
                base_sections = _sections(base_data)
                fn_start = base_symbol.value
                fn_end = fn_start + base_symbol.size
                text_relocations = _function_text_relocations(
                    base_data, base_sections, base_symbol.section_index,
                    fn_start, fn_end,
                )
                verify_consistent_recolor(
                    base_bytes, target_bytes,
                    jumptable_targets=_jumptable_targets(
                        base_data, base_sections, base_symbol.section_index,
                        fn_start, fn_end,
                    ),
                    relocated_offsets=set(text_relocations),
                    call_targets={
                        offset: name
                        for offset, (rtype, name) in text_relocations.items()
                        if rtype == 10
                    },
                )
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
                row = {
                    "unit": unit,
                    "function": function,
                    "instructions": instruction_count,
                    "reason": str(error),
                }
                row.update(classify_rejection(unit, function, str(error)))
                rejected.append(row)

    # AN EMPTY SELECTION IS NOT A VERDICT (run-55 item 3). Reported by CR:
    # `webfrank_audit.py --grep CritterCollideItems` printed
    # `0 register-only; 0 eligible in 0 TUs` for a function that was in fact
    # eligible — `--grep` filters UNIT PATHS, so a function name matches
    # nothing and the tool answers the question it was not asked. The
    # tool's own footer already says absence is not ineligibility
    # (claim.law.RQ_webfrank-audit-silence-is-not-ineligibility), and that
    # is exactly the sentence a reader discounts when the headline reads
    # like a measurement.
    #
    # CALIBRATED TWO-SIDED at e8e3959d0 over 256 units in
    # compile_commands.json and the 3,024 distinct function names in the
    # split objects: 2,981 names (98.6%) match ZERO unit paths, so this
    # refusal catches essentially the whole confusion class. The other 43
    # (1.4%) are SDK functions whose name IS their unit path (CARDCheck ->
    # dolphin/card/CARDCheck, GXInit -> dolphin/gx/GXInit); for those
    # `--grep <fn>` selects the right TU anyway, so nothing is lost by the
    # refusal not firing. Every legitimate path grep still passes:
    # `--grep game` 92 units, `--grep dolphin` 90, `--grep MSL` 40,
    # `--grep game/ui/select` 1.
    if args.grep and units_selected == 0:
        print(f"REFUSED: --grep {args.grep!r} matched 0 of {units_total}"
              " unit paths, so NOTHING WAS SCANNED. This flag filters UNIT"
              " PATHS (game/enemy, dolphin/card), not function names —"
              f" if {args.grep!r} is a function, run"
              f" `--function {args.grep}` instead. A zero here is an empty"
              " scope, never a verdict of ineligibility"
              " (claim.law.RQ_webfrank-audit-silence-is-not-ineligibility).")
        return 2
    if units_parsed == 0:
        print(f"REFUSED: {units_selected} unit(s) selected of {units_total},"
              " and NONE had both a split target object and a compiled"
              " object, so nothing was scanned. Run `ninja` first (and note"
              " that build/GUNE5D/obj/** is the dtk SPLIT reference, not a"
              " build artefact — `provision_worktree.py --resplit` restores"
              " it). Every count below would have been a zero reached by"
              " not looking.")
        return 2
    if args.function and functions_selected == 0:
        scope = f" under --grep {args.grep!r}" if args.grep else ""
        print(f"REFUSED: --function {args.function!r} matched 0 functions in"
              f" {units_parsed} scanned unit(s){scope}, so NOTHING WAS"
              " SCANNED. Check the spelling against"
              " `python tools/gdl/fndiff.py <unit> --count`; a zero here is"
              " an empty scope, never a verdict of ineligibility.")
        return 2

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
        f"  [scope: {units_selected} of {units_total} unit(s)"
        + (f", {functions_selected} function(s) matched"
           f" --function {args.function!r}" if args.function else "")
        + "]"
    )
    for item in rejected:
        print(
            f"  REJECT {item['unit']}::{item['function']} "
            f"({item['instructions']} insns): {item['reason']}"
        )
        if item.get("repair_candidate"):
            print(f"    -> REPAIRABLE CLASS: {item['repair_hint']}")
            print(f"    -> try: {item['next']}")
    repairable = [item for item in rejected if item.get("repair_candidate")]
    if repairable:
        print(
            f"  {len(repairable)} of {len(rejected)} rejection(s) are"
            " verify_consistent_recolor refusals, which are POINTERS, not"
            " verdicts: do NOT record them as ineligibility without running"
            f" {REPAIR_TOOL} first (AGENTS.md discipline 14 — a guard's"
            " refusal measures the GUARD as much as the function)."
        )
    print(
        "NO OPINION on functions not listed above: this scanner only sees"
        " single-class REGISTER_ONLY candidates. Absence here is NOT"
        " ineligibility — mixed schedule+recolor parks close via the manual"
        " composed 4-step (claim.law: audit silence is not ineligibility)."
    )
    try:
        printable = output_path.relative_to(REPO)
    except ValueError:
        printable = output_path
    print(f"report: {printable}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
