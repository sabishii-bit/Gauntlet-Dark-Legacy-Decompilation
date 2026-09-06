"""Finite full-TU FontInit loop-identity controls, not a build transform.

Replay only on the reviewed source snapshot. The actual Ninja compiler and
flags are held fixed, and an unchanged scratch TU must reproduce the entire
raw object before variants are measured. Preserve every source, object and
compiler diagnostic under build/; never mutate production source or outputs.
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
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical

UNIT, FN = 'game/ui/btext', 'FontInit'
SOURCE_SHA = 'a59bf6be59d618262b70e6c73e422a412d999b9754ba75b402263abd66d860d1'
OLD = '''void FontInit(void)
{
    u32 i;
    u32 modeIndex;

    StringInitSub(0, &gStringMsgList);
    i = 0;
    modeIndex = i;
    for (; (s32)i < 2; i++, modeIndex++) {
        StringInitSub(gScrollModes_80343BB0[modeIndex], &gScrollMsgList[i]);
    }
    for (i = 1; i < 0xd; i++) {
        LoadFonts(i, gFontDefs8x8[i], gFontDefs[i]);
    }
    gFontsInited = 1;
}'''
SECOND = '''    for (i = 1; i < 0xd; i++) {
        LoadFonts(i, gFontDefs8x8[i], gFontDefs[i]);
    }'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_forms(source):
    source = source.replace('\r\n', '\n')
    if sha(source.encode()) != SOURCE_SHA or source.count(OLD) != 1:
        raise ValueError('reviewed source baseline changed; rederive controls')
    for text in ('extern s32 gScrollModes_80343BB0[2];',
                 'void StringInitSub(u32 mode, StrList* p)'):
        if source.count(text)!=1:
            raise ValueError('ambiguous type-control source anchor')
    renamed = re.sub(r'\bi\b', 'scrollIndex', OLD)
    second = re.sub(r'\bi\b', 'fontIndex', SECOND)
    split = OLD.replace('    u32 modeIndex;', '    u32 modeIndex;\n    u32 fontIndex;').replace(SECOND, second)
    scoped = OLD.replace(SECOND, '    {\n        u32 fontIndex;\n' + '\n'.join('    '+line for line in second.splitlines()) + '\n    }')
    # Scope the scroll identities too: a distinct lifetime, not a fake local.
    start, end = OLD.index('    i = 0;'), OLD.index(SECOND)
    first = OLD[start:end].rstrip('\n')
    both = OLD.replace('    u32 i;\n    u32 modeIndex;\n\n', '')
    both = both.replace(first, '    {\n        u32 i;\n        u32 modeIndex;\n' + '\n'.join('    '+line for line in first.splitlines())+'\n    }')
    both = both.replace(SECOND, '    {\n        u32 fontIndex;\n' + '\n'.join('    '+line for line in second.splitlines())+'\n    }')
    bodies = dict(baseline=OLD, rename_control=renamed, split_font_index=split,
                  scoped_font_index=scoped, both_scoped_indices=both)
    forms = {name: source.replace(OLD, body) for name, body in bodies.items()}
    table = source.replace('extern s32 gScrollModes_80343BB0[2];',
                           'extern int gScrollModes_80343BB0[2];')
    callee = source.replace('void StringInitSub(u32 mode, StrList* p)',
                            'void StringInitSub(unsigned int mode, StrList* p)')
    forms.update(table_element_int=table, callee_mode_uint=callee,
                 table_callee_type_joint=callee.replace('extern s32 gScrollModes_80343BB0[2];',
                                                        'extern int gScrollModes_80343BB0[2];'))
    single = OLD.replace('    u32 modeIndex;\n', '').replace('    modeIndex = i;\n', '')
    single = single.replace('i++, modeIndex++', 'i++').replace('[modeIndex]', '[i]')
    scoped_region = '#pragma opt_lifetimes off\n#pragma opt_propagation off\n'+OLD+'\n#pragma opt_propagation on\n#pragma opt_lifetimes reset'
    if source.count(scoped_region)!=1:
        raise ValueError('FontInit pragma region changed')
    for natural in (False, True):
        for propagation, lifetimes in ((False,False),(True,False),(False,True),(True,True)):
            if not natural and not propagation and not lifetimes:
                continue  # Already covered by the full-object baseline.
            body = single if natural else OLD
            region = ('' if lifetimes else '#pragma opt_lifetimes off\n')
            region += ('' if propagation else '#pragma opt_propagation off\n') + body
            region += ('' if propagation else '\n#pragma opt_propagation on')
            region += ('' if lifetimes else '\n#pragma opt_lifetimes reset')
            label = ('single_index' if natural else 'shared_index')
            label += ('_default_propagation' if propagation else '')
            label += ('_default_lifetimes' if lifetimes else '')
            forms[label] = source.replace(scoped_region, region)
    return forms


def differences(before, after, target):
    a, b = after['functions'][FN], target['functions'][FN]
    raw, retail = bytes.fromhex(a['body']), bytes.fromhex(b['body'])
    if not raw or not retail or len(raw)%4 or len(retail)%4:
        raise ValueError('nonempty complete instruction words required')
    words = [dict(offset=hex(i), ours=raw[i:i+4].hex(), target=retail[i:i+4].hex())
             for i in range(0, max(len(raw),len(retail)),4) if raw[i:i+4]!=retail[i:i+4]]
    def relocs(rows):
        return sorted((off & ~3 if kind==109 else off, kind, name, add)
                      for off, kind, name, add in rows)
    return dict(ours_count=len(raw)//4, target_count=len(retail)//4,
                differing_words=len(words), words=words,
                target_relocation_bindings_equal=relocs(a['relocations'])==relocs(b['relocations']),
                changed_bodies=sorted(name for name in before['functions'].keys()|after['functions'].keys()
                                      if before['functions'].get(name,{}).get('body')!=after['functions'].get(name,{}).get('body')),
                changed_function_records=sorted(name for name in before['functions'].keys()|after['functions'].keys()
                                      if before['functions'].get(name)!=after['functions'].get(name)),
                nontext_equal={k:v for k,v in before['sections'].items() if k!='.text'}=={k:v for k,v in after['sections'].items() if k!='.text'},
                all_symbols_equal=before['all_symbols']==after['all_symbols'],
                exception_records_equal=before['exception_records']==after['exception_records'])


def require_fidelity(raw, expected, error):
    if error or raw is None or raw!=expected:
        raise ValueError(error or 'full scratch baseline failed raw-object fidelity')


def main(argv=None):
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--only',nargs='+')
    args=ap.parse_args(argv)
    edge=cv.read_edges()[UNIT]
    paths=dict(source=ROOT/edge['src'],raw=ROOT/edge['body_o'],
               processed=ROOT/f'build/GUNE5D/src/{UNIT}.o',target=ROOT/f'build/GUNE5D/obj/{UNIT}.o',
               config=ROOT/'config/GUNE5D/webfrank.json',ninja=ROOT/'build.ninja',
               compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe')
    frozen={key:path.read_bytes() for key,path in paths.items()}
    forms=source_forms(frozen['source'].decode())
    if args.only and not set(args.only)<=forms.keys():
        ap.error('unknown controls: '+repr(sorted(set(args.only)-forms.keys())))
    folder=Path(tempfile.mkdtemp(prefix='r73_btext_probe_',dir=ROOT/'build'))
    baseline,target=canonical(paths['raw']),canonical(paths['target'])
    result=dict(schema_version=1,unit=UNIT,function=FN,edge=edge,
                hashes={key:sha(value) for key,value in frozen.items()},
                baseline=baseline,target=target,probes={})
    for key in ('raw','processed','target'):
        (folder/f'r73_btext_{key}.o').write_bytes(frozen[key])
    for name,full_source in forms.items():
        if args.only and name!='baseline' and name not in args.only:
            continue
        work=folder/('r73_btext_'+name)
        work.mkdir()
        source=work/'btext.c'
        source.write_bytes(full_source.encode())
        trial=dict(edge,src=str(source.relative_to(ROOT)),_command_trace=[])
        obj,error=cv.compile_with(trial,edge['mw'],edge['cflags'],work/'r73_btext.o',work)
        row=dict(source=str(source.relative_to(ROOT)),source_sha256=sha(source.read_bytes()),
                 status='COMPILE_FAILURE',error=error,commands=trial['_command_trace'])
        result['probes'][name]=row
        if name=='baseline':
            try:
                require_fidelity(obj.read_bytes() if obj else None,frozen['raw'],error)
            except ValueError:
                (folder/'r73_btext_results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
                raise  # Never measure variants after a failed fidelity control.
        if obj and not error:
            raw=obj.read_bytes()
            inv=canonical(obj)
            row.update(status='COMPILED',object=str(obj.relative_to(ROOT)),raw_sha256=sha(raw),
                       complete_raw_object_equal=raw==frozen['raw'],inventory=inv,
                       **differences(baseline,inv,target))
            print(name,f"{row['ours_count']}/{row['target_count']}", 'words',row['differing_words'],
                  'bodies',row['changed_bodies'],'data',row['nontext_equal'],
                  'relocs',row['target_relocation_bindings_equal'],flush=True)
        else:
            print(name,'COMPILE_FAILURE',error,flush=True)
    if any(path.read_bytes()!=frozen[key] for key,path in paths.items()):
        raise ValueError('production inputs or artifacts drifted during experiment')
    output=folder/'r73_btext_results.json'
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(output)


if __name__=='__main__':
    main()
