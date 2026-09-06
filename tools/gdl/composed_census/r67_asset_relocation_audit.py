"""Audit the verified GUNE5D embedded asset range, without interpreting its data.

Read-only except the requested JSON report. Classification as serialized data
comes from claim.embedded-static-payload-range-verified.20260831.v1 and the
examined relocation neighborhoods, not from hashing or alignment alone. Under
that classification, these inferred relocations are not native pointers.
Compare the entire payload at its actual linked address, including editable
builds where that address moves. This is not a source or gameplay certificate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/gdl'))
import webfrank

START, END = 0x80129734, 0x80238290
SIZE = END - START
PAYLOAD_SHA256 = '3ac9bbaaf14dffa0ce2dfc78db275bb566b1635cf4b903c17e45fa8a880049f3'
RETAIL_SHA1 = '7cba77aa496eb0fc5ffec60efd9680aa9635d679'
ANCHORS = {'lbl_80129740': 12, 'gDefaultFontData': 0x80237C60 - START}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked_payload(data):
    if data is None or len(data) != SIZE or sha(data) != PAYLOAD_SHA256:
        raise ValueError('retail payload does not match the verified range/hash')
    return data


def anchor_start(symbols):
    """Two independently named consumers must retain their relative positions."""
    starts, indexes = set(), set()
    for name, offset in ANCHORS.items():
        matches = [s for s in symbols if s.name == name and s.section_index != 0]
        if len(matches) != 1:
            raise ValueError('payload anchor missing or ambiguous: ' + name)
        starts.add(matches[0].value - offset)
        indexes.add(matches[0].section_index)
    if len(starts) != 1 or len(indexes) != 1 or min(starts) < 0:
        raise ValueError('payload anchor positions disagree')
    return starts.pop(), indexes.pop()


def compare_payload(expected, actual, linked_start):
    if len(expected) != SIZE or actual is None or len(actual) != SIZE:
        raise ValueError('missing/truncated payload cannot be compared')
    changes = []
    for at in range(0, SIZE, 4):
        if expected[at:at + 4] != actual[at:at + 4]:
            changes.append({'target_address': hex(START + at),
                'linked_address': hex(linked_start + at),
                'expected': expected[at:at + 4].hex(), 'actual': actual[at:at + 4].hex()})
    return {'status': 'PASS' if not changes else 'FAIL', 'length': SIZE,
        'linked_start': hex(linked_start), 'sha256': sha(actual),
        'changed_bytes': sum(a != b for a, b in zip(expected, actual)),
        'changed_words': len(changes), 'changes': changes}


def read_elf(path, executable):
    blob = path.read_bytes()
    if (blob[:6] != b'\x7fELF\x01\x02' or
            struct.unpack_from('>HH', blob, 16) != (2 if executable else 1, 20)):
        raise ValueError('expected big-endian ELF32 PowerPC ' + ('executable' if executable else 'object'))
    sections = webfrank._sections(blob)
    return blob, sections, webfrank._symbols(blob, sections)


def linked_payload(path):
    blob, sections, symbols = read_elf(path, True)
    start, index = anchor_start(symbols)
    section = sections[index]
    shoff = struct.unpack_from('>I', blob, 0x20)[0]
    shsize = struct.unpack_from('>H', blob, 0x2e)[0]
    base = struct.unpack_from('>I', blob, shoff + shsize * index + 12)[0]
    flags = struct.unpack_from('>I', blob, shoff + shsize * index + 8)[0]
    offset = start - base
    if section.section_type != 1 or not flags & 2 or offset < 0 or offset + SIZE > section.size:
        raise ValueError('linked payload outside allocated byte section')
    return start, blob[section.offset + offset:section.offset + offset + SIZE]


def object_relocations(path, expected):
    blob, sections, symbols = read_elf(path, False)
    start, index = anchor_start(symbols)
    section = sections[index]
    if start != 0 or section.name != '.data' or section.size != SIZE:
        raise ValueError('extracted payload unit does not match verified boundaries')
    if any(s.section_type == 9 and s.info == index for s in sections):
        raise ValueError('implicit-addend relocation form is not modeled')
    rels = webfrank._function_text_relocations_full(blob, sections, index, 0, SIZE)
    count = 0
    for table in sections:
        if table.section_type == webfrank.SHT_RELA and table.info == index:
            if table.entry_size not in (0, 12) or table.size % 12:
                raise ValueError('unexpected relocation table layout')
            count += table.size // 12
    if count != len(rels):
        raise ValueError('duplicate/out-of-range relocation entries')
    rows = []
    masked = bytearray(blob[section.offset:section.offset + SIZE])
    for offset, (kind, name, addend) in sorted(rels.items()):
        if kind != 1 or offset % 4 or offset + 4 > SIZE:
            raise ValueError('unexpected asset relocation width/alignment')
        rows.append({'offset': hex(offset), 'source': hex(START + offset),
            'type': 'R_PPC_ADDR32', 'symbol': name, 'addend': addend,
            'original_word': expected[offset:offset + 4].hex(),
            'neighbourhood_start': hex(START + max(0, offset - 8)),
            'neighbourhood': expected[max(0, offset - 8):min(SIZE, offset + 12)].hex()})
        masked[offset:offset + 4] = expected[offset:offset + 4]
    return rows, bytes(masked) == expected


def audit(retail, obj, elf, dol=None):
    paths = [retail, obj, elf] + ([dol] if dol else [])
    fingerprints = {str(p.resolve()): sha(p.read_bytes()) for p in paths}
    if hashlib.sha1(retail.read_bytes()).hexdigest() != RETAIL_SHA1:
        raise ValueError('wrong original GUNE5D DOL')
    expected = checked_payload(webfrank.RetailImage(retail).read(START, SIZE))
    rows, nonrelocated_exact = object_relocations(obj, expected)
    start, actual = linked_payload(elf)
    comparisons = {'elf': compare_payload(expected, actual, start)}
    if dol:
        comparisons['dol'] = compare_payload(expected, webfrank.RetailImage(dol).read(start, SIZE), start)
    for path, value in fingerprints.items():
        if sha(Path(path).read_bytes()) != value:
            raise ValueError('input changed during asset audit: ' + path)
    return {'schema_version': 1,
        'status': 'PASS' if not rows and nonrelocated_exact and all(
            r['status'] == 'PASS' for r in comparisons.values()) else 'FAIL',
        'range': [hex(START), hex(END)], 'target_sha256': PAYLOAD_SHA256,
        'classification_basis': 'Inherited asset-range evidence plus inspected relocation neighborhoods; hash verifies identity, not absence of native pointers.',
        'inferred_relocations': len(rows), 'relocations': rows,
        'object_nonrelocated_bytes_exact': nonrelocated_exact,
        'comparisons': comparisons, 'input_fingerprints': fingerprints,
        'limits': ['Only the verified hash-identified embedded asset range is classified.',
                   'No runtime gameplay, source ownership or raw compiler equivalence claim.']}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--retail', type=Path, default=ROOT / 'orig/GUNE5D/sys/main.dol')
    parser.add_argument('--object', type=Path, default=ROOT / 'build/GUNE5D/obj/auto_07_80129734_data.o')
    parser.add_argument('--elf', type=Path, default=ROOT / 'build/GUNE5D/main.elf')
    parser.add_argument('--dol', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args(argv)
    inputs = [args.retail, args.object, args.elf] + ([args.dol] if args.dol else [])
    if args.out.resolve() in {p.resolve() for p in inputs} or (
            args.out.exists() and any(args.out.samefile(p) for p in inputs if p.exists())):
        parser.error('report must not overwrite an input')
    try:
        result = audit(args.retail, args.object, args.elf, args.dol)
    except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
        result = {'schema_version': 1, 'status': 'FAIL', 'error': str(error)}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    summary = {k: v for k, v in result.items() if k not in {'relocations', 'input_fingerprints', 'comparisons'}}
    summary['comparisons'] = {k: {a: b for a, b in v.items() if a != 'changes'}
                              for k, v in result.get('comparisons', {}).items()}
    print(json.dumps(summary, indent=2))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
