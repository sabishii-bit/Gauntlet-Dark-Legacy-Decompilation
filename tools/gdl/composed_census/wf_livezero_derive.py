"""Derive and machine-prove the four run-37 live-zero WebFrank rules.

This is the audit trail for the rules in ``config/GUNE5D/webfrank.json`` for
game/ui/btext::{DrawStringTextMLines, FontInit, FindStringMessageListSub_
8001FC4C} and game/sys/memcard::init_all_dir_info.  Every hash in those
rules is produced here from the objects ninja built, and every rule is run
through the real ``webfrank.apply_patch`` against the extracted retail object
before it is printed, so re-running this after any rebuild re-establishes
that the shipped rules still reproduce the target bytes exactly.

Two mechanisms, both members of the live-zero copy-vs-remat family
(claim.law.live-zero-copy-vs-remat-is-allocator-not-source.20260831.v1):

  * DrawStringTextMLines / FontInit -- ours SHIFTS a live zero
    (``slwi rD,rS,2``) where the target COPIES it (``addi rD,rS,0``).  Our
    word is not a copy form at all, so ``equivalent_copy_form`` refuses it
    outright; these are the first two members of the LIVE-ZERO VALUE class
    (``equivalent_zero_form`` / ``"proof": "zero_value_dataflow"``).
  * FindStringMessageListSub / init_all_dir_info -- ours REMATERIALISES the
    zero (``li rD,0``) where the target copies it.  That pair the EXISTING
    combined copy-form mode already models; they are here because the same
    census turned them up and because init_all_dir_info's own record had
    denied that the existing composition could take it.

    python tools/gdl/composed_census/wf_livezero_derive.py [--out PATH]

Run from the repository root after a completed ninja.  Writes the derived
rules to ``build/GUNE5D/wf_livezero_rules.json`` by default -- never beside
this script.
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
sys.path.insert(0, HERE)

import webfrank as wf                              # noqa: E402
from cn_analyze import our_object, target_object    # noqa: E402

CASES = [
    {
        "unit": "game/ui/btext",
        "name": "DrawStringTextMLines",
        "windows": [(0x1F4, 0x1FC, [1, 0])],
        "zero_forms": [(0x1F4, 27)],
        "copy_forms": [],
        "recolor": False,
    },
    {
        "unit": "game/ui/btext",
        "name": "FontInit",
        "windows": [(0x28, 0x30, [1, 0])],
        "zero_forms": [(0x28, 30)],
        "copy_forms": [],
        "recolor": False,
    },
    {
        "unit": "game/ui/btext",
        "name": "FindStringMessageListSub_8001FC4C",
        "windows": [(0x1C, 0x2C, [2, 0, 3, 1])],
        "zero_forms": [],
        "copy_forms": [(0x24, 28)],
        "recolor": True,
    },
    {
        "unit": "game/sys/memcard",
        "name": "init_all_dir_info",
        "windows": [(0x14, 0x28, [1, 2, 3, 0, 4]), (0x68, 0x70, [1, 0])],
        "zero_forms": [],
        "copy_forms": [(0x18, 29), (0x1C, 29)],
        "recolor": True,
    },
]


def function_bytes(data, sections, name):
    symbol = wf._find_symbol(data, sections, name)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return symbol, start, bytes(data[start:start + symbol.size])


def window_rule(data, sections, symbol, body, relocations, lo, hi, order):
    """One permutation window, with its four hashes derived from the object."""
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
        ROOT, "build/GUNE5D/wf_livezero_rules.json"))
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
            rule["equivalent_copy_form"] = [
                {"at": hex(at), "proof": "constant_dataflow_recolor",
                 "our_source": source}
                for at, source in case["copy_forms"]
            ]
        if case["recolor"]:
            rule["copy_register_fields"] = True

        probe = bytearray(odata)
        try:
            _before, _after, changed = wf.apply_patch(
                probe, dict(rule), bytes(tdata))
        except ValueError as failure:
            print(f"  REFUSED: {failure}")
            failures += 1
            continue
        final = bytes(probe[ostart:ostart + symbol.size])
        print(f"  apply_patch OK: changed={changed}")
        print(f"  BYTE-EQUAL TO TARGET: {final == target}")
        if final != target:
            failures += 1
            continue
        derived.setdefault(case["unit"], []).append(rule)

    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    json.dump(derived, open(arguments.out, "w"), indent=2)
    print(f"\nwrote {arguments.out}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
