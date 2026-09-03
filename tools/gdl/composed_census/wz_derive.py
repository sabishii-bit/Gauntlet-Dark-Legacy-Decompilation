"""WZ lane (run 41): derive + machine-prove the two banked live-zero customers.

game/sfx/psfx::LoadPdataFile and game/pb/pb_diag::pbDiagDrawMenu.  Same
harness shape as tools/gdl/composed_census/wf_livezero_derive.py: every hash
is produced from the objects ninja built, and every rule is run through the
real webfrank.apply_patch against the extracted retail object before it is
printed.

    python WZ_scratch/wz_derive.py [--out PATH]
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    _parent = os.path.dirname(ROOT)
    if _parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = _parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                              # noqa: E402
from cn_analyze import our_object, target_object    # noqa: E402

CASES = [
    {
        "unit": "game/sfx/psfx",
        "name": "LoadPdataFile",
        "windows": [(0x118, 0x120, [1, 0])],
        "zero_forms": [],
        "copy_forms": [(0x118, "constant_dataflow_recolor", 23)],
        "recolor": True,
    },
    {
        "unit": "game/pb/pb_diag",
        "name": "pbDiagDrawMenu",
        "windows": [(0x74, 0x7C, [1, 0]), (0x148, 0x150, [1, 0])],
        "zero_forms": [],
        "copy_forms": [(0x118, "dominating_def", None)],
        "recolor": True,
    },
]


def function_bytes(data, sections, name):
    symbol = wf._find_symbol(data, sections, name)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return symbol, start, bytes(data[start:start + symbol.size])


def window_rule(data, sections, symbol, body, relocations, lo, hi, order):
    region = body[lo:hi]
    atoms = [region[i * 4:i * 4 + 4] for i in range((hi - lo) // 4)]
    permuted = b"".join(atoms[source] for source in order)

    records = []
    for section in sections:
        if (section.section_type == wf.SHT_RELA
                and section.info == symbol.section_index):
            for offset in range(section.offset,
                                section.offset + section.size, 12):
                records.append(wf.struct.unpack_from(">IIi", data, offset))
    region_start = symbol.value + lo
    region_end = symbol.value + hi
    region_records = [
        (offset - region_start, info, addend)
        for offset, info, addend in records
        if region_start <= offset < region_end
    ]
    window_symbols = {
        offset - lo: name
        for offset, (_kind, name) in relocations.items()
        if lo <= offset < hi
    }
    before = wf._relocation_sha256(region_records, window_symbols)
    destination_by_source = {
        source: destination for destination, source in enumerate(order)
    }
    moved = []
    moved_symbols = {}
    for offset, info, addend in region_records:
        moved_offset = destination_by_source[offset // 4] * 4 + offset % 4
        moved.append((moved_offset, info, addend))
        moved_symbols[moved_offset] = window_symbols[offset]
    moved.sort(key=lambda item: item[0])
    after = wf._relocation_sha256(moved, moved_symbols)

    print(f"  window [0x{lo:x},0x{hi:x}) order {order}: "
          f"{len(region_records)} relocation(s) inside")
    for offset, info, addend in region_records:
        print(f"    +0x{offset:x} type {info & 0xFF} "
              f"{window_symbols.get(offset)} addend {addend}")
    return {
        "start": hex(lo), "end": hex(hi), "order": order,
        "before_sha256": wf._sha256(region),
        "after_sha256": wf._sha256(permuted),
        "before_relocations_sha256": before,
        "after_relocations_sha256": after,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build/GUNE5D/wz_customer_rules.json"))
    arguments = parser.parse_args()

    derived = {}
    failures = 0
    for case in CASES:
        ours_path, _kind = our_object(case["unit"])
        target_path = target_object(case["unit"])
        odata = bytearray(open(ours_path, "rb").read())
        tdata = bytearray(open(target_path, "rb").read())
        osec, tsec = wf._sections(odata), wf._sections(tdata)
        symbol, ostart, ours = function_bytes(odata, osec, case["name"])
        _t, _ts, target = function_bytes(tdata, tsec, case["name"])
        print(f"\n=== {case['unit']}::{case['name']}: {len(ours) // 4} insns "
              f"(target {len(target) // 4}) ===")
        relocations = wf._function_text_relocations(
            odata, osec, symbol.section_index,
            symbol.value, symbol.value + symbol.size)
        windows = [
            window_rule(odata, osec, symbol, ours, relocations, lo, hi, order)
            for lo, hi, order in case["windows"]
        ]
        rule = {
            "function": case["name"],
            "before_sha256": wf._sha256(ours),
            "after_sha256": wf._sha256(target),
            "audit": {
                "classification": "MIXED",
                "instructions": len(ours) // 4,
                "permuted_instructions": sum(
                    len(order) for _lo, _hi, order in case["windows"]),
            },
            "instruction_permutation": (
                windows[0] if len(windows) == 1 else windows),
        }
        if case["zero_forms"]:
            rule["equivalent_zero_form"] = [
                {"at": hex(at), "proof": "zero_value_dataflow",
                 "declared_zero_register": register}
                for at, register in case["zero_forms"]
            ]
        if case["copy_forms"]:
            edits = []
            for at, proof, source in case["copy_forms"]:
                edit = {"at": hex(at), "proof": proof}
                if source is not None:
                    edit["our_source"] = source
                edits.append(edit)
            rule["equivalent_copy_form"] = edits
        if case["recolor"]:
            rule["copy_register_fields"] = True

        probe = bytearray(odata)
        try:
            _before, _after, changed = wf.apply_patch(
                probe, json.loads(json.dumps(rule)), bytes(tdata))
        except ValueError as failure:
            print(f"  REFUSED: {failure}")
            failures += 1
            continue
        final = bytes(probe[ostart:ostart + symbol.size])
        print(f"  apply_patch OK: changed={changed}")
        print(f"  BYTE-EQUAL TO TARGET: {final == target}")
        if final != target:
            for i in range(0, len(final), 4):
                if final[i:i + 4] != target[i:i + 4]:
                    print(f"    +0x{i:04x} O {final[i:i+4].hex()} "
                          f"T {target[i:i+4].hex()}")
            failures += 1
            continue
        derived.setdefault(case["unit"], []).append(rule)

    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    json.dump(derived, open(arguments.out, "w"), indent=2)
    print(f"\nwrote {arguments.out}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
