"""Read-only SOUNDS PDB array census and controlled natural-data experiments.

Xbox types corroborate names/dimensions; GameCube bytes and consumers decide
target layout. This reader never imports pdb20_dump (which writes on import).
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[3]


def pdb_streams(data):
    if not data.startswith(b"Microsoft C/C++ program database 2.00"):
        raise ValueError("expected PDB 2.0")
    if len(data) < 0x3E:
        raise ValueError("truncated PDB header")
    page_size = struct.unpack_from("<I", data, 0x2C)[0]
    root_size = struct.unpack_from("<I", data, 0x34)[0]
    if not 0 < page_size <= len(data):
        raise ValueError("invalid page size")

    def pages(indices, size):
        if any((p + 1) * page_size > len(data) for p in indices):
            raise ValueError("page outside PDB")
        return b"".join(data[p*page_size:(p+1)*page_size] for p in indices)[:size]

    n = (root_size + page_size - 1) // page_size
    root = pages(struct.unpack_from(f"<{n}H", data, 0x3C), root_size)
    count = struct.unpack_from("<H", root)[0]
    sizes = [struct.unpack_from("<I", root, 4 + i*8)[0] for i in range(count)]
    at, streams = 4 + count*8, []
    for size in sizes:
        size = 0 if size == 0xFFFFFFFF else size
        n = (size + page_size - 1) // page_size
        streams.append(pages(struct.unpack_from(f"<{n}H", root, at), size))
        at += n*2
    return streams


def describe_types(tpi):
    version, header, first, last, size = struct.unpack_from("<5I", tpi)
    types, at = {}, header
    for index in range(first, last):
        n, leaf = struct.unpack_from("<HH", tpi, at)
        if n < 2 or at + n + 2 > len(tpi):
            raise ValueError("invalid type record")
        types[index] = (leaf, tpi[at+4:at+n+2])
        at += n + 2

    def describe(index, depth=0):
        if depth > 10:
            raise ValueError("type recursion limit")
        primitive = {0x70: ("char", 1), 0x74: ("int", 4),
                     0x75: ("unsigned int", 4), 0x470: ("char*", 4)}
        if index in primitive:
            name, amount = primitive[index]
            return {"type_index": hex(index), "c_type": name, "size": amount}
        if index not in types:
            return {"type_index": hex(index), "unmodelled": True}
        leaf, body = types[index]
        out = {"type_index": hex(index), "leaf": hex(leaf)}
        if leaf in (0x1003, 0x1503):
            elem, idx, amount = struct.unpack_from("<IIH", body)
            if amount >= 0x8000:
                out["unmodelled_numeric_leaf"] = hex(amount)
                return out
            child = describe(elem, depth + 1)
            out.update(size=amount, element=child)
            if child.get("size") and amount % child["size"] == 0:
                out["count"] = amount // child["size"]
        elif leaf in (0x1001, 0x1002):
            out["target"] = describe(struct.unpack_from("<I", body)[0], depth + 1)
        else:
            out["unmodelled"] = True
        return out
    return describe


def sound_symbols(buf, describe):
    """Use a PROC's explicit pEnd interval, not last-seen function order."""
    at, rows, procedures, record_kinds = 4, [], [], {}
    while at + 4 <= len(buf):
        length, kind = struct.unpack_from("<HH", buf, at)
        end = at + length + 2
        if length < 2 or end > len(buf):
            raise ValueError("invalid symbol record")
        body = buf[at+4:end]
        record_kinds[at] = kind

        def pstr(off):
            n = body[off]
            if off + n + 1 > len(body):
                raise ValueError("truncated symbol name")
            return body[off+1:off+n+1].decode("latin1")

        if kind in (0x100A, 0x100B):
            parent, pend, pnext, size, dbgs, dbge, typ, off, seg = struct.unpack_from("<8IH", body)
            if not at < pend < len(buf):
                raise ValueError("invalid procedure end")
            procedures.append({"name": pstr(35), "record_offset": at,
                               "end_record_offset": pend, "size": size,
                               "segment": seg, "offset": off})
        elif kind in (0x1007, 0x1008):
            typ, off, seg = struct.unpack_from("<IIH", body)
            rows.append({"name": pstr(10), "record_offset": at, "kind": hex(kind),
                         "segment": seg, "offset": off, "type": describe(typ)})
        at = end
    for proc in procedures:
        if record_kinds.get(proc["end_record_offset"]) != 0x6:
            raise ValueError("procedure end is not an S_END record")
    for row in rows:
        owners = [p for p in procedures if p["record_offset"] < row["record_offset"] < p["end_record_offset"]]
        owners.sort(key=lambda p: p["end_record_offset"] - p["record_offset"])
        row["enclosing_function"] = owners[0]["name"] if owners else None
    return rows, procedures


def inspect_pdb(path):
    data = Path(path).read_bytes()
    streams = pdb_streams(data)
    describe = describe_types(streams[2])
    dbi, at, result = streams[3], 64, []
    modsize = struct.unpack_from("<I", dbi, 24)[0]
    while at < 64 + modsize:
        sn = struct.unpack_from("<H", dbi, at+34)[0]
        symsize = struct.unpack_from("<I", dbi, at+36)[0]
        end = dbi.index(b"\0", at+64)
        name = dbi[at+64:end].decode("latin1")
        end2 = dbi.index(b"\0", end+1)
        at = (end2+4) & ~3
        if "SOUNDS.OBJ" in name.upper():
            rows, procs = sound_symbols(streams[sn][:symsize], describe)
            result.append({"module": name, "data": rows, "procedures": procs})
    if len(result) != 1:
        raise ValueError(f"expected one SOUNDS module, got {len(result)}")
    return {"schema_version": 1, "pdb_sha256": hashlib.sha256(data).hexdigest(),
            "scope": "Xbox lexical scope and types only; not automatic GC correspondence",
            **result[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdb", type=Path)
    parser.add_argument("--out", type=Path, default=Path("build/r68_sound_pdb.json"))
    args = parser.parse_args()
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or not output.name.startswith("r68_sound") or output.suffix != ".json":
        parser.error("output must be build/r68_sound*.json")
    result = inspect_pdb(args.pdb)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{len(result['data'])} data records, {len(result['procedures'])} procedures -> {output}")


if __name__ == "__main__":
    main()
