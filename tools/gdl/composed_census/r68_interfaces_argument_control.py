"""Narrow raw-output certificate for the repaired place_logic12 index argument.

Checks the producer-to-consumer instruction interval and whole-TU isolation.
This is not a general CFG, prototype or semantic equivalence verifier.
"""
import argparse
import copy
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture, compare
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory

FUNCTION = 'fn_800606FC'
PRODUCER = 'generate_enemy'
CONSUMER = 'place_logic12_800631AC'


def gpr_writes(word):
    """Fail closed outside forms needed by this measured interval."""
    op, rt, ra = word >> 26, (word >> 21) & 31, (word >> 16) & 31
    if op in (7, 8, 12, 13, 14, 15, 32, 34, 40, 42):
        return {rt}
    if op in (24, 25, 26, 27, 28, 29):
        return {ra}
    if op in (33, 35, 41, 43):
        return {rt, ra}
    if op in (37, 39, 45, 49, 51, 53, 55):
        return {ra}
    if op in (10, 11, 36, 38, 44, 48, 50, 52, 54, 59, 63):
        return set()
    if op in (16, 18) and not word & 1:
        return set()
    if op == 19 and (word >> 1) & 1023 == 449:  # cror
        return set()
    if op == 31:
        xo = (word >> 1) & 1023
        if xo in (0, 32):  # cmp/cmpl
            return set()
        if xo in (266,):  # add
            return {rt}
        if xo in (444, 922, 954):  # or/mr, extsh, extsb
            return {ra}
    raise ValueError(f'unmodelled instruction form: 0x{word:08x}')


def interval(function):
    body = bytes.fromhex(function['body'])
    calls = {name: [r[0] for r in function['relocations'] if r[1] == 10 and r[2] == name and r[3] == 0]
             for name in (PRODUCER, CONSUMER)}
    if any(len(v) != 1 for v in calls.values()):
        raise ValueError('expected unique producer and consumer REL24 calls')
    start, end = calls[PRODUCER][0] + 4, calls[CONSUMER][0]
    if not 0 <= start < end < len(body):
        raise ValueError('invalid producer-to-consumer order')
    seed = struct.unpack_from('>I', body, start)[0]
    writes = []
    for at in range(start + 4, end, 4):
        word = struct.unpack_from('>I', body, at)[0]
        if 4 in gpr_writes(word):
            writes.append(dict(offset=at, word=f'{word:08x}'))
    return dict(producer=start - 4, consumer=end, seed_offset=start, seed_word=f'{seed:08x}',
                is_slot_copy=seed == 0x7C641B79, subsequent_r4_writes=writes,
                preserves_slot_to_consumer=seed == 0x7C641B79 and not writes)


def audit(before, after, target):
    controls = {label: interval(table[FUNCTION]) for label, table in
                (('before', before['functions']), ('after', after['functions']), ('target', target))}
    if not controls['after']['preserves_slot_to_consumer'] or not controls['target']['preserves_slot_to_consumer']:
        raise ValueError('current/target interval does not preserve returned slot in r4')
    if controls['before']['preserves_slot_to_consumer']:
        raise ValueError('before snapshot does not reproduce the missing-argument defect')
    expected = copy.deepcopy(before)
    old, new = before['functions'][FUNCTION], after['functions'][FUNCTION]
    if old['body'] == new['body'] or old['offset'] != new['offset'] or old['size'] != new['size']:
        raise ValueError('expected a same-size isolated function body repair')
    expected['functions'][FUNCTION]['body'] = new['body']
    text = bytearray.fromhex(expected['sections']['.text']['bytes'])
    text[old['offset']:old['offset'] + old['size']] = bytes.fromhex(new['body'])
    expected['sections']['.text']['bytes'] = text.hex()
    result = compare(expected, after, [])
    result['intervals'] = controls
    result['changed_words'] = sum(old['body'][i:i + 8] != new['body'][i:i + 8] for i in range(0, len(old['body']), 8))
    result['scope'] = 'Linear producer-to-consumer r4 preservation and whole-TU change isolation; not general CFG or whole-function equivalence'
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()) or not output.name.startswith('r68_interfaces_') or output.suffix != '.json':
        parser.error('--out must name build/**/r68_interfaces_*.json')
    try:
        current = capture('game/world/gauntworld')
        target, _ = inventory(ROOT / 'build/GUNE5D/obj/game/world/gauntworld.o')
        result = audit(json.loads(args.before.read_text()), current, target)
        result['current'] = current
    except (OSError, ValueError, KeyError) as error:
        result = dict(status='UNRESOLVED', error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'current'}, indent=2))
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
