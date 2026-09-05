#!/usr/bin/env python3
"""Resolve AudioStreamPlay's ErrorPrintf arguments after the pool-base addi.

This is a narrow calibration case, not a general PPC value prover. It refuses
unless one r31 address definition precedes all control flow, no later write
clobbers that home before the last ErrorPrintf, and each argument is the known
three-instruction setup with no interior branch entry. Normal PPC EABI calls
preserve r31. Matching ordered strings does not prove function or TU equality.
Reads the active raw Ninja edge, not a stale postprocess-directory guess.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/gdl"))
sys.path.insert(0, str(ROOT / "tools/gdl/composed_census"))
import cv_probe
import fndiff

UNIT = "game/audio/audio"
FUNCTION = "AudioStreamPlay"
ALLOWED = {"mflr", "lis", "stw", "stwu", "stmw", "addi", "li", "lwz", "cmpwi", "beq", "b", "bne", "bl", "crclr", "bge", "clrlwi", "blt", "mr", "lmw", "mtlr", "blr"}
NO_DEST = {"stw", "stmw", "cmpwi", "beq", "b", "bne", "bl", "crclr", "bge", "blt", "mtlr", "blr"}


def instructions(lines):
    result = []
    for line in lines:
        if line.startswith("    R_PPC_"):
            if not result or result[-1]["reloc"] is not None:
                raise ValueError("unexpected relocation attachment")
            result[-1]["reloc"] = tuple(line.strip().split(maxsplit=1))
        else:
            fields = line.strip().split(maxsplit=1)
            if not fields or fields[0] not in ALLOWED:
                raise ValueError("unmodelled instruction: " + line)
            result.append({"offset": 4 * len(result), "op": fields[0], "args": fields[1] if len(fields) == 2 else "", "reloc": None})
    return result


def writes(row, reg):
    if row["op"] in NO_DEST:
        return False
    if row["op"] == "lmw":
        return int(row["args"].split(",", 1)[0][1:]) <= reg
    if row["op"] == "stwu":
        match = re.fullmatch(r"r\d+,-?\d+\(r(\d+)\)", row["args"])
        if not match:
            raise ValueError("unmodelled updating store operands")
        return int(match.group(1)) == reg
    return row["args"].split(",", 1)[0] == "r" + str(reg)


def argument_sites(lines):
    ins = instructions(lines)
    calls = [i for i, row in enumerate(ins) if row["op"] == "bl" and row["reloc"] == ("R_PPC_REL24", "ErrorPrintf")]
    if len(calls) != 4:
        raise ValueError("expected four ErrorPrintf sites; re-adjudicate the call roster")
    defs = [i for i, row in enumerate(ins[:calls[-1]]) if writes(row, 31)]
    if len(defs) != 1:
        raise ValueError("r31 is not one invariant address home")
    index = defs[0]
    lo = ins[index]
    match = re.fullmatch(r"r31,r(\d+),(-?\d+)", lo["args"])
    if lo["op"] != "addi" or not match or not lo["reloc"] or lo["reloc"][0] != "R_PPC_ADDR16_LO":
        raise ValueError("r31 definition is not the expected relocated low half")
    reg, low_imm = map(int, match.groups())
    previous = [row for row in ins[:index] if writes(row, reg)]
    if not previous or previous[-1]["op"] != "lis" or previous[-1]["reloc"] != ("R_PPC_ADDR16_HA", lo["reloc"][1]) or previous[-1]["args"] != f"r{reg},0" or low_imm:
        raise ValueError("unproved or nonzero HA/LO address pair")
    if any(row["op"].startswith("b") for row in ins[:index]):
        raise ValueError("base does not dominate control flow")
    targets = {int(m.group(1), 16) for row in ins for m in [re.search(r"<fn\+0x([0-9a-f]+)>", row["args"])] if m and row["op"].startswith("b")}
    sites = []
    for call in calls:
        setup, cr, arg4 = ins[call-3:call]
        match = re.fullmatch(r"r3,r31,(-?\d+)", setup["args"])
        if setup["op"] != "addi" or not match or setup["reloc"] or cr["op"] != "crclr" or cr["args"] != "4*cr1+eq" or arg4["op"] != "addi" or arg4["args"] != "r4,r30,1048" or cr["reloc"] or arg4["reloc"]:
            raise ValueError("unmodelled ErrorPrintf setup")
        if any(row["offset"] in targets for row in ins[call-2:call+1]):
            raise ValueError("branch enters after argument setup")
        sites.append({"call_offset": ins[call]["offset"], "base_symbol": lo["reloc"][1], "displacement": int(match.group(1))})
    return sites


def cstring(blob):
    if blob is None or b"\0" not in blob:
        raise ValueError("string bytes missing or unterminated")
    head = blob.split(b"\0", 1)[0]
    if not head or not all(32 <= c < 127 or c in (10, 13) for c in head):
        raise ValueError("not a supported text string")
    return head.decode("ascii")


def compare_messages(target, ours):
    if len(target) != 4 or len(ours) != 4:
        raise ValueError("message coverage is incomplete")
    return [dict(target=t, ours=o, equal=t["string"] == o["string"]) for t, o in zip(target, ours)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("build/GUNE5D/r67_audio_effective_strings.json"))
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to((ROOT / "build").resolve()):
        parser.error("--out must remain under this checkout's build/")
    result = {"schema_version": 1, "status": "UNRESOLVED", "scope": "Four ordered ErrorPrintf string arguments only; not whole function semantics, historical TU ownership, or link equality"}
    try:
        target = ROOT / "build/GUNE5D/obj/game/audio/audio.o"
        raw = ROOT / cv_probe.read_edges()[UNIT]["body_o"]
        tf, of = fndiff.parse(target), fndiff.parse(raw)
        ts, os = argument_sites(tf[FUNCTION]), argument_sites(of[FUNCTION])
        symbols, sections = fndiff.object_sections(raw)
        for site in ts:
            base, addend = fndiff.split_addend(site["base_symbol"])
            address = fndiff.symbol_table()[base][1] + addend + site["displacement"]
            site.update(address=address, string=cstring(fndiff.dol_read(address, 256)))
        for site in os:
            base, addend = fndiff.split_addend(site["base_symbol"])
            if base not in symbols:
                raise ValueError("our message is not in the compiled local pool; external bytes are not compiler-output proof")
            section, start, size = symbols[base]
            offset = start + addend + site["displacement"]
            if offset < 0 or offset >= len(sections.get(section, b"")):
                raise ValueError("our effective address escapes readable section")
            site.update(section=section, section_offset=offset, base_object_size=size, string=cstring(sections[section][offset:offset+256]))
        rows = compare_messages(ts, os)
        base_screen = fndiff.datum_screen_from_lines(tf[FUNCTION], of[FUNCTION], target, raw)
        result.update(status="PASS" if all(r["equal"] for r in rows) else "FAIL", rows=rows, relocation_base_multiset={k:base_screen[k] for k in ("verdict", "target_only", "ours_only", "target_relocs", "ours_relocs")}, object_sha256={"target":hashlib.sha256(target.read_bytes()).hexdigest(), "raw":hashlib.sha256(raw.read_bytes()).hexdigest()}, raw_object=str(raw.relative_to(ROOT)))
    except (OSError, KeyError, ValueError) as exc:
        result["error"] = str(exc)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
