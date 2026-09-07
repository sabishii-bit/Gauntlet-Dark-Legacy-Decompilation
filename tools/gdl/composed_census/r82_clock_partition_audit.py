"""Finite original CAMERA/CLOCK/COMBAT reconstruction experiment, never a hook.

Snapshot the full pre-partition combat object with actual-Ninja fidelity, then
measure the three-way partition and all CLOCK allocated bytes/final bindings.
Reports a real InitPlayerMissiles regression instead of certifying the split.
Reproduce: --snapshot --out build/r82_before.json, then --experiment --before
build/r82_before.json --out build/r82_experiment.json. No production writes.
"""
import argparse
import copy
import difflib
import hashlib
import json
from pathlib import Path
import re
import sys
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.composed_census.r80_lights_retirement_audit import final_relocations, require
from tools.gdl.pooldump import dol_read
from tools.gdl import fndiff

OLD = 'game/game/combat'
TAIL = 'game/world/camera_tail'
CLOCK = 'game/sys/clock'
UNITS = (TAIL, CLOCK, OLD)
BASES = {'.text': 0x8002EFE8, 'extab': 0x80005BB0,
         'extabindex': 0x800091D8, '.sdata2': 0x803462E8}
SIZES = {'.text': 748, 'extab': 8, 'extabindex': 12, '.sdata2': 40}
CLOCK_FUNCTIONS = {'ResetClock': (0, 44), 'InitializeClockIRQ': (44, 44),
                   'ClockOncePerFrame': (88, 660)}
POOL = '0000000041f000003d088889427000003f800000468ca0003f000000447a00004330000000000000'


def partition_sources(source):
    """Three independently evidenced module bands, not function-only slices."""
    start=source.index('void DiffRate_8002951C(s32 camIdx)\n{')
    clock=source.index('void ResetClock(void)\n{')
    combat=source.index('void PlayerDamagedEnemy(')
    prefix=source[:start]
    externs=prefix
    for name in ('pmissile_sfxidx','WeapThrowFx','WeapHoldFxTree','FamiliarSpit',
                 'PhoenixTree','FamiliarTree','EnemyMissileTree','PlayerMissileTreeInfo'):
        externs,n=re.subn(r'(?m)^(?!extern)([^\n;]+\b'+name+r'(?:\[[^\n]*?)?;)',r'extern \1',externs)
        require(n==1,'expected one existing BSS definition: '+name)
    clock_body=source[clock:combat]
    for addr,literal in zip(range(0x803462E8,0x803462FC,4),
                            ('0.0f','30.0f','0.03333333507180214f','60.0f','1.0f')):
        clock_body=clock_body.replace('lbl_%08X'%addr,literal)
    typedef=source[source.index('typedef struct CombatItem {'):source.index('extern CombatItem* sItems;')]
    return {TAIL:externs+source[start:clock], CLOCK:externs+clock_body, OLD:prefix+typedef+source[combat:]}


def clock_oracle(before_target, target_relocations):
    """Project the original target's real CLOCK ranges; no object mutation."""
    old_bases={'.text':0x8002951C,'extab':0x80005B00,'extabindex':0x800090D0}
    target=dict(functions={},sections={},exception_records={})
    final={}
    for name,(off,size) in CLOCK_FUNCTIONS.items():
        original=before_target['functions'][name]
        require((original['offset'],original['size'])==
                (BASES['.text']-old_bases['.text']+off,size),'CLOCK target boundary drift: '+name)
        target['functions'][name]=dict(before_target['functions'][name],offset=off)
    for section,size in SIZES.items():
        if section=='.sdata2':
            data=dol_read(BASES[section],size)
            require(data is not None,'CLOCK pool absent from DOL')
            target['sections'][section]=dict(size=size,type=1,alignment=4,bytes=data.hex())
            final[section]=[]
        else:
            delta=BASES[section]-old_bases[section]
            old=before_target['sections'][section]
            target['sections'][section]=dict(old,size=size,bytes=old['bytes'][2*delta:2*(delta+size)])
            final[section]=[[p-delta,k,a] for p,k,a in target_relocations[section] if delta<=p<delta+size]
    target['exception_records']={'ClockOncePerFrame':before_target['exception_records']['ClockOncePerFrame']}
    return target,final


def report_function(folder, target, obj, name):
    """Generate a fresh isolated report with the current project's settings."""
    config=json.loads((ROOT/'objdiff.json').read_text())
    unit=next(u for u in config['units'] if u['name']=='main/'+OLD)
    unit=copy.deepcopy(unit)
    unit['target_path']=str(target.resolve())
    unit['base_path']=str(obj.resolve())
    config['units']=[unit]
    folder.mkdir(exist_ok=True)
    (folder/'objdiff.json').write_text(json.dumps(config),encoding='utf-8')
    report=folder/'report.json'
    command=[str(ROOT/'build/tools/objdiff-cli.exe'),'report','generate','-p',str(folder),'-o',str(report)]
    subprocess.run(command,check=True,cwd=ROOT,capture_output=True)
    result=json.loads(report.read_text())
    fn=next(f for f in result['units'][0]['functions'] if f['name']==name)
    return fn.get('fuzzy_match_percent',0.0)


def experiment(before_path,out):
    """Finite reconstruction evidence, explicitly not a shipping certificate."""
    before=json.loads(before_path.read_text())
    paths={n:ROOT/p for n,p in before['artifacts'].items()}
    require(before.get('fidelity') is True,'baseline fidelity absent')
    for n,p in paths.items():
        require(digest(p.read_bytes())==before['hashes'][n],'before artifact changed: '+n)
    require(canonical(paths['raw'])==before['inventory'],'snapshot inventory does not describe raw ELF')
    compiler=ROOT/'build/compilers'/before['edge']['mw']/'mwcceppc.exe'
    require(digest(compiler.read_bytes())==before['hashes']['compiler'],'compiler fingerprint changed')
    folder=Path(tempfile.mkdtemp(prefix='r82_architecture_',dir=ROOT/'build'))
    source=paths['source'].read_text()
    original=folder/'combat.c'
    original.write_text(source,encoding='utf-8')
    edge=dict(before['edge'],src=original.relative_to(ROOT).as_posix())
    trace=fresh(edge,folder,'baseline',paths['raw'].read_bytes())
    inventories={}
    objects={}
    traces={'baseline':trace}
    forms=partition_sources(source)
    radius_old='    r = ad - k;\n    r = (f32)(lbl_80346098 * r + k);'
    radius_new='    r = (f32)(lbl_80346098 * (ad - k) + k);'
    first=source.index('s32 adjust_radius_8002B2D4(s32 camIdx)\n{')
    end=source.index('\n}\n',first)+2
    body=source[first:end]
    require(body.count(radius_old)==1,'radius expression control drift')
    for label,cap,expr in (('radius_cap',True,False),('radius_expression',False,True),('radius_joint',True,True)):
        candidate=body.replace('lbl_80346158','2.0f') if cap else body
        if expr: candidate=candidate.replace(radius_old,radius_new)
        forms[label]=source[:first]+candidate+source[end:]
    wad='    void* weaponWad = player_multiple_models[idx].weaponWad;\n    void* powerupWad = player_multiple_models[idx].powerupWad;'
    fx='    void** holdFx = WeapHoldFxTree[idx];\n    s32* throwFx = WeapThrowFx[idx];'
    require(forms[OLD].count(wad)==forms[OLD].count(fx)==1,'initializer control drift')
    for label,keys in (('init_wad_order',(wad,)),('init_fx_order',(fx,)),('init_joint_order',(wad,fx))):
        candidate=forms[OLD]
        for key in keys: candidate=candidate.replace(key,'\n'.join(reversed(key.splitlines())))
        forms[label]=candidate
    for label,text in forms.items():
        sub=folder/label.replace('/','_')
        sub.mkdir()
        src=sub/'combat.c'
        src.write_text(text,encoding='utf-8')
        control=dict(edge,src=src.relative_to(ROOT).as_posix(),_command_trace=[])
        obj,error=cv.compile_with(control,edge['mw'],edge['cflags'],sub/'combat.o',sub)
        require(not error and obj is not None,str(error))
        objects[label]=obj
        inventories[label]=canonical(obj)
        (sub/'inventory.json').write_text(json.dumps(inventories[label],indent=2),encoding='utf-8')
        traces[label]=control['_command_trace']
    addresses={n:int(a,16) for n,a in re.findall(r'(?m)^([^\s=]+) = [^:]+:0x([0-9A-Fa-f]+);',
                                              (ROOT/'config/GUNE5D/symbols.txt').read_text())}
    # Compiler @N names are local to each ELF, never global map identities.
    addresses={n:a for n,a in addresses.items() if not n.startswith('@')}
    target=canonical(paths['target'])
    target_final=final_relocations(paths['target'],addresses,
                                  {'.text':0x8002951C,'extab':0x80005B00,'extabindex':0x800090D0,'.bss':0x80240560})
    clock_target,clock_final=clock_oracle(target,target_final)
    clock=check_clock(inventories[CLOCK],clock_target,final_relocations(objects[CLOCK],addresses,BASES),clock_final)
    changes={}
    partition_names=[n for u in UNITS for n in inventories[u]['functions']]
    require(len(partition_names)==len(set(partition_names)) and
            set(partition_names)==set(before['inventory']['functions']),
            'partition omitted, duplicated or introduced functions')
    for u in UNITS:
        changes[u]=[]
        for n,f in inventories[u]['functions'].items():
            old=before['inventory']['functions'][n]
            words=sum(old['body'][i:i+8]!=f['body'][i:i+8] for i in range(0,min(len(old['body']),len(f['body'])),8))
            if old['size']!=f['size'] or words or bindings(inventories[u],f)!=bindings(before['inventory'],old):
                changes[u].append(dict(function=n,instructions=[old['size']//4,f['size']//4],changed_words=words,
                                       relocation_descriptors_equal=bindings(inventories[u],f)==bindings(before['inventory'],old)))
    fuzzy={'before':report_function(folder/'report_before',paths['target'],paths['raw'],'InitPlayerMissiles')}
    for label in (OLD,'init_wad_order','init_fx_order','init_joint_order'):
        fuzzy[label]=report_function(folder/('report_'+label.replace('/','_')),paths['target'],objects[label],'InitPlayerMissiles')
    aligned={}
    target_lines=fndiff.parse(paths['target'])['InitPlayerMissiles']
    for label,obj in (('before',paths['raw']),('partition',objects[OLD])):
        lines=fndiff.parse(obj)['InitPlayerMissiles']
        diff=list(difflib.unified_diff(target_lines,lines,'target',label,lineterm='',n=2))
        (folder/('InitPlayerMissiles_'+label+'.diff')).write_text('\n'.join(diff)+'\n',encoding='utf-8')
        raw=[row for row in difflib.unified_diff(target_lines,lines,lineterm='',n=0)
             if row.startswith(('+','-')) and not row.startswith(('+++','---'))]
        aligned[label]=dict(real=fndiff.count_real(raw),target_instructions=len(fndiff.instruction_lines(target_lines)),
                            source_instructions=len(fndiff.instruction_lines(lines)),frame=fndiff.frame_size(lines))
    radius={}
    for label in ('baseline','radius_cap','radius_expression','radius_joint'):
        inv=before['inventory'] if label=='baseline' else inventories[label]
        f=inv['functions']['adjust_radius_8002B2D4']
        t=target['functions']['adjust_radius_8002B2D4']
        radius[label]=dict(instructions=[t['size']//4,f['size']//4],changed_words=sum(f['body'][i:i+8]!=t['body'][i:i+8] for i in range(0,len(t['body']),8)),
                           relocations=bindings(inv,f),sdata2=inv['sections']['.sdata2'])
    result=dict(schema_version=1,status='MEASURED_NOT_SHIPPING_CERTIFICATE',clock=clock,partition_changes=changes,
                init_player_missiles_fresh_report_fuzzy=fuzzy,radius=radius,baseline_hashes=before['hashes'],
                init_player_missiles_aligned=aligned,sibling_fuzzy_regressed=fuzzy[OLD]<fuzzy['before'],
                functions_partitioned=len(partition_names),
                artifacts=str(folder.relative_to(ROOT)),traces=traces,retired_rules=0,
                limitations=['CLOCK is native exact in isolation; inspect the independent sibling_fuzzy_regressed result.',
                             'Radius controls measure raw words only; their local literal has no certified retail pool ownership.',
                             'Relocation descriptors do not certify section-base plus instruction-immediate datum identity.',
                             'No production code, mapping or postprocessor change is authorized by this experiment.'])
    require(all(digest(p.read_bytes())==before['hashes'][n] for n,p in paths.items()),'snapshot artifacts changed during experiment')
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('radius','traces','baseline_hashes')}))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fresh(edge, folder, name, expected):
    control = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder / (name+'.o'), folder)
    require(not error and obj is not None, str(error))
    require(obj.read_bytes() == expected, 'complete actual-Ninja ELF fidelity failed: '+name)
    return control['_command_trace']


def snapshot(out):
    edge = cv.read_edges()[OLD]
    folder = Path(tempfile.mkdtemp(prefix='r82_clock_before_', dir=ROOT/'build'))
    paths = {'raw': ROOT/edge['body_o'], 'source': ROOT/edge['src'],
             'target': ROOT/f'build/GUNE5D/obj/{OLD}.o',
             'rules': ROOT/'config/GUNE5D/webfrank.json',
             'splits': ROOT/'config/GUNE5D/splits.txt',
             'compiler': ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe'}
    frozen = {n:p.read_bytes() for n,p in paths.items()}
    trace = fresh(edge, folder, 'baseline_control', frozen['raw'])
    artifacts = {}
    for n in ('source','raw','target','rules','splits'):
        p = folder/('r82_before_'+n+('.c' if n=='source' else '.bin'))
        p.write_bytes(frozen[n])
        artifacts[n] = p.relative_to(ROOT).as_posix()
    result = dict(schema_version=1, fidelity=True, edge=edge,
                  hashes={n:digest(b) for n,b in frozen.items()}, artifacts=artifacts,
                  inventory=canonical(paths['raw']), trace=trace)
    require(all(p.read_bytes()==frozen[n] for n,p in paths.items()), 'snapshot inputs changed')
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status='PASS', before=str(out), functions=len(result['inventory']['functions']),
                         raw_sha256=result['hashes']['raw'])))


def reference(inv, ref, add, depth=0):
    """Bind compiler-local symbols by complete datum or containing function.

    Named symbols never normalize by equal value. Anonymous text labels retain
    the containing function and interior offset. Relocated data includes its
    complete bytes and recursively resolved pointer atoms.
    """
    require(depth < 4, 'recursive/cyclic anonymous datum')
    if isinstance(ref, str):
        return ['named',ref,add]
    section, offset, size, info, other = ref
    if section == '.text':
        homes = [(n,f) for n,f in inv['functions'].items()
                 if f['offset'] <= offset+add < f['offset']+f['size']]
        require(len(homes)==1, 'unresolved anonymous code interior')
        n,f = homes[0]
        return ['code',n,offset+add-f['offset'],info,other]
    require(section in inv['sections'] and size > 0, 'unsupported anonymous datum')
    data = inv['sections'][section]
    require(data['bytes'] is not None and offset+size <= data['size'], 'datum out of bounds')
    relocs = [[p-offset,k,reference(inv,r,a,depth+1)] for p,k,r,a in data['relocations']
              if offset <= p < offset+size]
    return ['datum',section,size,info,other,data['bytes'][2*offset:2*(offset+size)],relocs,add]


def bindings(inv, fn):
    return [[p & ~3 if k==109 else p,k,reference(inv,r,a)]
            for p,k,r,a in fn['relocations']]


def check_clock(source, target, source_relocs, target_relocs):
    for inv in (source,target):
        require(set(inv['functions'])==set(CLOCK_FUNCTIONS), 'CLOCK function roster differs')
        require(set(inv['sections'])==set(SIZES), 'CLOCK allocated section roster differs')
        for n,(off,size) in CLOCK_FUNCTIONS.items():
            f=inv['functions'][n]
            require((f['offset'],f['size'],f['binding'])==(off,size,1), 'CLOCK function layout differs: '+n)
        for n,size in SIZES.items():
            s=inv['sections'][n]
            require(s['size']==size and s['type']==1 and len(bytes.fromhex(s['bytes']))==size,
                    'CLOCK section extent/type differs: '+n)
            alignment=s['alignment']
            require(alignment>0 and alignment&(alignment-1)==0 and BASES[n]%alignment==0,
                    'CLOCK incompatible alignment: '+n)
        require(inv['sections']['.sdata2']['bytes']==POOL, 'CLOCK pool bytes differ')
    for n in SIZES:
        require(source['sections'][n]['bytes']==target['sections'][n]['bytes'], 'CLOCK section bytes differ: '+n)
    require(source_relocs==target_relocs and set(source_relocs)==set(SIZES), 'CLOCK final-address bindings differ')
    require(len(source['exception_records'])==len(target['exception_records'])==1, 'CLOCK EH coverage differs')
    return dict(raw_instructions={n:size//4 for n,(_,size) in CLOCK_FUNCTIONS.items()},
                allocated_bytes=sum(SIZES.values()), final_relocations=sum(map(len,source_relocs.values())))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--snapshot',action='store_true')
    ap.add_argument('--experiment',action='store_true')
    ap.add_argument('--before',type=Path)
    ap.add_argument('--out',type=Path,required=True)
    a=ap.parse_args()
    if a.snapshot:
        snapshot(a.out)
    elif a.experiment:
        require(a.before is not None,'--before required')
        experiment(a.before,a.out)
    else:
        ap.error('choose --snapshot or --experiment')


if __name__=='__main__':
    main()
