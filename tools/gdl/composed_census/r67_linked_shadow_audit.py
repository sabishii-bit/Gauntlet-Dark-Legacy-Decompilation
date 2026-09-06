"""Adjudicate source-linked shadow-score demotions against actual linked bytes.

Read-only: consumes a successful reconstruction_preflight plus current Ninja
provenance. A PASS is a linked-function byte certificate for this bounded
cohort, not proof of original source, raw compiler equivalence, or mod safety.
Unlinked demotions stay unresolved. Uses shared ELF/DOL and relocation readers;
does not reproduce a linker or normalize away any differing linked byte.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/gdl'))
import build_provenance
import fndiff
import webfrank


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def choose_rows(preflight):
    if preflight.get('status') != 'PASS' or preflight.get('schema_version') != 1:
        raise ValueError('requires successful schema-1 preflight')
    comparison = preflight['relocation_comparison']
    rows = [r for r in comparison['changes']
            if r['source_linked'] and r['lost_report_100']]
    if len(rows) != comparison['source_linked_demotions'] or not rows:
        raise ValueError('missing/inconsistent linked-demotion population')
    keys = [(r['unit'], r['function']) for r in rows]
    if len(set(keys)) != len(keys):
        raise ValueError('duplicate linked-demotion row')
    return rows


def exact_linked_bytes(target, dol, elf, size):
    if size <= 0 or any(x is None or len(x) != size for x in (target, dol, elf)):
        return 'UNRESOLVED'
    return 'PASS' if target == dol == elf else 'FAIL'


def elf_body(blob, sections, symbol):
    shoff = struct.unpack_from('>I', blob, 0x20)[0]
    shsize = struct.unpack_from('>H', blob, 0x2e)[0]
    section = sections[symbol.section_index]
    address = struct.unpack_from('>I', blob, shoff + shsize * section.index + 12)[0]
    offset = symbol.value - address
    if offset < 0 or offset + symbol.size > section.size:
        raise ValueError('linked ELF function outside its section')
    return blob[section.offset + offset: section.offset + offset + symbol.size]


def audit(root, preflight):
    rows = choose_rows(preflight)
    for path, expected in preflight['input_fingerprints'].items():
        if digest(path) != expected:
            raise ValueError('preflight input changed: ' + path)
    provenance = build_provenance.collect_manifest(root)
    if provenance['status'] != 'PASS' or provenance['link_reconciliation']['status'] != 'PASS':
        raise ValueError('current link provenance did not reconcile')
    units = {u['report_name']: u for u in provenance['units']}
    linked = root / 'build/GUNE5D/main.elf'
    built = root / 'build/GUNE5D/main.dol'
    retail = root / 'orig/GUNE5D/sys/main.dol'
    fingerprints = {str(p): digest(p) for p in (linked, built, retail)}
    elf = linked.read_bytes()
    if elf[:6] != b'\x7fELF\x01\x02' or struct.unpack_from('>HH', elf, 16) != (2, 20):
        raise ValueError('requires linked big-endian ELF32 PowerPC executable')
    sections = webfrank._sections(elf)
    symbols = webfrank._symbols(elf, sections)
    target_image = webfrank.RetailImage(retail)
    built_image = webfrank.RetailImage(built)
    target_symbols = fndiff.symbol_table()
    results = []
    for row in rows:
        unit = units[row['unit']]
        if unit['linkage'] != 'source' or unit['linked_object'] != unit['source_object']:
            raise ValueError('report linked claim disagrees with actual link selection')
        fn = row['function']
        address = target_symbols[fn][1]
        size = row['size']
        matches = [s for s in symbols if s.name == fn and s.value == address and s.size == size]
        if len(matches) != 1:
            raise ValueError('linked symbol not uniquely bound by name/address/size: ' + fn)
        tbytes, obytes = target_image.read(address, size), built_image.read(address, size)
        ebytes = elf_body(elf, sections, matches[0])
        state = exact_linked_bytes(tbytes, obytes, ebytes, size)
        details = {}
        for side, path in [('target', unit['extracted_object']), ('source_link_input', unit['linked_object'])]:
            blob = (root / path).read_bytes()
            secs = webfrank._sections(blob)
            sym = webfrank._find_symbol(blob, secs, fn)
            rels = webfrank._function_text_relocations_full(blob, secs, sym.section_index,
                                                          sym.value, sym.value + sym.size)
            details[side] = {'object': path, 'sha256': digest(root / path),
                'function_size': sym.size, 'relocations': [
                    {'offset': at, 'type': kind, 'symbol': name, 'addend': addend,
                     'linked_instruction': obytes[at & ~3:(at & ~3) + 4].hex()}
                    for at, (kind, name, addend) in sorted(rels.items())]}
        results.append({**row, 'status': state, 'target_address': hex(address),
            'linked_bytes': size, 'byte_sha256': hashlib.sha256(obytes).hexdigest(),
            'disposition': 'LINKED_BYTES_EXACT_OBJECT_SCORE_DEMOTION' if state == 'PASS' else state,
            'provenance': details, 'pipeline_rules': [s['rule'] for s in unit['pipeline']],
            'function_postprocessor': unit['function_postprocessors'].get(fn),
            'source_semantics': 'UNRESOLVED; linked equality is not original-source recovery'})
    for path, expected in {**preflight['input_fingerprints'], **fingerprints}.items():
        if digest(path) != expected:
            raise ValueError('input changed during audit: ' + path)
    counts = Counter(r['status'] for r in results)
    return {'schema_version': 1, 'status': 'FAIL' if counts['FAIL'] else 'PASS' if counts['PASS'] == len(results) else 'UNRESOLVED',
        'scope': 'source-linked shadow demotions; final ELF and DOL bytes at exact target function addresses',
        'rows': results, 'counts': dict(counts), 'bytes_compared': sum(r['linked_bytes'] for r in results),
        'unlinked_demotions': preflight['relocation_comparison']['lost_report_100'] - len(results),
        'unlinked_status': 'UNRESOLVED; not adjudicated by a matching DOL using extracted fallbacks',
        'limitations': ['Selected source object and linked function identity are checked; weak-definition winner provenance is not reconstructed.',
                        'Exact final bytes do not certify raw compiler output, original source form or editable builds.'],
        'input_fingerprints': fingerprints}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preflight', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/GUNE5D/r67_linked_shadow_audit.json')
    args = parser.parse_args(argv)
    try:
        result = audit(ROOT, json.loads(args.preflight.read_text(encoding='utf-8')))
    except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
        result = {'schema_version': 1, 'status': 'FAIL', 'error': str(error)}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in {'rows', 'input_fingerprints'}}, indent=2))
    return 0 if result['status'] == 'PASS' else 2 if result['status'] == 'UNRESOLVED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
