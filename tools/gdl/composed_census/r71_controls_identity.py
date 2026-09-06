"""Finite full-TU InitControls source controls; never edits production files."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, changed_functions
from tools.gdl.composed_census.r70_dbgtext_lifetimes import canonical_sections
from tools.gdl.composed_census.r71_controls_retirement_audit import OLD, NEW

UNIT, FN = 'game/game/controls', 'InitControls'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def forms(source):
    # Reconstruct the same paired forms from either reviewed active spelling.
    if source.count(NEW) == 1:
        source = source.replace(NEW, OLD)
    elif source.count(OLD) != 1:
        raise ValueError('unrecognized InitControls source; reviewed baseline or helper form required')
    start = source.index('void InitControls(void)\n{')
    end = source.index('\n/* 0x8003480C', start)
    body = source[start:end]
    variants = {'baseline': body}
    for label, typename in [('index_long', 'long'), ('index_uint', 'unsigned int'), ('index_ulong', 'unsigned long')]:
        variants[label] = body.replace('    int i;', '    ' + typename + ' i;')
    for label, value in [('literal_ulong', '0UL'), ('literal_uint', '0U')]:
        variants[label] = body.replace('lbl_802407F8[i] = 0;', 'lbl_802407F8[i] = ' + value + ';')
    for label, typename in [('zero_uint', 'unsigned int'), ('zero_ulong', 'unsigned long')]:
        new = body.replace('    int i;', '    int i;\n    ' + typename + ' clear;')
        new = new.replace('    init_controls();', '    init_controls();\n    clear = 0;')
        variants[label] = new.replace('lbl_802407F8[i] = 0;', 'lbl_802407F8[i] = clear;')
        variants[label+'_joint_index'] = variants[label].replace('for (i = 0;', 'for (i = clear;')
    variants['separate_stores'] = body.replace(
        '        lbl_802407C8[i] = lbl_802407D8[i] = lbl_802407B8[i] = lbl_802407E8[i] =\n            lbl_802407F8[i] = 0;',
        '        lbl_802407F8[i] = 0;\n        lbl_802407E8[i] = 0;\n        lbl_802407B8[i] = 0;\n        lbl_802407D8[i] = 0;\n        lbl_802407C8[i] = 0;')
    variants['initializer_before_call'] = body.replace('int i;', 'int i = 0;').replace('for (i = 0;', 'for (;')
    variants['while_loop'] = body.replace('for (i = 0; i < 4; i++) {', 'i = 0;\n    while (i < 4) {').replace('lbl_802407F8[i] = 0;', 'lbl_802407F8[i] = 0;\n        ++i;')
    variants['existing_helper'] = '''void InitControls(void)
{
    init_controls();
    clear_pad_levels();
    init_all_dir_info();
    ctrls_initialized = 1;
}
'''
    result = {name: source[:start] + value + source[end:] for name, value in variants.items()}
    for symbol in ['B8', 'C8', 'D8', 'E8', 'F8']:
        result['array_uint_' + symbol] = source.replace('static u32 lbl_802407' + symbol + '[4];', 'static unsigned int lbl_802407' + symbol + '[4];')
    result['all_arrays_uint'] = source
    for symbol in ['B8', 'C8', 'D8', 'E8', 'F8']:
        result['all_arrays_uint'] = result['all_arrays_uint'].replace('static u32 lbl_802407' + symbol + '[4];', 'static unsigned int lbl_802407' + symbol + '[4];')
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--forms', required=True)
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    if not args.out.resolve().is_relative_to((ROOT/'build').resolve()) or not args.out.name.startswith('r71_controls_'):
        ap.error('output must be lane-prefixed under build/')
    edge = cv.read_edges()[UNIT]
    source_path = ROOT / edge['src']
    source_bytes = source_path.read_bytes()
    raw = (ROOT / edge['body_o']).read_bytes()
    folder = Path(tempfile.mkdtemp(prefix='r71_controls_identity_', dir=ROOT / 'build'))
    control = dict(edge, _command_trace=[])
    baseline_path, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder / 'r71_controls_control.o', folder)
    if error or not baseline_path or baseline_path.read_bytes() != raw:
        raise ValueError(error or 'actual Ninja raw baseline fidelity failed')
    baseline = object_inventory(baseline_path)
    target = object_inventory(ROOT / f'build/GUNE5D/obj/{UNIT}.o')
    expected = bytes.fromhex(target['functions'][FN]['body'])
    choices = forms(source_bytes.decode().replace('\r\n', '\n'))
    result = dict(schema_version=1, fidelity=True, compiler_edge=edge,
                  compiler_sha256=sha((ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe').read_bytes()),
                  source_sha256=sha(source_bytes), raw_sha256=sha(raw), baseline=baseline,
                  target=target, baseline_trace=control['_command_trace'], artifacts=str(folder), candidates={})
    for label in args.forms.split(','):
        candidate = choices[label]
        scratch = folder / ('r71_controls_' + label + '.c')
        scratch.write_text(candidate, encoding='utf-8')
        (folder / ('r71_controls_' + label + '.diff')).write_text(''.join(difflib.unified_diff(choices['baseline'].splitlines(True), candidate.splitlines(True), fromfile='baseline', tofile=label)), encoding='utf-8')
        trial = dict(edge, src=scratch.relative_to(ROOT).as_posix(), _command_trace=[])
        obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], folder / ('r71_controls_' + label + '.o'), folder)
        row = dict(source=str(scratch), source_sha256=sha(scratch.read_bytes()), error=error, trace=trial['_command_trace'])
        result['candidates'][label] = row
        if error or not obj:
            print(label, 'UNRESOLVED', error, flush=True)
            continue
        inv = object_inventory(obj)
        current = bytes.fromhex(inv['functions'][FN]['body'])
        row.update(object=str(obj), object_sha256=sha(obj.read_bytes()), inventory=inv,
                   target_instructions=len(expected)//4, raw_instructions=len(current)//4,
                   words=sum(expected[i:i+4] != current[i:i+4] for i in range(0,max(len(expected),len(current)),4)),
                   changed_functions=changed_functions(baseline['functions'], inv['functions']),
                   nontext_equal=canonical_sections(obj,inv)==canonical_sections(baseline_path,baseline),
                   exception_records_equal=inv['exception_records']==baseline['exception_records'])
        print(label, {key: row[key] for key in ('target_instructions','raw_instructions','words','changed_functions','nontext_equal','exception_records_equal')}, flush=True)
    if source_path.read_bytes() != source_bytes or (ROOT/edge['body_o']).read_bytes() != raw:
        raise ValueError('production input changed during controlled experiment')
    args.out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('written', args.out)

if __name__ == '__main__':
    main()
