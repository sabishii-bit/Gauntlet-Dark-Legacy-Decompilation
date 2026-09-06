"""Finite full-TU bank-lookup helper experiments; never a build transform.

Requires the reviewed audio.c snapshot and a fresh Ninja build. Replays the
actual compiler edge, requires a byte-identical complete raw-object control,
then preserves each source/object/diagnostic/inventory under build/. No source,
configuration, or production output is rewritten. PASS is experiment completion,
not source exactness or source-unreachability.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical

UNIT = 'game/audio/audio'
FUNCTIONS = ('AudioBankQueueName', 'AudioBankLoadName', 'AudioUnloadPart')
SOURCE_SHA = '89f75ffd25c4fa06e808824563ebb632a1e7f30959ebbf66777068b009ca0e76'
BASE_RAW_SHA = 'd663a1475139ab60b693393e1abc237f3b5954f4d0f1cc9bbc97fca4b587138e'
HELPER = '''static inline s32 AudioFindBank(char* bankName)
{
    s32 index;
    s32 offset;

    index = 0;
    offset = index;
    while (index < gAudioBankTbl[4]) {
        char* name = (char*)((u8*)gAudioBankTbl + offset + 20);
        if (strncmp(name, bankName, 16) == 0) {
            break;
        }
        index++;
        offset += 292;
    }
    if (index == gAudioBankTbl[4]) {
        sAudioSuspend = 1;
        index = -1;
    }
    return index;
}

'''
ANCHOR = '/* AudioBankLoadName: (re)load bank'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def function_span(source, name):
    matches = list(re.finditer(r'(?m)^(?:s32|void) '+name+r'\([^;]*?\)\n\{', source))
    if len(matches) != 1:
        raise ValueError('function anchor changed: '+name)
    start = matches[0].start()
    pos = source.index('{', start)
    level = 1
    end = pos + 1
    # These three reviewed bodies have no braces in strings/comments.
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return start, end


def lift(source, names, helper=HELPER, scan_only=False):
    if source.count(ANCHOR) != 1:
        raise ValueError('insertion anchor changed')
    for name in names:
        start, end = function_span(source, name)
        body = source[start:end]
        index, offset = {'AudioBankQueueName': ('bankIndex', 'bankOffset'),
                         'AudioBankLoadName': ('bankIdx', 'i'),
                         'AudioUnloadPart': ('i', 'scanOffset')}[name]
        a = body.index('    '+index+' = 0;')
        if scan_only:
            b = body.index('    if ('+index+' == gAudioBankTbl[4])', a)
        else:
            b = body.index('        '+index+' = -1;\n    }', a)
            b += len('        '+index+' = -1;\n    }\n')
        body = body[:a] + '    '+index+' = AudioFindBank(bankName);\n' + body[b:]
        source = source[:start] + body + source[end:]
    return source.replace(ANCHOR, helper+ANCHOR)


def source_forms(data):
    if sha(data) != SOURCE_SHA:
        raise ValueError('reviewed source snapshot changed; rederive controls')
    source = data.decode().replace('\r\n', '\n')
    forms = {'baseline': source}
    for name in FUNCTIONS:
        forms['single_'+name] = lift(source, (name,))
    forms['shared_inline'] = lift(source, FUNCTIONS)
    forms['shared_global'] = lift(source, FUNCTIONS, HELPER.replace('static inline ', ''))
    failure = '''    if (index == gAudioBankTbl[4]) {
        sAudioSuspend = 1;
        index = -1;
    }
'''
    forms['scan_only_inline'] = lift(source, FUNCTIONS, HELPER.replace(failure, ''), True)
    # A separately named return-join axis, not a silent change in the primary form.
    early = HELPER.replace('            break;', '            return index;')
    early = early.replace(failure, '    sAudioSuspend = 1;\n    index = -1;\n')
    forms['early_return_inline'] = lift(source, FUNCTIONS, early)
    shared = forms['shared_inline']
    def edit_body(text, name, old, new):
        a, b = function_span(text, name)
        body = text[a:b]
        if body.count(old) != 1:
            raise ValueError('caller form anchor changed: '+name)
        return text[:a] + body.replace(old,new) + text[b:]
    # The lift separates the former reused bank/part induction identities.
    # Swap only the second scan's two declarations; do not touch its statements.
    swapped = edit_body(shared, 'AudioBankQueueName', '    s32 bankOffset;\n    s32 partIdx;',
                        '    s32 partIdx;\n    s32 bankOffset;')
    forms['queue_decl_swap'] = swapped
    # Directly receive the helper result in the already-existing foundBank.
    def direct_result(text):
        a,b = function_span(text,'AudioBankQueueName')
        body = text[a:b].replace('    s32 bankIndex;\n','')
        body = body.replace('    foundBank = bankIndex;\n','')
        body = re.sub(r'\bbankIndex\b','foundBank',body)
        return text[:a]+body+text[b:]
    forms['queue_direct_result'] = direct_result(shared)
    forms['queue_direct_result_decl_swap'] = direct_result(swapped)
    forms['unload_decl_swap'] = edit_body(shared,'AudioUnloadPart',
        '    s32 i;\n    s32 scanOffset;\n    s32 bankOffset;',
        '    s32 bankOffset;\n    s32 scanOffset;\n    s32 i;')
    forms['helper_decl_swap'] = lift(source, FUNCTIONS, HELPER.replace(
        '    s32 index;\n    s32 offset;', '    s32 offset;\n    s32 index;'))
    # Unload's fresh helper offset competes with its later bankOffset/ROM-id
    # webs. These probes alter actual variable identity/lifetime, not flags.
    base = forms['queue_direct_result_decl_swap']
    forms['unload_outer_swap_two'] = edit_body(base,'AudioUnloadPart',
        '    s32 scanOffset;\n    s32 bankOffset;',
        '    s32 bankOffset;\n    s32 scanOffset;')
    def scope_unload(text, locals_to_move):
        a,b = function_span(text,'AudioUnloadPart')
        body = text[a:b]
        for local in locals_to_move:
            body = body.replace('    s32 '+local+';\n','',1)
        split = body.index('    i = AudioFindBank(bankName);')
        remainder = body[split:-1]
        decls = ''.join('        s32 '+local+';\n' for local in locals_to_move)
        body = body[:split]+'    {\n'+decls+''.join('    '+line+'\n' for line in remainder.rstrip().splitlines())+'    }\n}'
        return text[:a]+body+text[b:]
    forms['unload_scoped_result'] = scope_unload(base,('i',))
    forms['unload_scoped_locals'] = scope_unload(base,('i','scanOffset','bankOffset','partId'))
    forms['unload_scaled_expression'] = edit_body(base,'AudioUnloadPart',
        '    bankOffset = i * 292;\n','')
    a,b = function_span(forms['unload_scaled_expression'],'AudioUnloadPart')
    body = forms['unload_scaled_expression'][a:b].replace('    s32 bankOffset;\n','')
    body = re.sub(r'\bbankOffset\b','(i * 292)',body)
    forms['unload_scaled_expression'] = forms['unload_scaled_expression'][:a]+body+forms['unload_scaled_expression'][b:]
    # Built-in int versus the project's signed-long s32 gives a different
    # real source type identity while preserving PPC 32-bit arithmetic.
    forms['helper_int_locals'] = lift(source,FUNCTIONS,HELPER.replace('s32 index','int index').replace('s32 offset','int offset'))
    forms['helper_index_int'] = lift(source,FUNCTIONS,HELPER.replace('s32 index','int index'))
    forms['helper_offset_int'] = lift(source,FUNCTIONS,HELPER.replace('s32 offset','int offset'))
    forms['unload_result_int'] = edit_body(base,'AudioUnloadPart','    s32 i;','    int i;')
    forms['unload_scalars_int'] = edit_body(base,'AudioUnloadPart',
        '    s32 i;\n    s32 scanOffset;\n    s32 bankOffset;\n    s32 partId;',
        '    int i;\n    int scanOffset;\n    int bankOffset;\n    int partId;')
    forms['helper_const_name'] = lift(source,FUNCTIONS,HELPER.replace('char* bankName','const char* bankName'))
    param = HELPER.replace('AudioFindBank(char* bankName)', 'AudioFindBank(char* bankName, s32 index)')
    param = param.replace('    s32 index;\n','').replace('    index = 0;\n','')
    forms['helper_initial_index_parameter'] = lift(source,FUNCTIONS,param).replace('AudioFindBank(bankName);','AudioFindBank(bankName, 0);')
    retained = lift(source,('AudioBankQueueName','AudioBankLoadName'))
    retained = edit_body(retained,'AudioBankQueueName','    s32 bankOffset;\n    s32 partIdx;',
                         '    s32 partIdx;\n    s32 bankOffset;')
    forms['retained_two_callers'] = direct_result(retained)
    return forms


def relocation_rows(rows):
    return sorted((off & ~3 if kind == 109 else off, kind, name, add)
                  for off, kind, name, add in rows)


def differences(before, after, target):
    functions = {}
    for name in FUNCTIONS:
        a, b = after['functions'][name], target['functions'][name]
        raw, retail = bytes.fromhex(a['body']), bytes.fromhex(b['body'])
        if not raw or not retail or len(raw)%4 or len(retail)%4:
            raise ValueError('complete nonempty instruction words required')
        words = [dict(offset=hex(i), ours=raw[i:i+4].hex(), target=retail[i:i+4].hex())
                 for i in range(0,max(len(raw),len(retail)),4) if raw[i:i+4] != retail[i:i+4]]
        functions[name] = dict(ours_count=len(raw)//4, target_count=len(retail)//4,
                               differing_words=len(words), words=words,
                               target_relocations_equal=relocation_rows(a['relocations'])==relocation_rows(b['relocations']),
                               helper_calls=[r for r in a['relocations'] if r[2]=='AudioFindBank'])
    nontext = lambda inv: {k:v for k,v in inv['sections'].items() if k!='.text'}
    return dict(functions=functions,
                changed_bodies=sorted(n for n in before['functions'].keys()|after['functions'].keys()
                                      if before['functions'].get(n,{}).get('body')!=after['functions'].get(n,{}).get('body')),
                changed_function_records=sorted(n for n in before['functions'].keys()|after['functions'].keys()
                                                if before['functions'].get(n)!=after['functions'].get(n)),
                nontext_equal=nontext(before)==nontext(after),
                all_symbols_equal=before['all_symbols']==after['all_symbols'],
                exception_records_equal=before['exception_records']==after['exception_records'])


def require_fidelity(actual, expected, error=None):
    if error or actual != expected:
        raise ValueError(error or 'full scratch baseline failed raw-object fidelity')


def audit_retirement(before, after, target, old_processed, new_processed):
    """Exact allocated-object and positional binding gate, not a score."""
    if old_processed != new_processed:
        raise ValueError('postprocessed allocated object or bindings changed')
    expected = copy.deepcopy(before)
    text = bytearray.fromhex(expected['sections']['.text']['bytes'])
    for name in FUNCTIONS[:2]:
        old, new = expected['functions'][name], target['functions'][name]
        if len(bytes.fromhex(old['body'])) != old['size'] or old['size'] != new['size']:
            raise ValueError('nonempty full function size/parity changed')
        if relocation_rows(old['relocations']) != relocation_rows(new['relocations']):
            raise ValueError('target positional relocation binding differs')
        old_bytes, new_bytes = bytes.fromhex(old['body']),bytes.fromhex(new['body'])
        sites = [i for i in range(0,len(old_bytes),4) if old_bytes[i:i+4]!=new_bytes[i:i+4]]
        if sites != [0x34]:
            raise ValueError('baseline no longer the reviewed one-word residual')
        old['body'] = new['body']
        off = old['offset']
        text[off:off+old['size']] = new_bytes
    expected['sections']['.text']['bytes'] = text.hex()
    if expected != after:
        raise ValueError('raw changes extend beyond the two retired words (including metadata/relocations/EH)')
    return dict(status='PASS',retired_functions=list(FUNCTIONS[:2]),
                retired_bytes=sum(target['functions'][name]['size'] for name in FUNCTIONS[:2]),
                raw_changed_words=2,processed_allocated_object_equal=True,
                raw_sibling_nontext_symbols_EH_equal=True,target_positional_relocations_equal=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--only', nargs='+')
    ap.add_argument('--baseline-dir', type=Path,
                    help='prior r74_audio_probe directory, required to replay after source retirement')
    ap.add_argument('--audit',action='store_true',help='audit current production against --baseline-dir')
    args = ap.parse_args(argv)
    edge = cv.read_edges()[UNIT]
    paths = dict(source=ROOT/edge['src'], raw=ROOT/edge['body_o'],
                 processed=ROOT/f'build/GUNE5D/src/{UNIT}.o',target=ROOT/f'build/GUNE5D/obj/{UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json', ninja=ROOT/'build.ninja',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe',
                 types=ROOT/'include/types.h')
    frozen = {k:p.read_bytes() for k,p in paths.items()}
    baseline_source = (args.baseline_dir/'r74_audio_baseline/audio.c').read_bytes() if args.baseline_dir else frozen['source']
    baseline_raw = args.baseline_dir/'r74_audio_raw.o' if args.baseline_dir else paths['raw']
    if sha(baseline_raw.read_bytes()) != BASE_RAW_SHA:
        raise ValueError('reviewed original raw object changed')
    forms = source_forms(baseline_source)
    if args.only and not set(args.only)<=forms.keys():
        ap.error('unknown forms: '+repr(set(args.only)-forms.keys()))
    folder = Path(tempfile.mkdtemp(prefix='r74_audio_probe_',dir=ROOT/'build'))
    before,target = canonical(baseline_raw),canonical(paths['target'])
    result = dict(schema_version=1,unit=UNIT,edge=edge,hashes={k:sha(v) for k,v in frozen.items()},
                  baseline=before,target=target,probes={},limitations=['Finite helper-source forms only; not source-unreachability.'])
    output = folder/'r74_audio_results.json'
    for key in ('raw','processed','target'):
        original = (args.baseline_dir/f'r74_audio_{key}.o').read_bytes() if args.baseline_dir else frozen[key]
        (folder/f'r74_audio_{key}.o').write_bytes(original)
        (folder/f'r74_audio_current_{key}.o').write_bytes(frozen[key])
    # Every retained-tree replay first fidelity-gates its CURRENT source, too.
    control_dir = folder/'r74_audio_current_control'
    control_dir.mkdir()
    control_source = control_dir/'audio.c'
    control_source.write_bytes(frozen['source'])
    current_edge = dict(edge,src=str(control_source.relative_to(ROOT)),_command_trace=[])
    current_obj,error = cv.compile_with(current_edge,edge['mw'],edge['cflags'],control_dir/'r74_audio.o',control_dir)
    require_fidelity(current_obj.read_bytes() if current_obj else None,frozen['raw'],error)
    result['current_control'] = dict(complete_raw_object_equal=True,commands=current_edge['_command_trace'])
    if args.audit:
        if not args.baseline_dir:
            ap.error('--audit requires --baseline-dir')
        # Comments do not change this source-shape certificate.
        strip_comments = lambda s: '\n'.join(line for line in re.sub(r'/\*.*?\*/','',s,flags=re.S).splitlines() if line.strip())
        if strip_comments(frozen['source'].decode()) != strip_comments(forms['retained_two_callers']):
            raise ValueError('production source is not precisely the reviewed two-caller form')
        verdict = audit_retirement(before,canonical(paths['raw']),target,
                                   canonical(args.baseline_dir/'r74_audio_processed.o'),canonical(paths['processed']))
        rules = json.loads(frozen['config'])['units'][UNIT]
        if {r['function'] for r in rules} != {'AudioAng','AudioUnloadPart'}:
            raise ValueError('unexpected remaining audio pin roster')
        verdict['current_control'] = result['current_control']
        verdict['hashes'] = result['hashes']
        if any(p.read_bytes()!=frozen[k] for k,p in paths.items()):
            raise ValueError('production inputs/artifacts drifted during audit')
        output.write_text(json.dumps(verdict,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(verdict))
        print(output)
        return
    for name,source_text in forms.items():
        if args.only and name!='baseline' and name not in args.only:
            continue
        work = folder/('r74_audio_'+name)
        work.mkdir()
        source = work/'audio.c'
        source.write_bytes(source_text.encode())
        trial = dict(edge,src=str(source.relative_to(ROOT)),_command_trace=[])
        obj,error = cv.compile_with(trial,edge['mw'],edge['cflags'],work/'r74_audio.o',work)
        row = dict(source=str(source.relative_to(ROOT)),source_sha256=sha(source.read_bytes()),
                   status='COMPILE_FAILURE',error=error,commands=trial['_command_trace'])
        result['probes'][name] = row
        if name=='baseline':
            try:
                require_fidelity(obj.read_bytes() if obj else None,baseline_raw.read_bytes(),error)
            except ValueError:
                output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
                raise
        if obj and not error:
            inv = canonical(obj)
            row.update(status='COMPILED',object=str(obj.relative_to(ROOT)),raw_sha256=sha(obj.read_bytes()),
                       complete_raw_object_equal=obj.read_bytes()==baseline_raw.read_bytes(),inventory=inv,
                       **differences(before,inv,target))
            asm = subprocess.run([str(ROOT/'build/binutils/powerpc-eabi-objdump.exe'),'-dr',str(obj)],
                                 capture_output=True,text=True,check=True).stdout
            (work/'r74_audio.asm').write_text(asm,encoding='utf-8')
            print(name,'; '.join(f"{fn} T{r['target_count']}/O{r['ours_count']} words={r['differing_words']} calls={len(r['helper_calls'])}" for fn,r in row['functions'].items()),
                  'nontext',row['nontext_equal'],'changed',row['changed_bodies'],flush=True)
        else:
            print(name,'COMPILE_FAILURE',error,flush=True)
        output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    if any(p.read_bytes()!=frozen[k] for k,p in paths.items()):
        raise ValueError('production inputs/artifacts drifted during experiment')
    print(output)


if __name__ == '__main__':
    main()
