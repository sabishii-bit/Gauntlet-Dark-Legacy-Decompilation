"""Capture a faithful raw TU and audit a visibility-only source repair.

Only configured raw compiler objects are read; fresh compilation must reproduce
the complete object before a snapshot is trusted. Compare allows only explicit
LOCAL-to-GLOBAL function binding changes, never body, layout, datum, relocation
or exception-table changes. Output/scratch artifacts remain under build/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory


def sha(data):
    return hashlib.sha256(data).hexdigest()


def capture(unit):
    edge = cv.read_edges()[unit]
    source_path, raw_path = ROOT / edge['src'], ROOT / edge['body_o']
    source, raw = source_path.read_bytes(), raw_path.read_bytes()
    folder = Path(tempfile.mkdtemp(prefix='r68_interfaces_raw_', dir=ROOT / 'build'))
    trial = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], folder / 'control.o', folder)
    if not obj or error:
        raise ValueError(error or 'no fresh compiler object')
    if obj.read_bytes() != raw:
        raise ValueError('fresh complete compiler output differs from active raw object')
    functions, data = inventory(obj)
    elf = Elf(str(obj))
    sections = {}
    relocations = {}
    for i, header in enumerate(elf.sh):
        if header[2] & 2:
            sections[elf.names[i]] = dict(type=header[1], flags=header[2], size=header[5], alignment=header[8],
                                         bytes=data.get(elf.names[i]))
        if header[1] == 4:
            _, entries = elf.relas(elf.names[i])
            relocations[elf.names[header[7]]] = [(off, info & 255, elf.symname(info >> 8).decode(), add)
                                               for off, info, add in entries]
    symbols = []
    for i in range(elf.symcount):
        symbol = elf.sym(i)
        section = elf.names[symbol[5]] if symbol[5] < len(elf.names) else symbol[5]
        symbols.append(dict(name=elf.symname(i).decode(), value=symbol[1], size=symbol[2],
                            type=symbol[3] & 15, binding=symbol[3] >> 4, other=symbol[4], section=section))
    if source_path.read_bytes() != source or raw_path.read_bytes() != raw:
        raise ValueError('production source/raw changed during capture')
    snapshot = dict(schema_version=1, unit=unit, fidelity=True, source_sha256=sha(source), raw_sha256=sha(raw),
                compiler=edge['mw'], flags=edge['cflags'], raw_object=edge['body_o'],
                commands=trial['_command_trace'], functions=functions, sections=sections,
                relocations=relocations, symbols=symbols)
    return json.loads(json.dumps(snapshot))


def compare(before, after, exports):
    if not before.get('fidelity') or not after.get('fidelity'):
        raise ValueError('comparison requires two faithful snapshots')
    for key in ('unit', 'compiler', 'flags'):
        if before[key] != after[key]:
            raise ValueError('changed control identity: ' + key)
    changes = []
    for key in ('sections', 'relocations'):
        if before[key] != after[key]:
            changes.append(key)
    a, b = before['functions'], after['functions']
    if a.keys() != b.keys() or not a:
        changes.append('function roster')
    for name in a.keys() & b.keys():
        for key in ('body', 'relocations', 'offset', 'size'):
            if a[name][key] != b[name][key]:
                changes.append(name + ':' + key)
        expected = (0, 1) if name in exports else (a[name]['binding'], a[name]['binding'])
        if (a[name]['binding'], b[name]['binding']) != expected:
            changes.append(name + ':binding')
    if not set(exports) <= a.keys() & b.keys():
        changes.append('missing requested export')
    normalized = []
    for symbol in after['symbols']:
        symbol = dict(symbol)
        if symbol['name'] in exports and symbol['type'] == 2 and symbol['section'] != '':
            symbol['binding'] = 0
        normalized.append(symbol)
    if sorted(json.dumps(s, sort_keys=True) for s in before['symbols']) != sorted(json.dumps(s, sort_keys=True) for s in normalized):
        changes.append('other symbol changes')
    return dict(status='PASS' if not changes else 'FAIL', changes=changes, unit=before['unit'],
                functions=len(a), allocated_sections=len(before['sections']),
                relocations=sum(len(v) for v in before['relocations'].values()),
                exports={name: dict(before=a.get(name, {}).get('binding'), after=b.get(name, {}).get('binding'))
                         for name in exports}, before_raw_sha256=before['raw_sha256'], after_raw_sha256=after['raw_sha256'])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('unit')
    parser.add_argument('--before', type=Path)
    parser.add_argument('--exports', nargs='+', default=[])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()) or not output.name.startswith('r68_interfaces_') or output.suffix != '.json':
        parser.error('--out must name build/**/r68_interfaces_*.json')
    result = dict(status='UNRESOLVED')
    try:
        current = capture(args.unit)
        if args.before:
            previous = json.loads(args.before.read_text())
            result = compare(previous, current, args.exports)
            result['current'] = current
        else:
            result = current
            result['status'] = 'PASS'
    except (OSError, ValueError, KeyError) as error:
        result = dict(status='UNRESOLVED', error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('current', 'symbols', 'functions', 'sections', 'relocations', 'commands')
                      or k in ('functions', 'relocations') and isinstance(v, int)}, indent=2))
    print('written', output)
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
