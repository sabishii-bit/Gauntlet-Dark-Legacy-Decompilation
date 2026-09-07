"""Private full-enemy-TU controls for independent reset-store ordering.

Uses actual Ninja flags and requires byte-identical whole raw baseline ELF.
No production source/config writes and no automatic postprocessor retirement.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.composed_census.r84_audio_source_probe import measure_body

UNIT, FUNCTION = 'game/enemy/enemy', 'fn_80051164'
STORES = ('        p[345 + i] = 0;', '        p[300 + i] = -1;',
          '        p[255 + i] = 0;')
TAIL = '    lbl_8034471C = 0;\n    lbl_80344738 = -1;'


def source_controls(body, tails=False):
    old = '\n'.join(STORES)
    if body.count(old) != 1 or body.count(TAIL) != 1:
        raise ValueError('expected original three-store loop and scalar resets')
    forms = {}
    for order in itertools.permutations(range(3)):
        label = 'stores_' + ''.join(map(str, order))
        candidate = body.replace(old, '\n'.join(STORES[i] for i in order))
        forms[label] = candidate
        if tails:
            forms[label + '_tail_reverse'] = candidate.replace(
                TAIL, '\n'.join(reversed(TAIL.splitlines())))
    return forms


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', default='build/r87_enemy_reset_order')
    ap.add_argument('--tails', action='store_true')
    args = ap.parse_args()
    folder = (ROOT / args.out).resolve()
    if not folder.is_relative_to((ROOT/'build').resolve()):
        raise ValueError('--out must remain under this checkout build directory')
    folder.mkdir(parents=True, exist_ok=True)
    edge = cv.read_edges()[UNIT]
    path = ROOT/edge['src']
    source = path.read_text()
    first = source.index('void fn_80051164(void)\n{')
    last = source.index('\n}\n', first) + 2
    body = source[first:last]
    raw = ROOT/edge['body_o']
    frozen = raw.read_bytes()
    original = canonical(raw)
    target = canonical(ROOT/f'build/GUNE5D/obj/{UNIT}.o')
    baseline = folder/'baseline'
    baseline.mkdir(exist_ok=True)
    edge = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(edge, edge['mw'], edge['cflags'],
                                 baseline/'enemy.o', baseline)
    if error or obj is None or obj.read_bytes() != frozen:
        raise ValueError(error or 'actual-Ninja whole-ELF baseline mismatch')
    for name, evidence in [('enemy.c', path), ('raw.o', raw),
                           ('processed.o', ROOT/f'build/GUNE5D/src/{UNIT}.o'),
                           ('target.o', ROOT/f'build/GUNE5D/obj/{UNIT}.o')]:
        dest = baseline/name
        if dest.exists() and dest.read_bytes() != evidence.read_bytes():
            raise ValueError('immutable baseline moved ' + name)
        dest.write_bytes(evidence.read_bytes())
    (baseline/'inventory.json').write_text(json.dumps(original, indent=2))
    (baseline/'edge.json').write_text(json.dumps(edge, indent=2))
    objdump = ROOT/'build/binutils/powerpc-eabi-objdump.exe'
    target_asm = subprocess.run([str(objdump), '-dr', '--disassemble='+FUNCTION,
                                 str(baseline/'target.o')], capture_output=True,
                                text=True, check=True).stdout
    (baseline/'target.asm.txt').write_text(target_asm)
    print(json.dumps(dict(baseline_fidelity=True,
                          raw_sha256=hashlib.sha256(frozen).hexdigest(),
                          function_count=len(original['functions']),
                          baseline=measure_body(target['functions'][FUNCTION]['body'],
                                                original['functions'][FUNCTION]['body']))), flush=True)
    rows = []
    for label, candidate in source_controls(body, args.tails).items():
        output = folder/label
        output.mkdir(exist_ok=True)
        src = output/'enemy.c'
        src.write_text(source[:first]+candidate+source[last:], encoding='utf-8')
        command = dict(edge, src=src.relative_to(ROOT).as_posix(), _command_trace=[])
        obj, error = cv.compile_with(command, edge['mw'], edge['cflags'],
                                     output/'enemy.o', output)
        row = dict(form=label, error=error, trace=command['_command_trace'])
        if obj is not None and not error:
            inv = canonical(obj)
            (output/'inventory.json').write_text(json.dumps(inv, indent=2))
            row.update(measure_body(target['functions'][FUNCTION]['body'],
                                    inv['functions'][FUNCTION]['body']))
            row['changed_functions'] = [n for n,v in inv['functions'].items()
                                        if v != original['functions'].get(n)]
            row['changed_bodies'] = [n for n,v in inv['functions'].items()
                                     if v['body'] != original['functions'].get(n, {}).get('body')]
            row['non_text_sections_equal'] = {
                n:v for n,v in inv['sections'].items() if n != '.text'} == {
                n:v for n,v in original['sections'].items() if n != '.text'}
            row['symbol_layout_equal'] = inv['all_symbols'] == original['all_symbols']
            row['function_relocations_equal'] = inv['functions'][FUNCTION]['relocations'] == original['functions'][FUNCTION]['relocations']
            row['exception_records_equal'] = inv['exception_records'] == original['exception_records']
            row['function_roster_equal'] = set(inv['functions']) == set(original['functions'])
            target_body = target['functions'][FUNCTION]['body']
            actual_body = inv['functions'][FUNCTION]['body']
            row['word_differences'] = [dict(offset=hex(i//2),
                target=target_body[i:i+8], actual=actual_body[i:i+8])
                for i in range(0, min(len(target_body), len(actual_body)), 8)
                if target_body[i:i+8] != actual_body[i:i+8]]
            row['sha256'] = hashlib.sha256(obj.read_bytes()).hexdigest()
            assembly = subprocess.run([str(objdump), '-dr', '--disassemble='+FUNCTION,
                                       str(obj)], capture_output=True, text=True,
                                      check=True).stdout
            (output/'function.asm.txt').write_text(assembly)
        rows.append(row)
        print(json.dumps({k:v for k,v in row.items() if k not in ('trace', 'word_differences')}), flush=True)
    (folder/'results.json').write_text(json.dumps(rows, indent=2))
    if raw.read_bytes() != frozen or path.read_text() != source:
        raise ValueError('production inputs changed while probing')


if __name__ == '__main__':
    main()
