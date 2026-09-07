"""Manual LIGHTS.OBJ split / InitLighting native-retirement certificate.

Recompile all three complete source snapshots with the actual Ninja edge.
Require complete raw ELF fidelity, unchanged remaining ITEMS function/data/EH
inventories, and exact LIGHTS allocated bytes, layout and final-address
relocation bindings. This tool never edits source, configuration or objects.
The full matching link/checksum is a separate required gate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical

ITEMS = 'game/world/items'
LIGHTS = 'game/world/lights'
FUNCTIONS = {'InitLighting': (0, 152), 'DoLighting': (152, 324)}
BASES = {'.text': 0x80067904, 'extab': 0x80006608,
         'extabindex': 0x8000A15C, '.sdata2': 0x80347188}
SIZES = {'.text': 476, 'extab': 16, 'extabindex': 24, '.sdata2': 4}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function_source(source, name):
    start = source.index('void ' + name + '(s32 flag)\n{')
    end = source.index('\n}\n', start) + 2
    return source[start:end]


def final_relocations(path, addresses, bases):
    elf = Elf(str(path))
    result = {name: [] for name, h in zip(elf.names, elf.sh) if h[2] & 2}
    for index, header in enumerate(elf.sh):
        if header[1] == 9:
            raise ValueError('implicit-addend relocations unsupported')
        if header[1] != 4:
            continue
        section = elf.names[header[7]]
        if section not in result:
            continue
        _, entries = elf.relas(elf.names[index])
        for offset, info, addend in entries:
            symbol = elf.sym(info >> 8)
            name = elf.symname(info >> 8).decode()
            if 0 < symbol[5] < len(elf.sh):
                owner = elf.names[symbol[5]]
                require(owner in bases, 'unmapped defining section: ' + owner)
                address = bases[owner] + symbol[1]
                if name in addresses:
                    require(addresses[name] == address, 'defined symbol address drift: ' + name)
            elif symbol[5] == 0:
                require(name in addresses, 'unresolved external: ' + name)
                address = addresses[name]
            else:
                raise ValueError('unsupported special symbol section: ' + name)
            kind = info & 255
            result[section].append([offset & ~3 if kind == 109 else offset,
                                    kind, address + addend])
    return {name: sorted(rows) for name, rows in result.items()}


def check_inventories(before, items, lights, target, source_relocs, target_relocs):
    require(set(before['functions']) - set(items['functions']) == set(FUNCTIONS)
            and len(before['functions']) == 53 and len(items['functions']) == 51,
            'unexpected ITEMS function partition')
    require(set(items['functions']) <= set(before['functions']), 'new ITEMS function')
    for name, function in items['functions'].items():
        for key in ('body', 'size', 'relocations', 'binding'):
            require(function[key] == before['functions'][name][key],
                    'remaining ITEMS function changed: ' + name + '/' + key)
        require(items['exception_records'].get(name) == before['exception_records'].get(name),
                'remaining ITEMS exception record changed: ' + name)
    for name in set(before['sections']) | set(items['sections']):
        if name not in ('.text', 'extab', 'extabindex'):
            require(before['sections'].get(name) == items['sections'].get(name),
                    'ITEMS nontext changed: ' + name)
    require(set(lights['functions']) == set(target['functions']) == set(FUNCTIONS),
            'unexpected LIGHTS function roster')
    for name, (offset, size) in FUNCTIONS.items():
        for inventory in (lights, target):
            fn = inventory['functions'][name]
            require((fn['offset'], fn['size'], fn['binding']) == (offset, size, 1),
                    'LIGHTS function layout changed: ' + name)
        require(lights['functions'][name]['body'] == target['functions'][name]['body'],
                'LIGHTS function body differs: ' + name)
    require(set(lights['sections']) == set(target['sections']) == set(SIZES),
            'unexpected LIGHTS allocated sections')
    for name, size in SIZES.items():
        a, b = lights['sections'][name], target['sections'][name]
        require(a['size'] == b['size'] == size and a['type'] == b['type'] == 1,
                'LIGHTS extent/type differs: ' + name)
        require(a['bytes'] == b['bytes'] and len(bytes.fromhex(a['bytes'])) == size,
                'LIGHTS section bytes differ: ' + name)
        for section in (a, b):
            align = section['alignment']
            require(align > 0 and align & (align - 1) == 0 and BASES[name] % align == 0,
                    'incompatible LIGHTS section alignment: ' + name)
    require(lights['sections']['.sdata2']['bytes'] == '00000000', 'wrong zero datum')
    require(source_relocs == target_relocs and set(source_relocs) == set(SIZES),
            'positional final-address relocation binding differs')
    # Exception record labels can differ between anonymous compiler pools and
    # the target. Their complete bytes and resolved relocations above decide.
    require(len(lights['exception_records']) == len(target['exception_records']) == 2,
            'LIGHTS exception coverage differs')
    return dict(status='PASS', retired_rules=1, instructions={'InitLighting': 38, 'DoLighting': 81},
                raw_target_words=0, unchanged_items_functions=51,
                items_nontext_and_function_EH_preserved=True,
                lights_allocated_bytes_and_final_bindings_exact=True)


def capture(before_dir):
    edges = cv.read_edges()
    items_edge, lights_edge = edges[ITEMS], edges[LIGHTS]
    old = {key: before_dir / ('r80_items_before_' + key + suffix)
           for key, suffix in (('source', '.c'), ('raw', '.bin'), ('rules', '.bin'))}
    require(all(path.is_file() for path in old.values()), 'missing preserved pre-split evidence')
    original_source = old['source'].read_text()
    new_source = (ROOT / lights_edge['src']).read_text()
    for name in FUNCTIONS:
        require(function_source(original_source, name).replace('sLightingZero', '0.0f')
                == function_source(new_source, name), 'lighting source differs beyond zero recovery: ' + name)
    old_summary = json.loads((before_dir / 'r80_items_results.json').read_text())
    for key, path in old.items():
        require(digest(path) == old_summary['hashes'][key], 'pre-split hash differs: ' + key)
    for key in ('mw', 'cflags', 'command_template', 'rule', 'extab_padding'):
        require(items_edge[key] == lights_edge[key] == old_summary['edge'][key],
                'compiler edge differs: ' + key)
    expected = json.loads(old['rules'].read_bytes())
    rules = expected['units'][ITEMS]
    require(sum(row['function'] == 'InitLighting' for row in rules) == 1, 'missing original pin')
    expected['units'][ITEMS] = [row for row in rules if row['function'] != 'InitLighting']
    require(expected == json.loads((ROOT / 'config/GUNE5D/webfrank.json').read_bytes()),
            'configuration differs beyond InitLighting retirement')
    paths = {'before': old['raw'], 'items': ROOT / items_edge['body_o'],
             'lights': ROOT / lights_edge['body_o'],
             'target': ROOT / ('build/GUNE5D/obj/' + LIGHTS + '.o')}
    hashes = {name: digest(path) for name, path in paths.items()}
    folder = Path(tempfile.mkdtemp(prefix='r80_lights_audit_', dir=ROOT / 'build'))
    traces = {}
    for name, edge in (('before', items_edge), ('items', items_edge), ('lights', lights_edge)):
        control = dict(edge, _command_trace=[])
        if name == 'before':
            source = folder / 'items.c'
            source.write_bytes(old['source'].read_bytes())
            control['src'] = source.relative_to(ROOT).as_posix()
        obj, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder / (name + '.o'), folder)
        require(not error and obj is not None, str(error))
        require(obj.read_bytes() == paths[name].read_bytes(), 'complete raw ELF fidelity failed: ' + name)
        traces[name] = control['_command_trace']
    addresses = {name: int(address, 16) for name, address in re.findall(
        r'(?m)^([^\s=]+) = [^:]+:0x([0-9A-Fa-f]+);',
        (ROOT / 'config/GUNE5D/symbols.txt').read_text())}
    require(addresses.get('sLightingZero') == BASES['.sdata2'], 'zero datum map changed')
    inventories = {name: canonical(path) for name, path in paths.items()}
    source_relocs = final_relocations(paths['lights'], addresses, BASES)
    target_relocs = final_relocations(paths['target'], addresses, BASES)
    result = check_inventories(**inventories, source_relocs=source_relocs, target_relocs=target_relocs)
    require(all(digest(path) == hashes[name] for name, path in paths.items()), 'input changed during audit')
    result.update(hashes=hashes, traces=traces, compiler=items_edge['mw'], flags=items_edge['cflags'],
                  final_relocations=source_relocs, artifact_folder=str(folder.relative_to(ROOT)),
                  limitations=['Manual certificate, not a build hook or a compiler-internal proof.',
                               'Full linked Ninja checksum gate is separately required.',
                               'The remaining ITEMS TU is not claimed target-exact.'])
    (folder / 'inventories.json').write_text(json.dumps(inventories, indent=2), encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-dir', type=Path, default=ROOT / 'build/r80_items_controls')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = capture(args.before_dir.resolve())
    args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in result.items() if key not in ('traces', 'final_relocations', 'flags')}))


if __name__ == '__main__':
    main()
