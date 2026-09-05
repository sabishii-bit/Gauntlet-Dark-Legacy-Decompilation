"""Bounded camera DiffRate argument certificate and whole-TU change isolation.

Conservative direct-branch CFG reachability verifies that entry r3 reaches the
named call unchanged, rather than mistaking the absence of a final mr for an
absent argument. This is a diagnostic, not a binary transformation or a general
semantic equivalence proof. Unknown control-flow/operand forms refuse.
"""
import argparse
from collections import defaultdict
import copy
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture, compare
from tools.gdl.composed_census.r68_interfaces_argument_control import gpr_writes
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory

UNIT = 'game/world/camera'
FUNCTION = 'camera_mode_dest'
CONSUMER = 'DiffRate_8002951C'


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def successors(at, word, size):
    op = word >> 26
    if op in (16, 18):
        if word & 2:
            raise ValueError('absolute branch unsupported')
        if word & 1:
            return [at + 4] if at + 4 < size else []
        bits = 16 if op == 16 else 26
        target = at + signed(word & ((1 << bits) - 4), bits)
        if not 0 <= target < size or target % 4:
            raise ValueError('branch outside function')
        return [target, at + 4] if op == 16 and at + 4 < size else [target]
    if op == 19 and (word >> 1) & 1023 in (16, 528):
        if word != 0x4E800020:
            raise ValueError('indirect or conditional return unsupported')
        return []
    return [at + 4] if at + 4 < size else []


def reachable(edges, start):
    seen, todo = set(), [start]
    while todo:
        at = todo.pop()
        if at not in seen:
            seen.add(at)
            todo.extend(edges[at])
    return seen


def entry_argument(function, callee):
    body = bytes.fromhex(function['body'])
    if len(body) % 4:
        raise ValueError('unaligned body')
    calls = [r[0] for r in function['relocations'] if tuple(r[1:]) == (10, callee, 0)]
    if len(calls) != 1:
        raise ValueError('expected unique REL24 consumer')
    call = calls[0]
    if not 0 <= call < len(body) or call % 4:
        raise ValueError('invalid consumer offset')
    words = dict(enumerate(struct.unpack('>' + 'I' * (len(body) // 4), body)))
    words = {at * 4: word for at, word in words.items()}
    if words[call] >> 26 != 18 or words[call] & 3 != 1:
        raise ValueError('consumer is not relative bl')
    edges, reverse = {}, defaultdict(list)
    for at, word in words.items():
        edges[at] = successors(at, word, len(body))
        for destination in edges[at]:
            reverse[destination].append(at)
    path_nodes = reachable(edges, 0) & reachable(reverse, call)
    if call not in reachable(edges, 0):
        raise ValueError('consumer unreachable from entry')
    writes = []
    for at in sorted(path_nodes - {call}):
        word = words[at]
        if word >> 26 in (16, 18) and word & 1:
            raise ValueError('intervening call on consumer path')
        if word >> 26 == 31 and (word >> 1) & 1023 == 339:  # mfspr
            destinations = {(word >> 21) & 31}
        elif word >> 26 == 47:  # stmw: stores only
            destinations = set()
        else:
            destinations = gpr_writes(word)
        if 3 in destinations:
            writes.append(dict(offset=at, word=f'{word:08x}'))
    return dict(call=call, path_nodes=sorted(path_nodes), r3_writes=writes,
                preserves_entry_r3=not writes)


def audit(before, after, target, provider):
    if set(provider) != {'target', 'raw'}:
        raise ValueError('require exactly target and raw providers')
    controls = {
        'before': entry_argument(before['functions'][FUNCTION], 'DiffRate'),
        'after': entry_argument(after['functions'][FUNCTION], CONSUMER),
        'target': entry_argument(target[FUNCTION], CONSUMER),
    }
    if controls['before']['r3_writes'] != [{'offset': 0x2A4, 'word': '80600000'}]:
        raise ValueError('baseline does not reproduce the frame-ticks clobber')
    if any(not controls[k]['preserves_entry_r3'] for k in ('after', 'target')):
        raise ValueError('current/target does not preserve entry camera index')
    if any(controls[k]['call'] != 0x2DC for k in controls):
        raise ValueError('unexpected consumer position')
    old, new = before['functions'][FUNCTION], after['functions'][FUNCTION]
    if old['offset'] != new['offset'] or old['size'] != new['size']:
        raise ValueError('argument repair must preserve function layout')
    changed = [i // 2 for i in range(0, len(old['body']), 8) if old['body'][i:i+8] != new['body'][i:i+8]]
    if changed != [0x2A4, 0x2B4, 0x2B8, 0x2BC]:
        raise ValueError('unexpected instruction changes')
    if new['body'][0x2A4*2:0x2E0*2] != target[FUNCTION]['body'][0x2A4*2:0x2E0*2]:
        raise ValueError('repaired argument block differs from target bytes')
    for label, fn in provider.items():
        if fn['body'][0xC*2:0x10*2] != '1c03018c' or fn['binding'] != 1:
            raise ValueError(label + ': provider does not consume r3 as 396-byte index')
    expected = copy.deepcopy(before)
    expected['functions'][FUNCTION]['body'] = new['body']
    text = bytearray.fromhex(expected['sections']['.text']['bytes'])
    text[old['offset']:old['offset']+old['size']] = bytes.fromhex(new['body'])
    expected['sections']['.text']['bytes'] = text.hex()
    old_symbols = [s for s in expected['symbols'] if s['name'] == 'DiffRate']
    if len(old_symbols) != 1 or old_symbols[0]['section'] or old_symbols[0]['binding'] != 1:
        raise ValueError('expected unique undefined DiffRate import')
    old_symbols[0]['name'] = CONSUMER
    # One lfd is rescheduled by four bytes; move its own unchanged relocation.
    changes = []
    for key, rels, base in [('function', expected['functions'][FUNCTION]['relocations'], 0),
                            ('section', expected['relocations']['.text'], old['offset'])]:
        # MWCC SDA21 points at the low halfword, unlike DTK's word offset.
        pool = [r for r in rels if r[0] == base + 0x2BA]
        call = [r for r in rels if r[0] == base + 0x2DC and r[2] == 'DiffRate']
        ticks = [r for r in rels if r[0] == base + 0x2A6 and r[2] == 'gFrameTicks']
        if len(pool) != 1 or pool[0][1] != 109 or len(call) != 1 or len(ticks) != 1:
            raise ValueError('expected pool/call/ticks relocations missing')
        changes.append(dict(scope=key, pool_symbol=pool[0][2], from_offset=pool[0][0], to_offset=pool[0][0]+4))
        pool[0][0] += 4
        call[0][2] = CONSUMER
    result = compare(expected, after, [])
    result.update(controls=controls, changed_word_offsets=changed, relocation_changes=changes,
                  target_equal_block='0x2a4:0x2e0', provider_index_word='1c03018c',
                  scope='Entry-r3 preservation on every direct-CFG path to this call, exact local target block, whole-TU isolation. Not whole-function semantic equivalence or historical source provenance.')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / 'build') or not output.name.startswith('r69_camera_') or output.suffix != '.json':
        parser.error('--out must name build/**/r69_camera_*.json')
    try:
        current = capture(UNIT)
        target, _ = inventory(ROOT / 'build/GUNE5D/obj/game/world/camera.o')
        providers = {}
        for label, path in [('target', 'obj/game/game/combat.o'), ('raw', 'src/game/game/.postprocess/body/combat.o')]:
            functions, _ = inventory(ROOT / 'build/GUNE5D' / path)
            providers[label] = functions[CONSUMER]
        result = audit(json.loads(args.before.read_text())['current'], current, target, providers)
        result['current'] = current
    except (OSError, ValueError, KeyError) as error:
        result = dict(status='UNRESOLVED', error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'current'}, indent=2))
    print('written', output)
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
