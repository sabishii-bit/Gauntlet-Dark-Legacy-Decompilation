"""Audit explicit undefined-symbol renames against unchanged raw TU output.

This is a diagnostic, not a linker alias or build transform. Target relocation
witnesses establish that each destination is actually referenced by the retail
TU. Caller ABI and datum extent still require separate manual target evidence.
"""
import argparse
from collections import Counter
import copy
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture, compare
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory


def audit(before, after, mapping, target_functions):
    if not mapping or len(set(mapping.values())) != len(mapping) or set(mapping) & set(mapping.values()):
        raise ValueError('require nonempty one-to-one disjoint import rename mapping')
    normalized = copy.deepcopy(before)
    counts = {}
    for old, new in mapping.items():
        initial = [s for s in before['symbols'] if s['name'] == old]
        final = [s for s in after['symbols'] if s['name'] == new]
        if len(initial) != 1 or len(final) != 1 or any(s['section'] != '' or s['binding'] != 1 for s in initial + final):
            raise ValueError('rename requires unique GLOBAL undefined symbols: ' + old)
        if any(s['name'] == new for s in before['symbols']) or any(s['name'] == old for s in after['symbols']):
            raise ValueError('rename would merge symbols or leave stale imports: ' + old)
        source_sites = [(section, r[0], r[1], r[3]) for section, rels in before['relocations'].items() for r in rels if r[2] == old]
        witnesses = [(name, r[0], r[1], r[3]) for name, fn in target_functions.items() for r in fn['relocations'] if r[2] == new]
        if not source_sites or not witnesses:
            raise ValueError('missing source sites or target relocation witness: ' + old)
        for function, body in before['functions'].items():
            sites = Counter((r[1], r[3]) for r in body['relocations'] if r[2] == old)
            if sites:
                target_sites = Counter((r[1], r[3]) for r in target_functions.get(function, {}).get('relocations', []) if r[2] == new)
                if sites != target_sites:
                    raise ValueError('target per-function import multiplicity/type/addend differs: ' + function + ':' + old)
        counts[old] = dict(destination=new, source_sites=source_sites, target_witnesses=witnesses)
    for symbol in normalized['symbols']:
        symbol['name'] = mapping.get(symbol['name'], symbol['name'])
    for rels in normalized['relocations'].values():
        for relocation in rels:
            relocation[2] = mapping.get(relocation[2], relocation[2])
    for function in normalized['functions'].values():
        for relocation in function['relocations']:
            relocation[2] = mapping.get(relocation[2], relocation[2])
    result = compare(normalized, after, [])
    result['import_renames'] = counts
    result['target_scope'] = 'Each affected function has equal target import multiplicity/type/addend; this alone does not prove source ABI or datum extent'
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('unit')
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--rename', action='append', required=True, metavar='OLD=NEW')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()) or not output.name.startswith('r68_interfaces_') or output.suffix != '.json':
        parser.error('--out must name build/**/r68_interfaces_*.json')
    try:
        pairs = [value.split('=') for value in args.rename]
        if any(len(p) != 2 or not all(p) for p in pairs) or len({p[0] for p in pairs}) != len(pairs):
            raise ValueError('invalid or duplicate rename option')
        target_path = ROOT / 'build/GUNE5D/obj' / (args.unit + '.o')
        target, _ = inventory(target_path)
        current = capture(args.unit)
        result = audit(json.loads(args.before.read_text()), current, dict(pairs), target)
        result['current'] = current
        result['target_object'] = str(target_path.relative_to(ROOT))
    except (OSError, ValueError, KeyError) as error:
        result = dict(status='UNRESOLVED', error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'current'}, indent=2))
    print('written', output)
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
