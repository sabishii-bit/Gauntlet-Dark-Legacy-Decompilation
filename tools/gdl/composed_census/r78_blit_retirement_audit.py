"""Manual DrawBlit retirement audit; no build hook or object rewriting.

Certifies compiler instruction bytes, positional datums, and preservation of
the prior object apart from the explicit pool insertion. This TU is still
NonMatching: neither its pool layout nor final linked addresses are certified.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.pooldump import dol_read

UNIT = 'game/mb/mb_blit'
FN = 'DrawBlit'


def checked_output(path, root=ROOT):
    path = Path(path).resolve()
    if not path.is_relative_to((root/'build').resolve()) or not path.name.startswith('r78_blit_'):
        raise ValueError('output must be r78_blit_-prefixed under build/')
    return path


def verify_archive(folder, manifest):
    named = {'source':('r78_blit_baseline.c','src/game/mb/mb_blit.c'),
             'raw':('r78_blit_raw.o',manifest['edge']['body_o']),
             'target':('r78_blit_target.o','build/GUNE5D/obj/game/mb/mb_blit.o'),
             'rules':('r78_blit_rules.json','config/GUNE5D/webfrank.json'),
             'splits':('r78_blit_splits.txt','config/GUNE5D/splits.txt')}
    hashes = {k.replace('\\','/'):v for k,v in manifest['input_hashes'].items()}
    for label,(filename,key) in named.items():
        actual = hashlib.sha256((folder/filename).read_bytes()).hexdigest()
        if hashes.get(key.replace('\\','/')) != actual:
            raise ValueError('archive hash '+label)
    capture = json.loads((folder/'r78_blit_capture.json').read_text())
    if capture['baseline_commit'] != 'be899a902' or capture['processed_sha256'] != '3b800c4338f718f9dab04608bf3f923f201d95ade31709d2c9f5012149336f72':
        raise ValueError('baseline capture anchor')
    if hashlib.sha256((folder/'r78_blit_processed.o').read_bytes()).hexdigest() != capture['processed_sha256']:
        raise ValueError('archive hash processed')
    return capture


def old_pool(value):
    if isinstance(value, list) and len(value) == 5 and value[0] == '.sdata2':
        value = value.copy()
        if value[1] >= 88:
            value[1] -= 8
    return value


def bindings(rows, new, start, size):
    result = []
    for off, kind, symbol, addend in rows:
        inside = start <= off < start + size
        if new and symbol == ['.sdata2', 80, 4, 1, 0]:
            if not inside:
                raise ValueError('new UV pool reference outside DrawBlit')
            symbol = 'UV-LITERAL'
        elif new:
            symbol = old_pool(symbol)
        if inside and symbol == 'lbl_80348AD4':
            symbol = ['.sdata2', 68, 4, 1, 0]
        if inside and symbol == 'lbl_80348AE0':
            symbol = 'UV-LITERAL'
        result.append([off, kind, symbol, addend])
    return result


def audit(before, after, target):
    if before['functions'].keys() != after['functions'].keys():
        raise ValueError('function roster')
    fn = after['functions'][FN]
    if fn['size'] != 1500 or fn['body'] != target['functions'][FN]['body']:
        raise ValueError('DrawBlit raw instruction bytes')
    for name, old in before['functions'].items():
        new = after['functions'][name]
        if {k:v for k,v in old.items() if k != 'relocations'} != {k:v for k,v in new.items() if k != 'relocations'}:
            raise ValueError('function bytes/layout '+name)
        size = 1500 if name == FN else 0
        if bindings(old['relocations'], False, 0, size) != bindings(new['relocations'], True, 0, size):
            raise ValueError('function relocation identities '+name)
    if before['sections'].keys() != after['sections'].keys():
        raise ValueError('section roster')
    for name, old in before['sections'].items():
        new = after['sections'][name]
        if name == '.sdata2':
            expected = copy.deepcopy(old)
            payload = bytes.fromhex(old['bytes'])
            if old['size'] != 144:
                raise ValueError('baseline pool extent')
            expected['bytes'] = (payload[:80] + bytes.fromhex('3d80000000000000') + payload[80:]).hex()
            expected['size'] = 152
            if expected != new:
                raise ValueError('exact UV insertion and padding')
        else:
            if {k:v for k,v in old.items() if k != 'relocations'} != {k:v for k,v in new.items() if k != 'relocations'}:
                raise ValueError('section payload/layout '+name)
            start, size = (fn['offset'], 1500) if name == '.text' else (0, 0)
            if bindings(old['relocations'],False,start,size) != bindings(new['relocations'],True,start,size):
                raise ValueError('section relocation identities '+name)
    if before['exception_records'] != after['exception_records']:
        raise ValueError('exception records')
    normalized = []
    uv = [['.sdata2',80,4,1,0],'.sdata2',80,4,1,0]
    if after['all_symbols'].count(uv) != 1:
        raise ValueError('new UV symbol roster')
    for row in after['all_symbols']:
        if row == uv:
            continue
        row = copy.deepcopy(row)
        row[0] = old_pool(row[0])
        if row[1] == '.sdata2' and row[2] >= 88:
            row[2] -= 8
        normalized.append(row)
    removed = ['lbl_80348AE0','',0,0,16,0]
    if before['all_symbols'].count(removed) != 1:
        raise ValueError('baseline UV external')
    prior = [s for s in before['all_symbols'] if s != removed]
    if sorted(prior,key=repr) != sorted(normalized,key=repr):
        raise ValueError('complete symbol identities/layout')
    pool_targets = {8:('lbl_80348A88',8),40:('lbl_80348A98',8),
                    68:('lbl_80348AD4',4),80:('lbl_80348AE0',4)}
    positional = []
    datum_proofs = []
    payload = bytes.fromhex(after['sections']['.sdata2']['bytes'])
    for off,kind,symbol,addend in fn['relocations']:
        if isinstance(symbol,list):
            if symbol[0] != '.sdata2' or symbol[1] not in pool_targets:
                raise ValueError('uncovered DrawBlit pool binding')
            label,size = pool_targets[symbol[1]]
            if symbol != ['.sdata2',symbol[1],size,1,0] or addend != 0:
                raise ValueError('pool symbol type/extent/addend')
            actual = payload[symbol[1]:symbol[1]+size]
            wanted = dol_read(int(label[4:],16),size)
            if actual != wanted:
                raise ValueError('retail datum '+label)
            datum_proofs.append(dict(offset=off & ~3, target=label, bytes=actual.hex()))
            symbol = label
        positional.append([off & ~3 if kind == 109 else off,kind,symbol,addend])
    if positional != target['functions'][FN]['relocations']:
        raise ValueError('all positional target relocation bindings')
    return dict(status='PASS', instruction_words=375, sibling_bodies_preserved=len(after['functions'])-1,
                datum_proofs=datum_proofs, complete_prior_object_identity_preserved_except_pool_insertion=True,
                exception_metadata_preserved=True, old_pool_bytes=144, new_pool_bytes=152,
                scope='NonMatching TU; compiler body and positional datums only, not final linked addresses or TU data matching')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir',type=Path,required=True)
    parser.add_argument('--out',type=Path,default=ROOT/'build/r78_blit_retirement.json')
    args = parser.parse_args()
    if Path.cwd().resolve() != ROOT:
        raise ValueError('run from repository root')
    output = checked_output(args.out)
    folder = args.baseline_dir.resolve()
    edge = cv.read_edges()[UNIT]
    manifest = json.loads((folder/'r78_blit_report.json').read_text())
    capture = verify_archive(folder, manifest)
    for key in ('mw','cflags','command_template','rule','extab_padding'):
        if edge[key] != manifest['edge'][key]:
            raise ValueError('compiler edge changed '+key)
    if edge['mw'] != 'GC/1.2.5':
        raise ValueError('stock compiler required')
    original_hashes = {k.replace('\\','/'):v for k,v in manifest['input_hashes'].items()}
    for key in ('build/compilers/'+edge['mw']+'/mwcceppc.exe',
                'build/GUNE5D/obj/'+UNIT+'.o'):
        if hashlib.sha256((ROOT/key).read_bytes()).hexdigest() != original_hashes.get(key):
            raise ValueError('baseline compiler/target identity changed '+key)
    raw = ROOT/edge['body_o']
    paths = [ROOT/edge['src'],raw,ROOT/f'build/GUNE5D/src/{UNIT}.o',ROOT/f'build/GUNE5D/obj/{UNIT}.o',
             ROOT/'config/GUNE5D/webfrank.json',ROOT/'build.ninja',ROOT/'config/GUNE5D/splits.txt',
             ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe',ROOT/'orig/GUNE5D/sys/main.dol']
    frozen = {p:p.read_bytes() for p in paths}
    scratch = Path(tempfile.mkdtemp(prefix='r78_blit_fidelity_',dir=ROOT/'build'))
    traces = {}
    for name,source,expected in [('before',folder/'r78_blit_baseline.c',(folder/'r78_blit_raw.o').read_bytes()),
                                 ('after',ROOT/edge['src'],raw.read_bytes())]:
        sub = scratch/name
        sub.mkdir()
        renamed = sub/'mb_blit.c'
        renamed.write_bytes(source.read_bytes())
        trial = dict(edge,src=str(renamed),_command_trace=[])
        obj,error = cv.compile_with(trial,edge['mw'],edge['cflags'],scratch/f'r78_blit_{name}.o',scratch)
        if error or obj is None or obj.read_bytes() != expected:
            raise ValueError(error or 'complete raw ELF fidelity '+name)
        traces[name]=trial['_command_trace']
    config = json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text())
    oldconfig = json.loads((folder/'r78_blit_rules.json').read_text())
    expected = [r for r in oldconfig['units'][UNIT] if r['function'] != FN]
    if len(expected)!=1 or config['units'][UNIT] != expected:
        raise ValueError('remaining pin changed or DrawBlit still pinned')
    if (ROOT/'config/GUNE5D/splits.txt').read_bytes() != (folder/'r78_blit_splits.txt').read_bytes():
        raise ValueError('data ownership changed')
    current = canonical(ROOT/f'build/GUNE5D/src/{UNIT}.o')
    result = audit(canonical(folder/'r78_blit_processed.o'),current,canonical(ROOT/f'build/GUNE5D/obj/{UNIT}.o'))
    if canonical(raw)['functions'][FN] != current['functions'][FN]:
        raise ValueError('DrawBlit changed after raw compiler output')
    if any(p.read_bytes()!=blob for p,blob in frozen.items()):
        raise ValueError('input drift')
    result.update(complete_before_after_raw_fidelity=True, baseline_capture=capture, traces=traces,
                  sha256={str(p.relative_to(ROOT)):hashlib.sha256(b).hexdigest() for p,b in frozen.items()})
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('traces','sha256')},indent=2))


if __name__ == '__main__':
    main()
