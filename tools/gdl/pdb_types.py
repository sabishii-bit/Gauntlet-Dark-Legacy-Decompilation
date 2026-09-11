#!/usr/bin/env python3
"""Expand a struct/union/enum layout out of the Xbox PDB's TYPE stream.

    python tools/gdl/pdb_types.py --struct _blit_setup
    python tools/gdl/pdb_types.py --struct Hidden          # a GLOBAL's type
    python tools/gdl/pdb_types.py --index 0x3fd3 --json
    python tools/gdl/pdb_types.py --grep '^tPUP'           # names only

WHY THIS EXISTS. `describe_types` (composed_census.r68_sound_data_recovery)
models only LF_MODIFIER, LF_POINTER and LF_ARRAY, so every aggregate comes
back `{"unmodelled": true}`; `r75_pdb_signatures.py` does expand LF_STRUCTURE
but only for three hard-coded audio tags and refuses anything else. Lane I's
run-62 player work needed two layouts that `research/xbox_symbols/
xbox_structs.tsv` does not carry at all (`Hidden` 36x27, `Cheats` 20x18) and
had to hand-roll the expansion in lane scratch. This is that expansion,
promoted, generalised and tested.

WHAT IT READS. LF_STRUCTURE (0x1005) and LF_CLASS (0x1004) share a header --
`u16 count; u16 property; u32 fieldlist; u32 derived; u32 vshape;` then a
NUMERIC size and a length-prefixed name. LF_UNION (0x1006) is the same
without `derived`/`vshape`, LF_ENUM (0x1007) carries an underlying type and a
fieldlist. `property & 0x80` marks a FORWARD REFERENCE, whose fieldlist is 0
and whose size is 0: it is resolved by tag name to the defining record, and
when no defining record exists that is reported, never silently rendered as
an empty struct. The fieldlist (LF_FIELDLIST 0x1203) is walked for LF_MEMBER
(0x1405, the ST form this PDB actually uses; 0x1206 is the older non-ST
spelling and is accepted too), LF_STMEMBER, LF_BCLASS, LF_VFUNCTAB,
LF_METHOD/LF_ONEMETHOD/LF_NESTTYPE and LF_ENUMERATE (0x1502/0x1403).

WHAT IS AND IS NOT EVIDENCE. Offsets, sizes and member names are facts about
the XBOX build recorded in this PDB. They are corroboration for a GameCube
layout, never proof of it: verify every offset, width and stride against GC
accesses before using one (AGENTS.md, "Types, names and de-fakematching").
`xbox_structs.tsv` is a derived summary of the same PDB and inserts synthetic
`__alignN` padding rows the type stream does not contain; a member set that
differs from the TSV only by those rows agrees with it.

INTEGRITY, NOT RECOVERY. The subleaf walk counts every record it consumes and
compares that with the aggregate's declared member count. A fieldlist that
does not reconcile REFUSES: a layout silently missing a member is
indistinguishable in the output from a real one, and it is worse than no
answer. Unknown subleaves, unmodelled numeric leaves and truncated records
refuse the same way.

The PDB is a private, gitignored input under `research/xbox_symbols/` that is
not provisioned into every worktree. When it is absent this tool refuses with
the path it looked for, and the live tests skip.

IMPORTABLE CORE: numeric_leaf, prefixed_string, load_types, TypeTable,
render -- pure over bytes already read; no writing, no subprocess.
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
DEFAULT_PDB = REPO / "research" / "xbox_symbols" / "shell3D.pdb"

for _path in (HERE, HERE / "composed_census"):
    if str(_path) not in sys.path:
        sys.path.insert(0, str(_path))

import pdb_globals  # noqa: E402

try:
    from composed_census.r68_sound_data_recovery import (modifier_record,
                                                         pdb_streams,
                                                         pointer_modifiers,
                                                         pointer_record)
except ImportError:  # run from tools/gdl
    from r68_sound_data_recovery import (modifier_record, pdb_streams,
                                        pointer_modifiers, pointer_record)

#: Aggregate leaves this expands, and how many u32 sit between `property`
#: and the numeric size for each.
LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM = 0x1004, 0x1005, 0x1006, 0x1007
LF_MODIFIER, LF_POINTER = 0x1001, 0x1002
LF_ARRAY, LF_ARRAY_ST = 0x1003, 0x1503
LF_FIELDLIST = 0x1203
LF_PROCEDURE, LF_MFUNCTION = 0x1008, 0x1009
#: `property & FORWARD_REF` == the record is a forward declaration only.
FORWARD_REF = 0x80
#: fieldlist subleaf -> (name, fixed bytes after the 2-byte subleaf id,
#: where the type index sits inside those bytes and how wide it is, how many
#: numeric leaves follow, whether a length-prefixed name follows).
#:
#: Two families exist and BOTH appear in shell3D.pdb. 0x04xx are the
#: 16-bit-type-index forms (`_16t`), 0x14xx the 32-bit ST forms. cvinfo.h's
#: 0x12xx block is NOT a fieldlist family at all -- those are top-level type
#: records (LF_ARGLIST_ST 0x1201, LF_FIELDLIST 0x1203, LF_BITFIELD_ST 0x1205
#: ...), and reading 0x1206 as LF_MEMBER, as this project's lane scratch did,
#: is a guess that happens never to fire on this PDB.
SUBLEAVES = {
    #                          name             fixed  type   n  name?
    0x0400: ("LF_BCLASS_16t",       4, (0, 2), 1, False),
    0x0403: ("LF_ENUMERATE_ST",     2, None,   1, True),
    0x0404: ("LF_FRIENDFCN_16t",    2, (0, 2), 0, True),
    0x0405: ("LF_INDEX_16t",        2, (0, 2), 0, False),
    0x0406: ("LF_MEMBER_16t",       4, (0, 2), 1, True),
    0x0407: ("LF_STMEMBER_16t",     4, (0, 2), 0, True),
    0x0408: ("LF_METHOD_16t",       4, None,   0, True),
    0x0409: ("LF_NESTTYPE_16t",     2, (0, 2), 0, True),
    0x040A: ("LF_VFUNCTAB_16t",     2, (0, 2), 0, False),
    0x040B: ("LF_FRIENDCLS_16t",    2, (0, 2), 0, False),
    0x040C: ("LF_ONEMETHOD_16t",    4, (2, 2), 0, True),
    0x040D: ("LF_VFUNCOFF_16t",     6, (0, 2), 0, False),
    0x1400: ("LF_BCLASS",           6, (2, 4), 1, False),
    0x1401: ("LF_VBCLASS",         10, (2, 4), 2, False),
    0x1402: ("LF_IVBCLASS",        10, (2, 4), 2, False),
    0x1403: ("LF_FRIENDFCN_ST",     6, (2, 4), 0, True),
    0x1404: ("LF_INDEX",            6, (2, 4), 0, False),
    0x1405: ("LF_MEMBER_ST",        6, (2, 4), 1, True),
    0x1406: ("LF_STMEMBER_ST",      6, (2, 4), 0, True),
    0x1407: ("LF_METHOD_ST",        6, None,   0, True),
    0x1408: ("LF_NESTTYPE_ST",      6, (2, 4), 0, True),
    0x1409: ("LF_VFUNCTAB",         6, (2, 4), 0, False),
    0x140A: ("LF_FRIENDCLS",        6, (2, 4), 0, False),
    0x140B: ("LF_ONEMETHOD_ST",     6, (2, 4), 0, True),
    0x140C: ("LF_VFUNCOFF",        10, (2, 4), 0, False),
}
#: Subleaves that contribute a laid-out data member.
DATA_SUBLEAVES = (0x1405, 0x0406)
ENUMERATE_SUBLEAVES = (0x0403,)
#: Subleaves that continue the list in another LF_FIELDLIST record.
CONTINUATION_SUBLEAVES = (0x1404, 0x0405)
#: LF_ONEMETHOD carries an extra u32 vtable offset -- and ONLY then -- when
#: its attribute word's `mprop` field says "introducing virtual". Consuming
#: it unconditionally, or never, desynchronizes the walk at the first
#: `__vecDelDtor` in a C++ class and every member after it is invented.
ONEMETHOD_SUBLEAVES = (0x140B, 0x040C)
MTINTRO, MTPUREINTRO = 4, 6
#: LF_METHOD names an OVERLOAD GROUP and carries the group's size in its
#: first u16. The aggregate's declared member count counts each overload
#: separately, so reconciling one LF_METHOD as one member makes every
#: overloaded C++ class fail the count check for a reason that is not a
#: parsing defect.
METHOD_SUBLEAVES = (0x1407, 0x0408)
#: CodeView numeric leaves that carry an integer, and their struct format.
NUMERIC_FORMATS = {0x8000: ("<b", 1), 0x8001: ("<h", 2), 0x8002: ("<H", 2),
                   0x8003: ("<i", 4), 0x8004: ("<I", 4), 0x800A: ("<q", 8),
                   0x800B: ("<Q", 8)}
MAX_DEPTH = 12


class Unavailable(RuntimeError):
    """The PDB, or a type record inside it, could not be read as claimed."""


# --------------------------------------------------------------------------
# IMPORTABLE CORE


def numeric_leaf(body, at):
    """(value, next offset) for a CodeView numeric leaf at `at`.

    Values below 0x8000 are stored inline as a u16; above that the u16 is a
    leaf id naming the real width. A leaf this does not model REFUSES rather
    than returning the id as if it were a value -- 0x8004 read as a size is
    32772, a number that looks entirely plausible in a layout dump.
    """
    if at + 2 > len(body):
        raise Unavailable("numeric leaf at +%d runs past the record" % at)
    value = struct.unpack_from("<H", body, at)[0]
    if value < 0x8000:
        return value, at + 2
    fmt = NUMERIC_FORMATS.get(value)
    if fmt is None:
        raise Unavailable("unmodelled numeric leaf 0x%04X at +%d" % (value, at))
    if at + 2 + fmt[1] > len(body):
        raise Unavailable("numeric leaf 0x%04X at +%d runs past the record"
                          % (value, at))
    return struct.unpack_from(fmt[0], body, at + 2)[0], at + 2 + fmt[1]


def prefixed_string(body, at):
    """(text, next offset) for an old-CodeView length-prefixed name."""
    if at >= len(body):
        raise Unavailable("name at +%d runs past the record" % at)
    size = body[at]
    if at + 1 + size > len(body):
        raise Unavailable("name at +%d runs past the record" % at)
    return body[at + 1:at + 1 + size].decode("latin1"), at + 1 + size


def load_types(tpi):
    """({index: (leaf, body)}, header dict) for a PDB 2.0 TPI stream."""
    if len(tpi) < 20:
        raise Unavailable("TPI stream is too short to hold a header")
    version, header, first, last, size = struct.unpack_from("<5I", tpi)
    if not header <= len(tpi) or first > last:
        raise Unavailable("TPI header names an impossible record range")
    types, at = {}, header
    for index in range(first, last):
        if at + 4 > len(tpi):
            raise Unavailable("TPI record 0x%x starts past the stream" % index)
        length, leaf = struct.unpack_from("<HH", tpi, at)
        if length < 2 or at + length + 2 > len(tpi):
            raise Unavailable("TPI record 0x%x at +0x%x has length %d"
                              % (index, at, length))
        types[index] = (leaf, tpi[at + 4:at + length + 2])
        at += length + 2
    return types, {"version": version, "first": first, "last": last,
                   "records": len(types), "record_bytes": size}


class TypeTable(object):
    """Spelling, sizing and layout expansion over a loaded TPI stream.

    `by_name` maps a tag to its DEFINING record only: a forward reference
    never claims the name, so `--struct X` cannot resolve to an empty shell.
    """

    def __init__(self, types):
        self.types = types
        self.by_name = {}
        for index in sorted(types):
            leaf, body = types[index]
            if leaf not in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
                continue
            try:
                name, _at, property_bits = self._tag(leaf, body)
            except Unavailable:
                continue
            if not property_bits & FORWARD_REF:
                self.by_name.setdefault(name, index)

    # -- record shapes ----------------------------------------------------

    def _tag(self, leaf, body):
        """(name, offset past the name, property bits) for an aggregate."""
        if len(body) < 4:
            raise Unavailable("aggregate record is too short")
        _count, property_bits = struct.unpack_from("<HH", body)
        if leaf in (LF_CLASS, LF_STRUCTURE):
            _size, at = numeric_leaf(body, 16)
        elif leaf == LF_UNION:
            _size, at = numeric_leaf(body, 8)
        else:                                   # LF_ENUM
            at = 12
        name, at = prefixed_string(body, at)
        return name, at, property_bits

    def _aggregate(self, index):
        """Normalised (leaf, count, property, fieldlist, size, name)."""
        leaf, body = self.types.get(index, (None, b""))
        if leaf not in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
            raise Unavailable("type 0x%x is leaf %s, not an aggregate"
                              % (index, "absent" if leaf is None
                                 else "0x%04X" % leaf))
        count, property_bits = struct.unpack_from("<HH", body)
        if leaf in (LF_CLASS, LF_STRUCTURE):
            fieldlist = struct.unpack_from("<I", body, 4)[0]
            size, _at = numeric_leaf(body, 16)
        elif leaf == LF_UNION:
            fieldlist = struct.unpack_from("<I", body, 4)[0]
            size, _at = numeric_leaf(body, 8)
        else:
            fieldlist = struct.unpack_from("<I", body, 8)[0]
            size = None
        name, _at, _p = self._tag(leaf, body)
        return leaf, count, property_bits, fieldlist, size, name

    # -- spelling and sizing ----------------------------------------------

    def spell(self, index, depth=0):
        """A C-ish spelling for a type index. Never guesses silently."""
        if depth > MAX_DEPTH:
            return "?deep"
        primitive = pdb_globals.primitive_type(index)
        if primitive:
            return primitive[0]
        if index < 0x1000:
            return "prim0x%x" % index
        leaf, body = self.types.get(index, (None, b""))
        if leaf is None:
            return "type0x%x?" % index
        if leaf == LF_POINTER:
            try:
                target, attributes = pointer_record(body)
            except ValueError as error:
                raise Unavailable(str(error)) from error
            return pdb_globals.qualified_type_name(
                self.spell(target, depth + 1) + " *",
                pointer_modifiers(attributes))
        if leaf == LF_MODIFIER:
            try:
                target, flags = modifier_record(body)
            except ValueError as error:
                raise Unavailable(str(error)) from error
            return pdb_globals.qualified_type_name(
                self.spell(target, depth + 1), flags)
        if leaf in (LF_ARRAY, LF_ARRAY_ST):
            element = struct.unpack_from("<I", body)[0]
            total, _at = numeric_leaf(body, 8)
            width = self.size(element, depth + 1)
            inner = self.spell(element, depth + 1)
            if width:
                return "%s[%d]" % (inner, total // width)
            return "%s[/* %d bytes */]" % (inner, total)
        if leaf in (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM):
            keyword = {LF_CLASS: "class", LF_STRUCTURE: "struct",
                       LF_UNION: "union", LF_ENUM: "enum"}[leaf]
            name = self._tag(leaf, body)[0]
            return "%s %s" % (keyword, name)
        if leaf in (LF_PROCEDURE, LF_MFUNCTION):
            return "<function>"
        return "type0x%x(leaf 0x%04X)" % (index, leaf)

    def size(self, index, depth=0):
        """Bytes this type occupies, or None when the stream does not say."""
        if depth > MAX_DEPTH:
            return None
        primitive = pdb_globals.primitive_type(index)
        if primitive:
            return primitive[1]
        if index < 0x1000:
            return None
        leaf, body = self.types.get(index, (None, b""))
        if leaf is None:
            return None
        if leaf == LF_POINTER:
            try:
                pointer_record(body)
            except ValueError as error:
                raise Unavailable(str(error)) from error
            return 4                            # Xbox is a 32-bit target
        if leaf == LF_MODIFIER:
            try:
                target, _flags = modifier_record(body)
            except ValueError as error:
                raise Unavailable(str(error)) from error
            return self.size(target, depth + 1)
        if leaf in (LF_ARRAY, LF_ARRAY_ST):
            return numeric_leaf(body, 8)[0]
        if leaf in (LF_CLASS, LF_STRUCTURE, LF_UNION):
            _leaf, _c, property_bits, _fl, size, name = self._aggregate(index)
            if property_bits & FORWARD_REF:
                real = self.by_name.get(name)
                return self.size(real, depth + 1) if real is not None else None
            return size
        if leaf == LF_ENUM:
            return self.size(struct.unpack_from("<I", body, 4)[0], depth + 1)
        return None

    # -- the fieldlist walk -----------------------------------------------

    def fieldlist(self, index, depth=0):
        """([row], consumed subleaf count) for an LF_FIELDLIST.

        Every subleaf is consumed by its documented shape, and LF_INDEX
        continuations are followed so a list MSVC split across records still
        reconciles. An unknown subleaf refuses: skipping it would silently
        drop every member behind it, and a short layout is indistinguishable
        in the output from a complete one.
        """
        if depth > MAX_DEPTH:
            raise Unavailable("fieldlist continuation chain exceeded %d hops"
                              % MAX_DEPTH)
        leaf, body = self.types.get(index, (None, b""))
        if leaf != LF_FIELDLIST:
            raise Unavailable(
                "type 0x%x is leaf %s, not LF_FIELDLIST (0x1203)"
                % (index, "absent" if leaf is None else "0x%04X" % leaf))
        rows, consumed, at = [], 0, 0
        while at < len(body):
            if body[at] >= 0xF0:                # LF_PAD
                at += body[at] & 0x0F
                continue
            if at + 2 > len(body):
                raise Unavailable("fieldlist 0x%x truncated at +%d"
                                  % (index, at))
            sub = struct.unpack_from("<H", body, at)[0]
            shape = SUBLEAVES.get(sub)
            if shape is None:
                raise Unavailable(
                    "fieldlist 0x%x holds unmodelled subleaf 0x%04X at +%d"
                    % (index, sub, at))
            label, fixed, type_at, numerics, has_name = shape
            at += 2
            if at + fixed > len(body):
                raise Unavailable("fieldlist 0x%x: %s at +%d runs past the"
                                  " record" % (index, label, at - 2))
            type_index = None
            if type_at is not None:
                where, width = type_at
                type_index = struct.unpack_from(
                    "<H" if width == 2 else "<I", body, at + where)[0]
            if sub in ONEMETHOD_SUBLEAVES:
                attribute = struct.unpack_from("<H", body, at)[0]
                if (attribute >> 2) & 0x7 in (MTINTRO, MTPUREINTRO):
                    fixed += 4                  # the vbaseoff u32
            overloads = (struct.unpack_from("<H", body, at)[0]
                         if sub in METHOD_SUBLEAVES else 1)
            at += fixed
            values = []
            for _ in range(numerics):
                value, at = numeric_leaf(body, at)
                values.append(value)
            name = None
            if has_name:
                name, at = prefixed_string(body, at)
            if sub in CONTINUATION_SUBLEAVES:
                more, more_count = self.fieldlist(type_index, depth + 1)
                rows.extend(more)
                consumed += more_count
                continue
            consumed += overloads
            if sub in DATA_SUBLEAVES:
                rows.append({"kind": "member", "offset": values[0],
                             "name": name, "type": self.spell(type_index),
                             "type_index": type_index,
                             "size": self.size(type_index)})
            elif sub in ENUMERATE_SUBLEAVES:
                rows.append({"kind": "enumerator", "name": name,
                             "value": values[0]})
            else:
                rows.append({"kind": label, "name": name,
                             "offset": values[0] if values else None,
                             "type_index": type_index})
        return rows, consumed

    # -- the public expansion ---------------------------------------------

    def expand(self, index, depth=0):
        """{'name','size','members',...} for one aggregate type index.

        A forward reference is followed to its defining record and both
        indices are reported. The member count is reconciled against the
        record's own declared count; a fieldlist that does not reconcile
        refuses, because a layout quietly missing a member reads exactly
        like a complete one.
        """
        if depth > MAX_DEPTH:
            raise Unavailable("forward-reference chain exceeded %d hops"
                              % MAX_DEPTH)
        leaf, count, property_bits, fieldlist, size, name = \
            self._aggregate(index)
        keyword = {LF_CLASS: "class", LF_STRUCTURE: "struct",
                   LF_UNION: "union", LF_ENUM: "enum"}[leaf]
        if property_bits & FORWARD_REF:
            real = self.by_name.get(name)
            if real is None or real == index:
                raise Unavailable(
                    "type 0x%x is a FORWARD REFERENCE to %s %s and the TPI"
                    " holds no defining record for that tag: its layout is"
                    " not in this PDB" % (index, keyword, name))
            expanded = self.expand(real, depth + 1)
            expanded["forward_reference_from"] = "0x%x" % index
            return expanded
        rows, consumed = self.fieldlist(fieldlist)
        if consumed != count:
            raise Unavailable(
                "%s %s (type 0x%x) declares %d member(s) but its fieldlist"
                " 0x%x yields %d: the layout is NOT reconciled and is not"
                " reported" % (keyword, name, index, count, fieldlist,
                               consumed))
        return {"kind": keyword, "name": name, "type_index": "0x%x" % index,
                "fieldlist": "0x%x" % fieldlist, "size": size,
                "declared_members": count, "members": rows,
                "forward_reference_from": None}


def declarator(spelling, name):
    """`char[8]` + `name` -> `char name[8]`; a real C declaration.

    A dump that prints `char[8] name` is not something a reader can paste
    into a header, and array-of-pointer versus pointer-to-array is exactly
    the distinction a layout dump has to get right.
    """
    head, _sep, dims = spelling.partition("[")
    head = head.rstrip()
    joint = "" if head.endswith("*") else " "
    return "%s%s%s%s" % (head, joint, name, ("[" + dims) if dims else "")


def render(layout, note=None):
    """The layout as C-like text, offsets and sizes in the margin."""
    out = []
    if note:
        out.append("/* %s */" % note)
    head = "%s %s {" % (layout["kind"], layout["name"])
    if layout.get("forward_reference_from"):
        head += "   /* resolved from forward reference %s */" \
            % layout["forward_reference_from"]
    out.append(head)
    for row in layout["members"]:
        if row["kind"] == "member":
            out.append("    /* +0x%-4X %-4s */ %s;"
                       % (row["offset"],
                          "?" if row["size"] is None else row["size"],
                          declarator(row["type"], row["name"])))
        elif row["kind"] == "enumerator":
            out.append("    %-28s = %s," % (row["name"], row["value"]))
        else:
            out.append("    /* %s %s */" % (row["kind"], row["name"] or ""))
    out.append("};   /* type %s, fieldlist %s, size %s */"
               % (layout["type_index"], layout["fieldlist"],
                  "?" if layout["size"] is None else layout["size"]))
    return "\n".join(out)


# --------------------------------------------------------------------------
# CLI


def _global_rows(streams):
    try:
        return pdb_globals.read_globals(streams)["rows"]
    except (pdb_globals.Unavailable, ValueError, struct.error) as error:
        raise Unavailable("global symbol stream: %s" % error) from error


def resolve_request(table, streams, name):
    """(type index, note) for a tag OR a global symbol name.

    A tag wins; `Hidden` and `Cheats` are globals whose element struct has no
    tag at all, and answering "not found" for them would be wrong.
    """
    if name in table.by_name:
        return table.by_name[name], "struct tag %s" % name
    matches = [row for row in _global_rows(streams) if row["name"] == name]
    if not matches:
        raise Unavailable(
            "no struct tag and no PDB global named %r. Tags are matched"
            " exactly; use --grep to list them." % name)
    row = matches[0]
    index = row["type_index"]
    element, suffix = index, ""
    leaf = table.types.get(index, (None, b""))[0]
    while leaf in (LF_ARRAY, LF_ARRAY_ST):
        body = table.types[element][1]
        element = struct.unpack_from("<I", body)[0]
        total = numeric_leaf(body, 8)[0]
        width = table.size(element)
        suffix = "[%s]" % (total // width if width else "?")
        leaf = table.types.get(element, (None, b""))[0]
    return element, ("global %s : %s%s, %s bytes"
                     % (name, table.spell(element), suffix,
                        row["size"] if row["size"] is not None else "?"))


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("--pdb", type=Path, default=DEFAULT_PDB)
    parser.add_argument("--struct", action="append", default=[],
                        metavar="NAME",
                        help="a struct/union/enum tag, or a PDB global whose"
                             " (array of) aggregate type is expanded")
    parser.add_argument("--index", action="append", default=[],
                        metavar="0xNNNN", help="a TPI type index")
    parser.add_argument("--grep", metavar="RE",
                        help="list defined tags matching this expression")
    parser.add_argument("--json", dest="as_json", action="store_true")
    options = parser.parse_args(argv)
    if not (options.struct or options.index or options.grep):
        parser.error("nothing requested: pass --struct, --index or --grep")

    try:
        if not options.pdb.is_file():
            raise Unavailable(
                "no PDB at %s. It is a private, gitignored input under"
                " research/xbox_symbols/ and is not provisioned into every"
                " worktree; copy it in or pass --pdb." % options.pdb)
        streams = pdb_streams(options.pdb.read_bytes())
        if len(streams) < 3:
            raise Unavailable("PDB has no TPI stream")
        types, header = load_types(streams[2])
        table = TypeTable(types)
    except (Unavailable, ValueError, OSError, struct.error) as error:
        print("PDB_TYPES REFUSED: %s" % error)
        return 2

    payload = {"schema_version": 1, "tool": "tools/gdl/pdb_types.py",
               "tpi": header, "layouts": [], "tags": None,
               "limits": ["Xbox CodeView records only: an offset, width or"
                          " stride here is corroboration for a GameCube"
                          " layout, never proof of one.",
                          "xbox_structs.tsv inserts synthetic __alignN"
                          " padding rows the TYPE stream does not hold."]}
    failures = []
    if options.grep is not None:
        try:
            pattern = re.compile(options.grep)
        except re.error as error:
            print("PDB_TYPES REFUSED: bad --grep: %s" % error)
            return 2
        payload["tags"] = sorted(name for name in table.by_name
                                 if pattern.search(name))
        if not options.as_json:
            print("%d defined tag(s) match %r" % (len(payload["tags"]),
                                                  options.grep))
            for name in payload["tags"]:
                print("  %-40s type 0x%x  size %s"
                      % (name, table.by_name[name],
                         table.size(table.by_name[name])))

    requests = [(name, None) for name in options.struct]
    for text in options.index:
        try:
            requests.append((None, int(text, 0)))
        except ValueError:
            failures.append("bad --index %r" % text)
    for name, index in requests:
        try:
            note = None
            if index is None:
                index, note = resolve_request(table, streams, name)
            layout = table.expand(index)
            layout["requested"] = name or "0x%x" % index
            layout["note"] = note
            payload["layouts"].append(layout)
            if not options.as_json:
                print(render(layout, note))
                print("")
        except (Unavailable, ValueError, struct.error) as error:
            failures.append("%s: %s" % (name or "0x%x" % index, error))
    payload["refusals"] = failures
    if options.as_json:
        print(json.dumps(payload, indent=1))
    for text in failures:
        print("PDB_TYPES REFUSED: %s" % text)
    return 2 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
