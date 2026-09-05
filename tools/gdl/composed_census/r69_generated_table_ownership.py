"""Audit compiler-generated switch tables before assigning their TU ownership.

This is an ownership certificate, NOT a byte-match or CFG-equivalence proof.
The complete raw Ninja compilation is reproduced. Every emitted data word must
be an ADDR32 relocation into the expected owning function; every retail word
must independently point inside that same target function. Different branch
offsets remain explicit debt, never rewritten to retail offsets.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory
from tools.gdl import datadiff, fndiff, webfrank

CASES = {
    "game/shop/shop": (0x8012318C, 0x801232C8, (
        ("jumptable_8012318C", "do_shopping_8009AA48", 0, 40),
        ("jumptable_8012322C", "calculate_player_shopping_parameters_8009C0F0", 160, 39))),
    "game/mb/mb_particle": (0x80129544, 0x80129588, (
        ("jumptable_80129544", "MBDrawPsys", 0, 9),
        ("jumptable_80129568", "setupNewPMode_800CDCE4", 36, 8))),
}


def obligation(snapshot, base, end, tables, target_bytes, target_symbols):
    section = snapshot["sections"].get(".data")
    size = end - base
    if not snapshot.get("fidelity") or not section:
        raise ValueError("fresh raw fidelity and initialized data required")
    if (section["type"], section["flags"], section["size"], section["alignment"]) != (1, 3, size, 8):
        raise ValueError("complete source data extent/type/alignment mismatch")
    if len(target_bytes) != size or bytes.fromhex(section["bytes"]) != bytes(size):
        raise ValueError("unexpected data payload outside explicit relocations")
    objects = sorted((s for s in snapshot["symbols"] if s["section"] == ".data" and s["type"] == 1),
                     key=lambda s: s["value"])
    if [(s["value"], s["size"], s["binding"]) for s in objects] != [(off, count*4, 0) for _, _, off, count in tables]:
        raise ValueError("compiler local table coverage differs")
    relocs = snapshot["relocations"].get(".data", [])
    if len(relocs) != size // 4 or sorted(r[0] for r in relocs) != list(range(0, size, 4)):
        raise ValueError("every data word must have one relocation")
    rows, previous_end = [], 0
    for label, function, offset, count in tables:
        if offset != previous_end or tuple(target_symbols[label][:3]) != (".data", base+offset, count*4):
            raise ValueError("target table identity/extent or complete coverage differs")
        previous_end = offset + count*4
        target_fn = target_symbols[function]
        source_fn = snapshot["functions"].get(function)
        if target_fn[0] != ".text" or not source_fn:
            raise ValueError("missing owning function")
        table_symbol = next(s for s in objects if s["value"] == offset)
        uses = [r for r in source_fn["relocations"] if r[2] == table_symbol["name"]]
        if sorted((r[1], r[3]) for r in uses) != [(4, 0), (6, 0)]:
            raise ValueError("table lacks owning function HA/LO dispatch references")
        for at, kind, name, addend in relocs:
            if not offset <= at < previous_end:
                continue
            if kind != 1 or name != function or addend % 4 or not 0 <= addend < source_fn["size"]:
                raise ValueError("source branch target is not inside the expected function")
            absolute = struct.unpack_from(">I", target_bytes, at)[0]
            target_addend = absolute - target_fn[1]
            if target_addend % 4 or not 0 <= target_addend < target_fn[2]:
                raise ValueError("retail branch target is not inside the expected function")
            rows.append(dict(table=label, function=function, table_offset=at, source_offset=addend,
                             target_offset=target_addend, equal=addend == target_addend))
    if previous_end != size:
        raise ValueError("incomplete table extent")
    return dict(status="PASS", extent=size, source_alignment=8, target_claim_alignment=4,
                tables=len(tables), pointers=len(rows), equal_pointers=sum(r["equal"] for r in rows),
                differing_pointers=[r for r in rows if not r["equal"]], rows=rows,
                limit="Same function and dispatch-table identity; not equal basic blocks or whole-TU data recovery")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unit", choices=CASES)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / "build") or output.suffix != ".json":
        parser.error("output must be JSON under this checkout's build")
    result = dict(status="UNRESOLVED", unit=args.unit)
    try:
        base, end, tables = CASES[args.unit]
        current = capture(args.unit)
        target = webfrank.RetailImage(ROOT / "orig/GUNE5D/sys/main.dol").read(base, end-base)
        if target is None:
            raise ValueError("retail table bytes unavailable")
        certificate = obligation(current, base, end, tables, target, fndiff.symbol_table())
        claim = datadiff.parse_splits()[args.unit+".c"].get(".data")
        if claim is not None:
            if claim != (base, end):
                raise ValueError("claim differs from independently audited tables")
            extracted = object_inventory(ROOT / f"build/GUNE5D/obj/{args.unit}.o")
            if extracted["sections"][".data"]["size"] != end-base:
                raise ValueError("fresh extracted data extent differs")
            if set(n for n, s in extracted["symbols"].items() if s["section"] == ".data") != set(t[0] for t in tables):
                raise ValueError("extracted named table ownership differs")
        result.update(status="PASS", certificate=certificate, raw_sha256=current["raw_sha256"],
                      claim=claim, source_sha256=current["source_sha256"], functions=len(current["functions"]),
                      current=current)
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(result["status"], result.get("error", "ownership measured; branch-offset debt retained"), output)
    if "certificate" in result:
        c = result["certificate"]
        print("tables", c["tables"], "bytes", c["extent"], "pointers", c["pointers"], "equal", c["equal_pointers"])
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
