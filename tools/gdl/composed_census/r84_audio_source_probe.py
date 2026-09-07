"""Private native AudioAng controls using existing source types/helper."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical


def measure_body(target, actual):
    """Report full-body words only at parity; prefix counts are not full diffs."""
    if len(target) % 8 or len(actual) % 8:
        raise ValueError("body is not a whole PPC instruction stream")
    overlap = sum(target[i:i+8] != actual[i:i+8]
                  for i in range(0, min(len(target), len(actual)), 8))
    frame = None
    for offset in range(0, min(len(actual), 64), 8):
        word = int(actual[offset:offset + 8], 16)
        if word >> 16 == 0x9421:
            immediate = word & 0xffff
            frame = -(immediate - 0x10000 if immediate & 0x8000 else immediate)
            break
    return dict(instructions=[len(target)//8, len(actual)//8],
                differing_words=overlap if len(target) == len(actual) else None,
                overlap_prefix_differing_words=overlap, frame=frame)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--forms', default='all')
    ap.add_argument('--out', default='build/r84_audio_replay')
    a=ap.parse_args()
    folder=ROOT/a.out
    if not folder.resolve().is_relative_to((ROOT/'build').resolve()):
        raise ValueError('--out must remain under this checkout build directory')
    folder.mkdir(parents=True, exist_ok=True)
    edges=cv.read_edges()
    units=('game/audio/audio','game/audio/sndfx')
    sources={u:(ROOT/edges[u]['src']).read_text() for u in units}
    invs={u:canonical(ROOT/edges[u]['body_o']) for u in units}
    targets={u:canonical(ROOT/f'build/GUNE5D/obj/{u}.o') for u in units}
    source=sources[units[0]]
    snd=sources[units[1]]
    first=source.index('s32 AudioAng(f32* pos)\n{')
    end=source.index('\n}\n',first)+2
    body=source[first:end]
    types=snd[snd.index('typedef struct {\n    f32 x;'):snd.index('/* One active-voice record;')]
    helperstart=snd.index('static inline int sndFxComputePan(Vec3* pos)\n{')
    helperend=snd.index('\n}\n',helperstart)+2
    helper=snd[helperstart:helperend]
    typed=body.replace('AudioAng(f32* pos)','AudioAng(Vec3* pos)').replace('    f32 rel[3];','    AudioListener* L;\n    Vec3 rel;')
    typed=typed.replace('    rel[0] =','    L = (AudioListener*)gCameras;\n    rel[0] =',1)
    for old,new in [('rel[0]','rel.x'),('rel[1]','rel.y'),('rel[2]','rel.z'),('pos[0]','pos->x'),('pos[1]','pos->y'),('pos[2]','pos->z'),('gCameras[75]','L->px'),('gCameras[76]','L->py'),('gCameras[77]','L->pz'),('gCameras[1]','L->m4'),('gCameras[2]','L->m8'),('gCameras[3]','L->m12'),('NormalVector(rel)','NormalVector(&rel)')]:
        typed=typed.replace(old,new)
    wrapper='s32 AudioAng(f32* pos)\n{\n    return sndFxComputePan((Vec3*)pos);\n}'
    forms={
        'types_only':types+typed,
        'existing_helper':types+helper+'\n\n'+wrapper,
        'existing_helper_named':types+helper.replace('rel.y = 0.0f;','rel.y = lbl_80345930;').replace('/ 20.0f','/ lbl_80345960')+'\n\n'+wrapper,
    }
    clamp='''    if (pan < -256) {
        pan = -256;
    } else if (pan > 255) {
        pan = 255;
    }
    return pan;'''
    result_clamp='''    if (pan < -256) {
        result = -256;
    } else if (pan > 255) {
        result = 255;
    } else {
        result = pan;
    }
    return result;'''
    assert helper.count(clamp)==1
    result_helper=helper.replace('    int pan;','    int pan;\n    int result;').replace(clamp,result_clamp)
    forms['helper_result']=types+result_helper+'\n\n'+wrapper
    forms['helper_result_named']=types+result_helper.replace('rel.y = 0.0f;','rel.y = lbl_80345930;').replace('/ 20.0f','/ lbl_80345960')+'\n\n'+wrapper
    forms['helper_direct_body']=types+helper.replace('static inline int sndFxComputePan(Vec3* pos)','s32 AudioAng(Vec3* pos)')
    dotline='    dot = rel[0] * gCameras[1] + rel[1] * gCameras[2] + rel[2] * gCameras[3];'
    panexpr='((1.0 < dist) ? 1.0 : (f64)dist)'
    for label,before_dot in [('scale_before_dot',True),('scale_after_dot',False)]:
        edit=body.replace('    f32 dot;','    f32 dot;\n    f64 panScale;').replace(panexpr,'panScale')
        assignment='    panScale = (1.0 < dist) ? 1.0 : (f64)dist;'
        edit=edit.replace(dotline,assignment+'\n'+dotline if before_dot else dotline+'\n'+assignment)
        forms[label]=edit
    forms['dot_accumulator']=body.replace(dotline,'    dot = rel[1] * gCameras[2];\n    dot = rel[0] * gCameras[1] + dot;\n    dot = rel[2] * gCameras[3] + dot;')
    forms['implicit_dist_promotion']=body.replace('(f64)dist','dist')
    forms['typed_implicit_dist_promotion']=types+typed.replace('(f64)dist','dist')
    forms['direct_helper_result']=types+result_helper.replace('static inline int sndFxComputePan(Vec3* pos)','s32 AudioAng(Vec3* pos)')
    dotexpr='(rel[0] * gCameras[1] + rel[1] * gCameras[2] + rel[2] * gCameras[3])'
    forms['inline_dot_expression']=body.replace('    f32 dot;\n','').replace(dotline+'\n','').replace('127.5 * dot *','127.5 * '+dotexpr+' *')
    original_clamp='''    if (pan < -256) {
        result = -256;
    } else if (pan > 255) {
        result = 255;
    } else {
        result = pan;
    }
    return result;'''
    return_clamp='''    if (pan < -256) {
        return -256;
    }
    if (pan > 255) {
        return 255;
    }
    return pan;'''
    forms['clamp_early_returns']=body.replace('    s32 result;\n','').replace(original_clamp,return_clamp)
    forms['clamp_ternary_return']=body.replace('    s32 result;\n','').replace(original_clamp,'    return (pan < -256) ? -256 : ((pan > 255) ? 255 : pan);')
    forms['clamp_else_returns']=forms['clamp_early_returns'].replace('    }\n    if (pan > 255)', '    } else if (pan > 255)')
    ternary_helper=helper.replace(clamp,'    return (pan < -256) ? -256 : ((pan > 255) ? 255 : pan);')
    forms['helper_ternary']=types+ternary_helper+'\n\n'+wrapper
    forms['helper_ternary_named']=types+ternary_helper.replace('rel.y = 0.0f;','rel.y = lbl_80345930;').replace('/ 20.0f','/ lbl_80345960')+'\n\n'+wrapper
    division='    dist = NormalVector(rel) / lbl_80345960;  /* normalise by 20.0f */'
    for label,qual in [('scope_combined_initializer',''),('scope_const_initializer','const ')]:
        edit=body.replace('    f32 dist;\n','').replace(division,'    {\n    '+qual+'f32 dist = NormalVector(rel) / lbl_80345960;')
        edit=edit.replace('    return result;','    }\n    return result;')
        forms[label]=edit
    forms['register_dist']=body.replace('    f32 dist;','    register f32 dist;')
    for label,axes in [('basis_x',((1,'cameraX'),)),('basis_xz',((1,'cameraX'),(3,'cameraZ')))]:
        edit=body
        for index,name in axes:
            edit=edit.replace('gCameras[%d]'%index,name)
        decl=''.join('    f32 '+name+';\n' for _,name in axes)
        init=''.join('    '+name+' = gCameras[%d];\n'%index for index,name in axes)
        forms[label]=edit.replace('    f32 dot;','    f32 dot;\n'+decl.rstrip()).replace(division,division+'\n'+init.rstrip())
    if a.forms == 'all':
        a.forms = ','.join(list(forms) + ['shared_existing_helper'])
    if set(a.forms.split(',')) & {'shared_existing_helper','shared_ternary_helper'}:
        selected='shared_ternary_helper' if 'shared_ternary_helper' in a.forms.split(',') else 'shared_existing_helper'
        shared=folder/selected
        shared.mkdir(exist_ok=True)
        header=shared/'audio_pan_impl.h'
        type_header=shared/'audio_pan_types.h'
        header.write_text((ternary_helper if selected=='shared_ternary_helper' else helper)+'\n',encoding='utf-8')
        type_header.write_text(types,encoding='utf-8')
        type_inc='#include "'+type_header.as_posix()+'"\n'
        helper_inc='#include "'+header.as_posix()+'"\n'
        forms[selected]=type_inc+helper_inc+wrapper
        snd_shared=snd.replace(types,type_inc).replace(helper,helper_inc)
        src=shared/'sndfx.c'
        src.write_text(snd_shared,encoding='utf-8')
        edge=edges[units[1]]
        control=dict(edge,src=src.relative_to(ROOT).as_posix(),_command_trace=[])
        obj,error=cv.compile_with(control,edge['mw'],edge['cflags'],shared/'sndfx.o',shared)
        if error or obj is None: raise ValueError(error)
        shared_inv=canonical(obj)
        shared_result=dict(unit=units[1],full_elf_equal=obj.read_bytes()==(ROOT/edge['body_o']).read_bytes(),
                           baseline_sha256=hashlib.sha256((ROOT/edge['body_o']).read_bytes()).hexdigest(),
                           extracted_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                           canonical_equal=shared_inv==invs[units[1]],trace=control['_command_trace'])
        (shared/'sndfx_preservation.json').write_text(json.dumps(shared_result,indent=2),encoding='utf-8')
        print(json.dumps({k:v for k,v in shared_result.items() if k!='trace'}),flush=True)
    for u in units:
        out=folder/('baseline_'+u.rsplit('/',1)[1])
        out.mkdir(exist_ok=True)
        edge=edges[u]
        control=dict(edge,_command_trace=[])
        obj,error=cv.compile_with(control,edge['mw'],edge['cflags'],out/'baseline.o',out)
        if error or obj is None or obj.read_bytes()!=(ROOT/edge['body_o']).read_bytes(): raise ValueError(error or 'baseline fidelity '+u)
        for name,path in [('source',ROOT/edge['src']),('raw',ROOT/edge['body_o']),('processed',ROOT/f'build/GUNE5D/src/{u}.o'),('target',ROOT/f'build/GUNE5D/obj/{u}.o')]:
            dest=out/('before_'+name+('.c' if name=='source' else '.bin'))
            if dest.exists() and dest.read_bytes()!=path.read_bytes(): raise ValueError('immutable before changed '+str(dest))
            dest.write_bytes(path.read_bytes())
        (out/'inventory.json').write_text(json.dumps(invs[u],indent=2),encoding='utf-8')
        (out/'edge.json').write_text(json.dumps(control,indent=2),encoding='utf-8')
    rows=[]
    for label in a.forms.split(','):
        text=source[:first]+forms[label]+source[end:]
        out=folder/label
        out.mkdir(exist_ok=True)
        src=out/'audio.c'
        src.write_text(text,encoding='utf-8')
        edge=edges[units[0]]
        control=dict(edge,src=src.relative_to(ROOT).as_posix(),_command_trace=[])
        obj,error=cv.compile_with(control,edge['mw'],edge['cflags'],out/'audio.o',out)
        row=dict(form=label,error=error,trace=control['_command_trace'])
        if obj and not error:
            inv=canonical(obj)
            (out/'inventory.json').write_text(json.dumps(inv,indent=2),encoding='utf-8')
            old=invs[units[0]]
            tgt=targets[units[0]]['functions']['AudioAng']
            ours=inv['functions']['AudioAng']
            row.update(**measure_body(tgt['body'], ours['body']),
                       body_changed=[n for n,f in inv['functions'].items() if n not in old['functions'] or f['body']!=old['functions'][n]['body']],
                       sections={n:s['size'] for n,s in inv['sections'].items()},sha256=hashlib.sha256(obj.read_bytes()).hexdigest())
        rows.append(row)
        print(json.dumps({k:v for k,v in row.items() if k!='trace'}),flush=True)
    result_path=folder/'results.json'
    prior=json.loads(result_path.read_text()) if result_path.exists() else []
    prior=[row for row in prior if row['form'] not in a.forms.split(',')]
    result_path.write_text(json.dumps(prior+rows,indent=2),encoding='utf-8')

if __name__ == '__main__':
    main()
