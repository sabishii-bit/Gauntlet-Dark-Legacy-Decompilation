#!/usr/bin/env python3
"""Apply hash-guarded, individually audited MWCC corrections to an ELF object.

Frank changes scheduling by compiling with a synthetic profile side effect and
then removing the instrumentation.  ``webfrank`` is the deliberately narrower
allocator analogue. Its ordinary rules change only PowerPC register fields in
audited functions. A rule may name explicit GPR recolors or copy the four
five-bit register slots from the extracted target after proving every other
instruction bit already agrees.

The narrower scheduler extension may explicitly permute a proved-independent,
straight-line list of existing instruction atoms. It rejects control
instructions, requires a complete bijection and dependency audit, moves each
relocation with its instruction atom, and verifies region, relocation, and full
function hashes before and after. It never copies opcodes, immediates, branch
encodings, relocation payloads, or data from the target.

Every patch is guarded by complete before/after function SHA-256 hashes.  A
source, compiler, or layout change therefore fails the build instead of
silently applying a stale binary rewrite.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path


SHT_SYMTAB = 2
SHT_RELA = 4
REGISTER_FIELD_SHIFTS = (6, 11, 16, 21)
REGISTER_FIELD_MASK = sum(0x1F << shift for shift in REGISTER_FIELD_SHIFTS)


@dataclass(frozen=True)
class Section:
    index: int
    name: str
    section_type: int
    offset: int
    size: int
    link: int
    info: int
    entry_size: int


@dataclass(frozen=True)
class Symbol:
    name: str
    value: int
    size: int
    section_index: int


def _u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def _u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _cstring(data: bytes | bytearray, offset: int) -> str:
    end = data.index(0, offset)
    return bytes(data[offset:end]).decode("ascii")


def _sections(data: bytes | bytearray) -> list[Section]:
    section_header = _u32(data, 0x20)
    entry_size = _u16(data, 0x2E)
    count = _u16(data, 0x30)
    names_index = _u16(data, 0x32)
    raw = []
    for index in range(count):
        offset = section_header + index * entry_size
        raw.append(struct.unpack_from(">10I", data, offset))
    names_offset = raw[names_index][4]
    result = []
    for index, header in enumerate(raw):
        name_at, section_type, _, _, offset, size, link, info, _, item_size = header
        name = _cstring(data, names_offset + name_at) if name_at else ""
        result.append(
            Section(index, name, section_type, offset, size, link, info, item_size)
        )
    return result


def _symbols(data: bytes | bytearray, sections: list[Section]) -> list[Symbol]:
    result = []
    for table in sections:
        if table.section_type != SHT_SYMTAB:
            continue
        strings = sections[table.link]
        item_size = table.entry_size or 16
        for offset in range(table.offset, table.offset + table.size, item_size):
            name_at, value, size = struct.unpack_from(">III", data, offset)
            section_index = _u16(data, offset + 14)
            symbol_name = _cstring(data, strings.offset + name_at) if name_at else ""
            if symbol_name:
                result.append(Symbol(symbol_name, value, size, section_index))
    return result


def _find_symbol(data: bytes | bytearray, sections: list[Section], name: str) -> Symbol:
    for symbol in _symbols(data, sections):
        if symbol.name == name:
            return symbol
    raise KeyError(f"symbol {name!r} not found")


def _function_text_relocations(
    data: bytes | bytearray, sections: list[Section],
    text_index: int, fn_start: int, fn_end: int,
) -> dict[int, tuple[int, str]]:
    """Function-relative text relocation offsets -> (type, symbol name)."""
    relocations: dict[int, tuple[int, str]] = {}
    symtabs = [s for s in sections if s.section_type == SHT_SYMTAB]
    strtab = sections[symtabs[0].link] if symtabs else None
    for rela in sections:
        if rela.section_type != SHT_RELA or rela.info != text_index:
            continue
        item = rela.entry_size or 12
        for position in range(rela.offset, rela.offset + rela.size, item):
            r_offset, r_info, _ = struct.unpack_from(">IIi", data, position)
            if not fn_start <= r_offset < fn_end:
                continue
            name = ""
            if symtabs:
                entry = symtabs[0].entry_size or 16
                name_at = _u32(data, symtabs[0].offset + (r_info >> 8) * entry)
                if name_at:
                    name = _cstring(data, strtab.offset + name_at)
            relocations[r_offset - fn_start] = (r_info & 0xFF, name)
    return relocations


def _jumptable_targets(
    data: bytes | bytearray, sections: list[Section],
    text_index: int, fn_start: int, fn_end: int,
) -> set[int]:
    """Function-relative offsets of potential computed-branch targets:
    R_PPC_ADDR32 data-section relocations resolving into the function."""
    targets = set()
    symtabs = [s for s in sections if s.section_type == SHT_SYMTAB]
    if not symtabs:
        return targets
    symtab = symtabs[0]
    entry = symtab.entry_size or 16
    for rela in sections:
        if rela.section_type != SHT_RELA or rela.info == text_index:
            continue
        item = rela.entry_size or 12
        for position in range(rela.offset, rela.offset + rela.size, item):
            _, r_info, r_addend = struct.unpack_from(">IIi", data, position)
            if r_info & 0xFF != 1:  # R_PPC_ADDR32
                continue
            sym_offset = symtab.offset + (r_info >> 8) * entry
            value = _u32(data, sym_offset + 4)
            section_index = _u16(data, sym_offset + 14)
            if section_index != text_index:
                continue
            resolved = value + r_addend
            if fn_start <= resolved < fn_end:
                targets.add(resolved - fn_start)
    return targets


def _parse_int(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def _map_field(word: int, shift: int, mapping: dict[int, int]) -> int:
    old = (word >> shift) & 0x1F
    return (word & ~(0x1F << shift)) | (mapping.get(old, old) << shift)


def recolor_instruction(word: int, mapping: dict[int, int]) -> int:
    """Recolor GPR operands in the small, audited PPC integer-form subset."""
    opcode = word >> 26

    # D-form integer loads/stores and addi: RT/RS, RA, immediate.
    if opcode in {14, 32, 33, 34, 35, 36, 37, 38, 39,
                  40, 41, 42, 43, 44, 45, 46, 47}:
        word = _map_field(word, 21, mapping)
        return _map_field(word, 16, mapping)

    # rlwinm (including slwi/srwi aliases): RS, RA, SH/MB/ME.
    if opcode == 21:
        word = _map_field(word, 21, mapping)
        return _map_field(word, 16, mapping)

    if opcode == 31:
        xo = (word >> 1) & 0x3FF
        # subf, mulhw and add: RT, RA, RB.
        if xo in {40, 75, 266}:
            word = _map_field(word, 21, mapping)
            word = _map_field(word, 16, mapping)
            return _map_field(word, 11, mapping)
        # srawi: RS, RA, SH.
        if xo == 824:
            word = _map_field(word, 21, mapping)
            return _map_field(word, 16, mapping)

    raise ValueError(f"unsupported instruction 0x{word:08x} (opcode {opcode})")


# ---------------------------------------------------------------------------
# Form-aware operand model
#
# PPC packs immediates, rotate counts, XO bits, and CR selectors into the same
# bit positions that other forms use for registers, so a fixed four-slot mask
# cannot prove a difference is "register only".  Every register-field rule is
# therefore decoded per form, failing closed on anything unmodelled.
#
# An operand is (bank, shift, role, zero_none):
#   bank "g" = GPR, "f" = FPR
#   role "d" = define, "u" = use, "b" = read-modify (update-form base)
#   zero_none: a zero field means "no register" (D-form base, including the
#   SDA21 case where the linker fills the base register in), never GPR r0.
# ---------------------------------------------------------------------------

_XO31_OPERANDS: dict[int, tuple] = {}
for _xo in (8, 10, 11, 40, 75, 136, 138, 235, 266, 459, 491):
    # subfc addc mulhwu subf mulhw subfe adde mullw divwu divw: RT, RA, RB
    _XO31_OPERANDS[_xo] = (("g", 21, "d", False), ("g", 16, "u", False),
                           ("g", 11, "u", False))
for _xo in (104, 200, 202, 232, 234):  # neg subfze addze subfme addme: RT, RA
    _XO31_OPERANDS[_xo] = (("g", 21, "d", False), ("g", 16, "u", False))
for _xo in (24, 28, 60, 124, 284, 316, 412, 444, 476, 536, 792):
    # slw and andc nor eqv xor orc or nand srw sraw: RS, RA, RB
    _XO31_OPERANDS[_xo] = (("g", 21, "u", False), ("g", 16, "d", False),
                           ("g", 11, "u", False))
for _xo in (26, 824, 922, 954):  # cntlzw srawi extsh extsb: RS, RA
    _XO31_OPERANDS[_xo] = (("g", 21, "u", False), ("g", 16, "d", False))
for _xo in (0, 32):  # cmp cmpl: RA, RB (crfD is not a register colour)
    _XO31_OPERANDS[_xo] = (("g", 16, "u", False), ("g", 11, "u", False))
for _xo in (23, 87, 279, 343, 534, 790):  # indexed loads: RT, RA|0, RB
    _XO31_OPERANDS[_xo] = (("g", 21, "d", False), ("g", 16, "u", True),
                           ("g", 11, "u", False))
for _xo in (55, 119, 311, 375):  # indexed load-update
    _XO31_OPERANDS[_xo] = (("g", 21, "d", False), ("g", 16, "b", False),
                           ("g", 11, "u", False))
for _xo in (151, 215, 407, 662, 918):  # indexed stores: RS, RA|0, RB
    _XO31_OPERANDS[_xo] = (("g", 21, "u", False), ("g", 16, "u", True),
                           ("g", 11, "u", False))
for _xo in (183, 247, 439):  # indexed store-update
    _XO31_OPERANDS[_xo] = (("g", 21, "u", False), ("g", 16, "b", False),
                           ("g", 11, "u", False))
for _xo in (535, 599):  # lfsx lfdx
    _XO31_OPERANDS[_xo] = (("f", 21, "d", False), ("g", 16, "u", True),
                           ("g", 11, "u", False))
for _xo in (567, 631):  # lfsux lfdux
    _XO31_OPERANDS[_xo] = (("f", 21, "d", False), ("g", 16, "b", False),
                           ("g", 11, "u", False))
for _xo in (663, 727):  # stfsx stfdx
    _XO31_OPERANDS[_xo] = (("f", 21, "u", False), ("g", 16, "u", True),
                           ("g", 11, "u", False))
for _xo in (695, 759):  # stfsux stfdux
    _XO31_OPERANDS[_xo] = (("f", 21, "u", False), ("g", 16, "b", False),
                           ("g", 11, "u", False))
for _xo in (339, 371):  # mfspr mftb
    _XO31_OPERANDS[_xo] = (("g", 21, "d", False),)
_XO31_OPERANDS[467] = (("g", 21, "u", False),)  # mtspr
_XO31_OPERANDS[19] = (("g", 21, "d", False),)   # mfcr
_XO31_OPERANDS[144] = (("g", 21, "u", False),)  # mtcrf
for _xo in (598, 854):  # sync eieio
    _XO31_OPERANDS[_xo] = ()
for _xo in (86, 470, 982, 1014):  # dcbf dcbi dcbz icbi: RA|0, RB
    _XO31_OPERANDS[_xo] = (("g", 16, "u", True), ("g", 11, "u", False))

# A-form FP (opcode 59 always; opcode 63 when the low five XO bits are 16-31):
# operand shifts per five-bit XO; 21 is FRT (define), the rest are uses.
_A_FORM_FP_SHIFTS = {
    18: (21, 16, 11), 20: (21, 16, 11), 21: (21, 16, 11),
    22: (21, 11), 23: (21, 16, 11, 6), 24: (21, 11),
    25: (21, 16, 6), 26: (21, 11),
    28: (21, 16, 11, 6), 29: (21, 16, 11, 6),
    30: (21, 16, 11, 6), 31: (21, 16, 11, 6),
}
_XO63_X_OPERANDS = {
    0: (("f", 16, "u", False), ("f", 11, "u", False)),   # fcmpu
    32: (("f", 16, "u", False), ("f", 11, "u", False)),  # fcmpo
    12: (("f", 21, "d", False), ("f", 11, "u", False)),  # frsp
    14: (("f", 21, "d", False), ("f", 11, "u", False)),  # fctiw
    15: (("f", 21, "d", False), ("f", 11, "u", False)),  # fctiwz
    40: (("f", 21, "d", False), ("f", 11, "u", False)),  # fneg
    72: (("f", 21, "d", False), ("f", 11, "u", False)),  # fmr
    136: (("f", 21, "d", False), ("f", 11, "u", False)),  # fnabs
    264: (("f", 21, "d", False), ("f", 11, "u", False)),  # fabs
    583: (("f", 21, "d", False),),                       # mffs
    711: (("f", 11, "u", False),),                       # mtfsf
}
# CR-bit/branch-unit opcode-19 forms with no GPR/FPR operands.
_XO19_NO_OPERANDS = frozenset((0, 33, 129, 150, 193, 225, 257, 289, 417, 449))


def instruction_operands(word: int) -> tuple:
    """Return the word's register operand fields, failing closed.

    lmw/stmw (opcodes 46/47) are deliberately not modelled here: their
    register field names a range, so callers must special-case them.
    """
    opcode = word >> 26
    if opcode in (7, 8, 12, 13):  # mulli subfic addic addic.
        return (("g", 21, "d", False), ("g", 16, "u", False))
    if opcode in (14, 15):  # addi/addis; RA=0 is the li/lis literal zero
        return (("g", 21, "d", False), ("g", 16, "u", True))
    if opcode in (10, 11):  # cmplwi cmpwi
        return (("g", 16, "u", False),)
    if opcode == 20:  # rlwimi
        return (("g", 21, "u", False), ("g", 16, "b", False))
    if opcode == 21:  # rlwinm
        return (("g", 21, "u", False), ("g", 16, "d", False))
    if opcode == 23:  # rlwnm
        return (("g", 21, "u", False), ("g", 16, "d", False),
                ("g", 11, "u", False))
    if opcode in (24, 25, 26, 27, 28, 29):  # ori oris xori xoris andi. andis.
        return (("g", 21, "u", False), ("g", 16, "d", False))
    if 32 <= opcode <= 45:  # integer D-form loads/stores
        update = opcode % 2 == 1
        store = opcode in (36, 37, 38, 39, 44, 45)
        return (("g", 21, "u" if store else "d", False),
                ("g", 16, "b" if update else "u", not update))
    if 48 <= opcode <= 55:  # FP D-form loads/stores
        update = opcode % 2 == 1
        store = opcode >= 52
        return (("f", 21, "u" if store else "d", False),
                ("g", 16, "b" if update else "u", not update))
    if opcode in (16, 18):  # bc/b: BO/BI/displacement are not registers
        return ()
    if opcode == 19:
        xo = (word >> 1) & 0x3FF
        if xo in (16, 528) or xo in _XO19_NO_OPERANDS:
            return ()
        raise ValueError(
            f"unsupported instruction 0x{word:08x} (opcode 19 xo {xo})"
        )
    if opcode == 31:
        xo = (word >> 1) & 0x3FF
        operands = _XO31_OPERANDS.get(xo)
        if operands is None:
            raise ValueError(
                f"unsupported instruction 0x{word:08x} (opcode 31 xo {xo})"
            )
        return operands
    if opcode in (59, 63):
        xo5 = (word >> 1) & 0x1F
        if opcode == 59 or 16 <= xo5 <= 31:
            shifts = _A_FORM_FP_SHIFTS.get(xo5)
            if shifts is None:
                raise ValueError(
                    f"unsupported instruction 0x{word:08x} "
                    f"(opcode {opcode} A-form xo {xo5})"
                )
            return tuple(
                ("f", shift, "d" if shift == 21 else "u", False)
                for shift in shifts
            )
        xo = (word >> 1) & 0x3FF
        operands = _XO63_X_OPERANDS.get(xo)
        if operands is None:
            raise ValueError(
                f"unsupported instruction 0x{word:08x} (opcode 63 xo {xo})"
            )
        return operands
    raise ValueError(f"unsupported instruction 0x{word:08x} (opcode {opcode})")


def register_slot_mask(word: int) -> int:
    """Bits that hold register operands in this word's form (0 for lmw/stmw,
    branches, and other forms with no recolourable fields)."""
    opcode = word >> 26
    if opcode in (46, 47):
        return 0
    return sum(0x1F << shift for _, shift, _, _ in instruction_operands(word))


# ---------------------------------------------------------------------------
# Function CFG, effects, and the semantic guards
#
# Byte-level guards alone cannot distinguish a register recolor from an
# instruction reorder (two independent instructions that differ only in
# register fields swap without touching a single non-register bit), so the
# register-field paths are backed by a position-lockstep renaming
# bisimulation, and the permutation path by a def-use chain audit.
# Memory model assumption, matching what the compiler's own scheduler
# assumes: the r1 frame is only accessed r1-relative, and distinct r1
# displacements are distinct locations.
# ---------------------------------------------------------------------------

_CALL_VOLATILE = tuple(
    [("g", 0)] + [("g", n) for n in range(3, 13)]
    + [("f", n) for n in range(0, 14)]
)
_CALL_RETURNS = (("g", 3), ("g", 4), ("f", 1))
_CALL_CLOBBERED = frozenset(_CALL_VOLATILE) | {
    ("cr", 0), ("cr", 1), ("cr", 5), ("cr", 6), ("cr", 7), "ctr", "lr", "ca",
}
_CALL_ARGUMENTS = frozenset(
    {("g", n) for n in range(3, 11)} | {("f", n) for n in range(1, 9)}
)
# MWCC's callee-save millicode: preserves all registers, touching only the
# named save range and the r11 save-area pointer.
_HELPER_CALL_RE = re.compile(r"^_(save|rest)(gpr|fpr)_(\d+)$")

# Operand pairs the compiler may freely exchange: the two factors of an
# integer/FP multiply or add, the two sources of symmetric logicals, and the
# FRA/FRC factors of fused multiply-adds.  Keyed by (opcode, xo); values are
# the two commuting field shifts.
_COMMUTATIVE_31 = {
    266: (16, 11), 235: (16, 11), 75: (16, 11), 11: (16, 11), 10: (16, 11),
    28: (21, 11), 444: (21, 11), 316: (21, 11),
    284: (21, 11), 476: (21, 11), 124: (21, 11),
}
for _xo in (23, 87, 279, 343, 534, 790,       # non-update indexed loads
            151, 215, 407, 662, 918,          # non-update indexed stores
            535, 599, 663, 727,               # non-update FP indexed
            86, 470, 982, 1014):              # cache ops
    # EA = RA + RB is symmetric while RA is nonzero; the swap attempt is
    # guarded so an RA=0 encoding never commutes.
    _COMMUTATIVE_31[_xo] = (16, 11)
_COMMUTATIVE_FP = {
    21: (16, 11),  # fadd(s)
    25: (16, 6),   # fmul(s)
    28: (16, 6), 29: (16, 6), 30: (16, 6), 31: (16, 6),  # fused multiplies
}


def _helper_call(name: str | None):
    if not name:
        return None
    match = _HELPER_CALL_RE.match(name)
    if not match:
        return None
    return (match.group(1), "g" if match.group(2) == "gpr" else "f",
            int(match.group(3)))


def _commutative_shifts(word: int):
    opcode = word >> 26
    if opcode == 31:
        return _COMMUTATIVE_31.get((word >> 1) & 0x3FF)
    if opcode in (59, 63):
        xo5 = (word >> 1) & 0x1F
        if opcode == 59 or 16 <= xo5 <= 31:
            return _COMMUTATIVE_FP.get(xo5)
    return None


def _sign_extend(value: int, bits: int) -> int:
    if value >= 1 << (bits - 1):
        value -= 1 << bits
    return value


def _successors(
    words: list[int],
    relocated_indexes: frozenset[int] | set[int],
    jumptable_indexes: frozenset[int] | set[int],
) -> tuple[list[list[int]], list[bool]]:
    """Per-word successor lists and call flags, failing closed on any
    control form whose targets cannot be established."""
    count = len(words)
    successors: list[list[int]] = []
    calls: list[bool] = []
    for index, word in enumerate(words):
        opcode = word >> 26
        call = False
        if opcode == 16:
            if word & 3:
                raise ValueError(f"+0x{index * 4:x}: unsupported bc AA/LK form")
            if index in relocated_indexes:
                raise ValueError(f"+0x{index * 4:x}: relocated conditional branch")
            bo = (word >> 21) & 0x1F
            target = index + _sign_extend(word & 0xFFFC, 16) // 4
            edges = [target]
            if bo & 0x14 != 0x14:
                edges.append(index + 1)
            succ = edges
        elif opcode == 18:
            if word & 2:
                raise ValueError(f"+0x{index * 4:x}: absolute branch")
            if word & 1:
                call = True
                succ = [index + 1]
            elif index in relocated_indexes:
                succ = []  # cross-function tail branch: an exit
            else:
                succ = [index + _sign_extend(word & 0x03FFFFFC, 26) // 4]
        elif opcode == 19:
            xo = (word >> 1) & 0x3FF
            bo = (word >> 21) & 0x1F
            if xo == 16:
                if word & 1:  # blrl: indirect call through LR
                    call = True
                    succ = [index + 1]
                else:
                    succ = [] if bo & 0x14 == 0x14 else [index + 1]
            elif xo == 528:
                if word & 1:
                    call = True
                    succ = [index + 1]
                else:
                    if not jumptable_indexes:
                        raise ValueError(
                            f"+0x{index * 4:x}: bctr without discoverable "
                            "jumptable targets"
                        )
                    succ = sorted(jumptable_indexes)
                    if bo & 0x14 != 0x14:
                        succ.append(index + 1)
            elif xo in _XO19_NO_OPERANDS:
                succ = [index + 1]
            else:
                raise ValueError(
                    f"+0x{index * 4:x}: unsupported opcode-19 form xo {xo}"
                )
        elif opcode == 17:
            raise ValueError(f"+0x{index * 4:x}: sc unsupported")
        else:
            succ = [index + 1]
        successors.append([s for s in succ if 0 <= s < count])
        calls.append(call)
    return successors, calls


def _map_define(state: dict, key: tuple, target_register: int) -> None:
    bank = key[0]
    for other in [
        other for other, value in state.items()
        if other[0] == bank and value == target_register and other != key
    ]:
        del state[other]
    state[key] = target_register


def _recolor_transfer(index: int, cur: int, tgt: int, state: dict) -> dict:
    opcode = cur >> 26
    if opcode in (46, 47):  # lmw/stmw name register ranges: identity only
        if cur != tgt:
            raise ValueError(f"+0x{index * 4:x}: lmw/stmw fields may not differ")
        base = (cur >> 16) & 0x1F
        if state.get(("g", base)) != base:
            raise ValueError(
                f"+0x{index * 4:x}: lmw/stmw base r{base} is not identity-mapped"
            )
        first = (cur >> 21) & 0x1F
        if opcode == 47:
            for n in range(first, 32):
                if state.get(("g", n)) != n:
                    raise ValueError(
                        f"+0x{index * 4:x}: stmw saved register r{n} is not "
                        "identity-mapped"
                    )
        else:
            for n in range(first, 32):
                _map_define(state, ("g", n), n)
        return state
    try:
        operands = instruction_operands(cur)
    except ValueError as error:
        raise ValueError(f"+0x{index * 4:x}: {error}") from None
    allowed = 0
    for _, shift, _, _ in operands:
        allowed |= 0x1F << shift
    if (cur ^ tgt) & ~allowed:
        raise ValueError(
            f"+0x{index * 4:x}: non-register bits differ "
            f"(0x{cur:08x} vs 0x{tgt:08x})"
        )
    fields = {
        shift: (bank, role, zero_none,
                (cur >> shift) & 0x1F, (tgt >> shift) & 0x1F)
        for bank, shift, role, zero_none in operands
    }
    # If the straight operand pairing of a commutative pair fails, the
    # compiler may simply have exchanged the two sources; accept the swapped
    # pairing when it is consistent instead.
    remap: dict[int, int] = {}
    pair = _commutative_shifts(cur)
    if pair is not None and all(shift in fields for shift in pair):
        first, second = pair
        bank, _, zero_1, cur_1, tgt_1 = fields[first]
        _, _, zero_2, cur_2, tgt_2 = fields[second]
        zero_involved = (zero_1 or zero_2) and 0 in (cur_1, cur_2, tgt_1, tgt_2)
        straight = (state.get((bank, cur_1)) == tgt_1
                    and state.get((bank, cur_2)) == tgt_2)
        if not straight and not zero_involved \
                and (state.get((bank, cur_1)) == tgt_2
                     and state.get((bank, cur_2)) == tgt_1):
            remap = {first: tgt_2, second: tgt_1}
    for shift, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        expected = remap.get(shift, tgt_r)
        if zero_none and (cur_r == 0 or expected == 0):
            if cur_r != expected:
                raise ValueError(
                    f"+0x{index * 4:x}: base register presence differs "
                    f"({bank}{cur_r} vs {bank}{expected})"
                )
            continue
        if role in ("u", "b") and state.get((bank, cur_r)) != expected:
            raise ValueError(
                f"+0x{index * 4:x}: use of {bank}{cur_r} does not correspond "
                f"to {bank}{expected} under the running renaming"
            )
    for _, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        if zero_none and cur_r == 0:
            continue
        if role in ("d", "b"):
            _map_define(state, (bank, cur_r), tgt_r)
    return state


def verify_consistent_recolor(
    current: bytes,
    target: bytes,
    *,
    jumptable_targets=(),
    relocated_offsets=(),
    call_targets=None,
) -> None:
    """Prove *target* is *current* under a position-consistent register
    renaming — a pure allocator recolor.  Anything else (an instruction
    reorder that only moves register fields, an operand rotation across
    fixed ABI registers, an immediate difference hiding in a register slot)
    fails closed and must be modelled explicitly instead."""
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("recolor verification needs equal word-aligned sizes")
    words_cur = [_u32(current, off) for off in range(0, len(current), 4)]
    words_tgt = [_u32(target, off) for off in range(0, len(target), 4)]
    count = len(words_cur)
    relocated = {off // 4 for off in relocated_offsets}
    jumptable = set()
    for off in jumptable_targets:
        if off % 4 or not 0 <= off < len(current):
            raise ValueError(f"invalid jumptable target +0x{off:x}")
        jumptable.add(off // 4)
    successors, calls = _successors(words_cur, relocated, jumptable)

    identity = {("g", n): n for n in range(32)}
    identity.update({("f", n): n for n in range(32)})
    in_maps: dict[int, dict] = {0: identity}
    pending = [0]
    while pending:
        index = pending.pop()
        state = dict(in_maps[index])
        state = _recolor_transfer(index, words_cur[index], words_tgt[index], state)
        if calls[index]:
            helper = _helper_call(
                call_targets.get(index * 4) if call_targets else None
            )
            if helper is None:
                for key in _CALL_VOLATILE:
                    state.pop(key, None)
                for key in _CALL_RETURNS:
                    _map_define(state, key, key[1])
            elif helper[0] == "rest":
                # The millicode restores the prologue-saved (identity) values.
                _, bank, first = helper
                for n in range(first, 32):
                    _map_define(state, (bank, n), n)
            # save helpers preserve every register: no map change
        for successor in successors[index]:
            known = in_maps.get(successor)
            if known is None:
                merged = dict(state)
            else:
                merged = {
                    key: value for key, value in known.items()
                    if state.get(key) == value
                }
            if known is None or merged != known:
                in_maps[successor] = merged
                pending.append(successor)
    for index in range(count):
        if words_cur[index] != words_tgt[index] and index not in in_maps:
            raise ValueError(
                f"+0x{index * 4:x}: differing word is unreachable from the "
                "function entry"
            )


def _word_effects(word: int) -> tuple[set, set]:
    """Architectural resources read and written by one instruction.

    Memory is ("stack", disp) for r1-relative D-form accesses, "mem" for
    other D-form accesses, and "anymem" (may-alias everything) for indexed
    and string/multiple forms.  Implicit FPSCR status updates are not
    modelled, matching the reordering freedom the compiler itself assumes.
    """
    opcode = word >> 26
    reads: set = set()
    writes: set = set()
    if opcode in (46, 47):
        base = (word >> 16) & 0x1F
        first = (word >> 21) & 0x1F
        if base:
            reads.add(("g", base))
        registers = {("g", n) for n in range(first, 32)}
        if opcode == 46:
            writes |= registers
            reads.add("anymem")
        else:
            reads |= registers
            writes.add("anymem")
        return reads, writes
    if opcode in (16, 18) or (opcode == 19 and ((word >> 1) & 0x3FF) in (16, 528)):
        bo = (word >> 21) & 0x1F
        if opcode != 18:
            if not bo & 0x10:
                reads.add(("cr", ((word >> 16) & 0x1F) >> 2))
            if not bo & 0x04:
                reads.add("ctr")
                writes.add("ctr")
        if opcode == 19:
            xo = (word >> 1) & 0x3FF
            reads.add("lr" if xo == 16 else "ctr")
        if word & 1 and opcode != 16:
            writes.add("lr")
        if opcode == 18 and word & 1:
            writes.add("lr")
        return reads, writes
    if opcode == 19:
        xo = (word >> 1) & 0x3FF
        if xo == 0:  # mcrf
            reads.add(("cr", (word >> 18) & 7))
            writes.add(("cr", (word >> 23) & 7))
        elif xo == 150:  # isync
            pass
        elif xo in _XO19_NO_OPERANDS:  # crand family, whole-field granularity
            writes.add(("cr", ((word >> 21) & 0x1F) >> 2))
            reads.add(("cr", ((word >> 16) & 0x1F) >> 2))
            reads.add(("cr", ((word >> 11) & 0x1F) >> 2))
        else:
            raise ValueError(f"unsupported opcode-19 form xo {xo}")
        return reads, writes
    for bank, shift, role, zero_none in instruction_operands(word):
        register = (word >> shift) & 0x1F
        if zero_none and register == 0:
            continue
        if role in ("u", "b"):
            reads.add((bank, register))
        if role in ("d", "b"):
            writes.add((bank, register))
    if 32 <= opcode <= 55:
        base = (word >> 16) & 0x1F
        displacement = _sign_extend(word & 0xFFFF, 16)
        store = opcode in (36, 37, 38, 39, 44, 45) or opcode >= 52
        location = ("stack", displacement) if base == 1 else "mem"
        (writes if store else reads).add(location)
    if opcode in (10, 11):
        writes.add(("cr", (word >> 23) & 7))
    if opcode in (13, 28, 29):
        writes.add(("cr", 0))
    if opcode in (8, 12, 13):
        writes.add("ca")
    if opcode == 31:
        xo = (word >> 1) & 0x3FF
        if word & 1:
            writes.add(("cr", 0))
        if xo in (0, 32):
            writes.add(("cr", (word >> 23) & 7))
            writes.discard(("cr", 0))
        if xo in (8, 10, 792, 824):
            writes.add("ca")
        if xo in (136, 138, 200, 202, 232, 234):
            reads.add("ca")
            writes.add("ca")
        if xo in (339, 467):
            spr = ((word >> 16) & 0x1F) | (((word >> 11) & 0x1F) << 5)
            name = {1: "xer", 8: "lr", 9: "ctr"}.get(spr, f"spr{spr}")
            (reads if xo == 339 else writes).add(name)
        if xo == 19:
            reads |= {("cr", n) for n in range(8)}
        if xo == 144:
            crm = (word >> 12) & 0xFF
            writes |= {("cr", 7 - n) for n in range(8) if crm & (1 << n)}
        if xo in (23, 55, 87, 119, 279, 311, 343, 375, 534, 790,
                  535, 567, 599, 631):
            reads.add("anymem")
        if xo in (151, 183, 215, 247, 407, 439, 662, 918,
                  663, 695, 727, 759, 86, 470, 1014):
            writes.add("anymem")
    if opcode in (59, 63):
        if word & 1:
            writes.add(("cr", 1))
        xo = (word >> 1) & 0x3FF
        if opcode == 63 and not 16 <= (xo & 0x1F) <= 31:
            if xo in (0, 32):
                writes.add(("cr", (word >> 23) & 7))
                writes.discard(("cr", 1))
            if xo == 583:
                reads.add("fpscr")
            if xo == 711:
                writes.add("fpscr")
    return reads, writes


def _is_memory(resource) -> bool:
    return resource in ("mem", "anymem") or (
        isinstance(resource, tuple) and resource[0] == "stack"
    )


def _touches(resource, group) -> bool:
    if resource in group:
        return True
    if "anymem" in group and _is_memory(resource):
        return True
    if resource == "anymem" and any(_is_memory(item) for item in group):
        return True
    return False


def _frame_size(words: list[int]) -> int:
    if words and (words[0] >> 26) == 37 \
            and ((words[0] >> 21) & 0x1F) == 1 and ((words[0] >> 16) & 0x1F) == 1:
        return -_sign_extend(words[0] & 0xFFFF, 16)
    return 0


def _live_at_exit(resource, frame_size: int) -> bool:
    if isinstance(resource, tuple):
        kind, number = resource
        if kind == "g":
            return number in (1, 2, 3, 4, 13) or number >= 14
        if kind == "f":
            return number == 1 or number >= 14
        if kind == "cr":
            return number in (2, 3, 4)
        if kind == "stack":
            return number >= frame_size
    return resource not in ("ca", "ctr", "lr")


def _resource_dead_after(
    words: list[int],
    start_index: int,
    resource,
    successors: list[list[int]],
    calls: list[bool],
    call_targets=None,
) -> bool:
    """True when *resource* is overwritten on every path from *start_index*
    before it can be observed (reads, calls, and function exits included)."""
    frame = _frame_size(words)
    if start_index >= len(words):
        return not _live_at_exit(resource, frame)
    pending = [start_index]
    seen: set[int] = set()
    while pending:
        index = pending.pop()
        if index in seen:
            continue
        seen.add(index)
        reads, writes = _word_effects(words[index])
        if _touches(resource, reads):
            return False
        if calls[index]:
            helper = _helper_call(
                call_targets.get(index * 4) if call_targets else None
            )
            if helper is None:
                if resource in _CALL_ARGUMENTS or _is_memory(resource):
                    return False
                if resource in _CALL_CLOBBERED:
                    continue
            else:
                kind, bank, first = helper
                affected = {(bank, n) for n in range(first, 32)}
                if resource == ("g", 11):
                    return False  # millicode reads the save-area pointer
                if kind == "save" and resource in affected:
                    return False  # the saved range is read
                if kind == "rest":
                    if _is_memory(resource):
                        return False  # the save area is read back
                    if resource in affected:
                        continue  # restored: overwritten
        if resource in writes and resource not in ("mem", "anymem", "fpscr"):
            continue
        if not successors[index]:
            if _live_at_exit(resource, frame):
                return False
            continue
        pending.extend(successors[index])
    return True


def check_permutation_dependences(region: bytes, order: list[int],
                                  exit_dead=None) -> None:
    """Fail unless the permutation preserves every def-use chain.

    Each read must see the same writer atom before and after reordering, and
    each resource's final writer must be unchanged — unless *exit_dead*
    certifies the resource is never observed after the region."""
    words = [_u32(region, off) for off in range(0, len(region), 4)]
    raw_effects = []
    stack_locations = set()
    for index, word in enumerate(words):
        try:
            reads, writes = _word_effects(word)
        except ValueError as error:
            raise ValueError(f"atom {index}: {error}") from None
        if ("g", 1) in writes:
            raise ValueError(f"atom {index}: permutation region redefines r1")
        raw_effects.append((reads, writes))
        for item in reads | writes:
            if isinstance(item, tuple) and item[0] == "stack":
                stack_locations.add(item)

    def expand(group):
        if "anymem" in group:
            group = (group - {"anymem"}) | {"mem"} | stack_locations
        return group

    effects = [(expand(reads), expand(writes)) for reads, writes in raw_effects]

    def trace(sequence):
        last: dict = {}
        chains: dict = {}
        for atom in sequence:
            reads, writes = effects[atom]
            for resource in reads:
                chains[(atom, resource)] = last.get(resource, "entry")
            for resource in writes:
                last[resource] = atom
        return chains, last

    baseline_chains, baseline_last = trace(range(len(words)))
    permuted_chains, permuted_last = trace(order)
    if baseline_chains != permuted_chains:
        broken = sorted(
            key for key in set(baseline_chains) | set(permuted_chains)
            if baseline_chains.get(key) != permuted_chains.get(key)
        )
        raise ValueError(
            f"instruction permutation breaks def-use chains: {broken[:4]}"
        )
    for resource in set(baseline_last) | set(permuted_last):
        if baseline_last.get(resource) == permuted_last.get(resource):
            continue
        if exit_dead is not None and exit_dead(resource):
            continue
        raise ValueError(
            f"instruction permutation changes the final write of {resource} "
            "and the resource is not provably dead at region exit"
        )


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def _relocation_sha256(relocations: list[tuple[int, int, int]]) -> str:
    payload = bytearray()
    for offset, info, addend in relocations:
        payload += struct.pack(">IIi", offset, info, addend)
    return _sha256(payload)


def _is_control_instruction(word: int) -> bool:
    """Return true for PPC branch/call/system/XL control instructions."""
    opcode = word >> 26
    return opcode in {16, 17, 18, 19}


def permute_instruction_atoms(
    current: bytes,
    order: list[int],
    relocations: list[tuple[int, int, int]],
    *,
    before_sha256: str,
    after_sha256: str,
    before_relocations_sha256: str,
    after_relocations_sha256: str,
    exit_dead=None,
) -> tuple[bytes, list[tuple[int, int, int]], int]:
    """Apply one explicit instruction-atom permutation, failing closed.

    ``order[destination]`` names the source instruction atom. Relocations use
    byte offsets relative to ``current`` and move with their source atom while
    retaining the original within-instruction byte offset, symbol, and addend.
    The permutation must preserve every def-use chain
    (check_permutation_dependences); ``exit_dead`` may certify a resource
    whose final region write moves as never observed after the region.
    """
    if _sha256(current) != before_sha256:
        raise ValueError("instruction permutation input hash changed")
    if len(current) % 4:
        raise ValueError("instruction permutation region is not word-aligned")

    count = len(current) // 4
    if len(order) != count or sorted(order) != list(range(count)):
        raise ValueError("instruction permutation is not a bijection")

    atoms = [current[offset:offset + 4] for offset in range(0, len(current), 4)]
    for atom in atoms:
        if _is_control_instruction(_u32(atom, 0)):
            raise ValueError("instruction permutation region contains a control op")

    check_permutation_dependences(current, order, exit_dead)

    if _relocation_sha256(relocations) != before_relocations_sha256:
        raise ValueError("instruction permutation relocation input hash changed")

    destination_by_source = {
        source: destination for destination, source in enumerate(order)
    }
    moved_relocations = []
    for offset, info, addend in relocations:
        if not 0 <= offset < len(current):
            raise ValueError("instruction permutation relocation is outside region")
        source = offset // 4
        within_atom = offset % 4
        destination = destination_by_source[source]
        moved_relocations.append((destination * 4 + within_atom, info, addend))
    moved_relocations.sort(key=lambda item: item[0])

    # The transform may change relocation offsets and ordering, never their
    # symbol/addend payload or their byte position within an instruction atom.
    before_payload = sorted(
        (offset % 4, info, addend) for offset, info, addend in relocations
    )
    after_payload = sorted(
        (offset % 4, info, addend)
        for offset, info, addend in moved_relocations
    )
    if before_payload != after_payload or len(moved_relocations) != len(relocations):
        raise ValueError("instruction permutation failed to preserve relocations")
    if _relocation_sha256(moved_relocations) != after_relocations_sha256:
        raise ValueError("instruction permutation relocation output hash changed")

    output = b"".join(atoms[source] for source in order)
    if _sha256(output) != after_sha256:
        raise ValueError("instruction permutation output hash changed")
    moved = sum(destination != source for destination, source in enumerate(order))
    return output, moved_relocations, moved


def copy_register_fields(current: bytes, target: bytes) -> tuple[bytes, int]:
    """Copy only genuine register operand fields from *target*.

    Each differing word is decoded so the permitted mask matches its form —
    immediates, rotate counts, XO bits, and CR selectors can never ride along
    in a register slot. All other instruction bits must already agree, and
    apply_patch additionally proves the whole change is a position-consistent
    recolor with verify_consistent_recolor.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("register-field functions must have equal aligned sizes")
    output = bytearray(current)
    changed = 0
    for offset in range(0, len(current), 4):
        word = _u32(current, offset)
        wanted = _u32(target, offset)
        difference = word ^ wanted
        if not difference:
            continue
        try:
            allowed = register_slot_mask(word)
        except ValueError as error:
            raise ValueError(f"+0x{offset:x}: {error}") from None
        if difference & ~allowed:
            raise ValueError(
                f"non-register instruction bits differ at +0x{offset:x}: "
                f"0x{difference:08x}"
            )
        recolored = (word & ~allowed) | (wanted & allowed)
        struct.pack_into(">I", output, offset, recolored)
        changed += 1
    if output != target:
        raise ValueError("register-field copy did not reproduce target bytes")
    return bytes(output), changed


def apply_patch(
    data: bytearray, patch: dict, target_data: bytes | None = None
) -> tuple[str, str, int]:
    sections = _sections(data)
    symbol = _find_symbol(data, sections, patch["function"])
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    end = start + symbol.size
    before = _sha256(data[start:end])
    if before != patch["before_sha256"]:
        raise ValueError(
            f"{symbol.name}: input hash {before} != expected {patch['before_sha256']}"
        )

    original_function = bytes(data[start:end])
    text_relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    relocated_offsets = set(text_relocations)
    call_targets = {
        offset: name for offset, (reloc_type, name) in text_relocations.items()
        if reloc_type == 10  # R_PPC_REL24
    }
    jumptable_offsets = _jumptable_targets(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    deferred_exit: list = []
    permutation_region = None

    changed = 0
    permutation = patch.get("instruction_permutation")
    if permutation:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        if target_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({target_symbol.size} != {symbol.size})"
            )

        relative_start = _parse_int(permutation["start"])
        relative_end = _parse_int(permutation["end"])
        if (relative_start % 4 or relative_end % 4
                or not 0 <= relative_start < relative_end <= symbol.size):
            raise ValueError(
                f"{symbol.name}: invalid instruction permutation range"
            )

        relocation_sections = [
            section for section in sections
            if section.section_type == SHT_RELA
            and section.info == symbol.section_index
        ]
        if len(relocation_sections) != 1:
            raise ValueError(
                f"{symbol.name}: expected one relocation section for text, "
                f"found {len(relocation_sections)}"
            )
        relocation_section = relocation_sections[0]
        entry_size = relocation_section.entry_size or 12
        if entry_size != 12 or relocation_section.size % entry_size:
            raise ValueError(f"{symbol.name}: unsupported relocation layout")

        records = []
        for offset in range(
            relocation_section.offset,
            relocation_section.offset + relocation_section.size,
            entry_size,
        ):
            records.append(struct.unpack_from(">IIi", data, offset))

        section_region_start = symbol.value + relative_start
        section_region_end = symbol.value + relative_end
        region_records = [
            (offset - section_region_start, info, addend)
            for offset, info, addend in records
            if section_region_start <= offset < section_region_end
        ]
        region = bytes(data[start + relative_start:start + relative_end])
        permutation_region = (relative_start, relative_end)

        def _defer_exit_dead(resource):
            # Checked against both function images after the whole patch is
            # applied, when the final downstream register fields are known.
            deferred_exit.append(resource)
            return True

        permuted, moved_records, moved = permute_instruction_atoms(
            region,
            [_parse_int(index) for index in permutation["order"]],
            region_records,
            before_sha256=permutation["before_sha256"],
            after_sha256=permutation["after_sha256"],
            before_relocations_sha256=permutation["before_relocations_sha256"],
            after_relocations_sha256=permutation["after_relocations_sha256"],
            exit_dead=_defer_exit_dead,
        )
        data[start + relative_start:start + relative_end] = permuted

        outside_records = [
            record for record in records
            if not section_region_start <= record[0] < section_region_end
        ]
        records = outside_records + [
            (section_region_start + offset, info, addend)
            for offset, info, addend in moved_records
        ]
        records.sort(key=lambda item: item[0])
        if len(records) * entry_size != relocation_section.size:
            raise ValueError(f"{symbol.name}: relocation count changed")
        for index, record in enumerate(records):
            struct.pack_into(
                ">IIi", data,
                relocation_section.offset + index * entry_size,
                *record,
            )
        changed += moved

    pre_register = bytes(data[start:end])
    register_stage = bool(
        patch.get("copy_register_fields") or patch.get("recolors")
        or patch.get("register_fields")
    )

    if patch.get("copy_register_fields"):
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        target_text = target_sections[target_symbol.section_index]
        target_start = target_text.offset + target_symbol.value
        target_end = target_start + target_symbol.size
        target_function = target_data[target_start:target_end]
        if _sha256(target_function) != patch["after_sha256"]:
            raise ValueError(f"{symbol.name}: target function hash changed")
        recolored, field_changes = copy_register_fields(
            bytes(data[start:end]), target_function
        )
        data[start:end] = recolored
        changed += field_changes

    for region in patch.get("recolors", []):
        relative_start = _parse_int(region["start"])
        relative_end = _parse_int(region["end"])
        if relative_start % 4 or relative_end % 4 or not (0 <= relative_start <= relative_end <= symbol.size):
            raise ValueError(f"{symbol.name}: invalid recolor range {region}")
        mapping = {int(old): int(new) for old, new in region["gpr"].items()}
        if any(not 0 <= register < 32 for pair in mapping.items() for register in pair):
            raise ValueError(f"{symbol.name}: GPR map out of range: {mapping}")
        if len(set(mapping.values())) != len(mapping):
            raise ValueError(f"{symbol.name}: GPR map is not injective: {mapping}")

        for relative in range(relative_start, relative_end, 4):
            offset = start + relative
            word = _u32(data, offset)
            recolored = recolor_instruction(word, mapping)
            if recolored != word:
                struct.pack_into(">I", data, offset, recolored)
                changed += 1

    seen_edits = set()
    for edit in patch.get("register_fields", []):
        relative = _parse_int(edit["at"])
        if relative % 4 or not 0 <= relative <= symbol.size - 4:
            raise ValueError(f"{symbol.name}: invalid register-field offset {edit}")
        if relative in seen_edits:
            raise ValueError(f"{symbol.name}: duplicate register-field edit {edit}")
        seen_edits.add(relative)
        offset = start + relative
        word = _u32(data, offset)
        recolored = word
        for raw_shift, raw_value in edit["set"].items():
            shift = int(raw_shift)
            value = _parse_int(raw_value)
            if shift not in {6, 11, 16, 21} or not 0 <= value < 32:
                raise ValueError(f"{symbol.name}: invalid register field {edit}")
            recolored = (recolored & ~(0x1F << shift)) | (value << shift)
        if recolored != word:
            struct.pack_into(">I", data, offset, recolored)
            changed += 1

    after = _sha256(data[start:end])
    if after != patch["after_sha256"]:
        raise ValueError(
            f"{symbol.name}: output hash {after} != expected {patch['after_sha256']}"
        )

    final_function = bytes(data[start:end])
    if register_stage:
        try:
            verify_consistent_recolor(
                pre_register, final_function,
                jumptable_targets=jumptable_offsets,
                relocated_offsets=relocated_offsets,
                call_targets=call_targets,
            )
        except ValueError as error:
            audit = patch.get("unproven_recolor_audit")
            if not audit:
                raise ValueError(
                    f"{symbol.name}: register-field patch is not a consistent "
                    f"recolor ({error}); model the residual as an "
                    "instruction_permutation, or record a manual equivalence "
                    "audit in \"unproven_recolor_audit\""
                ) from None
            print(
                f"WEBFRANK {symbol.name}: UNPROVEN recolor equivalence "
                f"accepted by audit ({error}) — {audit}"
            )
    if deferred_exit:
        words_raw = [
            _u32(original_function, off)
            for off in range(0, len(original_function), 4)
        ]
        words_final = [
            _u32(final_function, off)
            for off in range(0, len(final_function), 4)
        ]
        successors, call_flags = _successors(
            words_final,
            {offset // 4 for offset in relocated_offsets},
            {offset // 4 for offset in jumptable_offsets},
        )
        exit_index = permutation_region[1] // 4
        for resource in deferred_exit:
            for label, words in (("raw", words_raw), ("patched", words_final)):
                if not _resource_dead_after(
                    words, exit_index, resource, successors, call_flags,
                    call_targets,
                ):
                    raise ValueError(
                        f"{symbol.name}: permutation changes the final write "
                        f"of {resource}, which is live after the region in "
                        f"the {label} function"
                    )
    return before, after, changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("config", type=Path)
    parser.add_argument("unit", help="unit key in the config, e.g. game/g3d/sndvoice")
    parser.add_argument("--target", type=Path,
                        help="extracted target object for register-field rules")
    args = parser.parse_args()

    config = json.loads(args.config.read_text(encoding="utf-8"))
    patches = config.get("units", {}).get(args.unit)
    if not patches:
        raise KeyError(f"no webfrank configuration for {args.unit!r}")

    data = bytearray(args.input.read_bytes())
    target_data = args.target.read_bytes() if args.target else None
    total = 0
    for patch in patches:
        _, _, changed = apply_patch(data, patch, target_data)
        total += changed
        print(
            f"WEBFRANK {patch['function']}: "
            f"adjusted {changed} instruction atoms/fields"
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"WEBFRANK {args.unit}: {total} instruction atoms/fields total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
