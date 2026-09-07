"""Manual DoWorldAnimation native-induction certificate, never a build hook.

Verify immutable complete-TU controls and actual Ninja compiler fidelity;
only the target body changes in raw output, and processed allocated bytes,
symbols, positional relocation bindings and EH metadata remain identical.
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
from tools.gdl.composed_census.r84_player_retirement_audit import substitute_bodies

UNIT,FN='game/world/gauntworld','DoWorldAnimation'
MANIFEST_SHA='6499a3dceed1467872e113b20d39922d4618cedb6bcb490abf638285a27a5807'
ORIGINAL_AFTER_CONFIG_SHA='e72df32915b1595f0c2062f0efcb6c4b1d006b6929b638a68adb239268fd068f'
TYPE_COMMENT='''/* Complete the worldinfo.h forward declarations with the layouts already
 * used by world.c. GC allocation/swap loops confirm the 0x10/0xA0 strides;
 * the track's keyframe-data pointer is at +0x0C. */'''
FUNCTION_COMMENT='/* Advance every active world-object animation track. */'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(condition,message):
    if not condition:
        raise ValueError(message)


def rule_delta(before,after):
    expected=copy.deepcopy(before)
    rows=expected['units'][UNIT]
    require(sum(r['function']==FN for r in rows)==1,'one prior pin required')
    expected['units'][UNIT]=[r for r in rows if r['function']!=FN]
    require(expected==after,'configuration exceeds one pin retirement')


def current_rule_scope(original_after,current):
    """Certify the owned unit; explicitly exclude reviewed foreign integrations."""
    require({k:v for k,v in original_after.items() if k!='units'}==
            {k:v for k,v in current.items() if k!='units'},
            'top-level configuration changed')
    require(current.get('units',{}).get(UNIT)==original_after['units'][UNIT],
            'current owned-unit rules changed')
    foreign=[]
    for unit in sorted(set(original_after['units'])|set(current['units'])):
        a=original_after['units'].get(unit)
        b=current['units'].get(unit)
        if unit!=UNIT and a!=b:
            foreign.append(dict(unit=unit,certified_here=False,
                                original_functions=[r['function'] for r in a or []],
                                current_functions=[r['function'] for r in b or []]))
    return foreign


def positional_target(current,target):
    require(current['size']==target['size']==252 and current['body']==target['body'],
            'DoWorldAnimation is not native byte exact')
    def relocs(rows):
        # Same instruction, encoding convention only: MWCC SDA21 uses +2,
        # extracted target uses +0. Names, kind, addend and other offsets stay.
        return sorted((off & ~3 if kind==109 else off,kind,name,add)
                      for off,kind,name,add in rows)
    require(len(current['relocations'])==12,'unexpected binding population')
    require(relocs(current['relocations'])==relocs(target['relocations']),
            'target positional relocation differs')


def allocation_proof(before_raw,before_processed,after_raw,after_processed,target):
    substitute_bodies(before_raw,after_raw,(FN,))
    require(before_processed==after_processed,'processed allocated object changed')
    positional_target(after_raw['functions'][FN],target['functions'][FN])
    positional_target(after_processed['functions'][FN],target['functions'][FN])
    a=bytes.fromhex(before_raw['functions'][FN]['body'])
    b=bytes.fromhex(after_raw['functions'][FN]['body'])
    require(sum(a[i:i+4]!=b[i:i+4] for i in range(0,len(a),4))==13,
            'unexpected raw repair count')
    return dict(raw_target_words=0,raw_repaired_words=13,
                raw_siblings_unchanged=len(after_raw['functions'])-1,
                processed_allocated_object_equal=True,raw_nontext_symbols_EH_equal=True,
                target_positionally_bound_relocations=12)


def audit(archive,integration=None):
    mpath=archive/'r85_worldanim_manifest.json'
    require(sha(mpath.read_bytes())==MANIFEST_SHA,'untrusted control manifest')
    manifest=json.loads(mpath.read_text())
    suffixes=dict(source='.c',raw='.o',processed='.o',target='.o',rules='.json',
                  splits='.txt',symbols='.txt',compiler='.exe')
    paths={key:archive/('r85_worldanim_'+key+suffix) for key,suffix in suffixes.items()}
    for key,path in paths.items():
        require(sha(path.read_bytes())==manifest['hashes'][key],'archived input changed: '+key)
    for name,digest in manifest['header_hashes'].items():
        require(sha((ROOT/name).read_bytes())==digest,'header changed: '+name)
    for row in manifest['rows']:
        folder=archive/row['label']
        require(row['error'] is None,'failed archived control')
        require(sha((folder/'gauntworld.c').read_bytes())==row['source_sha256'],'control source changed')
        require(sha((folder/'r85_worldanim.o').read_bytes())==row['raw_sha256'],'control object changed')
    baseline=manifest['rows'][0]
    require(baseline['label']=='baseline' and baseline['baseline_elf_equal'] is True,
            'missing complete ELF baseline fidelity')
    require((archive/'baseline/r85_worldanim.o').read_bytes()==paths['raw'].read_bytes(),
            'archived baseline ELF differs')
    chosen=(archive/'index_both/gauntworld.c').read_text()
    expected=chosen.replace(FUNCTION_COMMENT+'\nstruct worldanim',TYPE_COMMENT+'\nstruct worldanim')
    expected=expected.replace('\nvoid DoWorldAnimation(void)\n{','\n'+FUNCTION_COMMENT+'\nvoid DoWorldAnimation(void)\n{')
    require((ROOT/'src/game/world/gauntworld.c').read_text()==expected,
            'source exceeds verified typed-array control plus comments')
    edge=cv.read_edges()[UNIT]
    require(edge==manifest['edge'],'actual Ninja edge changed')
    compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe'
    require(sha(compiler.read_bytes())==manifest['hashes']['compiler'],'compiler changed')
    for key in ('target','splits','symbols'):
        current=ROOT/(f'build/GUNE5D/obj/{UNIT}.o' if key=='target' else f'config/GUNE5D/{key}.txt')
        require(sha(current.read_bytes())==manifest['hashes'][key],'current input changed: '+key)
    scratch=Path(tempfile.mkdtemp(prefix='r85_worldanim_fidelity_',dir=ROOT/'build'))
    fresh=dict(edge,_command_trace=[])
    obj,error=cv.compile_with(fresh,edge['mw'],edge['cflags'],scratch/'r85_worldanim.o',scratch)
    require(error is None and obj is not None,error or 'fresh compiler failed')
    raw=ROOT/edge['body_o']; processed=ROOT/f'build/GUNE5D/src/{UNIT}.o'
    require(obj.read_bytes()==raw.read_bytes(),'current complete ELF fidelity failed')
    require(canonical(archive/'index_both/r85_worldanim.o')==canonical(raw),'installed control drift')
    result=allocation_proof(canonical(paths['raw']),canonical(paths['processed']),
                            canonical(raw),canonical(processed),canonical(paths['target']))
    old=json.loads(paths['rules'].read_text()); new=json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text())
    integration=integration or ROOT/'build/r85_worldanim_integration'
    original_after_path=integration/'r85_worldanim_original_after_rules.json'
    require(sha(original_after_path.read_bytes())==ORIGINAL_AFTER_CONFIG_SHA,
            'untrusted original after-config')
    original_after=json.loads(original_after_path.read_text())
    rule_delta(old,original_after)
    foreign=current_rule_scope(original_after,new)
    return dict(schema_version=1,status='PASS',function=FN,raw_instructions=63,**result,
                compiler=edge['mw'],compiler_sha256=manifest['hashes']['compiler'],
                flags=edge['cflags'],fresh_command_trace=fresh['_command_trace'],
                retired_rules=1,remaining_unit_rules=[r['function'] for r in new['units'][UNIT]],
                original_full_config_retirement_proven=True,
                original_after_config_sha256=ORIGINAL_AFTER_CONFIG_SHA,
                current_owned_unit_and_top_level_config_equal=True,
                unrelated_config_deltas_not_certified=foreign,
                raw_sha256=sha(raw.read_bytes()),processed_sha256=sha(processed.read_bytes()),
                limitations=['Not a whole gauntworld.c target-match claim.',
                             'Unrelated unit configuration deltas require separate integration review.',
                             'Compiler induction-identity mechanism inferred from joint controls, not compiler IR.',
                             'Full Ninja/DOL gate remains separate.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive',type=Path,default=ROOT/'build/r85_worldanim_archive')
    p.add_argument('--integration',type=Path,default=ROOT/'build/r85_worldanim_integration')
    p.add_argument('--out',type=Path,default=ROOT/'build/r85_worldanim_retirement_audit.json')
    a=p.parse_args()
    result=audit(a.archive,a.integration)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('fresh_command_trace','flags')},indent=2))


if __name__=='__main__':
    main()
