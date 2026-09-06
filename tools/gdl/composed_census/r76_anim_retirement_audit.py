"""Strict finite InitAnim literal/pool ownership retirement evidence.

IMPORTABLE CORE: audit, bind.
Manual evidence only; never a build hook or object rewrite. The comparison
preserves every prior verified linked binding, with precisely 8 pool bytes
moving from extraction to source ownership. Full ninja remains separate.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
UNIT='game/anim/anim'; FN='InitAnim'
K='3fa1111110c7c411'; MAGIC='4330000080000000'
def bind(value,base):
    if isinstance(value,list):
        if len(value)==5 and value[0]=='.sdata2' and value[2:]==[8,1,0]:
            return ['ABSOLUTE-POOL',base+value[1],8]
        return [bind(v,base) for v in value]
    if value=='lbl_803457C0': return ['ABSOLUTE-POOL',0x803457C0,8]
    return value
def audit(baseline,candidate,target):
    # Baseline is the verified processed object. No baseline payload replaced.
    if baseline['functions'].keys()!=candidate['functions'].keys(): raise ValueError('function roster')
    for n,a in baseline['functions'].items():
        b=candidate['functions'][n]
        if {k:v for k,v in a.items() if k!='relocations'}!={k:v for k,v in b.items() if k!='relocations'}:
            raise ValueError('function text/layout '+n)
        if bind(a['relocations'],0x803457C8)!=bind(b['relocations'],0x803457C0): raise ValueError('function bindings '+n)
    if candidate['functions'][FN]['body']!=target['functions'][FN]['body']: raise ValueError('InitAnim target body')
    if baseline['sections'].keys()!=candidate['sections'].keys(): raise ValueError('section roster')
    for n,a in baseline['sections'].items():
        if n=='.sdata2': continue
        b=candidate['sections'][n]
        if {k:v for k,v in a.items() if k!='relocations'}!={k:v for k,v in b.items() if k!='relocations'}: raise ValueError('section payload '+n)
        if bind(a['relocations'],0x803457C8)!=bind(b['relocations'],0x803457C0): raise ValueError('section bindings '+n)
    if baseline['exception_records']!=candidate['exception_records']: raise ValueError('exception metadata')
    a=baseline['sections']['.sdata2']; b=candidate['sections']['.sdata2']
    if a['bytes']!=MAGIC or a['size']!=8 or b['bytes']!=K+MAGIC or b['size']!=16: raise ValueError('pool extent/payload')
    if a['relocations'] or b['relocations'] or a['alignment']!=8 or b['alignment']!=8: raise ValueError('pool alignment/bindings')
    if {k:v for k,v in a.items() if k not in ('size','bytes')}!={k:v for k,v in b.items() if k not in ('size','bytes')}: raise ValueError('pool flags/type')
    def symbols(inv):
        return [s for s in inv['all_symbols'] if not (isinstance(s[0],list) and s[0][0]=='.sdata2') and s[0]!='lbl_803457C0']
    if symbols(baseline)!=symbols(candidate): raise ValueError('nonpool symbols')
    before=[s for s in baseline['all_symbols'] if isinstance(s[0],list) and s[0][0]=='.sdata2']
    after=[s for s in candidate['all_symbols'] if isinstance(s[0],list) and s[0][0]=='.sdata2']
    expected_before=[[[ '.sdata2',0,8,1,0],'.sdata2',0,8,1,0]]
    expected_after=[[[ '.sdata2',off,8,1,0],'.sdata2',off,8,1,0] for off in (0,8)]
    if before!=expected_before or after!=expected_after: raise ValueError('pool symbol roster')
    external_before=[s for s in baseline['all_symbols'] if s[0]=='lbl_803457C0']
    external_after=[s for s in candidate['all_symbols'] if s[0]=='lbl_803457C0']
    if len(external_before)!=1 or external_before[0][1:]!=['',0,0,16,0] or external_after: raise ValueError('external constant ownership')
    return dict(status='PASS',functions=len(candidate['functions']),raw_exact_instructions=106,
                unchanged_sibling_bodies=len(candidate['functions'])-1,pool_bytes=16,source_owned_new_bytes=8,
                old_pool_start='0x803457C8',new_pool_start='0x803457C0',all_final_relocation_bindings_preserved=True,
                exception_metadata_preserved=True,scope='Manual finite ownership-aware object audit; full DOL and actual split gates separate')
def main(argv=None):
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-dir',type=Path,required=True)
    ap.add_argument('--out',type=Path,default=ROOT/'build/r76_anim_retirement.json')
    args=ap.parse_args(argv)
    folder=args.baseline_dir.resolve()
    outpath=args.out.resolve()
    if not outpath.is_relative_to((ROOT/'build').resolve()) or not outpath.name.startswith('r76_anim_'):
        ap.error('output must be under build/ with r76_anim_ basename')
    edge=cv.read_edges()[UNIT]
    if edge['mw']!='GC/1.2.5' or edge['raw']:
        raise ValueError('stock raw compiler edge without postprocessor required')
    paths=[ROOT/edge['src'],ROOT/edge['body_o'],ROOT/'config/GUNE5D/splits.txt',ROOT/'config/GUNE5D/webfrank.json']
    frozen={p:p.read_bytes() for p in paths}
    oldsource=(folder/'r76_anim_original.c').read_text()
    wanted=oldsource.replace('extern const f64 lbl_803457C0;\n','').replace('lbl_803457C0','0.0333333333')
    if oldsource.count('lbl_803457C0')!=6 or paths[0].read_text()!=wanted:
        raise ValueError('precisely five read substitutions and extern removal required')
    oldsplit=(folder/'r76_anim_splits.txt').read_text()
    start=oldsplit.index('game/anim/anim.c:\n')
    stop=oldsplit.index('\n\n',start)
    block=oldsplit[start:stop]
    if block.count('start:0x803457C8 end:0x803457D0')!=1:
        raise ValueError('old pool claim')
    expectsplit=oldsplit[:start]+block.replace('start:0x803457C8','start:0x803457C0')+oldsplit[stop:]
    if paths[2].read_text()!=expectsplit:
        raise ValueError('split edit exceeds precise 8-byte recovery')
    rules=json.loads((folder/'r76_anim_rules.json').read_text())
    if len(rules['units'][UNIT])!=1 or rules['units'][UNIT][0]['function']!=FN:
        raise ValueError('expected exactly one old rule')
    del rules['units'][UNIT]
    if json.loads(paths[3].read_text())!=rules:
        raise ValueError('rule edits exceed anim retirement')
    fidelity=Path(tempfile.mkdtemp(prefix='r76_anim_fidelity_',dir=ROOT/'build'))
    traces={}
    for name,src,expected in [('before',oldsource,(folder/'r76_anim_raw.o').read_bytes()),('after',paths[0].read_text(),frozen[paths[1]])]:
        d=fidelity/name; d.mkdir()
        p=d/'anim.c'; p.write_text(src)
        e=dict(edge,src=str(p.relative_to(ROOT)),_command_trace=[])
        obj,error=cv.compile_with(e,edge['mw'],edge['cflags'],d/'r76_anim.o',d)
        if error or obj is None or obj.read_bytes()!=expected:
            raise ValueError(error or 'complete raw fidelity '+name)
        traces[name]=e['_command_trace']
    target=canonical(ROOT/f'build/GUNE5D/obj/{UNIT}.o')
    current=canonical(paths[1])
    if target['sections']['.sdata2']['bytes']!=K+MAGIC:
        raise ValueError('new extracted ownership bytes differ')
    result=audit(canonical(folder/'r76_anim_processed.o'),current,target)
    result.update(complete_before_after_raw_fidelity=True,compiler=edge['mw'],traces=traces,
                  evidence_directory=str(fidelity.relative_to(ROOT)))
    if any(p.read_bytes()!=b for p,b in frozen.items()): raise ValueError('production drift')
    outpath.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='traces'},indent=2))
    return 0
if __name__=='__main__': raise SystemExit(main())
