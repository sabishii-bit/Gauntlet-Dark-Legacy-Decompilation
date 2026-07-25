import struct

PATH = 'W:/Repositories/Gauntlet-Dark-Legacy-Decompilation/research/xbox_symbols/shell3D.pdb'
OUT = 'W:/Repositories/Gauntlet-Dark-Legacy-Decompilation/research/xbox_symbols/functions_by_module.txt'
d = open(PATH, 'rb').read()

page_size, start_page, num_pages = struct.unpack_from('<IHH', d, 0x2C)
root_size, reserved = struct.unpack_from('<II', d, 0x34)
nroot_pages = (root_size + page_size - 1) // page_size
root_pages = struct.unpack_from('<%dH' % nroot_pages, d, 0x3C)

def read_pages(pages, size):
    out = b''.join(d[p*page_size:(p+1)*page_size] for p in pages)
    return out[:size]

root = read_pages(root_pages, root_size)
num_streams, = struct.unpack_from('<H', root, 0)
pos = 4
sizes = []
for i in range(num_streams):
    sz, ptr = struct.unpack_from('<II', root, pos)
    pos += 8
    sizes.append(sz if sz != 0xFFFFFFFF else 0)
streams = []
for i, sz in enumerate(sizes):
    np = (sz + page_size - 1) // page_size
    pgs = struct.unpack_from('<%dH' % np, root, pos)
    pos += 2*np
    streams.append((sz, pgs))

def get_stream(i):
    sz, pgs = streams[i]
    return read_pages(pgs, sz)

dbi = get_stream(3)
(magic, ver, age, gs, pdbv1, ps, pdbv2, symrec, pdbv3,
 gpmodi_size, sc_size, secmap_size, filinf_size, tsmap_size,
 mfc_idx, dbghdr_size, ecinfo_size, flags, machine, resvd) = struct.unpack_from('<iIIHHHHHHIIIIIIIIHHI', dbi, 0)
hdr_len = 64

mods = []
pos = hdr_len
end = hdr_len + gpmodi_size
while pos < end:
    sn, = struct.unpack_from('<H', dbi, pos+34)
    cbSyms, = struct.unpack_from('<I', dbi, pos+36)
    name_off = pos + 64
    e1 = dbi.index(b'\0', name_off)
    modname = dbi[name_off:e1].decode('latin-1')
    e2 = dbi.index(b'\0', e1+1)
    objname = dbi[e1+1:e2].decode('latin-1')
    pos = e2 + 1
    pos = (pos - hdr_len + 3) // 4 * 4 + hdr_len
    mods.append((modname, objname, sn, cbSyms))

S_LPROC32_ST = 0x100A
S_GPROC32_ST = 0x100B
S_LPROC32_SZ = 0x110F
S_GPROC32_SZ = 0x1110
S_LDATA32_ST = 0x1007
S_GDATA32_ST = 0x1008
S_PUB32_ST   = 0x1009

def pstr(buf, off):
    n = buf[off]
    return buf[off+1:off+1+n].decode('latin-1', 'replace')

def zstr(buf, off):
    e = buf.index(b'\0', off)
    return buf[off:e].decode('latin-1', 'replace')

total_fns = 0
kinds_seen = {}
out = open(OUT, 'w', encoding='utf-8', newline='\n')
out.write('# Function symbols per module, extracted from shell3D.pdb (Xbox GDL)\n')
out.write('# format: [seg:offset] size kind name   (kind: G=global fn, L=static fn, D=data)\n\n')
for modname, objname, sn, cbSyms in mods:
    if sn == 0xFFFF or sn >= len(streams) or cbSyms == 0:
        continue
    ms = get_stream(sn)[:cbSyms]
    p = 4  # skip signature u32
    entries = []
    while p + 4 <= len(ms):
        reclen, kind = struct.unpack_from('<HH', ms, p)
        if reclen < 2:
            break
        body = p + 4
        try:
            if kind in (S_LPROC32_ST, S_GPROC32_ST):
                parent, pend, pnext, length, dbgs, dbge, typind, off, seg = struct.unpack_from('<IIIIIIIIH', ms, body)
                flags_off = body + 34
                name = pstr(ms, flags_off + 1)
                entries.append((seg, off, length, 'G' if kind == S_GPROC32_ST else 'L', name))
            elif kind in (S_LPROC32_SZ, S_GPROC32_SZ):
                parent, pend, pnext, length, dbgs, dbge, typind, off, seg = struct.unpack_from('<IIIIIIIIH', ms, body)
                name = zstr(ms, body + 35)
                entries.append((seg, off, length, 'G' if kind == S_GPROC32_SZ else 'L', name))
            elif kind in (S_LDATA32_ST, S_GDATA32_ST):
                typind, off, seg = struct.unpack_from('<IIH', ms, body)
                name = pstr(ms, body + 10)
                entries.append((seg, off, 0, 'D', name))
        except Exception:
            pass
        kinds_seen[kind] = kinds_seen.get(kind, 0) + 1
        p += 2 + reclen
    if entries:
        out.write('== %s (%s)\n' % (modname, objname))
        for seg, off, length, k, name in entries:
            out.write('[%04X:%08X] %6X %s %s\n' % (seg, off, length, k, name))
        out.write('\n')
        total_fns += sum(1 for e in entries if e[3] in 'GL')
out.close()
print('total functions:', total_fns)
import collections
top = sorted(kinds_seen.items(), key=lambda x: -x[1])[:12]
print('record kinds:', ['0x%04X:%d' % kv for kv in top])
