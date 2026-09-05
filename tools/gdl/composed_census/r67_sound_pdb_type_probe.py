"""Read SOUNDS data types from PDB 2.0 without rewriting the symbol corpus.

The existing pdb20_dump.py writes its index at import. This bounded reader
uses its documented stream/module record layout but exposes no write to it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

EXPECTED_NAMES = frozenset(("MovieBanks", "MovieMusic", "letter", "pantab", "elev_desc"))
# Microsoft microsoft-pdb/include/cvinfo.h: TYPE_ENUM_e and SYM_ENUM_e.
TYPE_NAMES = {0x70: "char", 0x74: "signed int32", 0x470: "32-bit pointer to char"}


def inspect(path):
    data = Path(path).read_bytes()
    if not data.startswith(b"Microsoft C/C++ program database 2.00"):
        raise ValueError("expected PDB 2.0")
    if len(data) < 0x3E:
        raise ValueError("truncated PDB header")
    page_size, _start, _pages = struct.unpack_from("<IHH", data, 0x2C)
    if page_size == 0 or page_size > len(data):
        raise ValueError("invalid PDB page size")
    root_size = struct.unpack_from("<I", data, 0x34)[0]
    def pages(indices, size):
        if any(p*page_size >= len(data) for p in indices):
            raise ValueError("PDB stream page outside file")
        return b"".join(data[p*page_size:(p+1)*page_size] for p in indices)[:size]
    root = pages(struct.unpack_from("<%dH" % ((root_size+page_size-1)//page_size), data, 0x3C), root_size)
    count = struct.unpack_from("<H", root)[0]
    sizes = [struct.unpack_from("<I", root, 4+i*8)[0] for i in range(count)]
    sizes = [0 if n == 0xFFFFFFFF else n for n in sizes]
    pos = 4 + count*8
    streams = []
    for size in sizes:
        n = (size+page_size-1)//page_size
        streams.append(pages(struct.unpack_from("<%dH" % n, root, pos), size))
        pos += n*2
    dbi = streams[3]
    modsize = struct.unpack_from("<I", dbi, 24)[0]
    pos, found = 64, []
    while pos < 64+modsize:
        sn = struct.unpack_from("<H", dbi, pos+34)[0]
        symsize = struct.unpack_from("<I", dbi, pos+36)[0]
        end = dbi.index(b"\0", pos+64)
        name = dbi[pos+64:end].decode("latin1")
        end2 = dbi.index(b"\0", end+1)
        pos = (end2+4) & ~3
        if "SOUNDS.OBJ" not in name.upper():
            continue
        buf, at = streams[sn][:symsize], 4
        while at+4 <= len(buf):
            reclen, kind = struct.unpack_from("<HH", buf, at)
            if reclen < 2:
                raise ValueError("invalid symbol record length")
            body = at+4
            if kind in (0x1007, 0x1008):
                typ, off, seg = struct.unpack_from("<IIH", buf, body)
                n = buf[body+10]
                sym = buf[body+11:body+11+n].decode("latin1")
                if sym in EXPECTED_NAMES:
                    found.append({"module": name, "name": sym, "type_index": hex(typ),
                                  "segment": seg, "offset": hex(off), "record_kind": hex(kind)})
            at += reclen+2
    tpi = streams[2]
    version, header, first, last, size = struct.unpack_from("<5I", tpi)
    types, at = {}, header
    for index in range(first, last):
        n, leaf = struct.unpack_from("<HH", tpi, at)
        types[index] = (leaf, tpi[at+4:at+2+n])
        at += 2+n
    def describe(index, depth=0):
        if depth > 5:
            return {"index": hex(index), "truncated": True}
        if index not in types:
            return {"simple_type_code": hex(index), "simple_type_name": TYPE_NAMES.get(index)}
        leaf, body = types[index]
        out = {"index": hex(index), "leaf": hex(leaf), "payload_hex": body.hex()}
        if leaf in (0x1003, 0x1503):
            elem, idx, amount = struct.unpack_from("<IIH", body)
            out.update(element=describe(elem, depth+1), index_type=describe(idx, depth+1),
                       size_bytes=amount if amount < 0x8000 else None)
        elif leaf in (0x1001, 0x1002):
            out["target"] = describe(struct.unpack_from("<I", body)[0], depth+1)
        return out
    for row in found:
        row["type"] = describe(int(row["type_index"], 16))
    missing = sorted(EXPECTED_NAMES - {r["name"] for r in found})
    return {"schema_version": 1, "status": "PASS" if not missing and len(found) == 5 else "UNRESOLVED",
            "pdb_sha256": hashlib.sha256(data).hexdigest(), "tpi_version": version,
            "missing_names": missing, "rows": found,
            "type_code_reference": "https://github.com/microsoft/microsoft-pdb/blob/master/include/cvinfo.h",
            "scope": "Xbox type/name evidence, not GameCube ownership proof"}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("pdb", type=Path)
    p.add_argument("--out", type=Path, default=Path("build/r67_sound_pdb_types.json"))
    args = p.parse_args()
    root = Path(__file__).resolve().parents[3]
    output = args.out.resolve()
    if not output.is_relative_to((root / "build").resolve()) or not output.name.startswith("r67_sound") or output.suffix != ".json":
        p.error("output must be a build/r67_sound*.json artifact")
    result = inspect(args.pdb)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["status"] == "PASS" else 2)
