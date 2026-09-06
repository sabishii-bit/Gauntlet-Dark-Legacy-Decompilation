"""Manual source-native AllocMem32 retirement audit; not a build transform.

The baseline directory contains original full objects, baseline/ml_mem.c and
r75_mem_rules.json from the bounded R75 probes. All allocated output facts
must stay equal except the eight target register words in AllocMem32.
"""
import argparse
import copy
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

UNIT, FN = 'game/sys/ml_mem', 'AllocMem32'
SITES = (0x24, 0x2c, 0x64, 0x70, 0x7c, 0x98, 0xbc, 0xc8)


def reconstruct(source):
    if source.count('void* AllocMem(u32 size)') != 1:
        raise ValueError('expected original unsigned allocator declaration')
    pattern = r'void\* AllocMem32\(int size\)\n\{.*?\n\}'
    matches = list(re.finditer(pattern, source, re.S))
    if len(matches) != 1:
        raise ValueError('expected unique original AllocMem32')
    m = matches[0]
    body = m.group()
    start = body.index('    if (mlmMemReserved')
    body = body[:start] + '    result = AllocMem(size);\n    return (u8*)result + pad;\n}'
    body = body.replace('    u8 unused[8];\n', '')
    return (source[:m.start()] + body + source[m.end():]).replace(
        'void* AllocMem(u32 size)', 'void* AllocMem(int size)')


def audit(before, after, target, processed_before, processed_after, rules_before, rules_after):
    expected_rules = copy.deepcopy(rules_before)
    rules = expected_rules['units'][UNIT]
    if sum(r['function'] == FN for r in rules) != 1:
        raise ValueError('expected exactly one prior rule')
    expected_rules['units'][UNIT] = [r for r in rules if r['function'] != FN]
    if rules_after != expected_rules:
        raise ValueError('configuration differs beyond the retired rule')
    if processed_before != processed_after:
        raise ValueError('processed allocation, metadata or bindings changed')
    old, wanted = before['functions'][FN], target['functions'][FN]
    a, b = bytes.fromhex(old['body']), bytes.fromhex(wanted['body'])
    if len(a) != 224 or len(b) != 224 or old['size'] != 224 or wanted['size'] != 224:
        raise ValueError('expected complete 56/56 instruction bodies')
    sites = tuple(i for i in range(0, 224, 4) if a[i:i+4] != b[i:i+4])
    if sites != SITES:
        raise ValueError('baseline is not the reviewed eight-word residual')
    if processed_before['functions'][FN]['body'] != wanted['body']:
        raise ValueError('prior processed body is not target exact')
    expected = copy.deepcopy(before)
    expected['functions'][FN]['body'] = wanted['body']
    text = bytearray.fromhex(expected['sections']['.text']['bytes'])
    off = old['offset']
    text[off:off+224] = b
    expected['sections']['.text']['bytes'] = text.hex()
    if expected != after:
        raise ValueError('raw changes exceed target register words, including siblings/data/EH/bindings')
    if after['functions'][FN]['relocations'] != processed_before['functions'][FN]['relocations']:
        raise ValueError('retired function relocation bindings changed')
    return dict(status='PASS', function=FN, raw_instructions=56, raw_differing_words=0,
                retired_rules=1, retired_bytes=224, changed_raw_words=len(sites),
                raw_siblings_unchanged=len(before['functions'])-1,
                raw_metadata_data_EH_bindings_equal=True, processed_allocated_object_equal=True,
                remaining_rules=[r['function'] for r in rules_after['units'][UNIT]],
                limitation='Bindings are preserved from the verified baseline, not a new image-wide datum audit; no whole-TU or editable-link claim.')


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-dir', type=Path, required=True)
    p.add_argument('--out', type=Path, default=Path('build/r75_mlmem_retirement.json'))
    args = p.parse_args(argv)
    if not args.out.resolve().is_relative_to((ROOT/'build').resolve()):
        p.error('output must be under build/')
    edge = cv.read_edges()[UNIT]
    paths = dict(source=ROOT/edge['src'], raw=ROOT/edge['body_o'],
                 processed=ROOT/f'build/GUNE5D/src/{UNIT}.o', target=ROOT/f'build/GUNE5D/obj/{UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json', ninja=ROOT/'build.ninja',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe')
    frozen = {k:v.read_bytes() for k,v in paths.items()}
    base = args.baseline_dir
    old_source = base/'baseline/ml_mem.c'
    if reconstruct(old_source.read_text()) != paths['source'].read_text():
        raise ValueError('source edits exceed signedness, existing helper reuse and obsolete pad removal')
    folder = Path(tempfile.mkdtemp(prefix='r75_mlmem_fidelity_', dir=ROOT/'build'))
    traces = {}
    for name, src, raw in (('before', old_source, base/'r75_mem_raw.o'), ('after', paths['source'], paths['raw'])):
        trial = dict(edge, src=str(src.resolve()), _command_trace=[])
        obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], folder/(name+'.o'), folder)
        if error or obj is None or obj.read_bytes() != raw.read_bytes():
            raise ValueError(error or name+' complete raw object fidelity failed')
        traces[name] = trial['_command_trace']
    result = audit(canonical(base/'r75_mem_raw.o'), canonical(paths['raw']), canonical(paths['target']),
                   canonical(base/'r75_mem_processed.o'), canonical(paths['processed']),
                   json.loads((base/'r75_mem_rules.json').read_text()), json.loads(frozen['config']))
    if any(v.read_bytes() != frozen[k] for k,v in paths.items()):
        raise ValueError('production input drift')
    result.update(compiler=edge['mw'], cflags=edge['cflags'], fidelity=traces,
                  hashes={k:hashlib.sha256(v).hexdigest() for k,v in frozen.items()})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('fidelity','hashes','cflags')}))
    print(args.out)


if __name__ == '__main__':
    main()
