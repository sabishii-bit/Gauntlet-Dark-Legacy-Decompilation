"""Manual ExpToLevel existing-helper retirement certificate; not a build hook.

Verify the immutable full-TU control archive, fresh actual-Ninja fidelity,
all allocated bytes/layout/positional relocation bindings and existing pins.
The sole changed sibling must move strictly toward target at every changed bit.
Full Ninja/DOL and fresh-report fuzzy gates remain separate requirements.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical

UNIT,FN,CALLER='game/game/player','ExpToLevel','player_store_in_save'
MANIFEST_SHA='c48b6080a241342ab9c5de3e3687dd369b27199d2564f6eb14a15c917581b906'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(condition,message):
    if not condition:
        raise ValueError(message)


def monotonic_words(before,after,target):
    require(len(before)==len(after)==len(target) and len(before)%4==0,'word count differs')
    changed=became_exact=0
    for i in range(0,len(before),4):
        a,b,t=(int.from_bytes(x[i:i+4],'big') for x in (before,after,target))
        if a==b:
            continue
        require(((a^t)&(b^t))==(b^t),'changed word introduced non-target bits at '+hex(i))
        changed+=1
        became_exact+=b==t
    return dict(changed_words=changed,new_exact_words=became_exact)


def substitute_bodies(before,after,names):
    """Only listed function bodies may change, including section coverage."""
    expected=copy.deepcopy(before)
    text=bytearray.fromhex(expected['sections']['.text']['bytes'])
    for name in names:
        a=expected['functions'][name]
        b=after['functions'][name]
        require(a['size']==b['size'] and a['offset']==b['offset'],'function layout changed: '+name)
        body=bytes.fromhex(b['body'])
        require(len(body)==a['size'],'body size differs: '+name)
        a['body']=b['body']
        text[a['offset']:a['offset']+a['size']]=body
    expected['sections']['.text']['bytes']=text.hex()
    require(expected==after,'unexpected allocated bytes, symbol/layout, positional relocation or EH change')


def rule_delta(before,after):
    expected=copy.deepcopy(before)
    rows=expected['units'][UNIT]
    require(sum(r['function']==FN for r in rows)==1,'one old ExpToLevel pin required')
    expected['units'][UNIT]=[r for r in rows if r['function']!=FN]
    actual=copy.deepcopy(after)
    a=next(r for r in expected['units'][UNIT] if r['function']=='do_exit')
    b=next(r for r in actual['units'][UNIT] if r['function']=='do_exit')
    def erase_hashes(value):
        if isinstance(value,dict):
            for key in ('before_relocations_sha256','after_relocations_sha256'):
                if key in value:
                    value[key]='RELLOCATION_NAME_REFRESH'
            for child in value.values():
                erase_hashes(child)
        elif isinstance(value,list):
            for child in value:
                erase_hashes(child)
    erase_hashes(a)
    erase_hashes(b)
    require(expected==actual,'change exceeds one retirement and do_exit relocation-name hashes')


def audit(archive):
    mpath=archive/'r84_player_manifest.json'
    require(sha(mpath.read_bytes())==MANIFEST_SHA,'untrusted experiment manifest')
    manifest=json.loads(mpath.read_text())
    suffixes=dict(source='.c',raw='.o',processed='.o',target='.o',rules='.json',splits='.txt',symbols='.txt',compiler='.exe')
    paths={key:archive/('r84_player_'+key+suffix) for key,suffix in suffixes.items()}
    for key,path in paths.items():
        require(sha(path.read_bytes())==manifest['hashes'][key],'changed archived '+key)
    for name,digest in manifest['header_hashes'].items():
        require(sha((ROOT/name).read_bytes())==digest,'header changed: '+name)
    baseline,chosen=manifest['rows'][:2]
    require(baseline['baseline_elf_equal'] is True and baseline['error'] is None,'no archived baseline fidelity')
    require(chosen['label']=='existing_helper' and chosen['words']==[],'missing exact helper control')
    source=(ROOT/'src/game/game/player.c').read_text().replace('\r\n','\n')
    old=paths['source'].read_text().replace('\r\n','\n')
    require(old.count(baseline['probed_body'])==1,'original function ambiguous')
    expected=old.replace(baseline['probed_body'],chosen['probed_body'])
    expected=expected.replace('/* Inverse of LevelToExp: scan 99..1 for the level exp buys (running\n * rate = lv*30 maintained incrementally, 99-step guard). */','/* Inverse of LevelToExp: scan 99..1 for the level exp buys, using the\n * shared level curve. */')
    require(source==expected,'source change is not precisely existing helper consumption')
    edge=cv.read_edges()[UNIT]
    require(edge==manifest['edge'],'actual Ninja edge changed')
    compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe'
    require(sha(compiler.read_bytes())==manifest['hashes']['compiler'],'compiler changed')
    for key in ('target','splits','symbols'):
        current=ROOT/(f'build/GUNE5D/obj/{UNIT}.o' if key=='target' else f'config/GUNE5D/{key}.txt')
        require(sha(current.read_bytes())==manifest['hashes'][key],'changed current '+key)
    scratch=Path(tempfile.mkdtemp(prefix='r84_player_fidelity_',dir=ROOT/'build'))
    fresh=dict(edge,_command_trace=[])
    obj,error=cv.compile_with(fresh,edge['mw'],edge['cflags'],scratch/'r84_player.o',scratch)
    require(error is None and obj is not None,error or 'fresh compile failed')
    current_raw=ROOT/edge['body_o']
    require(obj.read_bytes()==current_raw.read_bytes(),'current complete ELF fidelity failed')
    current_processed=ROOT/f'build/GUNE5D/src/{UNIT}.o'
    before={key:canonical(paths[key]) for key in ('raw','processed','target')}
    after={key:canonical(path) for key,path in [('raw',current_raw),('processed',current_processed)]}
    target=before['target']['functions']
    for stage in ('raw','processed'):
        f=after[stage]['functions'][FN]
        require(f['size']==96 and f['body']==target[FN]['body'],'ExpToLevel not raw target-exact')
        require(f['relocations']==target[FN]['relocations']==[],'unexpected ExpToLevel relocations')
    substitute_bodies(before['raw'],after['raw'],(FN,CALLER))
    substitute_bodies(before['processed'],after['processed'],(CALLER,))
    a,b,t=(bytes.fromhex(v['functions'][CALLER]['body']) for v in (before['raw'],after['raw'],before['target']))
    change=monotonic_words(a,b,t)
    require(len(a)==524 and change==dict(changed_words=24,new_exact_words=22),'unexpected caller improvement')
    old_rules=json.loads(paths['rules'].read_text())
    new_rules=json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text())
    rule_delta(old_rules,new_rules)
    pins=[r['function'] for r in new_rules['units'][UNIT]]
    require(len(pins)==6,'unexpected remaining player pins')
    for name in pins:
        for stage in ('raw','processed'):
            require(before[stage]['functions'][name]==after[stage]['functions'][name],'changed pin body/bindings: '+name)
    return dict(schema_version=1,status='PASS',function=FN,compiler=edge['mw'],compiler_sha256=sha(compiler.read_bytes()),flags=edge['cflags'],archive_manifest_sha256=MANIFEST_SHA,current_fidelity=True,raw_target_words=0,raw_instructions=24,untouched_siblings=len(after['raw']['functions'])-2,caller=CALLER,caller_improvement=change,remaining_pins=pins,raw_nontext_symbols_positional_relocations_EH_unchanged=True,processed_object_change_only_caller_body=True,trace=fresh['_command_trace'],current_raw_sha256=sha(current_raw.read_bytes()),current_processed_sha256=sha(current_processed.read_bytes()),limitations=['Not a whole player.c target-match certificate.','Original helper provenance and compiler-internal causality are not established.','Fresh report fuzzy and full Ninja/DOL gates are separately required.'])


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--archive',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args()
    require(args.out.resolve().is_relative_to((ROOT/'build').resolve()) and args.out.name.startswith('r84_player_'),'output must be lane-prefixed under build')
    result=audit(args.archive)
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='trace'},indent=2))


if __name__=='__main__':
    main()
