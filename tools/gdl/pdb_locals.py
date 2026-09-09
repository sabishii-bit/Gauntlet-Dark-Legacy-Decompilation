"""List the local-variable symbols the Xbox PDB keeps per function.

    python tools/gdl/pdb_locals.py --pdb <shell3D.pdb> <module-substring> [fn ...]

Walks the module streams whose name contains the substring (case-insensitive,
e.g. `player.obj`) and prints, under each S_LPROC32/S_GPROC32 record, the
S_BPREL32 (stack, frame offset) and S_REGISTER (register-resident) locals with
their names and TPI type indices. Only the names the optimised Xbox build kept
survive; a function with no rows has none recorded, not none in the source.
Parameters show as positive bprel offsets, locals as negative ones. Use it to
name the locals behind a frame pad: MWCC reserves a slot for every declared
local it never promotes, so the Xbox names are the only source of those names.
The PDB is private material and is not in the repository.
"""
import argparse
import struct
import sys

try:                       # `--help` exits 0 on stdout before any work
    import cliscreen
except ImportError:        # imported as tools.gdl.<module>
    from tools.gdl import cliscreen

cliscreen.help_only(__doc__)

S_LPROC32_ST, S_GPROC32_ST = 0x100A, 0x100B
S_BPREL32_ST, S_REGISTER_ST, S_BLOCK32_ST, S_END = 0x1006, 0x1001, 0x0007, 0x0006


def main(argv=None):
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--pdb", default="research/xbox_symbols/shell3D.pdb")
    ap.add_argument("module")
    ap.add_argument("functions", nargs="*")
    args = ap.parse_args(argv)
    try:
        data = open(args.pdb, "rb").read()
    except OSError as exc:
        print("PDB_LOCALS REFUSED: %s" % exc)
        return 2
    want = set(args.functions)
    modsub = args.module.lower()

    page_size, = struct.unpack_from("<I", data, 0x2C)
    root_size, = struct.unpack_from("<I", data, 0x34)
    nroot = (root_size + page_size - 1) // page_size
    root_pages = struct.unpack_from("<%dH" % nroot, data, 0x3C)

    def read_pages(pages, size):
        return b"".join(data[p * page_size:(p + 1) * page_size] for p in pages)[:size]

    root = read_pages(root_pages, root_size)
    num_streams, = struct.unpack_from("<H", root, 0)
    pos = 4
    sizes = []
    for _ in range(num_streams):
        size, _ptr = struct.unpack_from("<II", root, pos)
        pos += 8
        sizes.append(size if size != 0xFFFFFFFF else 0)
    streams = []
    for size in sizes:
        count = (size + page_size - 1) // page_size
        pages = struct.unpack_from("<%dH" % count, root, pos)
        pos += 2 * count
        streams.append((size, pages))

    def get_stream(index):
        size, pages = streams[index]
        return read_pages(pages, size)

    dbi = get_stream(3)
    gpmodi_size, = struct.unpack_from("<I", dbi, 24)
    hdr_len = 64
    mods = []
    pos = hdr_len
    while pos < hdr_len + gpmodi_size:
        stream_no, = struct.unpack_from("<H", dbi, pos + 34)
        cb_syms, = struct.unpack_from("<I", dbi, pos + 36)
        name_end = dbi.index(b"\0", pos + 64)
        modname = dbi[pos + 64:name_end].decode("latin-1")
        obj_end = dbi.index(b"\0", name_end + 1)
        pos = (obj_end + 1 - hdr_len + 3) // 4 * 4 + hdr_len
        mods.append((modname, stream_no, cb_syms))

    def pstr(buf, off):
        return buf[off + 1:off + 1 + buf[off]].decode("latin-1", "replace")

    shown = 0
    for modname, stream_no, cb_syms in mods:
        if modsub not in modname.lower() or stream_no == 0xFFFF or cb_syms == 0:
            continue
        syms = get_stream(stream_no)[:cb_syms]
        pos = 4
        current = None
        depth = 0
        while pos + 4 <= len(syms):
            reclen, kind = struct.unpack_from("<HH", syms, pos)
            if reclen < 2:
                break
            body = pos + 4
            if kind in (S_LPROC32_ST, S_GPROC32_ST):
                length, = struct.unpack_from("<I", syms, body + 12)
                current = pstr(syms, body + 35)
                depth = 1
                if not want or current in want:
                    print("== %s  size 0x%X" % (current, length))
                    shown += 1
            elif kind == S_BLOCK32_ST:
                depth += 1
            elif kind == S_END:
                depth -= 1
                if depth <= 0:
                    current = None
            elif current is not None and (not want or current in want):
                if kind == S_BPREL32_ST:
                    off, typind = struct.unpack_from("<iI", syms, body)
                    print("   bprel %+d  type 0x%X  %s" % (off, typind, pstr(syms, body + 8)))
                elif kind == S_REGISTER_ST:
                    typind, reg = struct.unpack_from("<IH", syms, body)
                    print("   reg %d  type 0x%X  %s" % (reg, typind, pstr(syms, body + 6)))
            pos += 2 + reclen
    if shown == 0:
        print("PDB_LOCALS: no function matched in modules containing %r" % args.module)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
