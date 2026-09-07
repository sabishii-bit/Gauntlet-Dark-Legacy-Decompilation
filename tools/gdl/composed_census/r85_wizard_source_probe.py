"""Private complete-TU controls for the existing calc_wizard_pos fields."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.composed_census.r84_audio_source_probe import measure_body


def source_controls(body):
    typed = body.replace('u8* p = arr + i * sizeof(Player);', 'Player* p = (Player*)(arr + i * sizeof(Player));')
    typed = typed.replace('*(s32*)(p + offsetof(Player, state))','p->state')
    for index in range(3):
        typed=typed.replace('*(f32*)(p + offsetof(Player, pos[%d]))'%index,'p->pos[%d]'%index)
    typed=typed.replace('(f32*)(p + offsetof(Player, pos[1]))','&p->pos[1]')
    forms={'typed_local':typed}
    forms['typed_array']=typed.replace('u8* arr = (u8*)gPlayers;','Player* arr = gPlayers;').replace('Player* p = (Player*)(arr + i * sizeof(Player));','Player* p = &arr[i];')
    forms['typed_py_split']=typed.replace('f32* py = &p->pos[1];','f32* py;\n            py = &p->pos[1];')
    forms['typed_y_uses']=typed.replace('out[1] = out[1] + p->pos[1];','out[1] = out[1] + *py;')
    vector=typed.replace('f32* py = &p->pos[1];','f32* pos = p->pos;')
    for index in range(3):
        vector=vector.replace('p->pos[%d]'%index,'pos[%d]'%index)
    vector=vector.replace('(f64)*py','(f64)pos[1]')
    forms['vector_pointer']=vector
    forms['vector_y_pointer']=vector.replace('f32* pos = p->pos;','f32* pos = p->pos;\n            f32* py = &pos[1];').replace('(f64)pos[1]','(f64)*py')
    forms['typed_state_initializer']=typed.replace('        s32 st;\n','').replace('        st = p->state;','        s32 st = p->state;')
    forms['typed_bare_p']=typed.replace('        Player* p = (Player*)(arr + i * sizeof(Player));','        Player* p;\n        p = (Player*)(arr + i * sizeof(Player));')
    forms['typed_bare_p_state_initializer']=forms['typed_bare_p'].replace('        s32 st;\n','').replace('        st = p->state;','        {\n        s32 st = p->state;').replace('    }\n    for (i = 0; i < 3;', '        }\n    }\n    for (i = 0; i < 3;')
    forms['typed_outer_p']=typed.replace('    s32 i;','    s32 i;\n    Player* p;').replace('        Player* p =','        p =')
    forms['typed_direct_state_guard']=typed.replace('        s32 st;\n','').replace('        st = p->state;\n','').replace('if (st == 1 || st == 8)','if (p->state == 1 || p->state == 8)')
    forms['typed_outer_py']=typed.replace('    s32 i;','    s32 i;\n    f32* py;').replace('            f32* py =','            py =')
    forms['typed_py_before_guard']=typed.replace('        s32 st;','        s32 st;\n        f32* py;').replace('        if (st == 1 || st == 8) {\n            f32* py = &p->pos[1];','        py = &p->pos[1];\n        if (st == 1 || st == 8) {')
    forms['typed_py_after_first_store']=typed.replace('            f32* py = &p->pos[1];','            f32* py;').replace('            out[0] = out[0] + p->pos[0];','            out[0] = out[0] + p->pos[0];\n            py = &p->pos[1];')
    forms['typed_py_before_height_guard']=typed.replace('            f32* py = &p->pos[1];','            f32* py;').replace('            if (lbl_80345A28 == count) {','            py = &p->pos[1];\n            if (lbl_80345A28 == count) {')
    forms['typed_const_p']=typed.replace('Player* p =','const Player* p =').replace('f32* py =','const f32* py =')
    forms['double_rounded_count']=typed.replace('f32 count = lbl_80345A40;','f64 count = lbl_80345A40;').replace('out[i] / count','out[i] / (f32)count')
    forms['bare_count_late_init']=typed.replace('f32 count = lbl_80345A40;','f32 count;').replace('    out[2] = gBossPos[2];','    out[2] = gBossPos[2];\n    count = lbl_80345A40;')
    forms['first_player_boolean']=typed.replace('            f32* py = &p->pos[1];','            f32* py = &p->pos[1];\n            s32 firstPlayer = (lbl_80345A28 == count);').replace('if (lbl_80345A28 == count)','if (firstPlayer)')
    forms['height_select_conditional']=typed.replace('            if (lbl_80345A28 == count) {\n                out[1] = (f32)(lbl_80345A48 * (f64)*py);\n            }','            out[1] = (lbl_80345A28 == count) ? (f32)(lbl_80345A48 * (f64)*py) : out[1];')
    forms['height_count_else']=typed.replace('            if (lbl_80345A28 == count) {','            if (lbl_80345A28 != count) {\n            } else {')
    forms['height_inner_py']=typed.replace('            f32* py = &p->pos[1];\n','').replace('            if (lbl_80345A28 == count) {','            if (lbl_80345A28 == count) {\n                f32* py = &p->pos[1];')
    forms['raw_first_player_boolean']=body.replace('            f32* py = (f32*)(p + offsetof(Player, pos[1]));','            f32* py = (f32*)(p + offsetof(Player, pos[1]));\n            s32 firstPlayer = (lbl_80345A28 == count);').replace('if (lbl_80345A28 == count)','if (firstPlayer)')
    forms['first_component_value']=typed.replace('            f32* py = &p->pos[1];','            f32 x = out[0];\n            f32* py = &p->pos[1];').replace('out[0] = out[0] + p->pos[0];','out[0] = x + p->pos[0];')
    forms['first_component_after_py']=typed.replace('            f32* py = &p->pos[1];','            f32* py = &p->pos[1];\n            f32 x = out[0];').replace('out[0] = out[0] + p->pos[0];','out[0] = x + p->pos[0];')
    forms['first_component_split']=typed.replace('            f32* py = &p->pos[1];','            f32 x;\n            f32* py;\n            x = out[0];\n            py = &p->pos[1];').replace('out[0] = out[0] + p->pos[0];','out[0] = x + p->pos[0];')
    forms['position_component_value']=typed.replace('            f32* py = &p->pos[1];','            f32* py = &p->pos[1];\n            f32 px = p->pos[0];').replace('out[0] = out[0] + p->pos[0];','out[0] = out[0] + px;')
    forms['first_sum_value']=typed.replace('            f32* py = &p->pos[1];','            f32 x = out[0] + p->pos[0];\n            f32* py = &p->pos[1];').replace('out[0] = out[0] + p->pos[0];','out[0] = x;')
    # Retail bytes: count seed is f32 ONE, not zero. Keep double arithmetic
    # for the comparison/increment and the special first-player height.
    literals = [('lbl_80345A40', '1.0f'), ('lbl_80345A28', '1.0'), ('lbl_80345A48', '2.0')]
    for prefix, candidate in [('raw', body), ('typed', typed)]:
        for mask in range(1, 8):
            edited = candidate
            for index, (name, value) in enumerate(literals):
                if mask & (1 << index):
                    edited = edited.replace(name, value)
            forms[prefix + '_literals_' + str(mask)] = edited
    return forms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--forms', default='all')
    ap.add_argument('--out', default='build/r85_wizard_replay')
    args = ap.parse_args()
    folder = ROOT/args.out
    if not folder.resolve().is_relative_to((ROOT/'build').resolve()):
        raise ValueError('--out must remain under this checkout build directory')
    folder.mkdir(parents=True, exist_ok=True)
    unit = 'game/ui/auxscreen'
    edge = cv.read_edges()[unit]
    source = (ROOT/edge['src']).read_text()
    first = source.index('void calc_wizard_pos(f32* out)\n{')
    last = source.index('\n}\n', first)+2
    body = source[first:last]
    raw = ROOT/edge['body_o']
    original = canonical(raw)
    target = canonical(ROOT/f'build/GUNE5D/obj/{unit}.o')
    baseline = folder/'baseline'
    baseline.mkdir(exist_ok=True)
    command = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(command, edge['mw'], edge['cflags'], baseline/'auxscreen.o', baseline)
    if error or obj is None or obj.read_bytes() != raw.read_bytes():
        raise ValueError(error or 'whole raw baseline mismatch')
    for name,path in [('source.c', ROOT/edge['src']),('raw.o',raw),('processed.o',ROOT/f'build/GUNE5D/src/{unit}.o'),('target.o',ROOT/f'build/GUNE5D/obj/{unit}.o')]:
        dest=baseline/name
        if dest.exists() and dest.read_bytes()!=path.read_bytes():
            raise ValueError('immutable baseline moved '+name)
        dest.write_bytes(path.read_bytes())
    (baseline/'inventory.json').write_text(json.dumps(original,indent=2))
    (baseline/'edge.json').write_text(json.dumps(command,indent=2))
    forms = source_controls(body)
    if args.forms == 'all':
        args.forms = ','.join(forms)
    rows=[]
    for label in args.forms.split(','):
        output=folder/label
        output.mkdir(exist_ok=True)
        src=output/'auxscreen.c'
        src.write_text(source[:first]+forms[label]+source[last:],encoding='utf-8')
        command=dict(edge,src=src.relative_to(ROOT).as_posix(),_command_trace=[])
        obj,error=cv.compile_with(command,edge['mw'],edge['cflags'],output/'auxscreen.o',output)
        row=dict(form=label,error=error,trace=command['_command_trace'])
        if obj and not error:
            inv=canonical(obj)
            (output/'inventory.json').write_text(json.dumps(inv,indent=2))
            ours=inv['functions']['calc_wizard_pos']
            row.update(measure_body(target['functions']['calc_wizard_pos']['body'],ours['body']))
            row['changed_functions']=[name for name,value in inv['functions'].items() if value!=original['functions'].get(name)]
            row['changed_bodies']=[name for name,value in inv['functions'].items() if value['body']!=original['functions'].get(name,{}).get('body')]
            row['sections_equal']=inv['sections']==original['sections']
            row['non_text_sections_equal']={n:v for n,v in inv['sections'].items() if n!='.text'}=={n:v for n,v in original['sections'].items() if n!='.text'}
            row['sha256']=hashlib.sha256(obj.read_bytes()).hexdigest()
        rows.append(row)
        print(json.dumps({k:v for k,v in row.items() if k!='trace'}),flush=True)
    path=folder/'results.json'
    old=json.loads(path.read_text()) if path.exists() else []
    path.write_text(json.dumps([row for row in old if row['form'] not in args.forms.split(',')]+rows,indent=2))

if __name__ == '__main__':
    main()
