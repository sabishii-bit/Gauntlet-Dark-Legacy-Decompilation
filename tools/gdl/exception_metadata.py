"""Shared function-indexed MWCC exception metadata comparison.

IMPORTABLE CORE: exception_records, compare_exception_records -- pure over
ELF bytes / decoded records, no build or subprocess. Originally the R60
enemy probe's decoder; datadiff and that probe now share this implementation.

ADDR32 index relocations resolve to function identity and extab bytes, so
record reordering and anonymous metadata symbol names are not byte defects.
Extra records need separate link-reachability review; this is not
whole-object or whole-link equality.

EXTAB PAYLOAD RELOCATIONS (run-59 item 3). An MWCC exception record can
point AT a function -- a destructor, or the partial-array destructor -- and
the pointer word is emitted as a zero placeholder plus a relocation, so its
raw bytes prove nothing about the eventual value. This decoder used to
refuse the whole object for that, which is fail-closed but blind: MEASURED
over all 668 built objects at afe555163, exactly two units carry such
relocations and all six of their entries are R_PPC_ADDR32 with addend 0 and
a named .text symbol --

    game/movie/movieplayer (target)  4 entries: __dla__FPv, __dl__FPv,
        dtor_800DBB94, __dt__15MoviePlayerBaseFv
    Runtime.PPCEABI.H/NMWException   2 entries, both
        __dt__26__partial_array_destructorFv

-- so the form is now DECODED instead: each record carries its payload
relocations as (offset within the record, type, symbol name, addend)
alongside the bytes, which proves the pointer's identity rather than
comparing placeholders. That is strictly stronger than the refusal it
replaces and it is what caught the difference the refusal hid: OUR
movieplayer object emits NO extab payload relocations where the target
emits four.

Anything still unmodelled -- an implicit-addend table, a relocation type
other than ADDR32, an unnamed or misaligned target, an entry outside every
record -- raises `UnsupportedExceptionMetadata`, which NAMES the form. It
is a ValueError subclass, so the callers that already degrade on ValueError
(retire_audit -> Refused, datadiff -> UNRESOLVED) keep degrading, and a
caller that wants to tell "unmodelled form" from "malformed object" can.
"""
import struct

import webfrank as wf

#: R_PPC_ADDR32 -- the only extab payload relocation this decoder models.
R_PPC_ADDR32 = 1


class UnsupportedExceptionMetadata(ValueError):
    """A form this decoder does not model. NOT a difference, and not a pass.

    A ValueError subclass on purpose: `datadiff.exception_table` and
    `retire_audit.object_image` already catch ValueError and turn it into
    UNRESOLVED / Refused respectively, so nothing regresses to a traceback,
    while `except UnsupportedExceptionMetadata` can now separate "this
    decoder does not model the form" from "this object is malformed".
    """


def exception_records(data):
    """Resolve supported ELF32-BE extabindex records; reject incomplete data."""
    if data[:6] != b"\x7fELF\x01\x02":
        raise UnsupportedExceptionMetadata("expected big-endian ELF32")
    try:
        return _exception_records(data)
    except (IndexError, struct.error, UnicodeError) as exc:
        raise UnsupportedExceptionMetadata(
            f"malformed exception metadata: {exc}") from exc


def _symbol_table(data, sections, rs):
    """The (symbol table, string table) a relocation section resolves through."""
    table = sections[rs.link]
    strings = sections[table.link]
    if (table.section_type != wf.SHT_SYMTAB or (table.entry_size or 16) != 16
            or table.size % 16 or table.offset + table.size > len(data)
            or strings.offset + strings.size > len(data)):
        raise UnsupportedExceptionMetadata("malformed exception symbol table")
    return table, strings


def _symbol(data, table, strings, symbol_index):
    """(name, value, section index) for one symbol-table entry."""
    sp = table.offset + symbol_index * 16
    if not table.offset <= sp <= table.offset + table.size - 16:
        raise UnsupportedExceptionMetadata("relocation symbol out of range")
    name_at, value = struct.unpack_from(">II", data, sp)
    section = wf._u16(data, sp + 14)
    if name_at >= strings.size:
        raise UnsupportedExceptionMetadata("relocation symbol name out of range")
    start = strings.offset + name_at
    end = data.find(b"\0", start, strings.offset + strings.size)
    if end < 0:
        raise UnsupportedExceptionMetadata("unterminated exception symbol name")
    return (bytes(data[start:end]).decode("ascii") if name_at else ""), value, section


def _extab_payload_relocations(data, sections, rs, extab):
    """{extab offset: (type, symbol name, addend)} for the extab PAYLOAD.

    Run-59 item 3. These patch pointer words INSIDE an exception record --
    a destructor address, in every case measured in this tree. The word's
    raw bytes are a zero placeholder, so the relocation IS the value, and
    decoding it is what makes the record comparable at all.
    """
    if rs.section_type != wf.SHT_RELA:
        raise UnsupportedExceptionMetadata(
            "extab payload uses implicit-addend (SHT_REL) relocations")
    stride = rs.entry_size or 12
    if stride != 12 or rs.size % stride or rs.offset + rs.size > len(data):
        raise UnsupportedExceptionMetadata(
            "partial or unsupported extab payload relocation table")
    table, strings = _symbol_table(data, sections, rs)
    out = {}
    for at in range(rs.offset, rs.offset + rs.size, stride):
        offset, info, addend = struct.unpack_from(">IIi", data, at)
        kind = info & 255
        if kind != R_PPC_ADDR32:
            raise UnsupportedExceptionMetadata(
                f"unmodelled extab payload relocation type {kind} at "
                f"extab+0x{offset:x}")
        if offset % 4 or offset + 4 > extab.size:
            raise UnsupportedExceptionMetadata(
                f"extab payload relocation at +0x{offset:x} is misaligned "
                "or outside the section")
        if offset in out:
            raise UnsupportedExceptionMetadata(
                f"duplicate extab payload relocation at +0x{offset:x}")
        name, _value, _section = _symbol(data, table, strings, info >> 8)
        if not name:
            raise UnsupportedExceptionMetadata(
                f"unnamed extab payload relocation target at +0x{offset:x}; "
                "an anonymous pointer cannot be compared by identity")
        out[offset] = (kind, name, addend)
    return out


def _exception_records(data):
    sections = wf._sections(data)
    indices = [s for s in sections if s.name.lstrip(".") == "extabindex"]
    tables = [s for s in sections if s.name.lstrip(".") == "extab"]
    if not indices and not tables:
        return {}
    if len(indices) != 1 or len(tables) != 1:
        raise UnsupportedExceptionMetadata(
            "expected one extab and one extabindex section")
    index, extab = indices[0], tables[0]
    for section in (index, extab):
        if section.section_type != 1 or section.offset + section.size > len(data):
            raise UnsupportedExceptionMetadata(
                "exception section contents missing or truncated")
    if index.size % 12:
        raise UnsupportedExceptionMetadata("partial extabindex record")
    if not index.size:
        if extab.size:
            raise UnsupportedExceptionMetadata("extab bytes have no index records")
        return {}
    reloc = {}
    payload = {}
    for rs in sections:
        if rs.section_type not in (wf.SHT_RELA, 9):
            continue
        if rs.info == extab.index and rs.size:
            payload.update(
                _extab_payload_relocations(data, sections, rs, extab))
            continue
        if rs.info != index.index:
            continue
        if rs.section_type != wf.SHT_RELA:
            raise UnsupportedExceptionMetadata(
                "expected explicit-addend index relocations")
        stride = rs.entry_size or 12
        if stride != 12 or rs.size % stride or rs.offset + rs.size > len(data):
            raise UnsupportedExceptionMetadata(
                "partial or unsupported index relocation table")
        table, strings = _symbol_table(data, sections, rs)
        for at in range(rs.offset, rs.offset + rs.size, stride):
            offset, info, addend = struct.unpack_from(">IIi", data, at)
            if info & 255 != R_PPC_ADDR32 or offset in reloc:
                raise UnsupportedExceptionMetadata(
                    "expected unique ADDR32 index relocation")
            name, value, section = _symbol(data, table, strings, info >> 8)
            reloc[offset] = (name, value + addend, section, addend)
    expected_offsets = {o + k for o in range(0, index.size, 12) for k in (0, 8)}
    if set(reloc) != expected_offsets:
        raise UnsupportedExceptionMetadata(
            "incomplete or unexpected index relocations")
    starts = sorted({reloc[o + 8][1] for o in range(0, index.size, 12)})
    if starts[0] != 0:
        raise UnsupportedExceptionMetadata("unindexed exception metadata prefix")
    ends = dict(zip(starts, starts[1:] + [extab.size]))
    result = {}
    covered = set()
    for offset in range(0, index.size, 12):
        name, _, fn_section, addend = reloc[offset]
        _, start, section, _ = reloc[offset + 8]
        if not name or name in result or addend or sections[fn_section].name != ".text":
            raise UnsupportedExceptionMetadata("expected unique named function entry")
        if section != extab.index or not 0 <= start < ends[start] <= extab.size:
            raise UnsupportedExceptionMetadata("exception metadata outside extab")
        inside = {at for at in payload if start <= at < ends[start]}
        covered |= inside
        result[name] = {
            "length": wf._u32(data, index.offset + offset + 4),
            "metadata": bytes(data[extab.offset + start:extab.offset + ends[start]]).hex(),
            # Offsets are RECORD-relative so a record compares equal
            # wherever the linker or a sibling's size puts it in extab.
            "relocations": sorted(
                [at - start, payload[at][0], payload[at][1], payload[at][2]]
                for at in inside),
        }
    stray = sorted(set(payload) - covered)
    if stray:
        raise UnsupportedExceptionMetadata(
            "extab payload relocation(s) outside every indexed record at "
            + ", ".join(f"+0x{at:x}" for at in stray))
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
