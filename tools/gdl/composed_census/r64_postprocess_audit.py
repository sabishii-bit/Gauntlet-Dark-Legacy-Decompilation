"""Audit the evidence for postprocessing, without editing game/build inputs.

Run after ninja: python tools/gdl/composed_census/r64_postprocess_audit.py
Writes generated diagnostic objects/source under build/r64_audit and a JSON
report (--out). Requires the local compiler, split objects and raw body objects.
This is a finite context experiment, not an all-source/all-flags impossibility
proof. The baseline whole-object fidelity check gates every real experiment.
The deliberately bad-baseline control is MOCKED and labelled as synthetic.
No compiler patch, WebFrank rule, source body or build setting is installed.
"""
import argparse
import collections
import contextlib
import hashlib
import io
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / 'tools/gdl'), str(ROOT / 'tools/gdl/composed_census')]
import cv_probe as cv
import matchtool
from cn_analyze import load
import cr_epilogue_census as cr

OUT = ROOT / 'build/r64_audit'
FIXTURE = Path(__file__).with_name('r64_flags_context.c')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def body(path, name):
    return load(str(path), name)[3]


def fidelity_negative_control():
    # Synthetic wrong baseline. Never touch or impersonate a real game object.
    with tempfile.TemporaryDirectory(prefix='r64_fidelity_') as td:
        fake = Path(td) / 'shipped.o'
        fake.write_bytes(b'SHIPPED')
        edge = dict(src='fake.c', mw='FAKE', cflags='', body_o=str(fake), rule='mwcc')
        calls = []
        def compile_fake(edge, mw, flags, output, workdir):
            calls.append(str(output))
            Path(output).write_bytes(b'DIFFERENT')
            return Path(output), None
        log = io.StringIO()
        with patch.object(cv, 'read_edges', return_value={'fake': edge}), \
             patch.object(cv, 'compile_with', side_effect=compile_fake), \
             patch.object(cv.matchtool, 'parse', return_value={}), \
             patch.object(sys, 'argv', ['cv_probe', 'fake', '--axes', 'opt', '-j', '1']), \
             contextlib.redirect_stdout(log):
            rc = cv.main()
        return dict(kind='synthetic calibration, not a game compile',
                    returncode=rc, compile_calls=len(calls), output=log.getvalue())


def compile_row(edge, label, extra='', src=None, functions=()):
    changed = dict(edge)
    if src is not None:
        changed['src'] = str(src)
    path = OUT / (label + '.o')
    got, err = cv.compile_with(changed, edge['mw'], edge['cflags'] + ' ' + extra, path, str(OUT))
    row = dict(label=label, source=changed['src'], compiler=edge['mw'], extra=extra, error=err)
    if err:
        return row
    row['object_sha256'] = sha(path.read_bytes())
    row['functions'] = {}
    for name in functions:
        b = body(path, name)
        row['functions'][name] = dict(instructions=len(b)//4, sha256=sha(b),
                                      words=[b[i:i+4].hex() for i in range(0, len(b), 4)] if len(b)<=64 else None)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT/'build/r64_postprocess_audit.json')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    edges = cv.read_edges()
    edge = edges['game/sys/sysservice']
    functions = ['sysClearFlags','sysSetFlags','padUpdate','sysResetService']
    baseline = compile_row(edge, 'original_base', functions=functions)
    if baseline['error']:
        raise SystemExit('REFUSED baseline compile: '+baseline['error'])
    fidelity = baseline['object_sha256'] == sha((ROOT/edge['body_o']).read_bytes())
    if not fidelity:
        raise SystemExit('REFUSED: baseline object differs; rebuild the current raw object first')
    original_source = (ROOT/edge['src']).read_text()
    pragma_block = '#pragma optimization_level 4\n#pragma peephole on\n#pragma scheduling on'
    if original_source.count(pragma_block) != 1:
        raise SystemExit('REFUSED: expected exactly one three-pragma restore block')
    diagnostic_source = OUT/'sysservice_no_pragma.c'
    diagnostic_source.write_text(original_source.replace(
        pragma_block, '/* Diagnostic copy: command-line optimizer control retained. */'))
    text = (ROOT / 'build.ninja').read_text()
    joined_ninja = re.sub(r'\$\r?\n\s+', ' ', text)
    rules = collections.Counter(re.findall(r'^build [^\n]+?:\s+(\w+)', joined_ninja, re.M))
    wf = json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text())['units']
    pragma_units = {}
    for unit, e in edges.items():
        source = ROOT / e['src']
        pragmas = [dict(line=i, text=s.strip()) for i,s in enumerate(source.read_text(errors='replace').splitlines(), 1)
                   if re.match(r'\s*#pragma\s+(optimization_level|opt_|peephole|scheduling|inline|cplusplus)', s)]
        if pragmas:
            pragma_units[unit] = pragmas
    result = dict(compilers=dict(collections.Counter(e['mw'] for e in edges.values())),
                  compiled_edges=len(edges), build_rules=dict(rules),
                  webfrank_units=len(wf), webfrank_rules=sum(len(v) for v in wf.values()),
                  webfrank_functions=len({(u,r['function']) for u,rs in wf.items() for r in rs}),
                  manual_equivalence_exceptions=[dict(unit=u,function=r['function']) for u,rs in wf.items()
                                                 for r in rs if r.get('unproven_recolor_audit')],
                  pragma_units=pragma_units,
                  pragma_unit_count=len(pragma_units),
                  baseline_fidelity=fidelity,
                  input_hashes=dict(source=sha((ROOT/edge['src']).read_bytes()),
                                    build_graph=sha((ROOT/'build.ninja').read_bytes()),
                                    compiler=sha((ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe').read_bytes())),
                  archive_variants=dict(all=len(cv.variants(edge,'all')), mwopt=len(cv.variants(edge,'mwopt'))),
                  synthetic_fidelity_failure=fidelity_negative_control())
    variants = [('', ''), ('level1', '-opt level=1'), ('nopeephole','-opt nopeephole'),
                ('noschedule','-schedule off'), ('off','-opt off')]
    result['full_tu'] = []
    for source_tag, source in [('original',None), ('no_restore_pragmas',OUT/'sysservice_no_pragma.c')]:
        for suffix, opts in variants:
            row = baseline if source_tag == 'original' and not suffix else compile_row(
                edge, source_tag+'_'+(suffix or 'base'), opts, source, functions)
            result['full_tu'].append(row)
            print(row['label'], row['error'] or {n:(v['instructions'],v['sha256'][:12]) for n,v in row['functions'].items()}, flush=True)
    epilogues = dict(pairs=0, raw_both_inline=0, raw_restore_mismatches=[],
                     raw_frame_order_mismatches=[],
                     errors=[], raw_shapes=collections.Counter(), target_shapes=collections.Counter())
    for unit, entry in edges.items():
        raw_path = ROOT/entry['body_o']
        target_path = ROOT/'build/GUNE5D/obj'/f'{unit}.o'
        if not raw_path.exists() or not target_path.exists():
            epilogues['errors'].append(dict(unit=unit, error='missing object'))
            continue
        raw = matchtool.parse(raw_path)
        target = matchtool.parse(target_path)
        for name in raw.keys() & target.keys():
            epilogues['pairs'] += 1
            r = cr.classify_tail(cr._insns(raw[name]))
            t = cr.classify_tail(cr._insns(target[name]))
            if not r or not t:
                continue
            epilogues['raw_both_inline'] += 1
            epilogues['raw_shapes'][r['restore']+'/'+r['frame_order']] += 1
            epilogues['target_shapes'][t['restore']+'/'+t['frame_order']] += 1
            detail = dict(unit=unit, function=name, raw=r, target=t)
            if r['restore'] != t['restore']:
                epilogues['raw_restore_mismatches'].append(detail)
            if r['frame_order'] != t['frame_order']:
                epilogues['raw_frame_order_mismatches'].append(detail)
    result['manifest_bound_raw_epilogues'] = epilogues
    result['synthetic_context'] = []
    for label, opts in [('isolated',''), ('preceding_function','-DCONTEXT=1'), ('extern','-DSTORAGE=1'),
                        ('volatile','-DSTORAGE=2'), ('extra_unused_parameter','-DARITY=1'),
                        ('level1','-opt level=1'), ('level1_override','-opt level=1 -DOVERRIDE=1'),
                        ('off','-opt off'), ('off_override','-opt off -DOVERRIDE=1')]:
        row = compile_row(edge,'synthetic_'+label,opts,FIXTURE,
                          ['sysClearFlags','sysSetFlags','optControl'])
        result['synthetic_context'].append(row)
        print(row['label'], row['error'] or {n:(v['instructions'],v['sha256'][:12]) for n,v in row['functions'].items()}, flush=True)
    patched_exe = ROOT/'tools/gdl/mwcc_p6/build/mwcceppc-125n-p6.exe'
    patch_result = dict(available=patched_exe.exists(), profile='experimental, not the default compiler')
    if patched_exe.exists():
        patch_result['compiler_sha256'] = sha(patched_exe.read_bytes())
        if patch_result['compiler_sha256'] != '5a4d1e1715954ddefc87a5a0dfbe38b6c3916e22214957b21af3bd147a760667':
            patch_result['error'] = 'unrecognized derived compiler; not executed'
        else:
            reg_edge = edges['game/sys/registry']
            output = OUT/'registry_125s.o'
            command = ([str(ROOT/'build/tools/sjiswrap.exe')] if 'sjis' in reg_edge['rule'] else []) + \
                      [str(patched_exe)] + shlex.split(reg_edge['cflags']) + ['-c',reg_edge['src'],'-o',str(output)]
            proc = subprocess.run(command,capture_output=True,text=True,cwd=str(ROOT))
            patch_result['returncode'] = proc.returncode
            patch_result['messages'] = proc.stdout+proc.stderr
            if proc.returncode == 0:
                patch_result['object_sha256'] = sha(output.read_bytes())
                patch_result['equals_current_p6frank_object'] = output.read_bytes() == (ROOT/'build/GUNE5D/src/game/sys/registry.o').read_bytes()
                patch_result['equals_stock_raw_object'] = output.read_bytes() == (ROOT/reg_edge['body_o']).read_bytes()
    result['registry_existing_125s_experiment'] = patch_result
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print('baseline_fidelity',result['baseline_fidelity'])
    print('compiler_inventory',result['compilers'])
    print('postprocessors',{k:v for k,v in rules.items() if any(t in k for t in ['frank','fix','retail'])})
    print('synthetic wrong-baseline sweep returncode',result['synthetic_fidelity_failure']['returncode'],
          'compile calls',result['synthetic_fidelity_failure']['compile_calls'])
    print('existing 1.2.5s registry experiment',patch_result)
    print('wrote',args.out)


if __name__ == '__main__':
    main()
