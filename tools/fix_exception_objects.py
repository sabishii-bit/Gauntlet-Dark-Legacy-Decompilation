#!/usr/bin/env python3
"""Reproduce the original CodeWarrior link-time processing for the two C++
exception-runtime TUs (NMWException.cpp / ExceptionPPC.cpp).

The retail Gauntlet Dark Legacy build linked with CodeWarrior's function-level
dead-stripping and weak-symbol deduplication. mwcc GC/1.x unconditionally
emits weak out-of-line copies of inline virtuals (verified on every GC
compiler from 1.1 to 2.0): `throw bad_exception()` in ExceptionPPC.cpp drags
in a weak copy of the inline std::exception destructor that the original
linker stripped (it is referenced by nothing but its own extabindex entry).
The stripped copies' string literals could not be removed from the monolithic
.rodata, which is why the retail binary contains orphaned "bad_alloc" /
"exception" strings.

mwld's CLI cannot strip functions from a monolithic .text section, so this
script post-processes the objects instead:

ExceptionPPC.o:
  - remove the weak __dt__Q23std9exceptionFv (.text), its extab entry and
    its extabindex entry, fixing symbols/relocations;
  - append the orphaned "exception" string (from the stripped weak
    exception::what copy) to .rodata: 0x86 -> 0x98.

NMWException.o:
  - rewrite .rodata to the retail layout {"std::exception", "bad_alloc"
    (orphan from the stripped weak bad_alloc::what copy), "exception"} and
    retarget the two string relocations;
  - reorder .sdata to the retail layout {__RTTI__Q23std9exception, thandler,
    uhandler} (the original emitted the RTTI record with the class's home
    emission, before the file-scope handler statics).

Usage: fix_exception_objects.py <NMWException.o> <ExceptionPPC.o> <stamp>
Idempotent: objects already in retail layout are left untouched.
"""
import struct
import sys

WEAK_NAME = b"__dt__Q23std9exceptionFv"


class Elf:
    def __init__(self, path):
        self.path = path
        self.data = bytearray(open(path, "rb").read())
        self.reload()

    def reload(self):
        d = self.data
        self.shoff = struct.unpack_from(">I", d, 0x20)[0]
        self.shentsize = struct.unpack_from(">H", d, 0x2E)[0]
        self.shnum = struct.unpack_from(">H", d, 0x30)[0]
        self.shstrndx = struct.unpack_from(">H", d, 0x32)[0]
        self.sh = [
            list(struct.unpack_from(">10I", d, self.shoff + i * self.shentsize))
            for i in range(self.shnum)
        ]
        stroff = self.sh[self.shstrndx][4]
        self.names = []
        for h in self.sh:
            end = d.index(b"\0", stroff + h[0])
            self.names.append(bytes(d[stroff + h[0] : end]).decode())
        self.sec = {n: i for i, n in enumerate(self.names)}
        self.isym = self.sec[".symtab"]
        self.symoff = self.sh[self.isym][4]
        self.symcount = self.sh[self.isym][5] // 16
        self.symstr = self.sh[self.sh[self.isym][6]][4]

    def sym(self, i):
        return list(struct.unpack_from(">3I2BH", self.data, self.symoff + i * 16))

    def set_sym(self, i, s):
        struct.pack_into(">3I2BH", self.data, self.symoff + i * 16, *s)

    def symname(self, i):
        s = self.sym(i)
        end = self.data.index(b"\0", self.symstr + s[0])
        return bytes(self.data[self.symstr + s[0] : end])

    def write_headers(self):
        for i, h in enumerate(self.sh):
            struct.pack_into(">10I", self.data, self.shoff + i * self.shentsize, *h)

    def resize_section(self, idx, new_bytes):
        """Replace section content, shifting later file content."""
        base, old = self.sh[idx][4], self.sh[idx][5]
        delta = len(new_bytes) - old
        self.data[base : base + old] = new_bytes
        self.sh[idx][5] = len(new_bytes)
        if delta:
            for h in self.sh:
                if h[4] > base:
                    h[4] += delta
            if self.shoff > base:
                self.shoff += delta
                struct.pack_into(">I", self.data, 0x20, self.shoff)
        self.write_headers()
        self.reload()

    def relas(self, name):
        i = self.sec.get(name)
        if i is None:
            return i, []
        base, sz = self.sh[i][4], self.sh[i][5]
        return i, [
            list(struct.unpack_from(">3I", self.data, base + k * 12))
            for k in range(sz // 12)
        ]

    def write_relas(self, idx, entries):
        blob = b"".join(struct.pack(">3I", *r) for r in entries)
        self.resize_section(idx, blob)

    def realign(self):
        """Re-pad the file so every section keeps sh_offset % sh_addralign
        == 0 after resizes. mwld's linker does not care, but dtk/objdiff's
        report generator rejects the object outright ("Invalid ELF section
        header offset/size/alignment")."""
        for _ in range(64):  # settles in a couple of passes
            fixed_any = False
            for i in sorted(range(1, self.shnum), key=lambda k: self.sh[k][4]):
                typ, off, align = self.sh[i][1], self.sh[i][4], self.sh[i][8]
                if typ == 8 or align <= 1 or off == 0:
                    continue  # SHT_NOBITS / no alignment demand
                pad = (-off) % align
                if not pad:
                    continue
                self.data[off:off] = b"\0" * pad
                for h in self.sh:
                    if h[4] >= off:
                        h[4] += pad
                if self.shoff >= off:
                    self.shoff += pad
                    struct.pack_into(">I", self.data, 0x20, self.shoff)
                fixed_any = True
                break  # offsets moved; restart the scan
            if not fixed_any:
                break
        self.write_headers()
        self.reload()

    def save(self):
        self.realign()
        self.write_headers()
        # atomic replace: concurrent readers (objdiff report) must never see
        # a truncated file
        import os

        tmp = self.path + ".tmp"
        with open(tmp, "wb") as fh:
            fh.write(self.data)
        os.replace(tmp, self.path)


def pad_to(b, n, size):
    return b + b"\0" * (size - len(b) - n) if False else b


def fix_exppc(path):
    elf = Elf(path)
    changed = False

    # --- 1) strip the weak exception dtor ---
    weak_idx = None
    for i in range(elf.symcount):
        s = elf.sym(i)
        if (
            (s[3] >> 4) == 2
            and s[5] == elf.sec[".text"]
            and elf.symname(i) == WEAK_NAME
        ):
            weak_idx = i
            break

    if weak_idx is not None:
        itext, iextab, ieti = elf.sec[".text"], elf.sec["extab"], elf.sec["extabindex"]
        ws = elf.sym(weak_idx)
        tcut, tlen = ws[1], (ws[2] + 3) & ~3

        _, relaeti = elf.relas(".relaextabindex")
        eti_cut = None
        for r in relaeti:
            if (r[1] >> 8) == weak_idx:
                eti_cut = r[0] - (r[0] % 12)
                break
        assert eti_cut is not None
        etab_cut = etab_len = None
        for r in relaeti:
            if r[0] == eti_cut + 8:
                ts = elf.sym(r[1] >> 8)
                assert ts[5] == iextab
                etab_cut = ts[1] + r[2]
                nxt = elf.sh[iextab][5]
                for j in range(elf.symcount):
                    sj = elf.sym(j)
                    if sj[5] == iextab and etab_cut < sj[1] < nxt:
                        nxt = sj[1]
                etab_len = (ts[2] if ts[1] == etab_cut and ts[2] else nxt - etab_cut)
                break
        assert etab_cut is not None and etab_len

        # fix .rela.text: drop relocs in cut, shift later ones
        irt, rt = elf.relas(".rela.text")
        rt = [r for r in rt if not (tcut <= r[0] < tcut + tlen)]
        for r in rt:
            if r[0] >= tcut + tlen:
                r[0] -= tlen
        elf.write_relas(irt, rt)

        # fix .relaextabindex: drop the entry's relocs, shift offsets/addends
        irei, rei = elf.relas(".relaextabindex")
        rei = [r for r in rei if not (eti_cut <= r[0] < eti_cut + 12)]
        for r in rei:
            if r[0] >= eti_cut + 12:
                r[0] -= 12
            s = elf.sym(r[1] >> 8)
            if (s[3] & 0xF) == 3 and s[5] == iextab and r[2] >= etab_cut + etab_len:
                r[2] -= etab_len
        elf.write_relas(irei, rei)

        # fix .rela.data addends into .text (jumptable case labels)
        ird, rd = elf.relas(".rela.data")
        if ird is not None:
            for r in rd:
                s = elf.sym(r[1] >> 8)
                if (s[3] & 0xF) == 3 and s[5] == itext and r[2] >= tcut + tlen:
                    r[2] -= tlen
            elf.write_relas(ird, rd)

        # cut section bytes (descending file order)
        cuts = sorted(
            [(itext, tcut, tlen), (iextab, etab_cut, etab_len), (ieti, eti_cut, 12)],
            key=lambda c: elf.sh[c[0]][4] + c[1],
            reverse=True,
        )
        for idx, off, ln in cuts:
            content = bytearray(elf.data[elf.sh[idx][4] : elf.sh[idx][4] + elf.sh[idx][5]])
            del content[off : off + ln]
            elf.resize_section(idx, bytes(content))

        # remove dead symbols (mwld validates extab symbol/function pairing,
        # so neutralized placeholders are not enough) and shift the rest
        remove = set()
        keep_syms = []
        for i in range(elf.symcount):
            s = elf.sym(i)
            if (
                i == weak_idx
                or (s[5] == iextab and etab_cut <= s[1] < etab_cut + etab_len)
                or (s[5] == ieti and eti_cut <= s[1] < eti_cut + 12)
            ):
                remove.add(i)
                continue
            if s[5] == itext and s[1] >= tcut + tlen:
                s[1] -= tlen
            elif s[5] == iextab and s[1] >= etab_cut + etab_len:
                s[1] -= etab_len
            elif s[5] == ieti and s[1] >= eti_cut + 12:
                s[1] -= 12
            keep_syms.append((i, s))
        old_to_new = {}
        for new_i, (old_i, _) in enumerate(keep_syms):
            old_to_new[old_i] = new_i
        blob = b"".join(struct.pack(">3I2BH", *s) for _, s in keep_syms)
        num_locals = sum(1 for _, s in keep_syms if (s[3] >> 4) == 0)
        elf.sh[elf.isym][6] = elf.sh[elf.isym][6]  # link unchanged
        elf.sh[elf.isym][7] = num_locals  # sh_info = first non-local
        elf.resize_section(elf.isym, blob)
        # reindex all relocation symbol references
        for rname in list(elf.sec):
            if not rname.startswith(".rela"):
                continue
            ir, rl = elf.relas(rname)
            for r in rl:
                r[1] = (old_to_new[r[1] >> 8] << 8) | (r[1] & 0xFF)
            elf.write_relas(ir, rl)
        changed = True

    # --- 2) append the orphaned "exception" string to .rodata ---
    iro = elf.sec[".rodata"]
    ro = bytes(elf.data[elf.sh[iro][4] : elf.sh[iro][4] + elf.sh[iro][5]])
    if len(ro) == 0x86:
        ro += b"\0\0" + b"exception\0" + b"\0" * 6  # pad + orphan + tail pad
        assert len(ro) == 0x98
        elf.resize_section(iro, ro)
        changed = True

    if changed:
        elf.save()
    return changed


def fix_nmw(path):
    elf = Elf(path)
    changed = False
    iro, isd = elf.sec[".rodata"], elf.sec[".sdata"]

    ro = bytes(elf.data[elf.sh[iro][4] : elf.sh[iro][4] + elf.sh[iro][5]])
    if len(ro) == 0x1B:
        # compiler layout: "exception"@0 (pad to 0xC), "std::exception"@0xC
        # retail layout (claim 0x801178E8): 4 stolen tail bytes of the
        # PRECEDING unit's "bad_alloc" string ("oc\0\0"), then
        # "std::exception"@4, "bad_alloc"@0x14 (orphan from the stripped
        # weak bad_alloc::what), "exception"@0x20. The 4-byte prefix exists
        # because retail packed this rodata at 0x801178EC (4 mod 8) but
        # mwld 1.3.2 refuses to place a compiled object's .rodata below
        # 8-alignment (sh_addralign and file-offset parity are both
        # ignored); starting the claim 4 earlier keeps every byte identical
        # while giving the linker an 8-aligned start.
        new_ro = (b"oc\0\0" + b"std::exception\0\0" + b"bad_alloc\0\0\0"
                  + b"exception\0" + b"\0" * 6)
        assert len(new_ro) == 0x30
        # retarget string relocs: old rodata+0x0 -> 0x20, old rodata+0xC -> 0x4
        for rname in (".rela.text", ".rela.sdata", ".rela.data"):
            ir, rl = elf.relas(rname)
            if ir is None:
                continue
            out = []
            for r in rl:
                s = elf.sym(r[1] >> 8)
                # remap the ADDEND only for section-base relocs: the ELF
                # section symbol (STT_SECTION) or mwcc's "...rodata.0" NOTYPE
                # label at offset 0. Relocs through named string symbols
                # (@14 etc., STT_OBJECT with size) get their shift via the
                # symbol st_value remap below (both would double-shift).
                stype = s[3] & 0xF
                secbase = s[5] == iro and (
                    stype == 3 or (stype == 0 and s[1] == 0 and s[2] == 0))
                if secbase:
                    r[2] = {0x0: 0x20, 0xC: 0x4}.get(r[2], r[2])
                out.append(r)
            elf.write_relas(ir, out)
        # local anon string symbols
        for i in range(elf.symcount):
            s = elf.sym(i)
            if s[5] == iro and s[2]:
                s[1] = {0x0: 0x20, 0xC: 0x4}.get(s[1], s[1])
                elf.set_sym(i, s)
        elf.resize_section(iro, new_ro)
        changed = True

    # .sdata: compiler {thandler@0, uhandler@4, rttirec@8}
    #         retail   {rttirec@0, thandler@8, uhandler@0xC}
    sd = bytes(elf.data[elf.sh[isd][4] : elf.sh[isd][4] + elf.sh[isd][5]])
    if len(sd) == 0x10:
        # detect compiler order via symbol positions
        remap = None
        for i in range(elf.symcount):
            if elf.symname(i) == b"__RTTI__Q23std9exception":
                if elf.sym(i)[1] == 0x8:
                    remap = {0x0: 0x8, 0x4: 0xC, 0x8: 0x0, 0xC: 0x4}
                break
        if remap:
            new_sd = bytearray(0x10)
            for src, dst in remap.items():
                new_sd[dst : dst + 4] = sd[src : src + 4]
            elf.resize_section(isd, bytes(new_sd))
            for rname in (".rela.sdata",):
                ir, rl = elf.relas(rname)
                for r in rl:
                    r[0] = remap.get(r[0], r[0])
                elf.write_relas(ir, rl)
            # relocs elsewhere targeting sdata section symbol by addend
            for rname in (".rela.text", ".rela.data"):
                ir, rl = elf.relas(rname)
                if ir is None:
                    continue
                for r in rl:
                    s = elf.sym(r[1] >> 8)
                    if (s[3] & 0xF) == 3 and s[5] == isd:
                        r[2] = remap.get(r[2], r[2])
                elf.write_relas(ir, rl)
            for i in range(elf.symcount):
                s = elf.sym(i)
                if s[5] == isd:
                    s[1] = remap.get(s[1], s[1])
                    elf.set_sym(i, s)
            changed = True

    if changed:
        elf.save()
    return changed


def main():
    nmw, exppc, stamp = sys.argv[1], sys.argv[2], sys.argv[3]
    a = fix_nmw(nmw)
    b = fix_exppc(exppc)
    # unconditionally re-align section offsets (dtk/objdiff report requires
    # sh_offset % sh_addralign == 0; also heals objects fixed before the
    # realign pass existed)
    for path in (nmw, exppc):
        Elf(path).save()
    # the retail link placed NMWException's .rodata at 0x801178EC (4 mod 8);
    # our compile declares sh_addralign 8, which would make mwld pad the
    # monolithic .rodata (+0x14 cascade). Drop it to 4 to pack like retail.
    e = Elf(nmw)
    ro = e.sec.get(".rodata")
    if ro is not None and e.sh[ro][8] > 4:
        e.sh[ro][8] = 4
        e.save()
    with open(stamp, "w") as fh:
        fh.write(f"nmw={'fixed' if a else 'ok'} exppc={'fixed' if b else 'ok'}\n")


if __name__ == "__main__":
    main()
