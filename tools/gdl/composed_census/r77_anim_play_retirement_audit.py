"""Manual source-native anim_play retirement evidence, never a build hook.

Captures the frozen pre-retirement objects, then audits precisely the recovered
literal/local/conditional source form against all previously linked bytes and
bindings. Requires a completed Ninja build; full DOL verification is separate.
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
from tools.gdl.pooldump import dol_read

UNIT = 'game/anim/anim_play'
FN = 'GetAnimAngXYZVal'
MAGIC = '4330000080000000'
POOL = ('0000000000000000400921fb54524550401921fb54524550c00921fb54524550'
        '3f8000000000000000000000000000003fc0000000000000bff921fb54524550'
        '3ff921fb5452455043300000800000003ff0000000000000')
LITERALS = {'lbl_803457F0': '0.0f', 'lbl_803457F8': '3.141592654',
            'lbl_80345800': '6.283185308', 'lbl_80345808': '-3.141592654',
            'lbl_80345810': '1.0f', 'lbl_80345818': '0.0',
            'lbl_80345820': '0.125', 'lbl_80345828': '-1.570796327',
            'lbl_80345830': '1.570796327', 'lbl_80345840': '1.0'}


def recovered_source(text):
    """One finite, reviewed source transformation, not a source search."""
    for line in ('extern const f32 lbl_803457F0;\n',
                 'extern f64 lbl_803457F8, lbl_80345800, lbl_80345808;\n',
                 'extern const f32 lbl_80345810;\n',
                 'extern f64 lbl_80345818, lbl_80345820, lbl_80345828, lbl_80345830;\n',
                 'extern f64 lbl_80345838;\n', 'extern f64 lbl_80345840;\n'):
        if text.count(line) != 1:
            raise ValueError('source declaration anchor: '+line.strip())
        text = text.replace(line, '')
    for label, literal in LITERALS.items():
        text = re.sub(r'\b'+label+r'\b', '('+literal+')', text)
    for fn, val in [('GetPYR', '0.0f'), ('GetXYZ', '0.0f'), ('GetScale', '1.0f')]:
        a = text.index('static inline s32 '+fn+'(')
        b = text.index('\n}', a)+2
        body = text[a:b]
        declaration = '    f32 dflt = ('+val+');\n'
        if body.count(declaration) != 1:
            raise ValueError('default-local anchor '+fn)
        body = body.replace(declaration, '').replace('dst[i] = dflt;', 'dst[i] = '+val+';')
        text = text[:a]+body+text[b:]
    for declaration in ('        f32 one = (1.0f);\n', '        f32 zero = (0.0f);\n'):
        if text.count(declaration) != 2:
            raise ValueError('loop-local anchor')
        text = text.replace(declaration, '')
    a = text.index('s32 GetAnimAngXYZVal(', text.index('static inline'))
    b = text.index('/* fn_80010850', a)
    body = re.sub(r'\bone\b', '1.0f', text[a:b])
    body = re.sub(r'\bzero\b', '0.0f', body)
    for var, literal in [('cycle', '6.283185308'), ('lower', '-3.141592654'), ('upper', '3.141592654')]:
        for line in ('        f64 '+var+';\n', '        '+var+' = ('+literal+');\n'):
            if body.count(line) != 1:
                raise ValueError('wrap-local anchor '+var)
            body = body.replace(line, '')
        body = re.sub(r'\b'+var+r'\b', '('+literal+')', body)
    old = '''        } else if (v <= (-3.141592654)) {
            v = (6.283185308) + v;
        }'''
    new = '''        } else {
            v = v <= (-3.141592654) ? (6.283185308) + v : v;
        }'''
    if body.count(old) != 1:
        raise ValueError('angle conditional anchor')
    text = text[:a]+body.replace(old, new)+text[b:]
    if text.count('bv = bv + (6.283185308);') != 1:
        raise ValueError('InterpPYR add anchor')
    text = text.replace('bv = bv + (6.283185308);', 'bv += (6.283185308);')
    text = re.sub(r'\((-?\d+\.\d+f?)\)', r'\1', text)
    text = re.sub(r'\n\s*\n\s*\n', '\n\n', text)
    return text


def bind(value, base):
    if isinstance(value, list):
        if len(value) == 5 and value[0] == '.sdata2' and value[2] in (4, 8) and value[3:] == [1, 0]:
            return ['ABSOLUTE-POOL', base+value[1], value[2]]
        return [bind(v, base) for v in value]
    if isinstance(value, str) and value in LITERALS:
        return ['ABSOLUTE-POOL', int(value[4:], 16), 4 if value in ('lbl_803457F0', 'lbl_80345810') else 8]
    return value


def audit(baseline, candidate, target):
    if len(bytes.fromhex(target['functions'][FN]['body'])) != 4188:
        raise ValueError('target instruction count')
    if baseline['functions'].keys() != candidate['functions'].keys():
        raise ValueError('function roster')
    for name, old in baseline['functions'].items():
        new = candidate['functions'][name]
        if {k:v for k,v in old.items() if k != 'relocations'} != {k:v for k,v in new.items() if k != 'relocations'}:
            raise ValueError('function bytes/layout '+name)
        if bind(old['relocations'], 0x80345838) != bind(new['relocations'], 0x803457F0):
            raise ValueError('function binding '+name)
    if candidate['functions'][FN]['body'] != target['functions'][FN]['body']:
        raise ValueError('target function bytes')
    if baseline['sections'].keys() != candidate['sections'].keys():
        raise ValueError('section roster')
    for name, old in baseline['sections'].items():
        new = candidate['sections'][name]
        if name == '.sdata2':
            if old['bytes'] != MAGIC or old['size'] != 8 or new['bytes'] != POOL or new['size'] != 88:
                raise ValueError('pool extent/payload')
            if {k:v for k,v in old.items() if k not in ('bytes','size')} != {k:v for k,v in new.items() if k not in ('bytes','size')}:
                raise ValueError('pool metadata')
        else:
            if {k:v for k,v in old.items() if k != 'relocations'} != {k:v for k,v in new.items() if k != 'relocations'}:
                raise ValueError('section bytes/layout '+name)
            if bind(old['relocations'], 0x80345838) != bind(new['relocations'], 0x803457F0):
                raise ValueError('section bindings '+name)
    if baseline['exception_records'] != candidate['exception_records']:
        raise ValueError('exception records')
    def is_pool_symbol(symbol):
        name = symbol[0]
        return (isinstance(name, list) and name[0] == '.sdata2') or (isinstance(name, str) and name in LITERALS)
    if [s for s in baseline['all_symbols'] if not is_pool_symbol(s)] != [s for s in candidate['all_symbols'] if not is_pool_symbol(s)]:
        raise ValueError('nonpool symbols')
    expected = {0:4, 8:8, 16:8, 24:8, 32:4, 40:8, 48:8, 56:8, 64:8, 72:8, 80:8}
    actual = [s for s in candidate['all_symbols'] if is_pool_symbol(s)]
    wanted = [[[ '.sdata2', off, size, 1, 0], '.sdata2', off, size, 1, 0] for off,size in expected.items()]
    if sorted(actual, key=repr) != sorted(wanted, key=repr):
        raise ValueError('pool symbol roster')
    prior = [s for s in baseline['all_symbols'] if is_pool_symbol(s)]
    expected_prior = [[[ '.sdata2', 0, 8, 1, 0], '.sdata2', 0, 8, 1, 0]]
    expected_prior += [[name, '', 0, 0, 16, 0] for name in LITERALS]
    if sorted(prior, key=repr) != sorted(expected_prior, key=repr):
        raise ValueError('baseline pool symbol roster')
    return {'status':'PASS', 'functions':len(candidate['functions']), 'raw_exact_instructions':1047,
            'unchanged_sibling_bodies':len(candidate['functions'])-1, 'pool_bytes':88, 'new_source_owned_bytes':80,
            'all_final_relocation_bindings_preserved':True, 'exception_metadata_preserved':True}


def capture():
    edge = cv.read_edges()[UNIT]
    if not edge['raw'] or edge['mw'] != 'GC/1.2.5':
        raise ValueError('expected stock pre-WebFrank edge')
    folder = Path(tempfile.mkdtemp(prefix='r77_anim_play_baseline_', dir=ROOT/'build'))
    files = {'source.c':ROOT/edge['src'], 'raw.o':ROOT/edge['body_o'],
             'processed.o':ROOT/f'build/GUNE5D/src/{UNIT}.o', 'target.o':ROOT/f'build/GUNE5D/obj/{UNIT}.o',
             'splits.txt':ROOT/'config/GUNE5D/splits.txt', 'rules.json':ROOT/'config/GUNE5D/webfrank.json'}
    hashes = {}
    for name, path in files.items():
        blob = path.read_bytes()
        (folder/('r77_anim_play_'+name)).write_bytes(blob)
        hashes[name] = hashlib.sha256(blob).hexdigest()
    (folder/'r77_anim_play_manifest.json').write_text(json.dumps({'edge':edge,'hashes':hashes}, indent=2))
    print(folder)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--baseline-dir', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT/'build/r77_anim_play_retirement.json')
    args = parser.parse_args(argv)
    if Path.cwd().resolve() != ROOT:
        raise ValueError('run from repository root')
    if args.capture:
        return capture()
    if not args.baseline_dir:
        parser.error('--baseline-dir required')
    folder = args.baseline_dir.resolve()
    manifest = json.loads((folder/'r77_anim_play_manifest.json').read_text())
    for name, sha in manifest['hashes'].items():
        if hashlib.sha256((folder/('r77_anim_play_'+name)).read_bytes()).hexdigest() != sha:
            raise ValueError('archived baseline drift '+name)
    edge = cv.read_edges()[UNIT]
    if edge['raw'] or edge['mw'] != 'GC/1.2.5':
        raise ValueError('stock untransformed edge required')
    for key in ('mw', 'cflags', 'rule', 'extab_padding', 'command_template'):
        if edge[key] != manifest['edge'][key]:
            raise ValueError('compiler workflow changed '+key)
    source = ROOT/edge['src']
    raw = ROOT/edge['body_o']
    inputs = (source, raw, ROOT/'config/GUNE5D/webfrank.json', ROOT/'config/GUNE5D/splits.txt',
              ROOT/'build.ninja', ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe',
              ROOT/'include/types.h', ROOT/'build/tools/sjiswrap.exe', ROOT/'build/tools/dtk.exe',
              ROOT/f'build/GUNE5D/obj/{UNIT}.o', ROOT/'orig/GUNE5D/sys/main.dol')
    frozen = {p:p.read_bytes() for p in inputs}
    oldsource = (folder/'r77_anim_play_source.c').read_text()
    if source.read_text() != recovered_source(oldsource):
        raise ValueError('unreviewed source form')
    rules = json.loads(frozen[ROOT/'config/GUNE5D/webfrank.json'])
    oldrules = json.loads((folder/'r77_anim_play_rules.json').read_text())
    if UNIT in rules['units'] or len(oldrules['units'][UNIT]) != 1 or oldrules['units'][UNIT][0]['function'] != FN:
        raise ValueError('pin retirement')
    def block(text):
        start = text.index(UNIT+'.c:\n')
        return text[start:text.index('\n\n',start)]
    if block((ROOT/'config/GUNE5D/splits.txt').read_text()) != block((folder/'r77_anim_play_splits.txt').read_text()).replace('start:0x80345838 end:0x80345840','start:0x803457F0 end:0x80345848'):
        raise ValueError('owned split edit')
    work = Path(tempfile.mkdtemp(prefix='r77_anim_play_fidelity_', dir=ROOT/'build'))
    traces = {}
    for name, code, expected in [('before',oldsource,(folder/'r77_anim_play_raw.o').read_bytes()), ('after',source.read_text(),raw.read_bytes())]:
        sub = work/name
        sub.mkdir()
        src = sub/'anim_play.c'
        src.write_text(code)
        trial = dict(edge, src=str(src.relative_to(ROOT)), _command_trace=[])
        obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], sub/'r77_anim_play.o', sub)
        if error or obj is None or obj.read_bytes() != expected:
            raise ValueError(error or 'complete raw fidelity '+name)
        traces[name] = trial['_command_trace']
    target = canonical(ROOT/f'build/GUNE5D/obj/{UNIT}.o')
    if target['sections']['.sdata2']['bytes'] != POOL or dol_read(0x803457F0,88).hex() != POOL:
        raise ValueError('retail pool bytes')
    result = audit(canonical(folder/'r77_anim_play_processed.o'), canonical(raw), target)
    if any(p.read_bytes() != blob for p,blob in frozen.items()):
        raise ValueError('production input drift')
    result.update(complete_before_after_raw_fidelity=True, compiler=edge['mw'], traces=traces,
                  input_sha256={str(p.relative_to(ROOT)):hashlib.sha256(blob).hexdigest() for p,blob in frozen.items()})
    out = args.out.resolve()
    if not out.is_relative_to((ROOT/'build').resolve()) or not out.name.startswith('r77_anim_play_'):
        raise ValueError('output must be lane-prefixed under build/')
    out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k != 'traces'}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
