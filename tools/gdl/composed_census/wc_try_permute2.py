"""WC lane (run 44): try a permutation ORDER change on a rule whose window
already carries relocations, computing all four window hashes exactly the way
apply_patch/permute_instruction_atoms do.

    python tools/gdl/composed_census/wc_try_permute2.py <unit> <function> \
        --window 0x28:0x70 --order 10,13,... [--form 0x34,0x40] [--closure]
"""
import argparse
import json
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from cn_analyze import our_object, target_object                # noqa: E402

SUB = re.compile(r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--window", required=True)
    parser.add_argument("--order", required=True)
    parser.add_argument("--form", default="")
    parser.add_argument("--closure", action="store_true")
    parser.add_argument("--audit", default="WC run 44 candidate")
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    start, end = (int(v, 0) for v in arguments.window.split(":"))
    order = [int(v) for v in arguments.order.split(",")]
    forms = [int(v, 0) for v in arguments.form.split(",")] \
        if arguments.form else []

    ours_path, kind = our_object(arguments.unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_object(arguments.unit), "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    osym = wf._find_symbol(odata, osec, arguments.function)
    tsym = wf._find_symbol(tdata, tsec, arguments.function)
    ostart = osec[osym.section_index].offset + osym.value
    tstart = tsec[tsym.section_index].offset + tsym.value
    body = bytes(odata[ostart:ostart + osym.size])
    target = bytes(tdata[tstart:tstart + tsym.size])

    relocation_sections = [s for s in osec if s.section_type == wf.SHT_RELA
                           and s.info == osym.section_index]
    records = []
    if relocation_sections:
        section = relocation_sections[0]
        size = section.entry_size or 12
        for offset in range(section.offset, section.offset + section.size,
                            size):
            records.append(struct.unpack_from(">IIi", odata, offset))
    region_start = osym.value + start
    region_end = osym.value + end
    region_records = [(offset - region_start, info, addend)
                      for offset, info, addend in records
                      if region_start <= offset < region_end]
    text_relocations = wf._function_text_relocations(
        odata, osec, osym.section_index, osym.value, osym.value + osym.size)
    window_symbols = {offset - start: name
                      for offset, (_k, name) in text_relocations.items()
                      if start <= offset < end}

    region = body[start:end]
    permuted = b"".join(struct.pack(">I", wf._u32(region, index * 4))
                        for index in order)
    before_relocations = wf._relocation_sha256(region_records, window_symbols)
    destination_by_source = {s: d for d, s in enumerate(order)}
    moved, moved_symbols = [], {}
    for offset, info, addend in region_records:
        at = destination_by_source[offset // 4] * 4 + offset % 4
        moved.append((at, info, addend))
        moved_symbols[at] = window_symbols[offset]
    moved.sort(key=lambda item: item[0])
    after_relocations = wf._relocation_sha256(moved, moved_symbols)

    print(f"{arguments.unit}::{arguments.function} ({kind}, "
          f"{len(body)//4} insns); window +0x{start:x}:0x{end:x}, "
          f"{len(order)} atoms, {len(region_records)} relocation(s)")
    for index, source in enumerate(order):
        at = start + index * 4
        ours = wf._u32(permuted, index * 4)
        tgt = wf._u32(target, at)
        if ours != tgt:
            print(f"  +0x{at:<4x} src{source:<3} ours=0x{ours:08x} "
                  f"tgt=0x{tgt:08x} ourform={wf.decode_copy_form(ours)} "
                  f"tgtform={wf.decode_copy_form(tgt)}")

    rule = {
        "function": arguments.function,
        "before_sha256": wf._sha256(body),
        "after_sha256": wf._sha256(target),
        "instruction_permutation": {
            "start": hex(start), "end": hex(end), "order": order,
            "before_sha256": wf._sha256(region),
            "after_sha256": wf._sha256(permuted),
            "before_relocations_sha256": before_relocations,
            "after_relocations_sha256": after_relocations,
        },
        "copy_register_fields": True,
    }
    if forms:
        rule["equivalent_copy_form"] = [{"at": hex(at), "proof": "unconditional"}
                                        for at in forms]

    # Try the STRICT proof alone first: a rule that does not need the wider
    # mode must not declare it (apply_patch refuses that by name).
    probe = bytearray(odata)
    try:
        wf.apply_patch(probe, json.loads(json.dumps(rule)), bytes(tdata))
    except ValueError as failure:
        print(f"  strict alone: REFUSED -- {failure}")
    else:
        final = bytes(probe[ostart:ostart + osym.size])
        print(f"  strict alone: "
              f"{'BYTE-EQUAL' if final == target else 'APPLIED-NOT-EQUAL'} "
              f"(no value_equality_recolor needed)")
        out = arguments.out or os.path.join(
            ROOT, "build", "GUNE5D", f"wc_perm2_{arguments.function}.json")
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(rule, handle, indent=2)
        print(f"  wrote {out}")
        return 0 if final == target else 1

    subs, seen, last = [], set(), ""
    for round_index in range(60):
        value_equality = {"audit": arguments.audit,
                          "substitutions": list(subs),
                          "compare_exchanges": []}
        if arguments.closure:
            value_equality["constant_equality"] = True
        rule["value_equality_recolor"] = value_equality
        probe = bytearray(odata)
        try:
            wf.apply_patch(probe, json.loads(json.dumps(rule)), bytes(tdata))
        except ValueError as failure:
            last = str(failure)
            grew = False
            for at, bank, o, t in SUB.findall(last):
                key = (at, bank, o, t)
                if key in seen:
                    continue
                seen.add(key)
                subs.append({"at": "0x" + at, "bank": bank,
                             "ours": int(o), "target": int(t)})
                grew = True
            if not grew:
                print(f"  REFUSED (round {round_index}): {last}")
                return 1
            continue
        final = bytes(probe[ostart:ostart + osym.size])
        verdict = "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL"
        print(f"  {verdict} after {round_index} round(s); "
              f"{len(subs)} substitution(s)")
        if not subs:
            rule.pop("value_equality_recolor")
            probe = bytearray(odata)
            wf.apply_patch(probe, json.loads(json.dumps(rule)), bytes(tdata))
            print("  and the STRICT recolor proof alone carries it")
        out = arguments.out or os.path.join(
            ROOT, "build", "GUNE5D", f"wc_perm2_{arguments.function}.json")
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(rule, handle, indent=2)
        print(f"  wrote {out}")
        return 0 if verdict == "BYTE-EQUAL" else 1
    print(f"  did not converge; last: {last}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
