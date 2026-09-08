#!/usr/bin/env python3
"""Every global in the Xbox PDB's shared symbol-record stream, with its size.

    python tools/gdl/pdb_globals.py                       # full dump
    python tools/gdl/pdb_globals.py --grep restore_pos
    python tools/gdl/pdb_globals.py --grep '^Sandglass' --json
    python tools/gdl/pdb_globals.py --publics             # add S_PUB32 rows

WHY THIS EXISTS. `research/xbox_symbols/functions_by_module.txt` is generated
from the PDB's PER-MODULE symbol streams, and it is not a global-data census.
Codex's r158 investigation found three objects that appear nowhere in it and
that the GameCube target plainly uses (`from-codex-r158-gamemain-global-symbols`):

    SandglassBlit   16 bytes, segment 9 offset 0xAD4880   -> lbl_80257630
    soft_reset      int[4],   segment 9 offset 0xAD4810   -> lbl_80257640
    restore_pos     float[4][3], segment 9 offset 0xAD4840 -> lbl_80257650

They live in the DBI's SHARED symbol-record stream instead. An empty module
search is not an absence certificate, and that is the whole point of this
tool: the roster answers "which module", this answers "does the name exist at
all, and how big is it".

WHAT IS AND IS NOT EVIDENCE. A name, a size and a type index are facts about
the PDB. `segment:offset` is a PDB section address and does NOT map to the
stripped XBE, let alone to the GameCube DOL: the PDB's DBI optional header
carries no OMAP stream, so there is no address translation to apply. Identify
a datum by its SIZE, its TYPE and the accesses around it -- never by assuming
one delta between a PDB offset and a GC address. Two objects of equal size are
not the same object.

REUSES the tested PDB 2.0 readers already in this repository --
`composed_census.r68_sound_data_recovery.pdb_streams` and `describe_types` --
rather than a fourth private parser. What is new here is the SHARED stream
walk (the per-module walk is `sound_symbols`), the CodeView primitive table
that gives a size to `float`, `long` and pointer types the type stream never
records a leaf for, and the REFSYM skip below.

THE REFSYM TRAP. `S_PROCREF` (0x400), `S_DATAREF` (0x401) and `S_LPROCREF`
(0x403) carry an old-CodeView length-prefixed name AFTER their declared record
length (Microsoft cvinfo.h REFSYM), and the next record starts at the next
4-byte boundary past it. A walker that trusts `reclen` alone desynchronizes at
the first one and then either crashes or invents symbols out of misaligned
bytes. Codex's private reader found this; it is reproduced here with an
explicit bound check, and a malformed stream REFUSES rather than guessing.

Writes the full dump to `build/GUNE5D/pdb_globals.txt` (ignored, never
tracked). The PDB itself is a private input under `research/xbox_symbols/`,
gitignored, and is not provisioned into every worktree: when it is absent this
tool refuses with the path it looked for, and the live tests skip.

IMPORTABLE CORE: primitive_type, type_size, iterate_symbols,
parse_data_symbol, read_globals, format_rows -- pure over bytes; no writing.
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"
DEFAULT_PDB = REPO / "research" / "xbox_symbols" / "shell3D.pdb"
DEFAULT_OUT = REPO / "build" / VERSION / "pdb_globals.txt"

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent / "composed_census"))

try:
    from composed_census.r68_sound_data_recovery import (describe_types,
                                                         pdb_streams)
except ImportError:  # run from tools/gdl, or as tools.gdl.pdb_globals
    from r68_sound_data_recovery import describe_types, pdb_streams  # noqa: F401

#: CodeView symbol kinds this reads. S_PUB32 carries flags where the other two
#: carry a type index, so it is opt-in and its rows have no type.
S_LDATA32, S_GDATA32, S_PUB32 = 0x1007, 0x1008, 0x1009
#: Records whose name sits OUTSIDE the declared length (cvinfo.h REFSYM).
REFSYM_KINDS = (0x400, 0x401, 0x403)
#: CodeView primitive type indices: (index & 0xFF) -> (C spelling, bytes).
#: The type stream stores no leaf for these, so a `float[4][3]` array knows
#: its own 48 bytes but its element would otherwise be `unmodelled`.
PRIMITIVE_BASE = {
    0x00: ("<no type>", 0), 0x03: ("void", 0),
    0x10: ("signed char", 1), 0x11: ("short", 2), 0x12: ("long", 4),
    0x13: ("__int64", 8),
    0x20: ("unsigned char", 1), 0x21: ("unsigned short", 2),
    0x22: ("unsigned long", 4), 0x23: ("unsigned __int64", 8),
    0x30: ("bool", 1), 0x40: ("float", 4), 0x41: ("double", 8),
    0x42: ("long double", 10),
    0x68: ("__int8", 1), 0x69: ("unsigned __int8", 1),
    0x70: ("char", 1), 0x71: ("wchar_t", 2), 0x72: ("__int16", 2),
    0x73: ("unsigned __int16", 2), 0x74: ("int", 4),
    0x75: ("unsigned int", 4), 0x76: ("__int64", 8),
    0x77: ("unsigned __int64", 8),
}
#: Pointer modes in bits 8-11 of a primitive index; 4 == near 32-bit.
_NEAR32 = 0x400


class Unavailable(RuntimeError):
    """The PDB could not be read, or its stream could not be walked."""


def primitive_type(index):
    """('C spelling', size) for a CodeView primitive index, else None.

    Indices at or above 0x1000 belong to the type stream, not here.
    """
    if not isinstance(index, int) or index >= 0x1000 or index < 0:
        return None
    base = PRIMITIVE_BASE.get(index & 0xFF)
    if base is None:
        return None
    mode = index & 0xF00
    if mode == _NEAR32:
        return (base[0] + " *", 4)
    if mode:
        return None
    return base


def type_size(description):
    """Bytes a `describe_types` description covers, or None when unknown.

    An array leaf records its own total size, so a nested `float[4][3]` is 48
    even when the innermost element is a primitive the type stream never
    modelled. A description with no size at all returns None: reporting 0
    would read as "an empty object", which is a different claim.
    """
    if not isinstance(description, dict):
        return None
    if isinstance(description.get("size"), int):
        return description["size"]
    if description.get("leaf") in ("0x1001", "0x1002"):
        return 4                        # near 32-bit pointer; Xbox is 32-bit
    try:
        index = int(str(description.get("type_index", "")), 0)
    except ValueError:
        return None
    primitive = primitive_type(index)
    return primitive[1] if primitive else None


def type_name(description):
    """A C-ish spelling for a description, always honest about gaps.

    Array dimensions come out in SOURCE order. CodeView nests them
    outermost-first, so a naive recursive spelling prints `float[3][4]` for
    what the source declared `float[4][3]` -- a reversed shape that reads as
    a different object, which is exactly the kind of confident-and-wrong
    detail this tool must not produce. Dimensions are collected on the way
    down and emitted in that order.
    """
    if not isinstance(description, dict):
        return "?"
    dims = []
    while isinstance(description, dict) \
            and description.get("leaf") in ("0x1003", "0x1503"):
        element = description.get("element")
        count = description.get("count")
        inner = type_size(element)
        if count is None and inner:
            count = description.get("size", 0) // inner
        # An extern declaration of an incomplete array carries size 0; `[]`
        # is its honest spelling, not `[0]`.
        dims.append("" if not count else str(count))
        description = element
    suffix = "".join("[%s]" % dim for dim in dims)
    if not isinstance(description, dict):
        return "?" + suffix
    try:
        index = int(str(description.get("type_index", "")), 0)
    except ValueError:
        index = None
    primitive = primitive_type(index) if index is not None else None
    if primitive:
        return primitive[0] + suffix
    if description.get("leaf") in ("0x1001", "0x1002"):
        return type_name(description.get("target")) + " *" + suffix
    return "type%s%s" % (description.get("type_index", "?"), suffix)


def iterate_symbols(buf):
    """Yield (record offset, kind, body bytes) over a CodeView symbol stream.

    Refuses on a record that cannot be a record. A stream walker that
    "recovers" by resynchronizing invents symbols out of misaligned bytes,
    and those are indistinguishable in the output from real ones.
    """
    at = 0
    while at + 4 <= len(buf):
        length, kind = struct.unpack_from("<HH", buf, at)
        end = at + length + 2
        if length == 0 and not any(buf[at:]):
            return                      # zero padding to the end of the page
        if length < 2 or end > len(buf):
            raise Unavailable(
                "malformed symbol record at +0x%x: length %d, kind 0x%x"
                % (at, length, kind))
        yield at, kind, buf[at + 4:end]
        if kind in REFSYM_KINDS:
            # cvinfo.h REFSYM: a length-prefixed name AFTER reclen, then the
            # next record starts at the following 4-byte boundary.
            if end >= len(buf):
                return
            end = (end + 1 + buf[end] + 3) & ~3
            if end <= at:
                raise Unavailable("REFSYM at +0x%x does not advance" % at)
        at = end


def parse_data_symbol(body, kind):
    """{'type_index', 'segment', 'offset', 'name'} from an LDATA/GDATA/PUB32.

    The name is the old-CodeView length-prefixed form at body offset 10.
    """
    if len(body) < 11:
        raise Unavailable("truncated data symbol (%d bytes)" % len(body))
    first, offset, segment = struct.unpack_from("<IIH", body)
    size = body[10]
    if 11 + size > len(body):
        raise Unavailable("data symbol name runs past its record")
    return {"type_index": None if kind == S_PUB32 else first,
            "public_flags": first if kind == S_PUB32 else None,
            "segment": segment, "offset": offset,
            "name": body[11:11 + size].decode("latin1")}


def symbol_stream_index(dbi):
    """The DBI header's shared symbol-record stream number."""
    if len(dbi) < 22:
        raise Unavailable("DBI stream is too short to hold a header")
    return struct.unpack_from("<H", dbi, 20)[0]


def read_globals(streams, publics=False):
    """[row] for every global in the shared symbol-record stream.

    `streams` is `pdb_streams(...)` output. Rows carry name, size, type
    spelling, type index and `segment:offset` -- a PDB address, never a GC or
    XBE one.
    """
    if len(streams) < 4:
        raise Unavailable("PDB has no DBI stream")
    index = symbol_stream_index(streams[3])
    if not 0 <= index < len(streams):
        raise Unavailable("DBI names symbol stream %d, which does not exist"
                          % index)
    buf = streams[index]
    describe = describe_types(streams[2])
    wanted = (S_LDATA32, S_GDATA32) + ((S_PUB32,) if publics else ())
    rows = []
    for record_offset, kind, body in iterate_symbols(buf):
        if kind not in wanted:
            continue
        row = parse_data_symbol(body, kind)
        description = (describe(row["type_index"])
                       if row["type_index"] is not None else None)
        rows.append({
            "name": row["name"],
            "kind": {S_LDATA32: "LDATA32", S_GDATA32: "GDATA32",
                     S_PUB32: "PUB32"}[kind],
            "segment": row["segment"], "offset": row["offset"],
            "type_index": row["type_index"],
            "type": type_name(description) if description else None,
            "size": type_size(description) if description else None,
            "record_offset": record_offset,
            "description": description})
    return {"schema_version": 1, "tool": "tools/gdl/pdb_globals.py",
            "symbol_stream": index, "symbol_stream_bytes": len(buf),
            "rows": rows,
            "limits": ["segment:offset is a PDB address. The DBI optional"
                       " header carries no OMAP stream, so it maps to neither"
                       " the stripped XBE nor the GameCube DOL.",
                       "Equal size is not identity: match a datum by size,"
                       " type AND the accesses around it.",
                       "Only LDATA32/GDATA32 (and S_PUB32 with --publics) are"
                       " read; this is not a complete symbol census."]}


def format_rows(result, rows=None):
    rows = result["rows"] if rows is None else rows
    out = ["PDB GLOBALS  stream %d, %d bytes, %d row(s)"
           % (result["symbol_stream"], result["symbol_stream_bytes"],
              len(rows)),
           "%-44s %-9s %-6s %-18s %-8s %s"
           % ("NAME", "KIND", "SIZE", "TYPE", "TYPEIDX", "SEG:OFFSET")]
    for row in rows:
        out.append("%-44s %-9s %-6s %-18s %-8s %d:%08x"
                   % (row["name"][:44], row["kind"],
                      "?" if row["size"] is None else row["size"],
                      (row["type"] or "?")[:18],
                      "-" if row["type_index"] is None
                      else "0x%x" % row["type_index"],
                      row["segment"], row["offset"]))
    out.append("LIMITS: " + " ".join(result["limits"]))
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("--pdb", type=Path, default=DEFAULT_PDB)
    parser.add_argument("--grep", help="regular expression over the NAME")
    parser.add_argument("--publics", action="store_true",
                        help="include S_PUB32 rows (no type, no size)")
    parser.add_argument("--json", dest="as_json", action="store_true")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT,
                        help="write the FULL dump here (default:"
                             " build/%s/pdb_globals.txt)" % VERSION)
    parser.add_argument("--no-out", action="store_true",
                        help="do not write the full dump")
    args = parser.parse_args(argv)
    if not args.no_out and not args.out.resolve().is_relative_to(
            (REPO / "build").resolve()):
        parser.error("--out must be under this checkout's build/")
    try:
        if not args.pdb.is_file():
            raise Unavailable(
                "no PDB at %s. It is a private, gitignored input under"
                " research/xbox_symbols/ and is not provisioned into every"
                " worktree; copy it in or pass --pdb." % args.pdb)
        result = read_globals(pdb_streams(args.pdb.read_bytes()),
                              publics=args.publics)
    except (Unavailable, ValueError, OSError, struct.error) as error:
        print("PDB_GLOBALS REFUSED: %s" % error)
        return 2
    if not args.no_out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(format_rows(result) + "\n", encoding="utf-8")
    rows = result["rows"]
    if args.grep:
        try:
            pattern = re.compile(args.grep)
        except re.error as error:
            print("PDB_GLOBALS REFUSED: bad --grep: %s" % error)
            return 2
        rows = [row for row in rows if pattern.search(row["name"])]
    if args.as_json:
        print(json.dumps(dict(result, rows=rows), indent=1))
    else:
        print(format_rows(result, rows))
        if not args.no_out:
            print("full dump: %s"
                  % args.out.resolve().relative_to(REPO.resolve()).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
