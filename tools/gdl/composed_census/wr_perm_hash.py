"""WR lane (run 42): compute an instruction_permutation window's four hashes.

    python tools/gdl/composed_census/wr_perm_hash.py <unit> <function> <start> <end> <order>

`order` is comma-separated, `order[destination] = source atom`.  Emits the
window object ready to paste into a webfrank.json rule, and prints the
resulting region against the target so the caller can see whether the
permutation alone lands on the target words.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                      # noqa: E402
from cn_analyze import our_object, target_object            # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("start")
    parser.add_argument("end")
    parser.add_argument("order")
    arguments = parser.parse_args()

    start = int(arguments.start, 16)
    end = int(arguments.end, 16)
    order = [int(value) for value in arguments.order.split(",")]

    ours_path, kind = our_object(arguments.unit)
    data = bytearray(open(ours_path, "rb").read())
    sections = wf._sections(data)
    symbol = wf._find_symbol(data, sections, arguments.function)
    base = sections[symbol.section_index].offset + symbol.value
    body = bytes(data[base:base + symbol.size])
    relocations = wf._function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size)

    tdata = bytearray(open(target_object(arguments.unit), "rb").read())
    tsec = wf._sections(tdata)
    tsym = wf._find_symbol(tdata, tsec, arguments.function)
    tbase = tsec[tsym.section_index].offset + tsym.value
    target = bytes(tdata[tbase:tbase + tsym.size])

    region = body[start:end]
    atoms = [region[i * 4:i * 4 + 4] for i in range(len(region) // 4)]
    output = b"".join(atoms[source] for source in order)

    symbols = {offset - start: name
               for offset, (_kind, name) in relocations.items()
               if start <= offset < end}
    section_records = []
    for reloc_section in [s for s in sections
                          if s.section_type == wf.SHT_RELA
                          and s.info == symbol.section_index]:
        entry = reloc_section.entry_size or 12
        for offset in range(reloc_section.offset,
                            reloc_section.offset + reloc_section.size, entry):
            section_records.append(wf.struct.unpack_from(">IIi", data, offset))
    lo = symbol.value + start
    hi = symbol.value + end
    records = [(offset - lo, info, addend)
               for offset, info, addend in section_records
               if lo <= offset < hi]

    destination_by_source = {s: d for d, s in enumerate(order)}
    moved = []
    moved_symbols = {}
    for offset, info, addend in records:
        target_offset = destination_by_source[offset // 4] * 4 + offset % 4
        moved.append((target_offset, info, addend))
        moved_symbols[target_offset] = symbols[offset]
    moved.sort(key=lambda item: item[0])

    window = {
        "start": hex(start),
        "end": hex(end),
        "order": order,
        "before_sha256": wf._sha256(region),
        "after_sha256": wf._sha256(output),
        "before_relocations_sha256": wf._relocation_sha256(records, symbols),
        "after_relocations_sha256": wf._relocation_sha256(moved,
                                                          moved_symbols),
    }
    print(f"source: {kind}")
    print(json.dumps(window, indent=2))
    print("\nwindow after permutation vs target:")
    for index in range(len(atoms)):
        at = start + index * 4
        ours_word = wf._u32(output, index * 4)
        target_word = wf._u32(target, at)
        flag = "=" if ours_word == target_word else "~"
        print(f"  +0x{at:04x} {flag} O {ours_word:08x}  T {target_word:08x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
