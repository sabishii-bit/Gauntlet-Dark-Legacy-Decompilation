"""Inventory the reconstructed options storage envelope and actual object users.

Read-only. Target names/relocations are DTK reconstructions, not historical
symbol linkage or proof of an original C aggregate. Target population comes
only from a current build_edges manifest, never an on-disk object glob. Missing
configured raw objects are explicit; an inventory is not ownership closure.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe
from tools.gdl.composed_census.r68_interfaces_raw_control import capture, compare

BASE, END = 0x80274E00, 0x80274EA0
PARTS = {
    'optglobals': (BASE, 0x40),
    'optionsStack': (BASE + 0x40, 0x10),
    'optionsAudioAndPrefs': (BASE + 0x50, 0x30),
    'optionsAudioAndPrefs30': (BASE + 0x80, 0x20),
}


def active_targets(snapshot, ninja_bytes, root):
    """Resolve exactly the active extracted-object set, refusing ambiguity."""
    if (not isinstance(snapshot, dict) or type(snapshot.get('schema_version')) is not int
            or snapshot['schema_version'] != 1 or snapshot.get('version') != 'GUNE5D'
            or snapshot.get('ninja_sha256') != hashlib.sha256(ninja_bytes).hexdigest()):
        raise ValueError('active build manifest is unsupported or stale against build.ninja')
    units = snapshot.get('units')
    if not isinstance(units, list) or not units:
        raise ValueError('active target population is empty or malformed')
    target_dir = (root / 'build/GUNE5D/obj').resolve()
    targets, names, paths = {}, set(), set()
    for unit in units:
        if not isinstance(unit, dict):
            raise ValueError('malformed active target row')
        name, value = unit.get('name'), unit.get('extracted_object')
        if (not isinstance(name, str) or not name.strip() or name != name.strip()
                or not isinstance(value, str) or not value.strip() or value != value.strip()):
            raise ValueError('active target name/path is missing or malformed')
        key = re.sub(r'\.(?:cpp|c|s)$', '', name.replace('\\', '/').removeprefix('src/'))
        path = (root / value.replace('\\', '/')).resolve()
        if not path.is_relative_to(target_dir) or path.suffix.lower() != '.o':
            raise ValueError('active target path escapes the expected object directory: ' + value)
        name_key, path_key = key.casefold(), path.as_posix().casefold()
        if not key or name_key in names or path_key in paths:
            raise ValueError('duplicate/ambiguous active target name or path')
        if not path.is_file():
            raise ValueError('missing active target object: ' + value)
        names.add(name_key)
        paths.add(path_key)
        targets[key] = path
    return targets


def validate_layout(parts):
    if parts != PARTS:
        raise ValueError('reconstructed options symbol envelope changed')
    cursor = BASE
    for address, size in sorted(parts.values()):
        if address != cursor or size <= 0:
            raise ValueError('gap, overlap, or empty subobject')
        cursor += size
    if cursor != END:
        raise ValueError('options envelope extent changed')
    return {'start': BASE, 'end': END, 'size': END - BASE}


def read_layout(text):
    parts = {}
    for name in PARTS:
        rows = re.findall(r'^' + re.escape(name) +
                          r'\s*=\s*\.bss:0x([0-9a-fA-F]+);[^\n]*?size:0x([0-9a-fA-F]+)',
                          text, re.M)
        if len(rows) != 1:
            raise ValueError('missing or duplicate target map symbol: ' + name)
        parts[name] = tuple(int(v, 16) for v in rows[0])
    validate_layout(parts)
    return parts


def object_rows(path):
    elf = Elf(str(path))
    definitions, uses, functions = [], [], []
    for i in range(elf.symcount):
        symbol = elf.sym(i)
        name = elf.symname(i).decode()
        if symbol[3] & 15 == 2 and symbol[5] != 0:
            functions.append((symbol[5], symbol[1], symbol[2], name))
        if name in PARTS and symbol[5] != 0:
            definitions.append(dict(name=name, value=symbol[1], size=symbol[2],
                                    binding=symbol[3] >> 4,
                                    section=elf.names[symbol[5]]))
    for i, header in enumerate(elf.sh):
        if header[1] != 4:
            continue
        _, entries = elf.relas(elf.names[i])
        for offset, info, addend in entries:
            name = elf.symname(info >> 8).decode()
            if name not in PARTS:
                continue
            owners = [fn for section, start, size, fn in functions
                      if section == header[7] and start <= offset < start + size]
            uses.append(dict(name=name, section=elf.names[header[7]], offset=offset,
                             type=info & 255, addend=addend, functions=owners))
    return definitions, uses


def scan(paths):
    if not paths:
        raise ValueError('empty object population is not a census')
    rows, missing = [], []
    for unit, path in sorted(paths.items()):
        if not path.is_file():
            missing.append({'unit': unit, 'path': str(path)})
            continue
        definitions, uses = object_rows(path)
        if definitions or uses:
            rows.append(dict(unit=unit, path=path.relative_to(ROOT).as_posix(),
                             sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                             definitions=definitions, uses=uses))
    by_name = {name: {'units': sorted({row['unit'] for row in rows
                                     if any(use['name'] == name for use in row['uses'])}),
                      'relocations': sum(use['name'] == name for row in rows for use in row['uses'])}
               for name in PARTS}
    return dict(configured_objects=len(paths), missing=missing, rows=rows, consumers=by_name)


def validate_retained(rows):
    definitions = [(row['unit'], definition) for row in rows for definition in row['definitions']]
    if len(definitions) != len(PARTS) or {definition['name'] for _, definition in definitions} != set(PARTS):
        raise ValueError('missing or duplicate retained target definition')
    if len({unit for unit, _ in definitions}) != 1:
        raise ValueError('target envelope is no longer one retained allocation')
    origin = next(d['value'] for _, d in definitions if d['name'] == 'optglobals')
    for _, definition in definitions:
        address, size = PARTS[definition['name']]
        if (definition['size'] != size or definition['value'] != origin + address - BASE
                or definition['section'] != '.bss' or definition['binding'] != 1):
            raise ValueError('retained target layout/binding changed')
    return {'unit': definitions[0][0], 'section_offset': origin, 'size': END - BASE}


def audit_raw(snapshot):
    if snapshot.get('fidelity') is not True or snapshot.get('unit') != 'game/ui/options':
        raise ValueError('requires fresh faithful options compiler snapshot')
    definitions = [s for s in snapshot['symbols'] if s['name'] == 'optglobals' and s['section'] == '.bss']
    if len(definitions) != 1 or definitions[0]['size'] != END - BASE or definitions[0]['value'] != 0:
        raise ValueError('raw options allocation no longer covers the target envelope')
    bss = snapshot['sections']['.bss']
    if bss['type'] != 8 or bss['size'] != END - BASE:
        raise ValueError('unexpected raw BSS extent/type')
    return dict(symbol=definitions[0], section=bss, raw_sha256=snapshot['raw_sha256'],
                compiler=snapshot['compiler'], flags=snapshot['flags'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True)
    parser.add_argument('--before', help='optional faithful snapshot; require complete raw restoration')
    args = parser.parse_args()
    manifest_path, ninja_path = ROOT / 'build/GUNE5D/build_edges.json', ROOT / 'build.ninja'
    manifest_bytes, ninja_bytes = manifest_path.read_bytes(), ninja_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    target_paths = active_targets(manifest, ninja_bytes, ROOT)
    layout = read_layout((ROOT / 'config/GUNE5D/symbols.txt').read_text())
    snapshot = capture('game/ui/options')
    raw = audit_raw(snapshot)
    restoration = None
    if args.before:
        restoration = compare(json.loads(Path(args.before).read_text()), snapshot, [])
        if restoration['status'] != 'PASS':
            raise ValueError('raw options object is not fully restored: ' + str(restoration))
    target = scan(target_paths)
    if target['missing']:
        raise ValueError('active target object disappeared during census')
    retained = validate_retained(target['rows'])
    source = scan({unit: ROOT / edge['body_o'] for unit, edge in cv_probe.read_edges().items()})
    result = dict(schema_version=1, status='INVENTORIED', envelope=validate_layout(layout),
                  reconstructed_parts=layout, retained_target=retained, raw_options=raw, restoration=restoration,
                  target=target, configured_raw_source=source,
                  target_population=dict(source='build/GUNE5D/build_edges.json units[].extracted_object',
                                         manifest_sha256=hashlib.sha256(manifest_bytes).hexdigest(),
                                         ninja_sha256=manifest['ninja_sha256']),
                  limitations=['Target names and relocations are reconstructed, not historical linkage proof.',
                               'C aggregate identity is a source hypothesis; equal extents do not prove ownership.',
                               'Consumer census covers these exact symbol names, not unnamed section references or pointer flow.',
                               'Other source objects are inventoried as built, not independently recompiled.',
                               'No config/source mutation and no ownership or exact-source closure is certified.'])
    if manifest_path.read_bytes() != manifest_bytes or ninja_path.read_bytes() != ninja_bytes:
        raise ValueError('active build manifest/Ninja changed during census')
    Path(args.out).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('status', 'envelope', 'restoration')}))
    print(json.dumps({'target': target['consumers'], 'source': source['consumers'],
                      'missing_raw_objects': len(source['missing'])}))


if __name__ == '__main__':
    main()
