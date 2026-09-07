"""Shared function-indexed MWCC exception metadata comparison.

IMPORTABLE CORE: exception_records, compare_exception_records -- pure over
ELF bytes / decoded records, no build or subprocess. Originally the R60
enemy probe's decoder; datadiff and that probe now share this implementation.

ADDR32 index relocations resolve to function identity and extab bytes, so
record reordering and anonymous metadata symbol names are not byte defects.
Unmodelled relocations IN extab refuse: comparing their placeholder bytes
would not prove their eventual relocated values. Extra records need separate
link-reachability review; this is not whole-object or whole-link equality.
"""
import struct

import webfrank as wf


def exception_records(data):
    """Resolve supported ELF32-BE extabindex records; reject incomplete data."""
    if data[:6] != b"\x7fELF\x01\x02":
        raise ValueError("expected big-endian ELF32")
    try:
        return _exception_records(data)
    except (IndexError, struct.error, UnicodeError) as exc:
        raise ValueError(f"malformed exception metadata: {exc}") from exc


def _exception_records(data):
    sections = wf._sections(data)
    indices = [s for s in sections if s.name.lstrip(".") == "extabindex"]
    tables = [s for s in sections if s.name.lstrip(".") == "extab"]
    if not indices and not tables:
        return {}
    if len(indices) != 1 or len(tables) != 1:
        raise ValueError("expected one extab and one extabindex section")
    index, extab = indices[0], tables[0]
    for section in (index, extab):
        if section.section_type != 1 or section.offset + section.size > len(data):
            raise ValueError("exception section contents missing or truncated")
    if index.size % 12:
        raise ValueError("partial extabindex record")
    if not index.size:
        if extab.size:
            raise ValueError("extab bytes have no index records")
        return {}
    reloc = {}
    for rs in sections:
        if rs.section_type not in (wf.SHT_RELA, 9):
            continue
        if rs.info == extab.index and rs.size:
            raise ValueError("UNRESOLVED extab payload relocations: raw bytes cannot prove relocated metadata")
        if rs.info != index.index:
            continue
        if rs.section_type != wf.SHT_RELA:
            raise ValueError("expected explicit-addend index relocations")
        stride = rs.entry_size or 12
        if stride != 12 or rs.size % stride or rs.offset + rs.size > len(data):
            raise ValueError("partial or unsupported index relocation table")
        table = sections[rs.link]
        strings = sections[table.link]
        if (table.section_type != wf.SHT_SYMTAB or (table.entry_size or 16) != 16
                or table.size % 16 or table.offset + table.size > len(data)
                or strings.offset + strings.size > len(data)):
            raise ValueError("malformed exception symbol table")
        for at in range(rs.offset, rs.offset + rs.size, stride):
            offset, info, addend = struct.unpack_from(">IIi", data, at)
            if info & 255 != 1 or offset in reloc:
                raise ValueError("expected unique ADDR32 index relocation")
            sp = table.offset + (info >> 8) * 16
            if not table.offset <= sp <= table.offset + table.size - 16:
                raise ValueError("index relocation symbol out of range")
            name_at, value = struct.unpack_from(">II", data, sp)
            section = wf._u16(data, sp + 14)
            if name_at >= strings.size:
                raise ValueError("index relocation symbol name out of range")
            start = strings.offset + name_at
            end = data.find(b"\0", start, strings.offset + strings.size)
            if end < 0:
                raise ValueError("unterminated exception symbol name")
            name = bytes(data[start:end]).decode("ascii") if name_at else ""
            reloc[offset] = (name, value + addend, section, addend)
    expected_offsets = {o + k for o in range(0, index.size, 12) for k in (0, 8)}
    if set(reloc) != expected_offsets:
        raise ValueError("incomplete or unexpected index relocations")
    starts = sorted({reloc[o + 8][1] for o in range(0, index.size, 12)})
    if starts[0] != 0:
        raise ValueError("unindexed exception metadata prefix")
    ends = dict(zip(starts, starts[1:] + [extab.size]))
    result = {}
    for offset in range(0, index.size, 12):
        name, _, fn_section, addend = reloc[offset]
        _, start, section, _ = reloc[offset + 8]
        if not name or name in result or addend or sections[fn_section].name != ".text":
            raise ValueError("expected unique named function entry")
        if section != extab.index or not 0 <= start < ends[start] <= extab.size:
            raise ValueError("exception metadata outside extab")
        result[name] = {
            "length": wf._u32(data, index.offset + offset + 4),
            "metadata": bytes(data[extab.offset + start:extab.offset + ends[start]]).hex(),
        }
    return result


def compare_exception_records(target, ours):
    return {
        "target_records": len(target), "ours_records": len(ours),
        "missing": sorted(set(target) - set(ours)),
        "extra": {k: ours[k] for k in sorted(set(ours) - set(target))},
        "changed": {k: {"target": target[k], "ours": ours[k]}
                    for k in target if k in ours and target[k] != ours[k]},
        "scope": "Function-indexed metadata only. Extra records require a separate link-reachability check; not a whole-TU flip verdict.",
    }


if __name__ == "__main__":
    # A library, not a command. Run-59 item 9: exiting 0 with no output at
    # all is indistinguishable from a tool that ran and found nothing.
    print(__doc__.strip())
