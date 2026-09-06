"""Capture/audit one source-native sysClearFlags pin retirement.

Run once before changing the private gSysFlags type to save the complete raw
and processed baseline, then with --before after the normal build. This does
not patch objects or source. It requires fresh actual-Ninja compiler fidelity,
one precise source change, removal of exactly one rule, an unchanged complete
processed object, and all raw differences confined to the now-exact function.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.gdl.composed_census.r68_interfaces_raw_control import capture

UNIT, FUNCTION = 'game/sys/sysservice', 'sysClearFlags'
OLD, NEW = 'static u32 gSysFlags;', 'static unsigned int gSysFlags;'
STOCK_SHA256 = '0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require_stock_hash(digest):
    if digest != STOCK_SHA256:
        raise ValueError('unsupported compiler binary: expected measured stock GC/1.2.5')


def source_and_config(before, after):
    old_source = before['source'].replace('\r\n', '\n')
    new_source = after['source'].replace('\r\n', '\n')
    if old_source.count(OLD) != 1 or old_source.replace(OLD, NEW) != new_source:
        raise ValueError('source change is not precisely the private flag type')
    expected = copy.deepcopy(before['config'])
    rules = expected['units'][UNIT]
    if sum(rule.get('function') == FUNCTION for rule in rules) != 1:
        raise ValueError('baseline does not contain one retirement rule')
    expected['units'][UNIT] = [rule for rule in rules if rule.get('function') != FUNCTION]
    if expected != after['config']:
        raise ValueError('configuration change is not exactly one rule removal')


def byte_certificate(before, after, start, size, target):
    if (not before or len(before) != len(after) or start < 0 or size != 16
            or start + size > len(after) or len(target) != size):
        raise ValueError('invalid whole-object/function extent')
    if before[:start] != after[:start] or before[start+size:] != after[start+size:]:
        raise ValueError('raw bytes outside sysClearFlags changed')
    if after[start:start+size] != target:
        raise ValueError('raw sysClearFlags is not target-exact')
    differences = [i for i in range(0, size, 4) if before[start+i:start+i+4] != target[i:i+4]]
    if differences != [0, 4]:
        raise ValueError('baseline residual is not the measured two-word load/AND difference')
    return dict(target_instructions=4, raw_instructions=4, changed_word_offsets=differences,
                before_words=before[start:start+size].hex(), after_words=target.hex(),
                outside_raw_bytes_unchanged=len(after)-size)


def canonical_relocations(rows):
    return sorted((off & ~3 if kind == 109 else off, kind, name, addend)
                  for off, kind, name, addend in rows)


def validate_snapshot(value):
    raw = value['raw']
    if (value.get('schema_version') != 1 or raw.get('schema_version') != 1
            or raw.get('fidelity') is not True or raw.get('unit') != UNIT
            or raw.get('compiler') != 'GC/1.2.5'):
        raise ValueError('faithful stock-compiler sysservice schema-1 snapshot required')
    if sha(bytes.fromhex(value['raw_bytes'])) != raw['raw_sha256']:
        raise ValueError('raw snapshot bytes/hash disagree')
    source = value['source'].replace('\r\n', '\n')
    if raw['source_sha256'] not in {sha(source.encode()), sha(source.replace('\n', '\r\n').encode())}:
        raise ValueError('source snapshot bytes/hash disagree')
    if len(raw['functions']) != 17 or FUNCTION not in raw['functions']:
        raise ValueError('expected complete 17-function roster')


def snapshot():
    compiler_path = ROOT / 'build/compilers/GC/1.2.5/mwcceppc.exe'
    compiler_sha256 = sha(compiler_path.read_bytes())
    require_stock_hash(compiler_sha256)
    raw = capture(UNIT)
    if sha(compiler_path.read_bytes()) != compiler_sha256:
        raise ValueError('compiler changed during capture')
    raw_path = ROOT / raw['raw_object']
    processed_path = ROOT / 'build/GUNE5D/src/game/sys/sysservice.o'
    target_path = ROOT / 'build/GUNE5D/obj/game/sys/sysservice.o'
    return dict(schema_version=1, compiler_sha256=compiler_sha256, raw=raw, raw_bytes=raw_path.read_bytes().hex(),
                processed_bytes=processed_path.read_bytes().hex(),
                source=(ROOT / 'src/game/sys/sysservice.c').read_text(),
                config=json.loads((ROOT / 'config/GUNE5D/webfrank.json').read_text()),
                target_sha256=sha(target_path.read_bytes()))


def audit(before, after):
    validate_snapshot(before)
    validate_snapshot(after)
    source_and_config(before, after)
    a, b = before['raw'], after['raw']
    for key in ('unit', 'compiler', 'flags', 'relocations', 'symbols'):
        if a[key] != b[key]:
            raise ValueError('raw compiler/relocation/symbol identity changed: ' + key)
    if (a['functions'].keys() != b['functions'].keys() or len(a['functions']) != 17
            or any(a['functions'][name] != b['functions'][name] for name in a['functions'] if name != FUNCTION)):
        raise ValueError('raw sibling roster, bodies, layout or relocations changed')
    if (before['target_sha256'] != after['target_sha256']
            or before['processed_bytes'] != after['processed_bytes']):
        raise ValueError('target or complete processed baseline changed')
    raw_path = ROOT / b['raw_object']
    target_path = ROOT / 'build/GUNE5D/obj/game/sys/sysservice.o'
    if (raw_path.read_bytes() != bytes.fromhex(after['raw_bytes'])
            or sha(target_path.read_bytes()) != after['target_sha256']):
        raise ValueError('live raw/target objects changed after capture')
    elf = Elf(str(raw_path))
    sections = [header for i, header in enumerate(elf.sh) if elf.names[i] == '.text']
    if len(sections) != 1:
        raise ValueError('raw .text section is ambiguous')
    function = b['functions'][FUNCTION]
    target, _ = inventory(target_path)
    target_function = target[FUNCTION]
    if canonical_relocations(function['relocations']) != canonical_relocations(target_function['relocations']):
        raise ValueError('raw function target relocation bindings differ')
    certificate = byte_certificate(bytes.fromhex(before['raw_bytes']), bytes.fromhex(after['raw_bytes']),
                                   sections[0][4] + function['offset'], function['size'],
                                   bytes.fromhex(target_function['body']))
    return dict(schema_version=1, status='PASS', function=FUNCTION, compiler=b['compiler'], flags=b['flags'],
                current_compiler_sha256=after['compiler_sha256'],
                raw_siblings_unchanged=16, raw_functions=17, complete_object=certificate,
                before_raw_sha256=a['raw_sha256'], after_raw_sha256=b['raw_sha256'],
                processed_sha256=sha(bytes.fromhex(after['processed_bytes'])),
                target_sha256=after['target_sha256'], source_sha256=b['source_sha256'],
                remaining_rules=[rule['function'] for rule in after['config']['units'][UNIT]],
                limitations=['Current target/callee names are reconstructed project bindings.',
                             'This proves a natural source alternative, not unique original type provenance.',
                             'The private global-only change preserves the public u32 API.',
                             'A separate full Ninja/link checksum gate remains required.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--before', type=Path)
    args = parser.parse_args()
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()) or not output.name.startswith('r70_sysservice_'):
        parser.error('output must be a lane-prefixed generated artifact under build/')
    after = snapshot()
    result = audit(json.loads(args.before.read_text()), after) if args.before else after
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result if args.before else dict(status='BASELINE_CAPTURED', raw_sha256=after['raw']['raw_sha256'],
                                                   functions=len(after['raw']['functions']), out=str(output))))


if __name__ == '__main__':
    main()
