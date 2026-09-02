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
import itertools
import json
import re
import struct
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
# ONE definition of where the stale marker lives — fndiff owns it because
# fndiff is the reader every other tool already goes through. Two spellings
# of a path is how a warning silently stops being emitted.
from fndiff import stale_marker_path  # noqa: E402


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
            # A data reference to the function's own ENTRY is a function
            # pointer, not a jumptable slot: collecting offset 0x0 gave
            # every bctr a spurious back-edge to the prologue, failing the
            # dependence audit 0x330 bytes before the first real difference
            # (claim.law.webfrank-cfg-entry-pseudotarget-false-negative).
            if fn_start < resolved < fn_end:
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

# The PPC EABI callee-saved GPRs.  r13 is the small-data base and is also
# preserved, but it is dedicated rather than allocatable and is deliberately
# excluded so this set means exactly "registers a normal callee restores".
_EABI_CALLEE_SAVED_GPRS = frozenset(range(14, 32))

# PowerPC text relocation types that patch ONLY an immediate/displacement
# field, leaving the opcode and every register field intact: ADDR16,
# ADDR16_LO, ADDR16_HI, ADDR16_HA, REL24, REL14.  Deliberately EXCLUDES
# R_PPC_ADDR32/R_PPC_REL32 (whole-word) and R_PPC_EMB_SDA21 (which rewrites
# the base register field to r2/r13), so a decoded write set may only be
# trusted for a relocated word whose type is in this set.
_IMMEDIATE_ONLY_RELOCATIONS = frozenset({3, 4, 5, 6, 10, 11})

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


def _fpr_move(word: int):
    """``(destination, source)`` when the word is a plain ``fmr fD,fB``.

    Rc set also writes CR1, so ``fmr.`` is not accepted: the copy closure
    below reasons only about the moved register value.
    """
    if (word >> 26) != 63 or ((word >> 1) & 0x3FF) != 72 or word & 1:
        return None
    return ((word >> 21) & 0x1F, (word >> 11) & 0x1F)


def _value_preserving_copy(word: int, index: int, relocated: set):
    """``(bank, destination, source)`` when the word provably moves one
    register's value into another, else ``None``.

    A RELOCATED word is never a copy.  ``addi rD,rA,0`` is the compiler's
    move form, but with an ADDR16_LO relocation the zero displacement is a
    link-time placeholder for a symbol's low half — reading it as a copy
    would be the exact unsoundness this whole mode exists to avoid.
    """
    if index in relocated:
        return None
    move = _fpr_move(word)
    if move is not None:
        return ("f", move[0], move[1])
    form = decode_copy_form(word)
    if form is not None and form[0] == "copy":
        return ("g", form[1], form[2])
    return None


def _compare_result_field(word: int):
    """The CR field number written by an ``fcmpu``/``fcmpo``, else ``None``.

    Only the floating compares are exchange-eligible: they are the measured
    population, and every accepted form must be one the tests exercise.
    """
    if (word >> 26) != 63 or ((word >> 1) & 0x3FF) not in (0, 32):
        return None
    return (word >> 23) & 7


def _compare_exchange_is_semantics_preserving(
    words: list[int],
    index: int,
    field: int,
    successors: list[list[int]],
    calls: list[bool],
    call_targets=None,
) -> bool:
    """True when exchanging the compare's two operands cannot be observed.

    Exchanging ``fcmpu``/``fcmpo`` operands swaps the FL and FG bits of the
    result field and leaves FE (equal) and FU (unordered) untouched, so the
    rewrite is equivalence-preserving exactly when every consumer of that CR
    field reads only FE or FU, and the field is dead at every exit.  Any
    other reader — an ordering branch, ``mcrf``, ``mfcr``, a CR-logical —
    fails closed, as does a field that escapes the function.
    """
    resource = ("cr", field)
    frame = _frame_size(words)
    pending = list(successors[index])
    seen: set[int] = set()
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        word = words[current]
        try:
            reads, writes = _word_effects(word)
        except ValueError:
            return False
        if resource in reads:
            opcode = word >> 26
            branch = opcode == 16 or (
                opcode == 19 and ((word >> 1) & 0x3FF) in (16, 528)
            )
            if not branch:
                return False           # mcrf/mfcr/CR-logical: whole field
            condition = (word >> 16) & 0x1F
            if condition >> 2 != field or condition & 3 not in (2, 3):
                return False           # LT/GT are exactly what the swap moves
        if calls[current]:
            helper = _helper_call(
                call_targets.get(current * 4) if call_targets else None
            )
            if helper is None and resource in _CALL_CLOBBERED:
                continue               # the call destroys the field
        if resource in writes:
            continue                   # redefined before any further use
        if not successors[current]:
            if _live_at_exit(resource, frame):
                return False
            continue
        pending.extend(successors[current])
    return True


def _relation_define(relation: set, bank: str, ours: int, target: int) -> set:
    """Retire every pair the write invalidates, then bind the new one.

    Our register now holds a new value, so every ``(ours, *)`` pair is stale;
    the target's register was overwritten too, so every ``(*, target)`` pair
    is stale.  This is the relational form of ``_map_define`` and it is
    strictly more aggressive than it: ``_map_define`` retires only the
    same-valued keys.
    """
    relation = {
        entry for entry in relation
        if not (entry[0] == bank and (entry[1] == ours or entry[2] == target))
    }
    relation.add((bank, ours, target))
    return relation


_VALUE_EQUALITY_STEP_LIMIT = 200000


def _value_equality_transfer(
    index: int,
    cur: int,
    tgt: int,
    relation: set,
    renaming: dict,
    our_copy,
    target_copy,
    substitutions: set,
    exchanges: set,
) -> tuple[set, dict]:
    opcode = cur >> 26
    if opcode in (46, 47):
        # lmw/stmw name a register RANGE, so no renaming is expressible and
        # the shipped identity requirement is kept exactly as it stands.
        renaming = _recolor_transfer(index, cur, tgt, renaming)
        if opcode == 46:
            for number in range((cur >> 21) & 0x1F, 32):
                relation = _relation_define(relation, "g", number, number)
        return relation, renaming
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
    compare_field = _compare_result_field(cur)
    pair = _commutative_shifts(cur)
    if pair is None and compare_field is not None:
        pair = (16, 11)
    remap: dict[int, int] = {}
    if pair is not None and all(shift in fields for shift in pair):
        first, second = pair
        bank, _, zero_1, cur_1, tgt_1 = fields[first]
        _, _, zero_2, cur_2, tgt_2 = fields[second]
        zero_involved = (zero_1 or zero_2) and 0 in (cur_1, cur_2, tgt_1, tgt_2)
        straight = ((bank, cur_1, tgt_1) in relation
                    and (bank, cur_2, tgt_2) in relation)
        if not straight and not zero_involved \
                and (bank, cur_1, tgt_2) in relation \
                and (bank, cur_2, tgt_1) in relation:
            remap = {first: tgt_2, second: tgt_1}
            if compare_field is not None:
                exchanges.add((index, bank, cur_1, cur_2, tgt_1, tgt_2))
    for shift, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        expected = remap.get(shift, tgt_r)
        if zero_none and (cur_r == 0 or expected == 0):
            if cur_r != expected:
                raise ValueError(
                    f"+0x{index * 4:x}: base register presence differs "
                    f"({bank}{cur_r} vs {bank}{expected})"
                )
            continue
        if role not in ("u", "b"):
            continue
        if (bank, cur_r, expected) not in relation:
            raise ValueError(
                f"+0x{index * 4:x}: use of {bank}{cur_r} is not value-equal "
                f"to {bank}{expected}"
            )
        if renaming.get((bank, cur_r)) != expected:
            substitutions.add((index, bank, cur_r, expected))
    for _, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        if zero_none and cur_r == 0:
            continue
        if role not in ("d", "b"):
            continue
        if our_copy is not None and target_copy is not None \
                and our_copy[1] == our_copy[2] and target_copy[1] == target_copy[2]:
            continue                   # `fmr fX,fX` on both sides: a no-op
        relation = _relation_define(relation, bank, cur_r, tgt_r)
        _map_define(renaming, (bank, cur_r), tgt_r)
        # THE COPY CLOSURE.  After `our: d_o <- s_o`, our d_o holds exactly
        # what our s_o holds, so every target register value-equal to s_o is
        # value-equal to d_o; symmetrically on the target side.  Closing over
        # the pairs that SURVIVED the define above is what lets one value
        # replicated across five registers in each stream be matched up in
        # any of the ways the two allocators chose.
        if our_copy is not None and our_copy[0] == bank \
                and our_copy[1] == cur_r and our_copy[2] != cur_r:
            source = our_copy[2]
            relation |= {
                (bank, cur_r, other) for kind, one, other in tuple(relation)
                if kind == bank and one == source
            }
        if target_copy is not None and target_copy[0] == bank \
                and target_copy[1] == tgt_r and target_copy[2] != tgt_r:
            source = target_copy[2]
            relation |= {
                (bank, one, tgt_r) for kind, one, other in tuple(relation)
                if kind == bank and other == source
            }
    return relation, renaming


def _declared_substitutions(declarations) -> set:
    declared = set()
    for entry in declarations or ():
        bank = entry["bank"]
        if bank not in ("g", "f"):
            raise ValueError(f"invalid value-equality bank {bank!r}")
        declared.add((_parse_int(entry["at"]) // 4, bank,
                      int(entry["ours"]), int(entry["target"])))
    return declared


def _declared_exchanges(declarations) -> set:
    declared = set()
    for entry in declarations or ():
        bank = entry["bank"]
        if bank not in ("g", "f"):
            raise ValueError(f"invalid comparison-exchange bank {bank!r}")
        ours = [int(value) for value in entry["ours"]]
        target = [int(value) for value in entry["target"]]
        if len(ours) != 2 or len(target) != 2:
            raise ValueError(
                f"comparison exchange at {entry['at']} needs two operands "
                "per side"
            )
        declared.add((_parse_int(entry["at"]) // 4, bank,
                      ours[0], ours[1], target[0], target[1]))
    return declared


def _format_substitution(site) -> str:
    index, bank, ours, target = site
    return f"+0x{index * 4:x} {bank}{ours}->{bank}{target}"


def _format_exchange(site) -> str:
    index, bank, cur_1, cur_2, tgt_1, tgt_2 = site
    return (f"+0x{index * 4:x} ({bank}{cur_1},{bank}{cur_2})"
            f"<->({bank}{tgt_1},{bank}{tgt_2})")


def verify_value_equality_recolor(
    current: bytes,
    target: bytes,
    *,
    jumptable_targets=(),
    relocated_offsets=(),
    target_relocated_offsets=(),
    call_targets=None,
    substitutions=(),
    compare_exchanges=(),
) -> None:
    """Prove *target* is *current* under a VALUE-equality correspondence.

    ``verify_consistent_recolor`` carries a partial function from our
    registers to target registers.  That is the right state for a pure
    allocator recolor and it cannot express a MULTI-SITE VALUE SPLIT: when
    both allocators replicate one value across several registers, the
    positional pairing of the definitions binds root-to-root while the later
    uses want root-to-copy, so the function-valued state fails at a use whose
    operands provably hold the same value.  Refining the function does not
    help — the required correspondence is genuinely many-to-many.

    This carries a RELATION instead, with the invariant

        (bank, a, b) in R  =>  our `a` and the target's `b` hold the same
                               value at this program point

    seeded with the identity at entry (both streams start from one machine
    state), narrowed at every definition, intersected at every join, and
    CLOSED OVER VALUE-PRESERVING COPIES in both streams.  Because every
    accepted use is backed by a member of R, the rewritten instruction reads
    operands holding the values our instruction read, which is what
    equivalence requires.

    The mode is strictly more permissive than the strict proof, so it is
    never the default: every use it accepts that the strict renaming would
    refuse must be DECLARED in *substitutions*, every comparison operand
    exchange in *compare_exchanges*, and an undeclared escape or a declared
    escape that never fires is an error.  A rule therefore cannot widen
    silently, and a source change that moves the residual fails the build.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("recolor verification needs equal word-aligned sizes")
    words_cur = [_u32(current, off) for off in range(0, len(current), 4)]
    words_tgt = [_u32(target, off) for off in range(0, len(target), 4)]
    count = len(words_cur)
    our_relocated = {off // 4 for off in relocated_offsets}
    target_relocated = {off // 4 for off in target_relocated_offsets}
    jumptable = set()
    for off in jumptable_targets:
        if off % 4 or not 0 <= off < len(current):
            raise ValueError(f"invalid jumptable target +0x{off:x}")
        jumptable.add(off // 4)
    successors, calls = _successors(words_cur, our_relocated, jumptable)

    our_copies = [
        _value_preserving_copy(word, index, our_relocated)
        for index, word in enumerate(words_cur)
    ]
    target_copies = [
        _value_preserving_copy(word, index, target_relocated)
        for index, word in enumerate(words_tgt)
    ]

    identity_relation = {
        (bank, number, number)
        for bank in ("g", "f") for number in range(32)
    }
    identity_renaming = {("g", n): n for n in range(32)}
    identity_renaming.update({("f", n): n for n in range(32)})
    incoming: dict[int, tuple[set, dict]] = {
        0: (identity_relation, identity_renaming)
    }
    used_substitutions: set = set()
    used_exchanges: set = set()
    pending = [0]
    steps = 0
    while pending:
        index = pending.pop()
        steps += 1
        if steps > _VALUE_EQUALITY_STEP_LIMIT:
            raise ValueError(
                "value-equality verification did not converge within "
                f"{_VALUE_EQUALITY_STEP_LIMIT} steps"
            )
        relation, renaming = incoming[index]
        relation, renaming = _value_equality_transfer(
            index, words_cur[index], words_tgt[index],
            set(relation), dict(renaming),
            our_copies[index], target_copies[index],
            used_substitutions, used_exchanges,
        )
        if calls[index]:
            helper = _helper_call(
                call_targets.get(index * 4) if call_targets else None
            )
            if helper is None:
                # A pair survives a call only when NEITHER side is clobbered.
                # The strict checker drops our-side volatile keys only, which
                # leaves a callee-saved-to-volatile correspondence standing
                # across a call; the relation drops both directions.
                volatile = frozenset(_CALL_VOLATILE)
                relation = {
                    entry for entry in relation
                    if (entry[0], entry[1]) not in volatile
                    and (entry[0], entry[2]) not in volatile
                }
                for key in _CALL_VOLATILE:
                    renaming.pop(key, None)
                for key in _CALL_RETURNS:
                    relation = _relation_define(relation, key[0], key[1],
                                                key[1])
                    _map_define(renaming, key, key[1])
            elif helper[0] == "rest":
                _, bank, first = helper
                for number in range(first, 32):
                    relation = _relation_define(relation, bank, number, number)
                    _map_define(renaming, (bank, number), number)
        for successor in successors[index]:
            known = incoming.get(successor)
            if known is None:
                merged = (set(relation), dict(renaming))
            else:
                merged = (
                    known[0] & relation,
                    {key: value for key, value in known[1].items()
                     if renaming.get(key) == value},
                )
            if known is None or merged != known:
                incoming[successor] = merged
                pending.append(successor)
    for index in range(count):
        if words_cur[index] != words_tgt[index] and index not in incoming:
            raise ValueError(
                f"+0x{index * 4:x}: differing word is unreachable from the "
                "function entry"
            )

    for site in sorted(used_exchanges):
        index, _bank, _c1, _c2, _t1, _t2 = site
        field = _compare_result_field(words_cur[index])
        if field is None or not _compare_exchange_is_semantics_preserving(
            words_cur, index, field, successors, calls, call_targets
        ):
            raise ValueError(
                f"+0x{index * 4:x}: the comparison's operands are exchanged "
                "but its CR field is read for ordering, or escapes the "
                "function, so the exchange is not equivalence-preserving"
            )

    declared_substitutions = _declared_substitutions(substitutions)
    declared_exchanges = _declared_exchanges(compare_exchanges)
    undeclared = sorted(used_substitutions - declared_substitutions)
    if undeclared:
        raise ValueError(
            "undeclared value-equality substitution(s): "
            + ", ".join(_format_substitution(site) for site in undeclared[:6])
        )
    undeclared_swaps = sorted(used_exchanges - declared_exchanges)
    if undeclared_swaps:
        raise ValueError(
            "undeclared comparison operand exchange(s): "
            + ", ".join(_format_exchange(site) for site in undeclared_swaps[:6])
        )
    stale = sorted(declared_substitutions - used_substitutions)
    if stale:
        raise ValueError(
            "declared value-equality substitution(s) never used: "
            + ", ".join(_format_substitution(site) for site in stale[:6])
        )
    stale_swaps = sorted(declared_exchanges - used_exchanges)
    if stale_swaps:
        raise ValueError(
            "declared comparison operand exchange(s) never used: "
            + ", ".join(_format_exchange(site) for site in stale_swaps[:6])
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
    # The M-form rotates (rlwimi/rlwinm/rlwnm) carry an Rc bit exactly like
    # the opcode-31 forms do, and it was NOT modelled here: `rlwinm.` read as
    # writing only rA, so a permutation could have moved one past a consumer
    # of CR0 with the dependence audit seeing nothing.  Found run 37 by the
    # live-zero class's own whitelist test, which expected the write-set check
    # to refuse a record-setting variant and got an acceptance instead.
    # Adding the write can only make every guard STRICTER.
    if opcode in (20, 21, 23) and word & 1:
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
        isinstance(resource, tuple) and resource[0] in ("stack", "global")
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
                                  exit_dead=None,
                                  memory_locations=None) -> None:
    """Fail unless the permutation preserves every def-use chain.

    Each read must see the same writer atom before and after reordering, and
    each resource's final writer must be unchanged — unless *exit_dead*
    certifies the resource is never observed after the region."""
    words = [_u32(region, off) for off in range(0, len(region), 4)]
    raw_effects = []
    stack_locations = set()
    named = dict(memory_locations or {})
    global_locations = set(named.values())
    for index, word in enumerate(words):
        try:
            reads, writes = _word_effects(word)
        except ValueError as error:
            raise ValueError(f"atom {index}: {error}") from None
        if ("g", 1) in writes:
            raise ValueError(f"atom {index}: permutation region redefines r1")
        location = named.get(index)
        if location is not None:
            reads = (reads - {"mem"}) | ({location} if "mem" in reads else set())
            writes = (writes - {"mem"}) | (
                {location} if "mem" in writes else set())
        elif named:
            # SOUNDNESS.  Once ANY access is split out of the single "mem"
            # resource, an access still spelled "mem" has an unknown address
            # and may alias every location that was split out.  Promoting it
            # to "anymem" is what keeps the refinement conservative; leaving
            # it as a plain "mem" would make it non-conflicting with exactly
            # the locations just separated from it.
            reads = (reads - {"mem"}) | ({"anymem"} if "mem" in reads else set())
            writes = (writes - {"mem"}) | (
                {"anymem"} if "mem" in writes else set())
        raw_effects.append((reads, writes))
        for item in reads | writes:
            if isinstance(item, tuple) and item[0] == "stack":
                stack_locations.add(item)

    def expand(group):
        if "anymem" in group:
            group = ((group - {"anymem"}) | {"mem"} | stack_locations
                     | global_locations)
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
        # Sort on repr, never on the resource itself: a chain key is
        # (atom, resource) and a resource is a str for "mem"/"lr"/"anymem"
        # but a tuple for ("g", N).  Two broken chains on ONE atom therefore
        # compared str against tuple and raised TypeError out of the ERROR
        # path — and since every caller catches ValueError only, one
        # candidate's perfectly correct refusal aborted the whole search.
        broken = sorted(
            (
                key for key in set(baseline_chains) | set(permuted_chains)
                if baseline_chains.get(key) != permuted_chains.get(key)
            ),
            key=lambda key: (key[0], repr(key[1])),
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


def _relocation_sha256(
    relocations: list[tuple[int, int, int]],
    symbols: dict[int, str] | None = None,
) -> str:
    """Hash a window's relocations by SYMBOL NAME, never by symbol index.

    The ELF ``r_info`` field packs the symbol-table INDEX, which is a
    TU-global quantity: adding or dropping any symbol anywhere in the
    translation unit renumbers it.  Hashing it therefore made every
    ``instruction_permutation`` rule in a TU abort the build after an
    unrelated edit elsewhere in that TU, with a message that reads like the
    edit's fault — measured on gauntworld::fn_8005FB48, where a _savefpr
    change moved indices 339->341 and 337->339 while both text hashes and
    every relocation's offset, type, addend and symbol NAME were unchanged.

    Only the relocation TYPE (the low byte of ``r_info``) and the symbol's
    name are identity here, so the hash tracks what the rules' own prose
    already reasons about.  Names are mandatory whenever a relocation is
    present: dropping the index without substituting the name would leave
    the hash blind to a re-symboling, which is a weaker guard, not a
    friendlier one.  The empty-list hash is unchanged, so the windows that
    carry no relocation need no migration.
    """
    payload = bytearray()
    for offset, info, addend in relocations:
        if symbols is None or offset not in symbols:
            raise ValueError(
                f"relocation hash needs the symbol name for the relocation "
                f"at +0x{offset:x}"
            )
        payload += struct.pack(">IIi", offset, info & 0xFF, addend)
        payload += symbols[offset].encode("utf-8") + b"\0"
    return _sha256(payload)


def _is_control_instruction(word: int) -> bool:
    """Return true for PPC branch/call/system/XL control instructions."""
    opcode = word >> 26
    return opcode in {16, 17, 18, 19}


# MWCC spells a compiler-generated constant-pool label `@NNNN`; the
# dtk-extracted retail object spells the same object `lbl_XXXXXXXX` (and a
# computed-branch table `jumptable_XXXXXXXX`).  The same datum therefore has
# two different names across the two objects and cannot be bound by name.
_OUR_POOL_LABEL = re.compile(r"^@\d+$")
_TARGET_POOL_LABEL = re.compile(r"^(?:lbl|jumptable)_[0-9A-Fa-f]+$")


def verify_relocation_binding(
    our_relocations: dict[int, tuple[int, str]],
    target_relocations: dict[int, tuple[int, str]],
    *,
    region_start: int = 0,
    region_end: int | None = None,
    words: list[int] | None = None,
) -> dict[str, str]:
    """Prove each relocation is bound to the instruction that should carry it.

    ``permute_instruction_atoms`` verifies its relocation work with a sorted
    multiset over ``(offset % 4, info, addend)``.  That proves CONSERVATION —
    no relocation was created, destroyed, retyped, re-symboled, or moved to a
    different byte position inside its instruction — but it drops the atom
    index, so it does NOT prove BINDING.  Any two relocated atoms sharing a
    within-instruction offset (every EMB_SDA21 pair, every ADDR16_LO pair)
    can be exchanged by a permutation and the multiset will not change.  The
    SDA load/store family encodes its displacement as zero and lets the
    relocation supply it, so two such words differ only in a register field
    and a matcher that pairs atoms by instruction WORD considers them freely
    interchangeable.  The result is text byte-identical to the target whose
    loads point at each other's globals — a real semantic defect that fndiff
    real, the opcode multiset, objdiff fuzzy and every other webfrank guard
    report as EXACT.

    This closes that by checking our post-permute relocations against the
    TARGET object's relocations, word by word.  Both sides are normalised to
    the WORD before comparing, because MWCC records an EMB_SDA21 entry at the
    instruction offset plus 2 while the extracted target records it at the
    instruction offset; that is a recording convention, not a mismatch.

    Non-pool symbols must match by exact name.  Compiler pool labels cannot
    (they are spelled differently in the two objects), so they are instead
    required to form a CONSISTENT one-to-one correspondence across the whole
    window, which is what forbids an exchange among them.  The established
    correspondence is returned so a caller can record it.

    claim.law.HV_permute-payload-check-does-not-bind-a-relocation-to-its-
    atom.20260901.v1
    """
    def _by_word(relocations, label):
        indexed: dict[int, tuple[int, str]] = {}
        for offset, (reloc_type, name) in relocations.items():
            if region_end is not None and not (
                region_start <= offset < region_end
            ):
                continue
            if offset < region_start:
                continue
            index = (offset - region_start) // 4
            if index in indexed:
                raise ValueError(
                    f"relocation binding: {label} carries two relocations on "
                    f"the word at +0x{index * 4:x}"
                )
            indexed[index] = (reloc_type, name)
        return indexed

    ours = _by_word(our_relocations, "our object")
    theirs = _by_word(target_relocations, "the target")

    only_ours = sorted(set(ours) - set(theirs))
    only_theirs = sorted(set(theirs) - set(ours))
    if only_theirs:
        # Our object dropped a relocation the target carries.  There is no
        # benign reading of that direction.
        raise ValueError(
            f"relocation binding: word +0x{only_theirs[0] * 4:x} is relocated "
            f"in the target but not in our object"
        )
    if only_ours:
        # The other direction IS benign and common: dtk resolves an address
        # when it extracts the retail object, so an ADDR16_HA/ADDR16_LO pair
        # that we still carry as relocations can appear in the target as
        # already-baked literals.  game/g3d/gcontrolpads::
        # G3DReadControlPadStates is a live instance and its rule documents
        # it.  Such a word has no target counterpart to bind against, so it
        # is exempt from the cross-object check -- but only when nothing in
        # the window could have been exchanged WITH it.
        #
        # An exchange can only survive into byte-correct text when the two
        # atoms are identical outside their register fields, since the
        # recolor stage that runs afterwards can rewrite nothing else.  So
        # the exemption is safe exactly when the unbindable atom is unique
        # in the window under that comparison.
        if words is None:
            raise ValueError(
                f"relocation binding: word +0x{only_ours[0] * 4:x} is "
                f"relocated in our object but not in the target, and no "
                f"region words were supplied to prove it is unexchangeable"
            )

        def _normalised(index):
            word = words[index]
            try:
                mask = register_slot_mask(word)
            except ValueError:
                # Unmodelled form: assume the widest register mask, which
                # makes MORE atoms look exchangeable and so fails closed.
                mask = REGISTER_FIELD_MASK
            return word & ~mask

        for index in only_ours:
            if not 0 <= index < len(words):
                raise ValueError(
                    f"relocation binding: word +0x{index * 4:x} is outside "
                    f"the supplied region words"
                )
            shape = _normalised(index)
            for other in sorted(ours):
                if other == index or not 0 <= other < len(words):
                    continue
                if _normalised(other) == shape:
                    raise ValueError(
                        f"relocation binding: word +0x{index * 4:x} is "
                        f"relocated in our object but not in the target, and "
                        f"word +0x{other * 4:x} is identical to it outside "
                        f"its register fields — the two could have been "
                        f"exchanged and neither can be bound"
                    )

    forward: dict[str, str] = {}
    backward: dict[str, str] = {}
    for index in sorted(set(ours) & set(theirs)):
        our_type, our_name = ours[index]
        their_type, their_name = theirs[index]
        if our_type != their_type:
            raise ValueError(
                f"relocation binding: word +0x{index * 4:x} has relocation "
                f"type {our_type} in our object and {their_type} in the target"
            )
        # An exact name match binds the relocation directly, whatever the
        # name looks like: our object often carries the target's own
        # `lbl_XXXXXXXX` placeholder spelling for an own-pool datum.  It is
        # still recorded in the correspondence, so a later differing pair in
        # the same window cannot contradict it.
        if our_name != their_name:
            our_pool = bool(_OUR_POOL_LABEL.match(our_name))
            their_pool = bool(_TARGET_POOL_LABEL.match(their_name))
            if not (our_pool and their_pool):
                raise ValueError(
                    f"relocation binding: word +0x{index * 4:x} carries "
                    f"symbol {our_name!r} in our object and {their_name!r} "
                    f"in the target — a relocation is bound to the wrong "
                    f"instruction"
                )
        if forward.setdefault(our_name, their_name) != their_name:
            raise ValueError(
                f"relocation binding: pool label {our_name!r} corresponds to "
                f"both {forward[our_name]!r} and {their_name!r} — the pool "
                f"correspondence is not one-to-one"
            )
        if backward.setdefault(their_name, our_name) != our_name:
            raise ValueError(
                f"relocation binding: target pool label {their_name!r} "
                f"corresponds to both {backward[their_name]!r} and "
                f"{our_name!r} — the pool correspondence is not one-to-one"
            )
    return forward


R_PPC_EMB_SDA21 = 109

# Byte width of each D-form load/store opcode's memory access.
_ACCESS_WIDTH = {
    32: 4, 33: 4, 34: 1, 35: 1, 36: 4, 37: 4, 38: 1, 39: 1,
    40: 2, 41: 2, 42: 2, 43: 2, 44: 2, 45: 2,
    48: 4, 49: 4, 50: 8, 51: 8, 52: 4, 53: 4, 54: 8, 55: 8,
}


def load_symbol_addresses(path) -> dict[str, tuple[str, int]]:
    """Parse the project's split map into ``name -> (section, address)``.

    This is the same file dtk splits by and the link is placed by, and the
    DOL checksum gate rests on it, so it is the project's own authority for
    where a global lives — not a new assumption introduced here.
    """
    addresses: dict[str, tuple[str, int]] = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        head, _, tail = line.partition("=")
        if not tail:
            continue
        body = tail.split("//")[0].strip().rstrip(";")
        section, _, value = body.partition(":")
        try:
            addresses[head.strip()] = (section.strip(), int(value, 16))
        except ValueError:
            continue
    return addresses


def resolve_memory_locations(
    function: bytes,
    declarations,
    relocations: dict[int, tuple[int, str]],
    symbol_addresses: dict[str, tuple[str, int]] | None,
) -> dict[int, tuple]:
    """Validate a rule's declared SDA memory locations and return
    ``function-relative instruction offset -> ("global", address)``.

    Every one of these must hold or the declaration is refused, because each
    is load-bearing for the non-aliasing conclusion:

      * the word is a D-form load/store whose base field is the SDA
        placeholder zero, so the linked effective address comes ENTIRELY
        from the relocation and not from a register value we cannot track;
      * the object carries an ``EMB_SDA21`` relocation on that word naming
        exactly the declared symbol;
      * the split map agrees with the declared section, address and access
        width;
      * and the declared access ranges are pairwise disjoint or identical.

    Distinct SYMBOL NAMES alone would not be a proof — two names can alias —
    so the addresses do the work and the names only bind the declaration to
    the object.
    """
    if not declarations:
        return {}
    if symbol_addresses is None:
        raise ValueError(
            "memory disambiguation needs the split map; pass --symbols "
            "(default: the symbols.txt beside the webfrank config)"
        )
    resolved: dict[int, tuple] = {}
    ranges: dict[int, tuple[int, int]] = {}
    for entry in declarations:
        at = _parse_int(entry["at"])
        if at % 4 or not 0 <= at < len(function):
            raise ValueError(f"invalid memory-disambiguation offset {entry}")
        word = _u32(function, at)
        opcode = word >> 26
        if opcode not in _ACCESS_WIDTH:
            raise ValueError(
                f"+0x{at:x}: memory disambiguation needs a D-form "
                f"load/store, found opcode {opcode}"
            )
        if ((word >> 16) & 0x1F) != 0:
            raise ValueError(
                f"+0x{at:x}: base register is not the SDA placeholder, so "
                f"the effective address is not determined by the relocation"
            )
        carried = [
            (kind, name) for offset, (kind, name) in relocations.items()
            if offset // 4 == at // 4
        ]
        if len(carried) != 1 or carried[0][0] != R_PPC_EMB_SDA21:
            raise ValueError(
                f"+0x{at:x}: expected exactly one EMB_SDA21 relocation, "
                f"found {carried}"
            )
        if carried[0][1] != entry["symbol"]:
            raise ValueError(
                f"+0x{at:x}: declared symbol {entry['symbol']!r} but the "
                f"object relocates against {carried[0][1]!r}"
            )
        known = symbol_addresses.get(entry["symbol"])
        if known is None:
            raise ValueError(
                f"+0x{at:x}: {entry['symbol']!r} is not in the split map, so "
                f"its address cannot be proved"
            )
        section, address = known
        if section != entry["section"] or address != _parse_int(
            entry["address"]
        ):
            raise ValueError(
                f"+0x{at:x}: declared {entry['section']}:{entry['address']} "
                f"but the split map has {section}:0x{address:x}"
            )
        width = _ACCESS_WIDTH[opcode]
        if int(entry["width"]) != width:
            raise ValueError(
                f"+0x{at:x}: declared width {entry['width']} but the opcode "
                f"accesses {width} bytes"
            )
        resolved[at] = ("global", address)
        ranges[at] = (address, address + width)
    items = sorted(ranges.items())
    for (first_at, first), (second_at, second) in itertools.combinations(
        items, 2
    ):
        if first == second:
            continue
        if first[0] < second[1] and second[0] < first[1]:
            raise ValueError(
                f"+0x{first_at:x} and +0x{second_at:x} access overlapping "
                f"ranges [0x{first[0]:x},0x{first[1]:x}) and "
                f"[0x{second[0]:x},0x{second[1]:x}); they may alias"
            )
    return resolved


def permutation_windows(
    permutation, function_size: int
) -> tuple[list[dict], list[tuple[int, int]]]:
    """Normalise a rule's ``instruction_permutation`` to a window list.

    A single dict is the original one-window form and keeps working
    unchanged, so no shipped rule needs rewriting.  A list expresses a
    function whose displaced words fall in two or more separated windows —
    the ordinary case rather than an edge case, because
    ``permute_instruction_atoms`` refuses any region containing a control
    instruction and MWCC's schedule differences cluster around basic-block
    boundaries (preheaders, compare/branch pairs, call sequences).

    Widening one region to swallow the intervening code is NOT the
    alternative: that region would contain control ops and be refused, and
    widening it to dodge them is exactly the unsound move the control-op
    refusal exists to prevent.

    Windows must be word-aligned, inside the function, non-empty, and
    pairwise DISJOINT in strictly ascending order.  Disjointness is the one
    genuinely new obligation the list form carries: overlapping windows
    would make the per-window before-hashes ill-defined.  Ascending order
    makes the applied order the written order.

    claim.law.HV_single-permutation-region-is-the-binding-schema-limit
    .20260901.v1
    """
    windows = permutation if isinstance(permutation, list) else [permutation]
    if not windows:
        raise ValueError("empty instruction permutation list")

    ranges: list[tuple[int, int]] = []
    for window in windows:
        relative_start = _parse_int(window["start"])
        relative_end = _parse_int(window["end"])
        if (relative_start % 4 or relative_end % 4
                or not 0 <= relative_start < relative_end <= function_size):
            raise ValueError(
                f"invalid instruction permutation range "
                f"+0x{relative_start:x}..+0x{relative_end:x}"
            )
        ranges.append((relative_start, relative_end))

    for (previous_start, previous_end), (next_start, _next_end) in zip(
        ranges, ranges[1:]
    ):
        if next_start < previous_end:
            raise ValueError(
                f"instruction permutation windows must be disjoint and "
                f"ascending (+0x{previous_start:x}..+0x{previous_end:x} then "
                f"+0x{next_start:x})"
            )
    return list(windows), ranges


def unpermute_target_windows(
    target: bytes, windows: list[dict], ranges: list[tuple[int, int]]
) -> bytes:
    """Build the RECOLOR TARGET for a post-recolor permutation.

    ``instruction_permutation`` runs before the recolor and is therefore
    audited in OUR colouring, which is the right and only sound order for it
    (claim.law.webfrank-permutation-is-audited-in-our-colouring).  But
    claim.law.C1_permute-recolor-composition-needs-a-permutation-legal-in-
    our-colouring.20260901.v1 identified a class that order can never reach:
    a displacement CAUSED BY the recolor, where the very register assignment
    that produced the reorder is what makes the reorder illegal before the
    registers are fixed.  C1 calls that class structurally unreachable, and
    it is — for a permutation that runs FIRST.

    It is reachable for one that runs LAST.  MEASURED on
    game/world/camera::camera_mode_level +0x8c0, where ours is
    ``addi r29,r4,200 ; li r4,0`` and the target is
    ``li r4,0 ; addi r28,r28,200``: swapping in our colouring breaks a
    def-use chain (our ``addi`` READS r4 and the ``li`` WRITES it, a WAR
    hazard that exists only because our build colours both webs r4), while
    the same swap in the target's colouring is between two independent
    words and ``check_permutation_dependences`` accepts it in its strictest
    form.

    So the pipeline gains a final stage, and this function supplies what it
    needs: the image the recolor should aim at.  Applying the permutation's
    INVERSE to the target yields an intermediate that is a pure renaming of
    our post-form stream — which lets the unmodified recolor stage and
    ``verify_consistent_recolor`` prove that link exactly as they always do
    — and the permutation then carries the intermediate to the target under
    ``check_permutation_dependences``.  Two existing proofs again, composed
    in the one order that makes both of them true.
    """
    output = bytearray(target)
    for window, (relative_start, relative_end) in zip(windows, ranges):
        count = (relative_end - relative_start) // 4
        order = [_parse_int(index) for index in window["order"]]
        if len(order) != count or sorted(order) != list(range(count)):
            raise ValueError(
                f"post-recolor permutation +0x{relative_start:x}.."
                f"+0x{relative_end:x} is not a bijection"
            )
        # permute_instruction_atoms produces output[dest] = input[order[dest]],
        # so the input this stage must be handed is target[dest] placed back
        # at order[dest].
        for destination, source in enumerate(order):
            word = _u32(target, relative_start + destination * 4)
            struct.pack_into(
                ">I", output, relative_start + source * 4, word
            )
    return bytes(output)


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
    our_symbols: dict[int, str] | None = None,
    target_relocations: dict[int, tuple[int, str]] | None = None,
    memory_locations: dict[int, tuple] | None = None,
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

    check_permutation_dependences(current, order, exit_dead, memory_locations)

    if _relocation_sha256(relocations,
                          our_symbols) != before_relocations_sha256:
        raise ValueError("instruction permutation relocation input hash changed")

    destination_by_source = {
        source: destination for destination, source in enumerate(order)
    }
    moved_relocations = []
    moved_named: dict[int, tuple[int, str]] = {}
    moved_symbols: dict[int, str] = {}
    for offset, info, addend in relocations:
        if not 0 <= offset < len(current):
            raise ValueError("instruction permutation relocation is outside region")
        source = offset // 4
        within_atom = offset % 4
        destination = destination_by_source[source]
        moved_offset = destination * 4 + within_atom
        moved_relocations.append((moved_offset, info, addend))
        if our_symbols is None or offset not in our_symbols:
            raise ValueError(
                f"instruction permutation has no symbol for the "
                f"relocation at +0x{offset:x}"
            )
        moved_named[moved_offset] = (info & 0xFF, our_symbols[offset])
        moved_symbols[moved_offset] = our_symbols[offset]
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
    if _relocation_sha256(moved_relocations,
                          moved_symbols) != after_relocations_sha256:
        raise ValueError("instruction permutation relocation output hash changed")

    # The payload check above proves conservation, never binding: it drops
    # the atom index, so any two relocated atoms sharing a within-instruction
    # offset can be exchanged without disturbing it.  Bind each relocation to
    # the instruction the TARGET carries it on.
    if target_relocations is not None:
        if our_symbols is None:
            raise ValueError(
                "instruction permutation relocation binding needs our symbols"
            )
        permuted_words = [
            _u32(current, order[index] * 4) for index in range(count)
        ]
        verify_relocation_binding(
            moved_named, target_relocations,
            region_start=0, region_end=len(current),
            words=permuted_words,
        )

    output = b"".join(atoms[source] for source in order)
    if _sha256(output) != after_sha256:
        raise ValueError("instruction permutation output hash changed")
    moved = sum(destination != source for destination, source in enumerate(order))
    return output, moved_relocations, moved


COPY_FORM_OPCODES = (14, 24, 31)


def decode_copy_form(word: int):
    """Classify a word as a register-to-register COPY or a constant LOAD.

    Returns ``("copy", rD, rS)`` when the word provably sets rD to the
    current contents of rS, ``("li", rD, K)`` when it provably sets rD to
    the literal constant K, and ``None`` for everything else.

    The only two copy encodings accepted are MWCC's two move forms:

      * ``or rD,rS,rS`` (``mr rD,rS``) with Rc clear — Rc set also writes
        CR0, which no ``addi`` does, so ``mr.`` is never a copy here.
      * ``addi rD,rS,0`` with **rS != 0**.  This is the whole reason the
        classification exists: ``addi`` treats a zero rA field as the
        literal value zero rather than as GPR 0, so ``addi rD,0,0`` is
        ``li rD,0`` — a constant load, not a copy of r0.

    ``ori rD,rS,0`` is a third valid copy form but is deliberately NOT
    accepted in this version: it is absent from the measured population,
    and every accepted form must be one the tests exercise.
    """
    opcode = word >> 26
    if opcode == 14:  # addi rD,rA,SIMM
        destination = (word >> 21) & 0x1F
        source = (word >> 16) & 0x1F
        immediate = _sign_extend(word & 0xFFFF, 16)
        if source == 0:
            return ("li", destination, immediate)
        if immediate == 0:
            return ("copy", destination, source)
        return None
    if opcode == 31 and ((word >> 1) & 0x3FF) == 444:  # or rA,rS,rB
        source = (word >> 21) & 0x1F
        destination = (word >> 16) & 0x1F
        second = (word >> 11) & 0x1F
        if second == source and not word & 1:
            return ("copy", destination, source)
        return None
    return None


def _entry_indexes(successors: list[list[int]]) -> set[int]:
    """Indexes reachable by anything other than plain fallthrough."""
    entries = {0}
    for index, targets in enumerate(successors):
        for target in targets:
            if target != index + 1:
                entries.add(target)
    return entries


def _prove_call_preserves_source(
    index: int, word: int, source: int, call_targets: dict | None,
) -> None:
    """Fail closed unless a *direct, named, non-millicode* ``bl`` at *index*
    provably leaves GPR *source* intact.

    This is the ONLY control form the backward scan may cross, and it is
    gated on four separately machine-checked facts.  Three of them are
    decided here from the bytes and the relocation table; the fourth is a
    stated ABI axiom, which is why this path carries its own proof label
    (``dominating_def_across_calls``) and can never be taken silently by a
    rule that merely asked for ``dominating_def``.

    1. *source* is one of r14-r31.  The PPC EABI makes exactly these GPRs
       callee-saved; a volatile source may be clobbered by any call and is
       refused outright.
    2. The word is ``bl`` — opcode 18 with LK set and AA clear.  ``bctrl``
       and ``blrl`` call an address this scan cannot resolve to a symbol,
       ``bla`` is absolute, and every conditional or non-linking branch is
       ordinary control flow rather than a call; all are refused, so the
       relaxation is strictly about calls and never about branches.
    3. The call carries a REL24 relocation naming its callee.  Without a
       name the callee cannot be screened at all, so an unrelocated (and
       therefore already-bound, unnameable) call is refused.
    4. The named callee is NOT MWCC's ``_savegpr_N``/``_restgpr_N``/
       ``_savefpr_N``/``_restfpr_N`` millicode.  This is the trap the check
       exists for: that millicode deliberately writes the callee-saved
       range it is named for — ``_restgpr_28`` really does redefine
       r28-r31 — so it is the one class of direct call that breaks the
       preservation contract.  ``_helper_call`` already models exactly this
       family for the recolor stage and is reused here rather than
       re-derived.

    THE RESIDUAL AXIOM, stated so it is auditable: for a direct call to a
    named non-millicode symbol, this function assumes the callee honours
    the PPC EABI and restores r14-r31.  That is an assumption about code
    outside this object, not a fact derived from the bytes in front of us,
    and it is the entire reason the relaxation needs an opt-in label.
    """
    offset = index * 4
    if source not in _EABI_CALLEE_SAVED_GPRS:
        raise ValueError(
            f"+0x{offset:x}: r{source} is volatile, so an interposed call "
            f"may clobber it; only r14-r31 may be carried across a call"
        )
    if (word >> 26) != 18 or (word & 3) != 1:
        raise ValueError(
            f"+0x{offset:x}: only a direct `bl` may be crossed, and "
            f"0x{word:08x} is not one"
        )
    name = (call_targets or {}).get(offset)
    if not name:
        raise ValueError(
            f"+0x{offset:x}: `bl` carries no REL24 relocation naming its "
            f"callee, so the callee cannot be screened"
        )
    if _helper_call(name) is not None:
        raise ValueError(
            f"+0x{offset:x}: `bl {name}` is MWCC register millicode, which "
            f"writes the callee-saved range it is named for"
        )


def prove_constant_source(
    words: list[int], site: int, source: int, constant: int,
    entry_indexes: set[int], relocated_indexes: set[int],
    *, across_calls: bool = False, call_targets: dict | None = None,
    relocation_types: dict | None = None,
) -> int:
    """Prove GPR *source* holds *constant* on entry to instruction *site*.

    The proof is a straight-line backward scan and is deliberately the
    weakest one that is obviously sound: walking back from the site it
    fails closed on the first index that can be entered from anywhere but
    its own fallthrough edge, on any control instruction, on a relocated
    word it cannot account for, and on any write to *source* that is not
    the exact ``li source,constant`` being looked for.  Reaching such a
    definition with no interposed write and no way into the span from
    elsewhere makes *source* equal to *constant* on every path that
    reaches the site.

    RELOCATED WORDS.  A relocated word may never be the definition: an
    unresolved address half is not the literal the proof needs, so that
    case is refused outright and unconditionally.  An *interposed*
    relocated word that does not write *source* cannot change *source*,
    but only once we know the relocation cannot rewrite the register
    fields the write set was decoded from — so its type must be in
    ``_IMMEDIATE_ONLY_RELOCATIONS`` and is checked BEFORE the word is
    decoded.  This distinction only matters once a rule composes stages:
    a permutation can move a relocated word into a proof span that did
    not contain one in the raw stream.

    With *across_calls* set the single exception described in
    ``_prove_call_preserves_source`` applies: a direct, named,
    non-millicode ``bl`` may be stepped over when *source* is callee-saved.
    Everything else about the scan is unchanged — in particular the
    entry-index check still runs at every step, so the span must still be
    straight-line, and any OTHER control form still fails closed.

    Returns the index of the dominating definition.
    """
    index = site - 1
    while index >= 0:
        if index + 1 in entry_indexes:
            raise ValueError(
                f"+0x{(index + 1) * 4:x} is a branch target: control can "
                f"enter between the definition and the rewrite site"
            )
        word = words[index]
        if _is_control_instruction(word):
            if not across_calls:
                raise ValueError(
                    f"+0x{index * 4:x}: control instruction inside the "
                    f"proof span"
                )
            # A crossable call is validated as a CALL, so its own REL24
            # relocation is the evidence rather than a disqualifier; the
            # relocated-word check below would otherwise reject every
            # named call by construction.
            _prove_call_preserves_source(index, word, source, call_targets)
            index -= 1
            continue
        relocated = index in relocated_indexes
        if relocated:
            # The write set below is decoded from opcode and register
            # fields, so it is only trustworthy when the relocation cannot
            # touch those bits.  R_PPC_EMB_SDA21 is the counterexample that
            # makes this a real check rather than a formality: it rewrites
            # the base REGISTER field to r2/r13.  Validate the type BEFORE
            # believing anything decoded from the word.
            kind = (relocation_types or {}).get(index)
            if kind not in _IMMEDIATE_ONLY_RELOCATIONS:
                raise ValueError(
                    f"+0x{index * 4:x}: relocated word inside the proof span "
                    f"carries relocation type {kind}, which may rewrite bits "
                    f"outside the immediate field"
                )
        _reads, writes = _word_effects(word)
        if ("g", source) in writes:
            if relocated:
                raise ValueError(
                    f"+0x{index * 4:x}: r{source} is defined by a relocated "
                    f"word, whose immediate is an unresolved address rather "
                    f"than the literal this proof requires"
                )
            form = decode_copy_form(word)
            if form is None or form[0] != "li" or form[1] != source:
                raise ValueError(
                    f"+0x{index * 4:x}: r{source} is redefined by a "
                    f"non-constant instruction"
                )
            if form[2] != constant:
                raise ValueError(
                    f"+0x{index * 4:x}: r{source} is defined as {form[2]}, "
                    f"not the required constant {constant}"
                )
            return index
        index -= 1
    raise ValueError(
        f"no dominating `li r{source},{constant}` reaches +0x{site * 4:x}"
    )


# R_PPC_EMB_SDA21 is the one text relocation that rewrites a REGISTER field
# rather than an immediate: it replaces the base register (shift 16) with the
# small-data base the linker picks.  Those are the only values it can write,
# and it touches no other field.
_SDA21_RELOCATION = 109
_SDA21_BASE_REGISTERS = frozenset({0, 2, 13})


def _relocation_cannot_write(kind: int | None, source: int) -> bool:
    """True when a relocation of *kind* provably cannot make its word write
    GPR *source*, so the word's decoded effects may be trusted.

    The blunt reading — "a relocation may rewrite register fields, so distrust
    the whole decode" — is sound but far too coarse to be useful: it stops the
    dataflow at every small-data access, and MWCC emits those constantly.  On
    game/world/camera::camera_mode_level it refused four `lfs` words between
    the ``li r30,0`` and the rewrite site, none of which can touch a GPR at
    all.

    So the question is answered per relocation type instead of by blanket
    suspicion, and it fails closed on every type not named here:

    * ``_IMMEDIATE_ONLY_RELOCATIONS`` patch an immediate or displacement and
      leave every register field alone, so the decode stands as-is.
    * ``R_PPC_EMB_SDA21`` rewrites exactly one field, the base register, and
      can only ever write r0, r2 or r13 into it.  It therefore cannot
      introduce a write of any OTHER register.  (If the pre-link decode
      already says the word writes *source* — an update-form base — the
      caller still resets the fact, so the conservative direction is kept.)
    * Everything else, R_PPC_ADDR32 and R_PPC_REL32 above all, replaces the
      whole word.  Nothing decoded from it means anything and it is refused.
    """
    if kind in _IMMEDIATE_ONLY_RELOCATIONS:
        return True
    if kind == _SDA21_RELOCATION:
        return source not in _SDA21_BASE_REGISTERS
    return False


def prove_constant_dataflow(
    words: list[int], site: int, source: int, constant: int,
    successors: list[list[int]], calls: list[bool],
    relocated_indexes: set[int],
    *, relocation_types: dict | None = None, call_targets: dict | None = None,
) -> None:
    """Prove GPR *source* holds *constant* on entry to *site* on EVERY path.

    ``prove_constant_source`` is a straight-line backward scan: it fails on
    the first branch target, control instruction, or interposed call it
    meets, so it can only reach a definition inside the site's own basic
    block.  That is the right proof for a local rematerialisation, and it is
    all the pure-form class ever needed.  The COMBINED form+recolor class
    does need more, because its defining shape is a value the allocator
    parked in a callee-saved register hundreds of instructions and many
    blocks earlier — MEASURED on game/world/camera::camera_mode_level, whose
    ``li r30,0`` sits at +0x774 and whose rewrite site is +0x8b0, with a
    loop, several branches and a call in between.

    So this is a whole-CFG forward dataflow over a two-point lattice
    (``True`` = provably *constant*, ``False`` = unknown) with intersection
    at merges, seeded unknown at the function entry.  It is strictly
    stronger than the backward scan and strictly sound: a value survives to
    the site only if EVERY path writes it with the exact
    ``li source,constant`` and nothing afterwards disturbs it.

    Every way of losing the value fails closed:

    * A write of *source* that is not the exact ``li source,constant``
      resets the fact to unknown.
    * ``_word_effects`` raises on any instruction form it does not model, so
      an unmodelled write can never be silently skipped.
    * A word carrying a relocation outside ``_IMMEDIATE_ONLY_RELOCATIONS``
      resets the fact unconditionally: its write set is decoded from
      register fields the relocation may itself rewrite (R_PPC_EMB_SDA21
      rewrites the base register to r2/r13), so nothing decoded from it may
      be trusted.  A relocated word can never BE the definition either — an
      unresolved address half is not the literal this proof requires.
    * Calls reset the fact unless *source* is one of r14-r31 AND the call is
      a direct, named, non-millicode ``bl``.  Indirect calls (``bctrl``,
      ``blrl``) name no callee that could be screened for millicode, so they
      reset it even for a callee-saved register; ``_restgpr_N``/``_restfpr_N``
      deliberately rewrite the saved range they are named for and reset it
      too.  The surviving case rests on the same stated ABI axiom as
      ``_prove_call_preserves_source`` — that a named non-millicode callee
      restores r14-r31 — which is why the proof modes using this prover are
      named separately and must be asked for by name.
    """
    if not 0 <= site < len(words):
        raise ValueError(f"constant dataflow site +0x{site * 4:x} is outside "
                         f"the function")
    if source == 0:
        raise ValueError(
            "constant dataflow refuses GPR r0 as a source: `addi rD,r0,K` is "
            "`li rD,K` and never reads r0, so the two encodings diverge there"
        )

    holds: dict[int, bool] = {0: False}
    pending = [0]
    while pending:
        index = pending.pop()
        state = holds[index]
        word = words[index]
        if index in relocated_indexes and not _relocation_cannot_write(
            (relocation_types or {}).get(index), source
        ):
            # The write set below is decoded from register fields this
            # relocation may rewrite; nothing decoded from the word can be
            # trusted, so the fact cannot survive it.
            state = False
        else:
            _reads, writes = _word_effects(word)
            if ("g", source) in writes:
                form = decode_copy_form(word)
                state = bool(
                    form is not None and form[0] == "li"
                    and form[1] == source and form[2] == constant
                    and index not in relocated_indexes
                )
            if calls[index]:
                if source not in _EABI_CALLEE_SAVED_GPRS:
                    state = False
                else:
                    name = (call_targets or {}).get(index * 4)
                    helper = _helper_call(name)
                    if not name or helper is not None or (word >> 26) != 18 \
                            or (word & 3) != 1:
                        # Indirect, unnameable, or register millicode: the
                        # callee-saved guarantee cannot be established.
                        state = False
        for successor in successors[index]:
            known = holds.get(successor)
            merged = state if known is None else (known and state)
            if known is None or merged != known:
                holds[successor] = merged
                pending.append(successor)

    if site not in holds:
        raise ValueError(
            f"+0x{site * 4:x} is not reachable from the function entry, so "
            f"no dataflow fact reaches it"
        )
    if not holds[site]:
        raise ValueError(
            f"r{source} is not provably {constant} on every path reaching "
            f"+0x{site * 4:x}"
        )


# The combined form+recolor proof modes.  They are named separately from the
# pure-form modes so that no existing rule can drift onto them and so a rule
# states, in its own text, that it is rewriting a word whose registers ALSO
# change.  claim.law.WF_inverse-copy-form-is-served-and-the-payoff-inversion-
# recurs-one-level-down.20260901.v1 measured this population at 91 sites
# (44 forward + 47 inverse) against 7 for the two pure-form arrows together.
_COMBINED_PROOFS = frozenset({
    "unconditional_recolor",
    "constant_dataflow_recolor",
    "constant_dataflow_inverse_recolor",
})


def encode_copy_like(target_word: int, destination: int, source: int) -> int:
    """Re-encode the TARGET's copy/constant word in OUR register colouring.

    This is the heart of the combined stage and the reason it needs no new
    trust.  A combined site cannot be closed by copying the target word —
    its registers are the target's, and dropping them into our stream would
    be a recolor that nothing has proved.  So instead the target's ENCODING
    is rebuilt around OUR registers, which leaves a word that differs from
    the target in register fields ONLY.  The existing, unmodified recolor
    stage then finishes it and, critically, PROVES the renaming while doing
    so.  The form change and the recolor are thereby discharged by two
    separate proofs that already exist, rather than by one new one.

    The caller must have established the value equivalence first; this
    function is purely the encoder.  It never invents an encoding: the shape
    always comes from the target word, so the result is guaranteed to reach
    the target under a register-field copy.
    """
    opcode = target_word >> 26
    if opcode == 14:  # addi: either `addi rD,rS,0` or `li rD,K`
        if (target_word >> 16) & 0x1F == 0:  # li rD,K — no source register
            return (14 << 26) | (destination << 21) | (target_word & 0xFFFF)
        if source == 0:
            raise ValueError(
                "cannot re-encode an `addi` copy with our source r0: "
                "`addi rD,r0,0` is `li rD,0`, not a copy of r0"
            )
        return (14 << 26) | (destination << 21) | (source << 16)
    if opcode == 31 and ((target_word >> 1) & 0x3FF) == 444:  # or rA,rS,rB
        if target_word & 1:
            raise ValueError("cannot re-encode a record-setting `mr.`")
        return ((31 << 26) | (source << 21) | (destination << 16)
                | (source << 11) | (444 << 1))
    raise ValueError(
        f"target word 0x{target_word:08x} is not a re-encodable copy form"
    )


def equivalent_copy_form(
    current: bytes, target: bytes, edits: list,
    relocated_offsets: set[int], target_relocated_offsets: set[int],
    jumptable_offsets: set[int], call_targets: dict | None = None,
    relocation_types: dict | None = None,
) -> tuple[bytes, int]:
    """Rewrite named words to the target's equivalent copy encoding.

    This is neither a renaming nor a permutation, so it gets its own proof
    obligation rather than reusing the recolor guards.  Two forms are
    accepted, and nothing else:

    ``unconditional`` — our word and the target word decode as the *same*
    copy ``rD <- rS`` with ``rS != 0``.  ``mr rD,rS`` and ``addi rD,rS,0``
    then have identical architectural effect, so no dataflow is needed.

    ``dominating_def`` — our word is ``li rD,K`` and the target word is a
    copy ``rD <- rS`` with ``rS != 0``, and a backward scan proves rS holds
    K at that point.  This is a value-equivalence obligation, discharged by
    ``prove_constant_source``.

    ``dominating_def_across_calls`` — the same obligation, discharged by
    the same scan, but permitting the scan to step over a direct, named,
    non-millicode ``bl`` when rS is callee-saved.  It is a SEPARATE label
    rather than a widening of ``dominating_def`` because it rests on an
    ABI axiom about code outside this object; see
    ``_prove_call_preserves_source`` for the four checks and the axiom.
    A rule must ask for it by name, so no existing rule can drift onto the
    weaker proof.

    ``dominating_def_inverse`` / ``dominating_def_inverse_across_calls`` —
    THE INVERSE DIRECTION.  The three modes above all require the TARGET
    word to be a register copy; this pair covers the opposite arrow, where
    OURS is the copy ``rD <- rS`` and the target is ``li rD,K``.  The
    obligation is the mirror image and is discharged by the same backward
    scan over the same (our) stream: our rS must hold K at the site, since
    it is our object that will execute the rewritten word.  A source of r0
    is refused outright — ``mr rD,r0`` reads GPR 0 while ``addi rD,r0,K``
    never does, which is the encoding asymmetry the whole class rests on.

    Every edit additionally requires that neither object carries a
    relocation on the rewritten word, and each rewritten word must equal
    the target word exactly.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("copy-form functions must have equal aligned sizes")
    words = [_u32(current, offset) for offset in range(0, len(current), 4)]
    successors, calls = _successors(
        words,
        {offset // 4 for offset in relocated_offsets},
        {offset // 4 for offset in jumptable_offsets},
    )
    entries = _entry_indexes(successors)
    relocated_indexes = {offset // 4 for offset in relocated_offsets}
    # Screen by WORD, never by exact offset.  MWCC records an EMB_SDA21
    # entry at the instruction offset PLUS 2 while the extracted target
    # records it at the instruction offset, so an exact membership test
    # against word offsets (always multiples of 4) never fires for one and
    # a relocated word slips through the "not a copy-form candidate"
    # precondition.  That precondition is load-bearing: a relocated
    # `addi rD,rA,0` reads as immediate zero before linking but is really
    # an address half, which decode_copy_form would classify as a register
    # COPY.  claim.law.HV_emb-sda21-relocation-offset-differs-between-
    # our-objects-and-the-target.20260901.v1
    target_relocated_indexes = {
        offset // 4 for offset in target_relocated_offsets
    }

    output = bytearray(current)
    changed = 0
    seen = set()
    for edit in edits:
        if "at" not in edit:
            # A KeyError here surfaced as a bare traceback a worker
            # misread as a tool bug; name the actual authoring mistake.
            raise ValueError(
                f"copy-form edit missing its 'at' key: {edit} — each"
                " equivalent_copy_form edit needs 'at' (byte offset)")
        offset = _parse_int(edit["at"])
        if offset % 4 or not 0 <= offset <= len(current) - 4:
            raise ValueError(f"invalid copy-form offset {edit}")
        if offset in seen:
            raise ValueError(f"duplicate copy-form edit at +0x{offset:x}")
        seen.add(offset)
        if (offset // 4 in relocated_indexes
                or offset // 4 in target_relocated_indexes):
            raise ValueError(
                f"+0x{offset:x}: relocated word is not a copy-form candidate"
            )
        word = _u32(current, offset)
        wanted = _u32(target, offset)
        if word == wanted:
            raise ValueError(f"+0x{offset:x}: word already matches target")

        ours = decode_copy_form(word)
        theirs = decode_copy_form(wanted)
        if ours is None:
            raise ValueError(
                f"+0x{offset:x}: our word 0x{word:08x} is not a copy form"
            )
        if theirs is None:
            raise ValueError(
                f"+0x{offset:x}: target word 0x{wanted:08x} is not a "
                f"register copy"
            )

        proof = edit.get("proof")
        if proof in _COMBINED_PROOFS:
            # THE COMBINED FORM+RECOLOR STAGE.  Every mode above requires the
            # two words' DESTINATIONS to agree, and refuses a mismatch by
            # name as "a recolor, not a form change".  That refusal is right
            # for a single-stage rule and it is what walls off the 91-site
            # population where the form change and the recolor land on ONE
            # word: the form stage sees a recolor it may not perform, and the
            # recolor stage sees an opcode change it has no model for, so
            # neither can take the word and no reordering of the two fixes
            # it.
            #
            # The way through is to stop trying to produce the target word
            # here.  This stage rewrites our word to the target's ENCODING
            # carrying OUR registers, which is a pure form change and is
            # discharged by the same value obligation as the pure modes.
            # What comes out differs from the target in register fields
            # only, so the UNCHANGED recolor stage completes it and proves
            # the renaming with verify_consistent_recolor — the one
            # component entitled to adjudicate a recolor.  Two existing
            # proofs, composed; no new trust, and no guard relaxed.
            if ours[0] != "copy" and ours[0] != "li":
                raise ValueError(f"+0x{offset:x}: unsupported form {ours[0]}")
            if proof == "unconditional_recolor":
                # Copy -> copy.  Our word already sets rD to the contents of
                # rS and the replacement sets the SAME rD to the SAME rS in
                # the target's encoding, so the two are architecturally
                # identical and there is no dataflow obligation at all --
                # exactly the `unconditional` rationale, with the registers
                # left for the recolor stage to prove.
                if ours[0] != "copy" or theirs[0] != "copy":
                    raise ValueError(
                        f"+0x{offset:x}: \"unconditional_recolor\" needs both "
                        f"words to decode as copies (ours {ours[0]}, target "
                        f"{theirs[0]})"
                    )
                destination, source = ours[1], ours[2]
            elif proof == "constant_dataflow_recolor":
                # Ours `li rD,K`, target a copy: our declared source must
                # provably hold K here.  The target's source register is the
                # TARGET's colour and says nothing about our stream, so the
                # rule names OUR register and the recolor stage is what
                # proves the two correspond.
                if ours[0] != "li" or theirs[0] != "copy":
                    raise ValueError(
                        f"+0x{offset:x}: \"constant_dataflow_recolor\" needs "
                        f"our word to be a constant load and the target to be "
                        f"a copy (ours {ours[0]}, target {theirs[0]})"
                    )
                if "our_source" not in edit:
                    raise ValueError(
                        f"+0x{offset:x}: \"constant_dataflow_recolor\" needs "
                        f'"our_source" (the register in OUR colouring that '
                        f"holds the constant at this site)"
                    )
                destination = ours[1]
                source = _parse_int(edit["our_source"])
                if not 0 <= source < 32:
                    raise ValueError(
                        f"+0x{offset:x}: our_source r{source} is out of range"
                    )
                prove_constant_dataflow(
                    words, offset // 4, source, ours[2], successors, calls,
                    relocated_indexes, relocation_types=relocation_types,
                    call_targets=call_targets,
                )
            else:  # constant_dataflow_inverse_recolor
                # Ours is the copy, the target is `li rD,K`: our OWN source
                # must hold K here.  No declaration is needed in this
                # direction because the source is read off our own word.
                if ours[0] != "copy" or theirs[0] != "li":
                    raise ValueError(
                        f"+0x{offset:x}: "
                        f"\"constant_dataflow_inverse_recolor\" needs our "
                        f"word to be a copy and the target to be a constant "
                        f"load (ours {ours[0]}, target {theirs[0]})"
                    )
                destination, source = ours[1], ours[2]
                prove_constant_dataflow(
                    words, offset // 4, source, theirs[2], successors, calls,
                    relocated_indexes, relocation_types=relocation_types,
                    call_targets=call_targets,
                )
            if source == 0:
                raise ValueError(
                    f"+0x{offset:x}: our word's source is GPR r0, whose "
                    f"encoding asymmetry this class refuses"
                )
            wanted = encode_copy_like(wanted, destination, source)
            if wanted == word:
                raise ValueError(
                    f"+0x{offset:x}: re-encoding is a no-op, so this site is "
                    f"a pure recolor and belongs to the recolor stage"
                )
            # The whole composition rests on this: what we write must differ
            # from the target in REGISTER FIELDS ONLY, or the recolor stage
            # cannot finish the word and cannot be the thing that proves it.
            target_word = _u32(target, offset)
            if (wanted ^ target_word) & ~register_slot_mask(wanted):
                raise ValueError(
                    f"+0x{offset:x}: re-encoded word 0x{wanted:08x} differs "
                    f"from the target 0x{target_word:08x} outside its "
                    f"register fields, so the recolor stage could not "
                    f"complete it"
                )
            expected_form = (
                ("li", destination, theirs[2]) if theirs[0] == "li"
                else ("copy", destination, source)
            )
            check = decode_copy_form(wanted)
            if check != expected_form:
                raise ValueError(
                    f"+0x{offset:x}: re-encoded word 0x{wanted:08x} decodes "
                    f"as {check}, not the intended {expected_form}"
                )
        elif theirs[0] == "copy":
            _kind, destination, source = theirs
            if source == 0:
                raise ValueError(
                    f"+0x{offset:x}: target copies from r0, which addi reads as "
                    f"the literal zero — not a provable copy"
                )
            if ours[1] != destination:
                raise ValueError(
                    f"+0x{offset:x}: destination differs (r{ours[1]} vs "
                    f"r{destination}) — that is a recolor, not a form change"
                )
            if ours[0] == "copy":
                if proof != "unconditional":
                    raise ValueError(
                        f"+0x{offset:x}: copy/copy site requires "
                        f'"proof": "unconditional"'
                    )
                if ours[2] != source:
                    raise ValueError(
                        f"+0x{offset:x}: source differs (r{ours[2]} vs "
                        f"r{source}) — that is a recolor, not a form change"
                    )
            elif ours[0] == "li":
                if proof not in (
                    "dominating_def", "dominating_def_across_calls"
                ):
                    raise ValueError(
                        f"+0x{offset:x}: constant-load site requires "
                        f'"proof": "dominating_def" (or, to cross a direct '
                        f'named call, "dominating_def_across_calls")'
                    )
                definition = prove_constant_source(
                    words, offset // 4, source, ours[2], entries,
                    relocated_indexes,
                    across_calls=(proof == "dominating_def_across_calls"),
                    call_targets=call_targets,
                    relocation_types=relocation_types,
                )
                edit["_proved_at"] = definition * 4
            else:
                raise ValueError(f"+0x{offset:x}: unsupported form {ours[0]}")
        elif ours[0] == "copy":
            # THE INVERSE DIRECTION: ours is the copy, the target is the
            # `li`.  Our word sets rD to the CURRENT CONTENTS of rS; the
            # target word sets rD to the literal K.  Rewriting ours to the
            # target encoding is value-preserving exactly when OUR rS holds
            # K at this point, which is the same obligation shape as
            # `dominating_def` and is discharged by the same backward scan
            # over the SAME (our) stream — the stream that will execute
            # these bytes.  Scanning the TARGET stream would prove a fact
            # about the target's registers, which says nothing about the
            # object being patched.
            #
            # It is a SEPARATELY NAMED proof mode rather than a widening of
            # `dominating_def` so that no existing rule can drift onto the
            # opposite direction, and so a rule states which way it is
            # rewriting.  claim.law.DC_copy-form-class-is-directional-and-
            # its-inverse-population-is-unserved.20260901.v1
            destination, constant = theirs[1], theirs[2]
            if ours[2] == 0:
                # `mr rD,r0` really does read GPR 0, but `addi rD,r0,K` is
                # `li rD,K` and never reads it.  The pair is the r0 encoding
                # asymmetry that decode_copy_form exists to expose, the
                # measured inverse population contains no r0 source, and
                # every accepted form must be one the tests exercise — so
                # this stays refused rather than becoming a fourth mode.
                raise ValueError(
                    f"+0x{offset:x}: our word copies GPR r0, whose value the "
                    f"target's literal load cannot be shown to reproduce — "
                    f"the r0 encoding asymmetry is refused in this class"
                )
            if ours[1] != destination:
                raise ValueError(
                    f"+0x{offset:x}: destination differs (r{ours[1]} vs "
                    f"r{destination}) — that is a recolor, not a form change"
                )
            if proof not in (
                "dominating_def_inverse", "dominating_def_inverse_across_calls"
            ):
                raise ValueError(
                    f"+0x{offset:x}: inverse copy/constant-load site requires "
                    f'"proof": "dominating_def_inverse" (or, to cross a '
                    f'direct named call, '
                    f'"dominating_def_inverse_across_calls")'
                )
            definition = prove_constant_source(
                words, offset // 4, ours[2], constant, entries,
                relocated_indexes,
                across_calls=(proof == "dominating_def_inverse_across_calls"),
                call_targets=call_targets,
                relocation_types=relocation_types,
            )
            edit["_proved_at"] = definition * 4
        else:
            # li -> li is an IMMEDIATE difference, never a form change, and
            # webfrank must never close one.
            raise ValueError(
                f"+0x{offset:x}: target word 0x{wanted:08x} is not a "
                f"register copy"
            )

        struct.pack_into(">I", output, offset, wanted)
        if _u32(output, offset) != wanted:
            raise ValueError(f"+0x{offset:x}: rewrite did not reach target")
        changed += 1
    return bytes(output), changed


# ---------------------------------------------------------------------------
# THE RANGE-PROOF CLASS (redundant rlwinm mask bits).
#
# BOUNDARY, stated before the code so a reader can check the code against it:
# this class closes a difference between two `rlwinm` words that agree in
# opcode, destination, source, rotate count and record bit and differ ONLY in
# the MB/ME mask field, and only when every source bit the two masks disagree
# about is PROVABLY ZERO in our own instruction stream.  It is a
# dataflow-equivalence class, so it is deliberately the narrowest possible
# one: a single opcode, a single field, and a bit-level fact derived from OUR
# object's bytes with no appeal to source, to the target's registers, or to
# any semantic argument about what the function means.
#
# It is NOT "any semantically equivalent stream": every other immediate
# difference — a rotate count, a displacement, a literal, a different opcode
# with the same effect — is refused by name, and the proof refuses outright
# rather than falling back to a weaker check.
# ---------------------------------------------------------------------------


def _ppc_mask(mb: int, me: int) -> int:
    """The 32-bit mask PowerPC's MB/ME pair names.

    Bits are numbered big-endian (bit 0 is 0x80000000).  ``mb > me`` is the
    legal WRAPPED form — bits mb..31 and 0..me — and reading it as an empty
    mask would understate what a word discards, which is the unsound
    direction.  So it is spelled out rather than assumed away.
    """
    if not 0 <= mb < 32 or not 0 <= me < 32:
        raise ValueError(f"invalid rlwinm mask field MB={mb} ME={me}")
    mask = 0
    index = mb
    while True:
        mask |= 1 << (31 - index)
        if index == me:
            break
        index = (index + 1) & 31
    return mask


def _rotl32(value: int, amount: int) -> int:
    amount &= 31
    value &= 0xFFFFFFFF
    if not amount:
        return value
    return ((value << amount) | (value >> (32 - amount))) & 0xFFFFFFFF


def decode_rlwinm(word: int):
    """``(rA, rS, SH, MB, ME, Rc)`` for an ``rlwinm``, else ``None``.

    Only primary opcode 21 is accepted.  ``rlwimi`` (20) reads its
    destination and ``rlwnm`` (23) takes its rotate from a register, so
    neither is a member of this class however similar the encoding looks.
    """
    if (word >> 26) != 21:
        return None
    return (
        (word >> 16) & 0x1F,   # rA — destination
        (word >> 21) & 0x1F,   # rS — source
        (word >> 11) & 0x1F,   # SH
        (word >> 6) & 0x1F,    # MB
        (word >> 1) & 0x1F,    # ME
        word & 1,              # Rc
    )


def redundant_mask_source_bits(ours: int, target: int) -> int:
    """The obligation for rewriting *ours* to *target*: a mask of SOURCE bits
    that must be provably zero.

    Both words must be ``rlwinm`` and must agree in everything but MB/ME.
    ``rlwinm rA,rS,SH,MB,ME`` computes ``ROTL(rS,SH) & MASK(MB,ME)``, so the
    two words differ exactly in the result bits where their masks disagree,
    and result bit *j* is source bit ``(j - SH) mod 32``.  Rotating the mask
    delta back by SH therefore names precisely the source bits whose being
    zero makes the two words compute the same value.

    The relation is SYMMETRIC: widening and narrowing a mask carry the same
    obligation, so no separate direction label is needed (unlike the copy-form
    class, whose two arrows have genuinely different proof shapes).
    """
    mine = decode_rlwinm(ours)
    theirs = decode_rlwinm(target)
    if mine is None or theirs is None:
        raise ValueError(
            f"the redundant-mask class needs two `rlwinm` words "
            f"(ours 0x{ours:08x}, target 0x{target:08x})"
        )
    if ours == target:
        raise ValueError("the two words are identical; there is nothing to prove")
    if mine[0] != theirs[0] or mine[1] != theirs[1]:
        raise ValueError(
            f"register field differs (ours rA=r{mine[0]},rS=r{mine[1]}; "
            f"target rA=r{theirs[0]},rS=r{theirs[1]}) — that is a recolor, "
            f"not a redundant mask"
        )
    if mine[2] != theirs[2]:
        raise ValueError(
            f"rotate count differs ({mine[2]} vs {theirs[2]}); this class "
            f"closes MASK fields only, never any other immediate"
        )
    if mine[5] != theirs[5]:
        raise ValueError(
            f"record bit differs (Rc {mine[5]} vs {theirs[5]}); the CR0 "
            f"update is a separate architectural effect"
        )
    delta = _ppc_mask(mine[3], mine[4]) ^ _ppc_mask(theirs[3], theirs[4])
    if not delta:
        # Two different MB/ME spellings of the SAME mask cannot happen for a
        # 32-bit rotate, but if the decode ever changes, refuse rather than
        # returning an empty (vacuously satisfiable) obligation.
        raise ValueError("mask fields differ but denote the same mask")
    return _rotl32(delta, 32 - (mine[2] & 31)) if mine[2] % 32 else delta


# The instruction forms whose result's provably-zero bits are computed from
# their operands'.  Everything absent from this table contributes NOTHING:
# its destination becomes fully unknown, which is the fail-closed direction.
def _modelled_zero_result(word: int, state: tuple) -> dict:
    opcode = word >> 26
    rd = (word >> 21) & 0x1F        # rD for D-form loads / rS for logicals
    ra = (word >> 16) & 0x1F        # rA: base for loads, destination for logicals
    rb = (word >> 11) & 0x1F
    uimm = word & 0xFFFF
    all_ones = 0xFFFFFFFF

    if opcode == 34:                                    # lbz  rD,d(rA)
        return {rd: 0xFFFFFF00}
    if opcode == 40:                                    # lhz  rD,d(rA)
        return {rd: 0xFFFF0000}
    if opcode == 14:                                    # addi rD,rA,SIMM
        if ra == 0:                                     #   -> li rD,SIMM
            return {rd: ~(_sign_extend(uimm, 16) & all_ones) & all_ones}
        if uimm == 0:                                   #   -> mr rD,rA
            return {rd: state[ra]}
        return {}
    if opcode == 15 and ra == 0:                        # lis rD,UIMM
        return {rd: ~((uimm << 16) & all_ones) & all_ones}
    if opcode == 24:                                    # ori   rA,rS,UIMM
        return {ra: state[rd] & ~uimm & all_ones}
    if opcode == 25:                                    # oris  rA,rS,UIMM
        return {ra: state[rd] & ~((uimm << 16) & all_ones) & all_ones}
    if opcode == 28:                                    # andi. rA,rS,UIMM
        return {ra: state[rd] | (~uimm & all_ones)}
    if opcode == 29:                                    # andis. rA,rS,UIMM
        return {ra: state[rd] | (~((uimm << 16) & all_ones) & all_ones)}
    if opcode == 21:                                    # rlwinm rA,rS,SH,MB,ME
        mask = _ppc_mask((word >> 6) & 0x1F, (word >> 1) & 0x1F)
        return {ra: (~mask & all_ones) | _rotl32(state[rd], rb)}
    if opcode == 20:                                    # rlwimi rA,rS,SH,MB,ME
        mask = _ppc_mask((word >> 6) & 0x1F, (word >> 1) & 0x1F)
        return {ra: (mask & _rotl32(state[rd], rb))
                    | (~mask & all_ones & state[ra])}
    if opcode == 31:
        xo = (word >> 1) & 0x3FF
        if xo == 87:                                    # lbzx rD,rA,rB
            return {rd: 0xFFFFFF00}
        if xo == 279:                                   # lhzx rD,rA,rB
            return {rd: 0xFFFF0000}
        if xo == 28:                                    # and  rA,rS,rB
            return {ra: state[rd] | state[rb]}
        if xo == 444:                                   # or   rA,rS,rB (mr)
            return {ra: state[rd] & state[rb]}
        return {}
    return {}


def _known_zero_transfer(word: int, state: tuple, *, trust_immediates: bool):
    """The out-state for one instruction, or ``None`` when its effects are not
    modelled at all (the caller then drops every fact).

    ``_word_effects`` supplies the WRITE SET, so a register written by a form
    this table does not model still becomes unknown — an unmodelled write can
    never be silently skipped.  With *trust_immediates* clear (a relocated
    word, whose immediate is an unresolved address half) every written
    register becomes unknown regardless of form.
    """
    try:
        _reads, writes = _word_effects(word)
    except ValueError:
        return None
    written = {
        item[1] for item in writes
        if isinstance(item, tuple) and len(item) == 2 and item[0] == "g"
    }
    if not written:
        return state
    modelled = _modelled_zero_result(word, state) if trust_immediates else {}
    return tuple(
        (modelled.get(register, 0) & 0xFFFFFFFF) if register in written
        else state[register]
        for register in range(32)
    )


def _zero_bit_states(
    words: list[int], successors: list[list[int]], calls: list[bool],
    relocated_indexes: set[int], relocation_types: dict | None,
) -> dict[int, tuple]:
    """The known-zero-bit dataflow itself, as a map from index to IN-state.

    Split out of ``prove_zero_bits`` so a second class can ask a different
    question of the SAME analysis without re-deriving it.  Nothing about the
    lattice, the transfers or the fail-closed behaviour changes here; see
    ``prove_zero_bits`` for the full statement of what each step guarantees.
    """
    bottom = (0,) * 32
    states: dict[int, tuple] = {0: bottom}
    pending = [0]
    steps = 0
    budget = 64 * (len(words) + 1) * 32
    while pending:
        steps += 1
        if steps > budget:
            raise ValueError(
                "zero-bit dataflow did not converge within its step budget"
            )
        index = pending.pop()
        state = states[index]
        word = words[index]
        kind = (relocation_types or {}).get(index)
        if index in relocated_indexes and kind not in _IMMEDIATE_ONLY_RELOCATIONS:
            out = bottom
        else:
            out = _known_zero_transfer(
                word, state,
                trust_immediates=index not in relocated_indexes,
            )
            if out is None:
                out = bottom
        if calls[index]:
            out = bottom
        for successor in successors[index]:
            known = states.get(successor)
            merged = out if known is None else tuple(
                a & b for a, b in zip(known, out)
            )
            if known is None or merged != known:
                states[successor] = merged
                pending.append(successor)
    return states


def prove_zero_bits(
    words: list[int], site: int, source: int, required: int,
    successors: list[list[int]], calls: list[bool],
    relocated_indexes: set[int],
    *, relocation_types: dict | None = None,
) -> None:
    """Prove every bit in *required* is zero in GPR *source* on entry to
    *site*, on EVERY path through the function.

    A whole-CFG forward dataflow over a per-GPR KNOWN-ZERO BIT MASK, meeting
    by intersection at merges and seeded with NO knowledge at the entry.  The
    lattice orders by information, the entry is the bottom element, and every
    modelled transfer is monotone, so the worklist converges; a step budget
    fails the proof closed if that ever stops being true.

    Every way of losing the fact loses it:

    * a write this table does not model makes its destination unknown;
    * ``_word_effects`` raising on an unmodelled FORM drops ALL facts, so an
      instruction webfrank cannot decode can never be stepped over silently;
    * a relocated word makes its writes unknown, and one carrying a
      relocation outside ``_IMMEDIATE_ONLY_RELOCATIONS`` drops all facts,
      because its write set is decoded from register fields the relocation
      may itself rewrite (R_PPC_EMB_SDA21 rewrites the base register);
    * a CALL drops ALL facts.  Deliberately: the copy-form class has an ABI
      axiom letting a callee-saved register survive a named call, and this
      class does not adopt it.  A range fact is a claim about VALUE, and
      nothing in this analysis establishes what value a callee leaves in a
      restored register; the axiom's own justification ("restores r14-r31")
      is about the register file, not about the bits.  A site whose
      definition is separated from it by a call is simply refused.

    The analysis runs over OUR stream, because our object executes it.
    """
    if not 0 <= site < len(words):
        raise ValueError(
            f"zero-bit dataflow site +0x{site * 4:x} is outside the function"
        )
    if not 0 <= source < 32:
        raise ValueError(f"zero-bit dataflow source r{source} is out of range")
    if not required:
        raise ValueError(
            "zero-bit dataflow refuses an empty obligation: a vacuous proof "
            "would accept a rewrite nothing had checked"
        )

    states = _zero_bit_states(
        words, successors, calls, relocated_indexes, relocation_types,
    )

    if site not in states:
        raise ValueError(
            f"+0x{site * 4:x} is not reachable from the function entry, so no "
            f"range fact reaches it"
        )
    proved = states[site][source]
    missing = required & ~proved & 0xFFFFFFFF
    if missing:
        raise ValueError(
            f"+0x{site * 4:x}: r{source} bits 0x{missing:08x} are not provably "
            f"zero on every path, so the mask fields are not interchangeable"
        )


def equivalent_mask_form(
    current: bytes, target: bytes, edits: list,
    *, relocated_offsets: set[int], target_relocated_offsets: set[int],
    jumptable_offsets: set[int], relocation_types: dict | None = None,
) -> tuple[bytes, int]:
    """Rewrite named ``rlwinm`` words whose mask bits are provably redundant.

    Each edit must carry ``"proof": "zero_bits_dataflow"`` and a
    ``declared_zero_bits`` mask that EQUALS the obligation computed from the
    two words.  The declaration is not decoration: it makes the rule state, in
    its own text, exactly which bits it is claiming are zero, so an audit can
    read the claim without re-deriving it, and a rule whose residual quietly
    changed shape fails instead of proving a different statement.

    The analysis is re-derived from the OUTPUT before every edit, so a second
    edit reasons about the stream that will actually execute rather than the
    one we started with.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("mask-form functions must have equal aligned sizes")
    relocated_indexes = {offset // 4 for offset in relocated_offsets}
    target_relocated_indexes = {
        offset // 4 for offset in target_relocated_offsets
    }

    output = bytearray(current)
    changed = 0
    seen = set()
    for edit in edits:
        if "at" not in edit:
            raise ValueError(
                f"mask-form edit missing its 'at' key: {edit} — each "
                f"equivalent_mask_form edit needs 'at' (byte offset)"
            )
        offset = _parse_int(edit["at"])
        if offset % 4 or not 0 <= offset <= len(current) - 4:
            raise ValueError(f"invalid mask-form offset {edit}")
        if offset in seen:
            raise ValueError(f"duplicate mask-form edit at +0x{offset:x}")
        seen.add(offset)
        if (offset // 4 in relocated_indexes
                or offset // 4 in target_relocated_indexes):
            raise ValueError(
                f"+0x{offset:x}: relocated word is not a mask-form candidate"
            )
        word = _u32(output, offset)
        wanted = _u32(target, offset)
        if word == wanted:
            raise ValueError(f"+0x{offset:x}: word already matches target")
        if edit.get("proof") != "zero_bits_dataflow":
            raise ValueError(
                f"+0x{offset:x}: the redundant-mask class requires "
                f'"proof": "zero_bits_dataflow" — it has exactly one proof '
                f"mode and no default"
            )
        try:
            required = redundant_mask_source_bits(word, wanted)
        except ValueError as failure:
            raise ValueError(f"+0x{offset:x}: {failure}") from None
        if "declared_zero_bits" not in edit:
            raise ValueError(
                f"+0x{offset:x}: the rule must state its obligation as "
                f'"declared_zero_bits" (computed: 0x{required:08x})'
            )
        declared = _parse_int(edit["declared_zero_bits"])
        if declared != required:
            raise ValueError(
                f"+0x{offset:x}: declared_zero_bits 0x{declared:08x} is not "
                f"the computed obligation 0x{required:08x}"
            )

        words = [_u32(output, at) for at in range(0, len(output), 4)]
        successors, calls = _successors(
            words, relocated_indexes,
            {at // 4 for at in jumptable_offsets},
        )
        prove_zero_bits(
            words, offset // 4, decode_rlwinm(word)[1], required,
            successors, calls, relocated_indexes,
            relocation_types=relocation_types,
        )
        struct.pack_into(">I", output, offset, wanted)
        if _u32(output, offset) != wanted:
            raise ValueError(f"+0x{offset:x}: rewrite did not reach target")
        changed += 1
    return bytes(output), changed


# ---------------------------------------------------------------------------
# THE LIVE-ZERO VALUE CLASS (a zero produced two ways).
#
# BOUNDARY, stated before the code so a reader can check the code against it:
# this class closes a difference between two words that PROVABLY WRITE THE
# LITERAL ZERO TO THE SAME GPR and have no other architectural effect
# whatever.  It exists because `equivalent_copy_form` covers only the
# `li rD,0` / `addi rD,rS,0` pair, and the measured live-zero population is
# wider than that pair: game/ui/btext::DrawStringTextMLines and ::FontInit
# both compute the zero as `slwi rD,rS,2` off a register the target simply
# copies, and neither word is a copy form at all, so that stage refuses them
# at "our word is not a copy form" with no proof mode offered.
#
# The obligation is discharged entirely inside the EXISTING known-zero-bit
# dataflow: both words' results are evaluated with `_modelled_zero_result`
# against the state `prove_zero_bits` already computes, and each must come
# out all-ones (every bit provably zero).  No new analysis, no new trust.
# The semantic distance is therefore unchanged: it is the same
# value-equality ceiling the range-proof class sits on, asked about a
# WHOLE REGISTER instead of a mask's worth of bits.
#
# It is NOT "any semantically equivalent stream".  Four separate walls:
#
#   * an opcode whitelist (`addi`/`li`, `rlwinm`, `or`/`mr`, all with Rc
#     clear) — every other form is refused BY NAME, so no load, no store, no
#     trapping form and no carry/CR producer can enter the class even if the
#     dataflow would happen to prove its result zero;
#   * both words must write exactly ONE resource and it must be the SAME
#     GPR, checked through `_word_effects`, so a second write can never ride
#     along;
#   * the value proved is the literal zero and nothing else — there is no
#     "equal constants" generalisation here, because zero is the only
#     constant the known-zero-bit lattice can express;
#   * the proof reads OUR pre-recolor registers, so the stage refuses to
#     compose with anything that would launder an unproven renaming into it.
# ---------------------------------------------------------------------------

# The forms this class will look at at all.  Each entry is (opcode, xo or
# None); every one is a non-trapping, non-memory ALU form whose result the
# known-zero-bit table models exactly, and every one is exercised by a test.
_ZERO_FORM_OPCODES = {
    14: None,    # addi rD,rA,SIMM  (rA == 0 spells `li rD,SIMM`)
    21: None,    # rlwinm rA,rS,SH,MB,ME  (`slwi` is the measured spelling)
    31: 444,     # or rA,rS,rB  (`mr` when rS == rB)
}


def decode_zero_form_destination(word: int) -> int:
    """Return the single GPR *word* writes, or raise naming the refusal.

    The whitelist is checked first and the write set second, so a form
    outside the class is rejected for BEING outside it rather than for
    whatever its effects happen to look like.  A record-setting variant is
    refused by the write-set check (Rc adds a CR write), and that refusal is
    kept rather than folded into the whitelist so the reason a reader sees
    is the true one.
    """
    opcode = word >> 26
    if opcode not in _ZERO_FORM_OPCODES:
        raise ValueError(
            f"0x{word:08x}: opcode {opcode} is outside the live-zero value "
            f"class, which accepts only addi/li, rlwinm and or/mr"
        )
    expected_xo = _ZERO_FORM_OPCODES[opcode]
    if expected_xo is not None and ((word >> 1) & 0x3FF) != expected_xo:
        raise ValueError(
            f"0x{word:08x}: opcode-31 form xo {(word >> 1) & 0x3FF} is "
            f"outside the live-zero value class, which accepts only "
            f"or/mr (xo 444)"
        )
    if opcode in (21, 31) and word & 1:
        # Stated by this class in its own right rather than left to the
        # write-set check below.  `_word_effects` did not model the M-form Rc
        # bit at all until run 37 (fixed alongside this), and a class whose
        # soundness depends on a shared model noticing a CR write should say
        # so itself instead of inheriting the omission.
        raise ValueError(
            f"0x{word:08x}: record-setting form also writes CR0, which the "
            f"live-zero value class refuses"
        )
    reads, writes = _word_effects(word)
    if any(_is_memory(resource) for resource in reads | writes):
        raise ValueError(
            f"0x{word:08x}: touches memory, which this class refuses"
        )
    if len(writes) != 1:
        raise ValueError(
            f"0x{word:08x}: writes {sorted(map(str, writes))} — the live-zero "
            f"value class requires exactly one written resource (a "
            f"record-setting form also writes CR0 and is refused here)"
        )
    resource = next(iter(writes))
    if not (isinstance(resource, tuple) and len(resource) == 2
            and resource[0] == "g"):
        raise ValueError(
            f"0x{word:08x}: writes {resource}, which is not a GPR"
        )
    return resource[1]


def prove_zero_result(
    words: list[int], site: int, word: int, destination: int,
    successors: list[list[int]], calls: list[bool],
    relocated_indexes: set[int],
    *, relocation_types: dict | None = None,
) -> None:
    """Prove *word*, executed at *site* in OUR stream, writes 0 to *destination*.

    ``prove_zero_bits`` asks whether some bits of a SOURCE register are zero
    on entry to a site; this asks whether every bit of the RESULT is, which
    is the same lattice evaluated one transfer further on.  The word need not
    be the one currently at *site* — that is the whole point, because the
    target's word is evaluated against OUR dataflow facts before it is
    written into our stream.

    Failing closed is inherited wholesale: an unmodelled form, a call, a
    merge with an unknown path, or a relocation the analysis may not trust
    all leave the result short of all-ones and the proof refuses.
    """
    if not 0 <= site < len(words):
        raise ValueError(
            f"live-zero site +0x{site * 4:x} is outside the function"
        )
    states = _zero_bit_states(
        words, successors, calls, relocated_indexes, relocation_types,
    )
    if site not in states:
        raise ValueError(
            f"+0x{site * 4:x} is not reachable from the function entry, so no "
            f"zero fact reaches it"
        )
    modelled = _modelled_zero_result(word, states[site])
    proved = modelled.get(destination)
    if proved is None:
        raise ValueError(
            f"+0x{site * 4:x}: the known-zero table does not model the result "
            f"of 0x{word:08x} in r{destination}"
        )
    missing = ~proved & 0xFFFFFFFF
    if missing:
        raise ValueError(
            f"+0x{site * 4:x}: 0x{word:08x} is not provably zero in "
            f"r{destination} on every path (bits 0x{missing:08x} unproven)"
        )


def equivalent_zero_form(
    current: bytes, target: bytes, edits: list,
    *, relocated_offsets: set[int], target_relocated_offsets: set[int],
    jumptable_offsets: set[int], relocation_types: dict | None = None,
) -> tuple[bytes, int]:
    """Rewrite named words whose result is provably the literal zero on both
    sides.

    Each edit must carry ``"proof": "zero_value_dataflow"`` and a
    ``declared_zero_register`` naming the GPR it claims both words zero.  As
    with ``declared_zero_bits`` the declaration is not decoration: it makes
    the rule state its own claim, so an audit reads it instead of
    re-deriving it, and a rule whose residual quietly changed shape fails
    rather than proving a different statement.

    The analysis is re-derived from the OUTPUT before every edit, so a second
    edit reasons about the stream that will actually execute.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("live-zero functions must have equal aligned sizes")
    relocated_indexes = {offset // 4 for offset in relocated_offsets}
    target_relocated_indexes = {
        offset // 4 for offset in target_relocated_offsets
    }

    output = bytearray(current)
    changed = 0
    seen = set()
    for edit in edits:
        if "at" not in edit:
            raise ValueError(
                f"live-zero edit missing its 'at' key: {edit} — each "
                f"equivalent_zero_form edit needs 'at' (byte offset)"
            )
        offset = _parse_int(edit["at"])
        if offset % 4 or not 0 <= offset <= len(current) - 4:
            raise ValueError(f"invalid live-zero offset {edit}")
        if offset in seen:
            raise ValueError(f"duplicate live-zero edit at +0x{offset:x}")
        seen.add(offset)
        if (offset // 4 in relocated_indexes
                or offset // 4 in target_relocated_indexes):
            raise ValueError(
                f"+0x{offset:x}: relocated word is not a live-zero candidate"
            )
        word = _u32(output, offset)
        wanted = _u32(target, offset)
        if word == wanted:
            raise ValueError(f"+0x{offset:x}: word already matches target")
        if edit.get("proof") != "zero_value_dataflow":
            raise ValueError(
                f"+0x{offset:x}: the live-zero value class requires "
                f'"proof": "zero_value_dataflow" — it has exactly one proof '
                f"mode and no default"
            )
        try:
            ours_destination = decode_zero_form_destination(word)
            target_destination = decode_zero_form_destination(wanted)
        except ValueError as failure:
            raise ValueError(f"+0x{offset:x}: {failure}") from None
        if ours_destination != target_destination:
            raise ValueError(
                f"+0x{offset:x}: destinations differ (r{ours_destination} vs "
                f"r{target_destination}) — that is a recolor, and this class "
                f"proves a VALUE, never a renaming"
            )
        if "declared_zero_register" not in edit:
            raise ValueError(
                f"+0x{offset:x}: the rule must state its claim as "
                f'"declared_zero_register" (computed: r{ours_destination})'
            )
        declared = _parse_int(edit["declared_zero_register"])
        if declared != ours_destination:
            raise ValueError(
                f"+0x{offset:x}: declared_zero_register r{declared} is not "
                f"the written register r{ours_destination}"
            )

        words = [_u32(output, at) for at in range(0, len(output), 4)]
        successors, calls = _successors(
            words, relocated_indexes,
            {at // 4 for at in jumptable_offsets},
        )
        for label, candidate in (("ours", word), ("target", wanted)):
            try:
                prove_zero_result(
                    words, offset // 4, candidate, ours_destination,
                    successors, calls, relocated_indexes,
                    relocation_types=relocation_types,
                )
            except ValueError as failure:
                raise ValueError(f"{label}: {failure}") from None
        struct.pack_into(">I", output, offset, wanted)
        if _u32(output, offset) != wanted:
            raise ValueError(f"+0x{offset:x}: rewrite did not reach target")
        changed += 1
    return bytes(output), changed


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
    data: bytearray, patch: dict, target_data: bytes | None = None,
    symbol_addresses: dict[str, tuple[str, int]] | None = None,
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
    relocation_types = {
        offset // 4: reloc_type
        for offset, (reloc_type, _name) in text_relocations.items()
    }
    jumptable_offsets = _jumptable_targets(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    deferred_exit: list = []

    changed = 0
    permutation = patch.get("instruction_permutation")
    if patch.get("memory_disambiguation") and not permutation:
        raise ValueError(
            f"{symbol.name}: \"memory_disambiguation\" only refines the "
            f"permutation dependence audit, so it is meaningless without an "
            f"\"instruction_permutation\""
        )
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
        target_function_relocations = _function_text_relocations(
            target_data, target_sections, target_symbol.section_index,
            target_symbol.value, target_symbol.value + target_symbol.size,
        )

        # A function whose displaced words fall in two separated windows
        # cannot be expressed as one region: permute_instruction_atoms
        # refuses any region containing a control instruction, and MWCC's
        # schedule differences cluster around basic-block boundaries, so a
        # widened region that swallows the intervening code is exactly the
        # unsound move the control-op refusal exists to prevent.  Accept a
        # LIST of windows instead, each applied in ascending order through
        # the unchanged permute_instruction_atoms with its own before/after
        # and relocation hashes.  A single dict keeps working unchanged.
        # claim.law.HV_single-permutation-region-is-the-binding-schema-
        # limit.20260901.v1
        windows, ranges = permutation_windows(permutation, symbol.size)

        # SDA memory disambiguation, opt-in and fully declared.  Without it
        # every non-stack memory access is one resource, so a store can never
        # cross a load however obviously distinct the two globals are.
        disambiguation = patch.get("memory_disambiguation") or {}
        memory_locations = resolve_memory_locations(
            original_function, disambiguation.get("locations", ()),
            text_relocations, symbol_addresses,
        )
        for at in memory_locations:
            if not any(start <= at < end for start, end in ranges):
                raise ValueError(
                    f"{symbol.name}: memory disambiguation at +0x{at:x} is "
                    f"outside every permutation window"
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

        for window, (relative_start, relative_end) in zip(windows, ranges):
            # Re-read the relocation table for every window: the previous
            # window rewrote it in place.
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

            window_symbols = {
                offset - relative_start: name
                for offset, (_reloc_type, name) in text_relocations.items()
                if relative_start <= offset < relative_end
            }
            window_target_relocations = {
                offset - relative_start: entry
                for offset, entry in target_function_relocations.items()
                if relative_start <= offset < relative_end
            }

            exit_index = relative_end // 4

            def _defer_exit_dead(resource, exit_index=exit_index):
                # Checked against both function images after the whole patch
                # is applied, when the final downstream register fields are
                # known.  Each resource carries ITS OWN window's exit point.
                deferred_exit.append((resource, exit_index))
                return True

            permuted, moved_records, moved = permute_instruction_atoms(
                region,
                [_parse_int(index) for index in window["order"]],
                region_records,
                before_sha256=window["before_sha256"],
                after_sha256=window["after_sha256"],
                before_relocations_sha256=window["before_relocations_sha256"],
                after_relocations_sha256=window["after_relocations_sha256"],
                exit_dead=_defer_exit_dead,
                our_symbols=window_symbols,
                target_relocations=window_target_relocations,
                memory_locations={
                    (at - relative_start) // 4: location
                    for at, location in memory_locations.items()
                    if relative_start <= at < relative_end
                },
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

            # The permutation just rewrote the relocation table, so the
            # offsets captured before it ran are stale.  Every later stage
            # consumes these sets as a FAIL-CLOSED input (a relocated word is
            # not a literal, and a relocated word is not a copy-form
            # candidate), and a stale set can silently move a word out from
            # under its own guard.  Re-derive them from the patched object
            # before the next window and before any stage that composes with
            # the permutation.
            text_relocations = _function_text_relocations(
                data, sections, symbol.section_index,
                symbol.value, symbol.value + symbol.size,
            )
            relocated_offsets = set(text_relocations)
            call_targets = {
                offset: name
                for offset, (reloc_type, name) in text_relocations.items()
                if reloc_type == 10  # R_PPC_REL24
            }
            relocation_types = {
                offset // 4: reloc_type
                for offset, (reloc_type, _name) in text_relocations.items()
            }

    copy_forms = patch.get("equivalent_copy_form")
    if copy_forms:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        if target_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({target_symbol.size} != {symbol.size})"
            )
        target_text = target_sections[target_symbol.section_index]
        target_function = target_data[
            target_text.offset + target_symbol.value:
            target_text.offset + target_symbol.value + target_symbol.size
        ]
        target_relocations = _function_text_relocations(
            target_data, target_sections, target_symbol.section_index,
            target_symbol.value, target_symbol.value + target_symbol.size,
        )
        rewritten, form_changes = equivalent_copy_form(
            bytes(data[start:end]), target_function, copy_forms,
            relocated_offsets=relocated_offsets,
            target_relocated_offsets=set(target_relocations),
            jumptable_offsets=jumptable_offsets,
            call_targets=call_targets,
            relocation_types=relocation_types,
        )
        data[start:end] = rewritten
        changed += form_changes

    # THE RANGE-PROOF (redundant rlwinm mask) STAGE.  It runs BEFORE the
    # register stage on purpose: its proof is a bit-level fact about OUR
    # colouring, and the recolor stage's own proof then carries that fact
    # into the target colouring.  With "unproven_recolor_audit" there IS no
    # such proof — an unproved renaming could change which value the site
    # reads — so the composition is refused by name rather than trusted.
    mask_forms = patch.get("equivalent_mask_form")
    if mask_forms:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        if patch.get("unproven_recolor_audit"):
            raise ValueError(
                f"{symbol.name}: a redundant-mask rewrite may not ride on "
                f"\"unproven_recolor_audit\" — the range proof reads our "
                f"pre-recolor registers, so an unproven renaming would "
                f"launder it into a claim about a different value"
            )
        mask_sections = _sections(target_data)
        mask_symbol = _find_symbol(target_data, mask_sections, symbol.name)
        if mask_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({mask_symbol.size} != {symbol.size})"
            )
        mask_text = mask_sections[mask_symbol.section_index]
        mask_target_function = target_data[
            mask_text.offset + mask_symbol.value:
            mask_text.offset + mask_symbol.value + mask_symbol.size
        ]
        mask_target_relocations = _function_text_relocations(
            target_data, mask_sections, mask_symbol.section_index,
            mask_symbol.value, mask_symbol.value + mask_symbol.size,
        )
        rewritten, mask_changes = equivalent_mask_form(
            bytes(data[start:end]), mask_target_function, mask_forms,
            relocated_offsets=relocated_offsets,
            target_relocated_offsets=set(mask_target_relocations),
            jumptable_offsets=jumptable_offsets,
            relocation_types=relocation_types,
        )
        data[start:end] = rewritten
        changed += mask_changes
        print(
            f"WEBFRANK {symbol.name}: redundant-mask range proof "
            f"({mask_changes} site(s))"
        )

    # THE LIVE-ZERO VALUE STAGE.  Like the range-proof stage it reads OUR
    # registers, so it runs before the register stage and refuses the audit
    # escape for the same reason.  It additionally refuses to run alongside a
    # register stage at all: the word it writes is the TARGET's, so a
    # subsequent recolor would be asked to treat one target-coloured word as
    # an identity inside our colouring, and no measured site needs the
    # composition.  Widening that is a deliberate, audited step for the lane
    # that first measures a customer needing it — not a default.
    zero_forms = patch.get("equivalent_zero_form")
    if zero_forms:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        if patch.get("unproven_recolor_audit"):
            raise ValueError(
                f"{symbol.name}: a live-zero rewrite may not ride on "
                f"\"unproven_recolor_audit\" — the zero proof reads our "
                f"pre-recolor registers, so an unproven renaming would "
                f"launder it into a claim about a different value"
            )
        if (patch.get("copy_register_fields") or patch.get("recolors")
                or patch.get("register_fields")):
            raise ValueError(
                f"{symbol.name}: the live-zero value class does not compose "
                f"with a register stage — it writes the TARGET's word into "
                f"our colouring, which no recolor proof models, and no "
                f"measured site needs both"
            )
        zero_sections = _sections(target_data)
        zero_symbol = _find_symbol(target_data, zero_sections, symbol.name)
        if zero_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({zero_symbol.size} != {symbol.size})"
            )
        zero_text = zero_sections[zero_symbol.section_index]
        zero_target_function = target_data[
            zero_text.offset + zero_symbol.value:
            zero_text.offset + zero_symbol.value + zero_symbol.size
        ]
        zero_target_relocations = _function_text_relocations(
            target_data, zero_sections, zero_symbol.section_index,
            zero_symbol.value, zero_symbol.value + zero_symbol.size,
        )
        rewritten, zero_changes = equivalent_zero_form(
            bytes(data[start:end]), zero_target_function, zero_forms,
            relocated_offsets=relocated_offsets,
            target_relocated_offsets=set(zero_target_relocations),
            jumptable_offsets=jumptable_offsets,
            relocation_types=relocation_types,
        )
        data[start:end] = rewritten
        changed += zero_changes
        print(
            f"WEBFRANK {symbol.name}: live-zero value proof "
            f"({zero_changes} site(s))"
        )

    pre_register = bytes(data[start:end])
    register_stage = bool(
        patch.get("copy_register_fields") or patch.get("recolors")
        or patch.get("register_fields")
    )

    # A combined form+recolor edit leaves a word that is DELIBERATELY not the
    # target word: it carries the target's encoding around our registers, and
    # only the recolor stage's proof turns it into the target.  Both halves of
    # that sentence are load-bearing preconditions, so both are asserted here
    # rather than left to the after-hash to catch by accident.
    combined = [
        edit for edit in (patch.get("equivalent_copy_form") or [])
        if edit.get("proof") in _COMBINED_PROOFS
    ]
    if combined:
        if not register_stage:
            raise ValueError(
                f"{symbol.name}: a combined form+recolor edit requires a "
                f"register stage in the same patch — the re-encoded word is "
                f"left in OUR colouring and only the recolor completes it"
            )
        if patch.get("unproven_recolor_audit"):
            raise ValueError(
                f"{symbol.name}: a combined form+recolor edit may not ride on "
                f"\"unproven_recolor_audit\" — the form rewrite is justified "
                f"ONLY by the recolor being machine-proven, so an audit "
                f"escape would launder an unproven renaming into a "
                f"value-changing opcode rewrite"
            )

    # The value-equality recolor mode.  It is strictly more permissive than
    # verify_consistent_recolor, so it is opt-in, it never runs unless the
    # strict proof has already failed, and it refuses every composition whose
    # interaction with it is not exercised by a test.
    value_equality = patch.get("value_equality_recolor")
    value_equality_relocations: set = set()
    if value_equality is not None:
        if not isinstance(value_equality, dict):
            raise ValueError(
                f"{symbol.name}: \"value_equality_recolor\" must be an object "
                f"carrying its audit and its declared sites"
            )
        if not register_stage:
            raise ValueError(
                f"{symbol.name}: a value-equality recolor requires a register "
                f"stage — it exists to prove that stage's output"
            )
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        if patch.get("unproven_recolor_audit"):
            raise ValueError(
                f"{symbol.name}: a value-equality recolor may not ride on "
                f"\"unproven_recolor_audit\": the whole point of the mode is "
                f"that every escape it takes is machine-proven and declared"
            )
        if patch.get("post_recolor_permutation"):
            raise ValueError(
                f"{symbol.name}: a value-equality recolor may not compose "
                f"with a post-recolor permutation; the relation is proved "
                f"position by position against the recolor's own output and "
                f"no measured site needs both"
            )
        value_equality_sections = _sections(target_data)
        value_equality_symbol = _find_symbol(
            target_data, value_equality_sections, symbol.name
        )
        if value_equality_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({value_equality_symbol.size} != {symbol.size})"
            )
        value_equality_relocations = set(_function_text_relocations(
            target_data, value_equality_sections,
            value_equality_symbol.section_index,
            value_equality_symbol.value,
            value_equality_symbol.value + value_equality_symbol.size,
        ))

    # The post-recolor permutation stage.  When present, the recolor must aim
    # at the INTERMEDIATE image (the target with this permutation undone),
    # because the recolor's job is to prove a pure renaming and the permuted
    # target is not one.
    post_permutation = patch.get("post_recolor_permutation")
    post_windows: list = []
    post_ranges: list = []
    recolor_target: bytes | None = None
    if post_permutation:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        if not register_stage:
            raise ValueError(
                f"{symbol.name}: a post-recolor permutation requires a "
                f"register stage — it exists to run AFTER the recolor"
            )
        if patch.get("unproven_recolor_audit"):
            raise ValueError(
                f"{symbol.name}: a post-recolor permutation may not ride on "
                f"\"unproven_recolor_audit\": the permutation is audited in "
                f"the TARGET colouring, which is only reached by the recolor "
                f"being machine-proven"
            )
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        if target_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({target_symbol.size} != {symbol.size})"
            )
        target_text = target_sections[target_symbol.section_index]
        target_start = target_text.offset + target_symbol.value
        target_function = target_data[
            target_start:target_start + target_symbol.size
        ]
        target_relocations = _function_text_relocations(
            target_data, target_sections, target_symbol.section_index,
            target_symbol.value, target_symbol.value + target_symbol.size,
        )
        post_windows, post_ranges = permutation_windows(
            post_permutation, symbol.size
        )
        # A window MAY carry relocations.  Moving a relocation is sound — the
        # pre-recolor permutation stage does it — but the payload multiset
        # permute_instruction_atoms conserves proves only CONSERVATION, never
        # BINDING: two relocated atoms sharing a within-instruction offset can
        # be exchanged and the multiset will not notice, producing byte-exact
        # text whose loads point at each other's globals.  So every relocated
        # window in this stage is bound word-by-word against the target object
        # via verify_relocation_binding, reached in the application loop below
        # by supplying our post-recolor symbols and the target's relocations —
        # the identical proof the pre-recolor stage carries.  A relocation-free
        # window reduces to the original path with no behaviour change.  This
        # is the audited extension the refusal that lived here named; its
        # provenance is DrawPsysSub's frame-slot reclassification record
        # (attempt.MB_drawpsyssub-frame-slot-reclassification.20260902.v2), the
        # first measured post-recolor site carrying a relocation.
        recolor_target = unpermute_target_windows(
            target_function, post_windows, post_ranges
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
            bytes(data[start:end]),
            target_function if recolor_target is None else recolor_target,
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
        if "at" not in edit:
            raise ValueError(
                f"{symbol.name}: register-field edit missing its 'at' key:"
                f" {edit}")
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

    # The recolor is verified against the image the recolor stage actually
    # produced, which with a post-recolor permutation is the INTERMEDIATE and
    # not the final function.  Verifying the final one instead would ask
    # verify_consistent_recolor to read a permuted stream position by
    # position, which it has no model for and would refuse.
    recolor_image = bytes(data[start:end])
    if register_stage:
        strict_failure: ValueError | None = None
        try:
            verify_consistent_recolor(
                pre_register, recolor_image,
                jumptable_targets=jumptable_offsets,
                relocated_offsets=relocated_offsets,
                call_targets=call_targets,
            )
        except ValueError as failure:
            strict_failure = failure
        if strict_failure is None and value_equality is not None:
            # Not a formality: a rule whose residual has since disappeared
            # must be rewritten, not left resting on the wider mode.
            raise ValueError(
                f"{symbol.name}: \"value_equality_recolor\" is declared but "
                f"the strict recolor proof succeeds — remove the declaration "
                f"rather than leaving the rule resting on the wider mode"
            )
        if strict_failure is not None:
            error = strict_failure
            if value_equality is not None:
                try:
                    verify_value_equality_recolor(
                        pre_register, recolor_image,
                        jumptable_targets=jumptable_offsets,
                        relocated_offsets=relocated_offsets,
                        target_relocated_offsets=value_equality_relocations,
                        call_targets=call_targets,
                        substitutions=value_equality.get("substitutions", ()),
                        compare_exchanges=value_equality.get(
                            "compare_exchanges", ()
                        ),
                    )
                except ValueError as wider:
                    raise ValueError(
                        f"{symbol.name}: value-equality recolor proof failed "
                        f"({wider})"
                    ) from None
                print(
                    f"WEBFRANK {symbol.name}: value-equality recolor proved "
                    f"({len(value_equality.get('substitutions', ()))} "
                    f"substitution(s), "
                    f"{len(value_equality.get('compare_exchanges', ()))} "
                    f"comparison exchange(s))"
                )
            else:
                audit = patch.get("unproven_recolor_audit")
                if not audit:
                    raise ValueError(
                        f"{symbol.name}: register-field patch is not a "
                        f"consistent recolor ({error}); model the residual as "
                        "an instruction_permutation, declare a proved "
                        "\"value_equality_recolor\", or record a manual "
                        "equivalence audit in \"unproven_recolor_audit\""
                    ) from None
                print(
                    f"WEBFRANK {symbol.name}: UNPROVEN recolor equivalence "
                    f"accepted by audit ({error}) — {audit}"
                )

    # Now, and only now, the target colouring exists in the object and the
    # permutation that was illegal in ours can be audited.  exit_dead is
    # deliberately NOT offered here: this stage gets the strictest form of
    # check_permutation_dependences, with no escape for a moved final write.
    #
    # Relocation binding for a post-recolor window (the audited extension the
    # setup comment names).  The recolor stage that just ran rewrites only
    # register fields, never relocation entries, so the object's relocations
    # still stand at their pre-permute positions with their original symbols;
    # re-derive them fresh (fail-closed: a stale set could move a word out
    # from under its own binding).
    if post_windows:
        post_relocations = _function_text_relocations(
            data, sections, symbol.section_index,
            symbol.value, symbol.value + symbol.size,
        )
        post_relocation_sections = [
            section for section in sections
            if section.section_type == SHT_RELA
            and section.info == symbol.section_index
        ]
        if len(post_relocation_sections) > 1:
            raise ValueError(
                f"{symbol.name}: expected at most one relocation section for "
                f"text, found {len(post_relocation_sections)}"
            )
        post_relocation_section = (
            post_relocation_sections[0] if post_relocation_sections else None
        )
        post_entry_size = (
            (post_relocation_section.entry_size or 12)
            if post_relocation_section is not None else 12
        )
        if post_relocation_section is not None and (
            post_entry_size != 12
            or post_relocation_section.size % post_entry_size
        ):
            raise ValueError(f"{symbol.name}: unsupported relocation layout")

    for window, (relative_start, relative_end) in zip(post_windows,
                                                      post_ranges):
        region = bytes(data[start + relative_start:start + relative_end])
        order = [_parse_int(index) for index in window["order"]]

        # Read the relocation table fresh every window: a previous window
        # rewrote it in place.
        section_region_start = symbol.value + relative_start
        section_region_end = symbol.value + relative_end
        records = []
        if post_relocation_section is not None:
            for record_offset in range(
                post_relocation_section.offset,
                post_relocation_section.offset + post_relocation_section.size,
                post_entry_size,
            ):
                records.append(struct.unpack_from(">IIi", data, record_offset))
        region_records = [
            (offset - section_region_start, info, addend)
            for offset, info, addend in records
            if section_region_start <= offset < section_region_end
        ]
        window_symbols = {
            offset - relative_start: name
            for offset, (_reloc_type, name) in post_relocations.items()
            if relative_start <= offset < relative_end
        }
        window_target_relocations = {
            offset - relative_start: entry
            for offset, entry in target_relocations.items()
            if relative_start <= offset < relative_end
        }

        # before/after relocation hashes are a self-consistency check on the
        # move; the load-bearing proof is permute_instruction_atoms' own
        # payload-conservation multiset plus verify_relocation_binding against
        # the target, reached here by supplying our_symbols and
        # target_relocations.
        before_relocations = _relocation_sha256(region_records, window_symbols)
        destination_by_source = {
            source: destination for destination, source in enumerate(order)
        }
        expected_moved = []
        expected_moved_symbols: dict[int, str] = {}
        for offset, info, addend in region_records:
            moved_offset = (
                destination_by_source[offset // 4] * 4 + offset % 4
            )
            expected_moved.append((moved_offset, info, addend))
            expected_moved_symbols[moved_offset] = window_symbols[offset]
        # permute_instruction_atoms sorts the moved entries by offset before
        # hashing; match that so the self-consistency hash lines up.
        expected_moved.sort(key=lambda item: item[0])
        after_relocations = _relocation_sha256(
            expected_moved, expected_moved_symbols
        )

        permuted, moved_records, moved = permute_instruction_atoms(
            region,
            order,
            region_records,
            before_sha256=_sha256(region),
            after_sha256=_sha256(bytes(
                target_function[relative_start:relative_end]
            )),
            before_relocations_sha256=before_relocations,
            after_relocations_sha256=after_relocations,
            exit_dead=None,
            our_symbols=window_symbols,
            target_relocations=window_target_relocations,
        )
        data[start + relative_start:start + relative_end] = permuted
        changed += moved

        # The text moved; its relocation entries must move with it or the
        # linker fixes up the wrong words.  Rewrite the table in place exactly
        # as the pre-recolor stage does.
        if post_relocation_section is not None and region_records:
            outside_records = [
                record for record in records
                if not section_region_start <= record[0] < section_region_end
            ]
            records = outside_records + [
                (section_region_start + offset, info, addend)
                for offset, info, addend in moved_records
            ]
            records.sort(key=lambda item: item[0])
            if len(records) * post_entry_size != post_relocation_section.size:
                raise ValueError(f"{symbol.name}: relocation count changed")
            for index, record in enumerate(records):
                struct.pack_into(
                    ">IIi", data,
                    post_relocation_section.offset + index * post_entry_size,
                    *record,
                )

    after = _sha256(data[start:end])
    if after != patch["after_sha256"]:
        raise ValueError(
            f"{symbol.name}: output hash {after} != expected "
            f"{patch['after_sha256']}"
        )
    final_function = bytes(data[start:end])

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
        # Each deferred resource is checked at ITS OWN window's exit point,
        # never at a shared one: with several windows a resource certified
        # dead after the last window could still be live after an earlier
        # one, and a single exit index would silently excuse it.
        for resource, exit_index in deferred_exit:
            for label, words in (("raw", words_raw), ("patched", words_final)):
                if not _resource_dead_after(
                    words, exit_index, resource, successors, call_flags,
                    call_targets,
                ):
                    raise ValueError(
                        f"{symbol.name}: permutation changes the final write "
                        f"of {resource}, which is live after the region "
                        f"ending at +0x{exit_index * 4:x} in the {label} "
                        f"function"
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
    parser.add_argument(
        "--symbols", type=Path, default=None,
        help="split map used to prove two SDA globals do not alias "
             "(default: symbols.txt beside the config)")
    args = parser.parse_args()

    symbols_path = args.symbols or (args.config.parent / "symbols.txt")
    symbol_addresses = (
        load_symbol_addresses(symbols_path) if symbols_path.exists() else None
    )

    # MARK THE OBJECT WE FAIL TO WRITE, CLEAR IT WHEN WE DO (run-35 item 4).
    # Any refusal below — a pin body-hash assertion, a guard rejection, a
    # malformed rule — aborts before args.output is written, leaving the
    # PREVIOUS successful object on disk under a name every reader trusts.
    # fndiff, fnasm, slotdiff and probe then score bytes that do not
    # correspond to the source in the tree, silently; PC nearly recorded a
    # verdict from one. The marker is what fndiff.stale_object_warning()
    # reads. Computed HERE, above the empty-unit early return, because that
    # return writes a perfectly good object too — a test caught a stale
    # marker surviving it, which would have turned the warning into
    # permanent noise nobody believes.
    marker = stale_marker_path(args.output)

    def clear_marker():
        try:
            marker.unlink()
        except OSError:
            pass

    config = json.loads(args.config.read_text(encoding="utf-8"))
    units = config.get("units", {})
    patches = units.get(args.unit)
    if patches == []:
        # A unit whose LAST rule was removed leaves an empty list; that
        # is "no rules", not an error — the KeyError here cost a lane a
        # build cycle when it disabled a unit's only rule.
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(args.input.read_bytes())
        clear_marker()
        print(f"WEBFRANK {args.unit}: no rules (empty unit) — "
              "object passed through unchanged")
        return 0
    if not patches:
        raise KeyError(f"no webfrank configuration for {args.unit!r}")

    data = bytearray(args.input.read_bytes())
    target_data = args.target.read_bytes() if args.target else None
    total = 0
    try:
        for patch in patches:
            _, _, changed = apply_patch(data, patch, target_data,
                                        symbol_addresses)
            total += changed
            print(
                f"WEBFRANK {patch['function']}: "
                f"adjusted {changed} instruction atoms/fields"
            )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(data)
    except BaseException as err:
        try:
            marker.parent.mkdir(parents=True, exist_ok=True)
            marker.write_text(
                f"WEBFRANK {args.unit} refused at"
                f" {datetime.now(timezone.utc).isoformat(timespec='seconds')}:"
                f" {type(err).__name__}: {err}",
                encoding="utf-8")
        except OSError:
            pass
        raise
    # Written successfully: the object is current again, so the marker must
    # go. Leaving it would turn a one-off failure into a permanent warning
    # nobody believes — which is how a real warning stops being read.
    clear_marker()
    print(f"WEBFRANK {args.unit}: {total} instruction atoms/fields total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
