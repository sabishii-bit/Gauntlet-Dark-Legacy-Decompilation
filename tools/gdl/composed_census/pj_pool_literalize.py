#!/usr/bin/env python3
"""PJ lane (run 55): turn `extern <T> lbl_ADDR` pool ghosts into literals.

An own-pool ghost is a TU importing its OWN compiler-generated pool datum as
`extern f32 lbl_803468B0;` instead of spelling the constant.  The ghost
keeps the datum out of our object, which is why the section stays
under-emitted and unclaimable (claim.law.symbolified-own-pool-literal-blocks
-tu-flip.20260831.v1).  This does the mechanical half of the fix: it reads
each datum's exact bytes out of the retail DOL, spells the shortest PLAIN
decimal (or C string) literal that round-trips to those bytes, rewrites every
whole-word reference, and deletes the declaration.

    python tools/gdl/composed_census/pj_pool_literalize.py \
        game/enemy/enemy --range 0x80346810 0x80346A70 --dry
    python tools/gdl/composed_census/pj_pool_literalize.py \
        game/game/gamemain 0x80346AD8 0x80346C04 --apply

Nothing is written without --apply.  THE EDIT IS NOT SAFE BY CONSTRUCTION:
per claim.law.PJ_replacing-an-extern-scalar-read-with-a-literal-is-not-
codegen-neutral-it-removes-a-reload-barrier.20260904.v1 a scalar conversion
can move instruction words (12 of 66 functions on one measured TU), so gate
every --apply with

    python tools/gdl/composed_census/pj_body_ab.py <unit>

and revert unless it reports zero moved words.  Two source shapes this tool
deliberately refuses to reason about, both measured in game/game/gamemain:
a `*(volatile T*)&lbl_X` load scaffold (rewritten to a plain read, since
`&literal` is not an lvalue and the compile fails outright) and a BLOB BASE
POINTER -- `char* fmt = lbl_80112370;` followed by `fmt + 284` indexes past
the NUL and is NOT a string ghost.  Screen every char symbol for offset
arithmetic before converting it; 2 of 8 candidates in one TU were of that
kind.

IMPORTABLE CORE: read_datum, spell_f32, spell_f64, spell_string (pure over
the retail DOL; they never build and importing has no effect).
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
from decimal import Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))

DECL = re.compile(
    r'^[ \t]*(?:DECL_SECT\("[^"]*"\)[ \t]*)?extern[ \t]+(?:const[ \t]+)?'
    r'(f32|f64|char)[ \t]+(lbl_[0-9A-Fa-f]{8})[ \t]*(\[[0-9]*\])?[ \t]*;'
    r'[^\n]*\n', re.M)

_SEGMENTS: list[tuple[int, int, int]] = []
_IMAGE = b''


def _load_image() -> None:
    global _IMAGE
    if _SEGMENTS:
        return
    for candidate in (os.path.join(ROOT, 'orig', 'GUNE5D', 'sys', 'main.dol'),
                      os.path.join(ROOT, 'build', 'GUNE5D',
                                   'main.retail.dol')):
        if os.path.exists(candidate):
            _IMAGE = open(candidate, 'rb').read()
            break
    else:
        raise SystemExit('no retail DOL found (orig/GUNE5D/sys/main.dol)')
    offsets = struct.unpack('>7I', _IMAGE[0x00:0x1C])
    doffsets = struct.unpack('>11I', _IMAGE[0x1C:0x48])
    addresses = struct.unpack('>7I', _IMAGE[0x48:0x64])
    daddresses = struct.unpack('>11I', _IMAGE[0x64:0x90])
    sizes = struct.unpack('>7I', _IMAGE[0x90:0xAC])
    dsizes = struct.unpack('>11I', _IMAGE[0xAC:0xD8])
    for off, addr, size in (list(zip(offsets, addresses, sizes))
                            + list(zip(doffsets, daddresses, dsizes))):
        if size:
            _SEGMENTS.append((addr, addr + size, off))


def read_datum(address: int, length: int) -> bytes:
    """*length* bytes at virtual *address* in the retail DOL."""
    _load_image()
    for start, end, offset in _SEGMENTS:
        if start <= address and address + length <= end:
            base = offset + address - start
            return _IMAGE[base:base + length]
    raise SystemExit('address 0x%08X is not in the DOL' % address)


def _plain(text: str) -> str:
    if 'e' not in text and 'E' not in text:
        return text if '.' in text else text + '.0'
    expanded = format(Decimal(text), 'f')
    return expanded if '.' in expanded else expanded + '.0'


def spell_f64(value: float) -> str:
    """Shortest plain decimal whose nearest double is exactly *value*."""
    text = '%.17g' % value
    for digits in range(1, 18):
        candidate = '%.*g' % (digits, value)
        if float(candidate) == value:
            text = candidate
            break
    expanded = _plain(text)
    return expanded if float(expanded) == value else text


def spell_f32(value: float) -> str:
    """Shortest plain decimal + `f` whose nearest float is exactly *value*."""
    bits = struct.pack('>f', value)
    text = '%.9g' % value
    for digits in range(1, 10):
        candidate = '%.*g' % (digits, value)
        if struct.pack('>f', float(candidate)) == bits:
            text = candidate
            break
    expanded = _plain(text)
    if struct.pack('>f', float(expanded)) != bits:
        expanded = text
    return expanded + 'f'


def spell_string(address: int) -> str:
    """The C literal for the NUL-terminated string at *address*.

    A declared dimension is deliberately ignored: `extern char x[8]` may
    hold a 5-byte string plus alignment padding, and spelling the padding
    into the literal is a silent value defect.
    """
    length = 0
    while read_datum(address + length, 1) != b'\x00':
        length += 1
        if length > 512:
            raise SystemExit('0x%08X: no NUL within 512 bytes' % address)
    out = []
    for byte in read_datum(address, length):
        if byte == 0x5C:
            out.append('\\\\')
        elif byte == 0x22:
            out.append('\\"')
        elif 0x20 <= byte < 0x7F:
            out.append(chr(byte))
        else:
            out.append('\\%03o' % byte)
    return '"%s"' % ''.join(out)


def literal_for(symbol: str, kind: str, dimension: str | None) -> str:
    address = int(symbol[4:], 16)
    if kind == 'f64':
        return spell_f64(struct.unpack('>d', read_datum(address, 8))[0])
    if kind == 'f32':
        return spell_f32(struct.unpack('>f', read_datum(address, 4))[0])
    del dimension
    return spell_string(address)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('unit', help='e.g. game/enemy/enemy')
    parser.add_argument('addresses', nargs='*',
                        help='0xADDR ... (omit with --range)')
    parser.add_argument('--range', nargs=2, metavar=('LO', 'HI'),
                        help='convert every declared symbol in [LO, HI)')
    parser.add_argument('--exclude', default='',
                        help='comma-separated addresses to leave alone')
    parser.add_argument('--apply', action='store_true',
                        help='write the file (default: report only)')
    args = parser.parse_args()

    unit = args.unit.replace('\\', '/')
    if unit.startswith('src/'):
        unit = unit[4:]
    if unit.endswith('.c') or unit.endswith('.cpp'):
        unit = unit.rsplit('.', 1)[0]
    source_rel = 'src/' + unit + '.c'
    source_abs = os.path.join(ROOT, source_rel)
    if not os.path.exists(source_abs):
        raise SystemExit('no such source: ' + source_rel)

    text = open(source_abs, encoding='utf-8', errors='replace').read()
    declared = {m.group(2): (m.group(1), m.group(3))
                for m in DECL.finditer(text)}
    skip = {'lbl_%08X' % int(a, 16)
            for a in args.exclude.split(',') if a.strip()}
    if args.range:
        low, high = int(args.range[0], 16), int(args.range[1], 16)
        wanted = [s for s in declared if low <= int(s[4:], 16) < high]
    else:
        wanted = ['lbl_%08X' % int(a, 16) for a in args.addresses]
    wanted = [s for s in sorted(set(wanted), key=lambda s: int(s[4:], 16))
              if s not in skip]
    if not wanted:
        raise SystemExit('nothing selected -- pass addresses or --range')

    converted, sites = [], 0
    for symbol in wanted:
        if symbol not in declared:
            print('  SKIP %s: not declared in %s' % (symbol, source_rel))
            continue
        kind, dimension = declared[symbol]
        literal = literal_for(symbol, kind, dimension)
        if literal.startswith('-'):
            literal = '(%s)' % literal
        text = re.sub(r'\*\(volatile\s+(?:f32|f64)\s*\*\)\s*&\s*'
                      + symbol + r'\b', symbol, text)
        stripped = DECL.sub(
            lambda m: '' if m.group(2) == symbol else m.group(0), text)
        count = len(re.findall(r'\b' + symbol + r'\b', stripped))
        text = re.sub(r'\b' + symbol + r'\b',
                      literal.replace('\\', '\\\\'), stripped)
        converted.append((symbol, kind, literal, count))
        sites += count

    for symbol, kind, literal, count in converted:
        print('  %s %-4s -> %-28s %d site(s)' % (symbol, kind, literal,
                                                 count))
    print('%d symbol(s), %d site(s)' % (len(converted), sites))
    if not args.apply:
        print('(report only -- pass --apply to write %s)' % source_rel)
        return 0
    with open(source_abs, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(text)
    print('[%s REWRITTEN ON DISK -- RE-READ IT BEFORE YOUR NEXT EDIT]'
          % source_rel)
    print('NOW GATE IT: python tools/gdl/composed_census/pj_body_ab.py %s'
          % unit)
    return 0


if __name__ == '__main__':
    sys.exit(main())
