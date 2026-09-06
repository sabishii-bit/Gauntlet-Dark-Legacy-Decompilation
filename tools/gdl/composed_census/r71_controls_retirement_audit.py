"""Manual before/after audit for InitControls' existing-helper rule retirement.

No build hook, source mutation, or object rewriting. A fresh actual-Ninja
compile is required in both snapshots. Compare all allocated bytes, symbol
layout and positional relocation bindings; anonymous pool names normalize
only to their own ELF section/offset/type/size, never to an arbitrary name.
The linked-DOL gate remains separate, and controls.c is still NonMatching.
"""
import argparse
import copy
import hashlib
import json
import re
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory

UNIT, FN = 'game/game/controls', 'InitControls'
OLD = '''void InitControls(void)
{
    int i;

    init_controls();
    for (i = 0; i < 4; i++) {
        lbl_802407C8[i] = lbl_802407D8[i] = lbl_802407B8[i] = lbl_802407E8[i] =
            lbl_802407F8[i] = 0;
    }
    init_all_dir_info();
    ctrls_initialized = 1;
}'''
NEW = '''void InitControls(void)
{
    init_controls();
    clear_pad_levels();
    init_all_dir_info();
    ctrls_initialized = 1;
}'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(path):
    elf = Elf(str(path))
    inv = object_inventory(path)
    names = {}
    symbols = []
    for i in range(elf.symcount):
        s = elf.sym(i)
        name = elf.symname(i).decode()
        if s[3] & 15 == 4:  # ELF STT_FILE records source filename, not runtime data.
            continue
        section = elf.names[s[5]] if s[5] < len(elf.names) else s[5]
        if name.startswith('@'):
            if not 0 < s[5] < len(elf.sh):
                raise ValueError('undefined anonymous pool symbol')
            names[name] = (section, s[1], s[2], s[3], s[4])
        symbols.append((names.get(name, name), section, s[1], s[2], s[3], s[4]))
    for section in inv['sections'].values():
        section['relocations'] = sorted((off, kind, names.get(name, name), add)
                                        for off, kind, name, add in section['relocations'])
    for function in inv['functions'].values():
        function['relocations'] = sorted((off, kind, names.get(name, name), add)
                                         for off, kind, name, add in function['relocations'])
    inv['all_symbols'] = sorted(symbols, key=repr)
    inv['symbols'] = {repr(names.get(name, name)): value for name, value in inv['symbols'].items()}
    return json.loads(json.dumps(inv))


def capture():
    edge = cv.read_edges()[UNIT]
    paths = dict(source=ROOT/edge['src'], raw=ROOT/edge['body_o'],
                 processed=ROOT/f'build/GUNE5D/src/{UNIT}.o',
                 target=ROOT/f'build/GUNE5D/obj/{UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe',
                 symbol_map=ROOT/'config/GUNE5D/symbols.txt')
    frozen = {key: path.read_bytes() for key, path in paths.items()}
    folder = Path(tempfile.mkdtemp(prefix='r71_controls_audit_', dir=ROOT/'build'))
    control = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder/'r71_controls_control.o', folder)
    if error or not obj or obj.read_bytes() != frozen['raw']:
        raise ValueError(error or 'fresh actual-Ninja complete raw fidelity failed')
    result = dict(schema_version=1, fidelity=True, edge=edge, trace=control['_command_trace'],
                  source=frozen['source'].decode().replace('\r\n','\n'),
                  hashes={key: sha(value) for key,value in frozen.items()},
                  config=json.loads(frozen['config']), inventories={key: canonical(paths[key]) for key in ('raw','processed','target')})
    result['pad_array_addresses'] = {name: int(address,16) for name,address in re.findall(
        r'(?m)^(lbl_802407[B-F]8) = \.bss:0x([0-9A-Fa-f]+); // type:object size:0x10\b',
        frozen['symbol_map'].decode())}
    # Keep full binary evidence locally, including non-allocated ELF metadata.
    for key in ('raw','processed','target'):
        (folder/f'r71_controls_{key}.o').write_bytes(frozen[key])
    result['artifacts'] = str(folder.relative_to(ROOT))
    if any(path.read_bytes() != frozen[key] for key,path in paths.items()):
        raise ValueError('input changed during capture')
    return result


def audit(before, after):
    for value in (before, after):
        if value.get('schema_version') != 1 or value.get('fidelity') is not True:
            raise ValueError('faithful schema-1 snapshot required')
    if before['source'].count(OLD) != 1 or before['source'].replace(OLD,NEW) != after['source']:
        raise ValueError('change is not precisely reuse of existing clear_pad_levels')
    for key in ('mw','cflags','command_template','rule'):
        if before['edge'][key] != after['edge'][key]:
            raise ValueError('compiler edge changed: '+key)
    for key in ('compiler','target'):
        if before['hashes'][key] != after['hashes'][key]:
            raise ValueError('changed '+key)
    expected = copy.deepcopy(before['config'])
    rules = expected['units'][UNIT]
    if sum(row['function']==FN for row in rules) != 1:
        raise ValueError('expected exactly one old InitControls rule')
    expected['units'][UNIT] = [row for row in rules if row['function'] != FN]
    if expected != after['config']:
        raise ValueError('configuration change is not exactly one rule removal')
    a,b = before['inventories'], after['inventories']
    if a['processed'] != b['processed']:
        raise ValueError('processed whole-object allocated bytes/layout/relocation bindings changed')
    raw_before,raw_after = a['raw'],b['raw']
    if raw_before['functions'].keys()!=raw_after['functions'].keys():
        raise ValueError('raw function roster changed')
    for name in raw_before['functions']:
        if name!=FN and raw_before['functions'][name]!=raw_after['functions'][name]:
            raise ValueError('raw sibling changed: '+name)
    for key in ('all_symbols','symbols','exception_records'):
        if raw_before[key]!=raw_after[key]:
            raise ValueError('raw metadata changed: '+key)
    for name in raw_before['sections']:
        if name!='.text' and raw_before['sections'][name]!=raw_after['sections'][name]:
            raise ValueError('raw nontext changed: '+name)
    current,target = raw_after['functions'][FN],b['target']['functions'][FN]
    if current['size']!=108 or current['body']!=target['body']:
        raise ValueError('InitControls raw body is not target-exact')
    # Target extractor uses SDA21 word offsets; MWCC uses halfword offsets.
    # Bind the anonymous BSS base to its actual named first array on both sides.
    for i,suffix in enumerate(('B8','C8','D8','E8','F8')):
        name='lbl_802407'+suffix
        symbol=raw_after['symbols'][repr(name)]
        if (symbol['section'],symbol['offset'],symbol['size'])!=('.bss',16*i,16):
            raise ValueError('named BSS pad-array allocation differs')
        if after['pad_array_addresses'].get(name)!=0x802407B8+16*i:
            raise ValueError('target map pad-array address differs')
    # The split target TEXT object leaves this BSS datum undefined; do not
    # pretend it owns a BSS allocation. Its named binding is checked against
    # the target map and the five raw array allocations above.
    target_base=[s for s in b['target']['all_symbols'] if s[0]=='lbl_802407B8']
    if target_base!=[['lbl_802407B8','',0,0,16,0]]:
        raise ValueError('unexpected target named BSS reference')
    bases=[s for s in raw_after['all_symbols'] if s[0]=='...bss.0']
    if len(bases)!=1 or bases[0][1:]!=['.bss',0,0,0,0]:
        raise ValueError('raw BSS base alias differs')
    def relocs(rows):
        return sorted((off & ~3 if kind==109 else off,kind,
                       'lbl_802407B8' if name=='...bss.0' else name,add)
                      for off,kind,name,add in rows)
    if relocs(current['relocations'])!=relocs(target['relocations']):
        raise ValueError('target positional relocation binding differs')
    return dict(schema_version=1,status='PASS',function=FN,raw_instructions=27,
                raw_target_words=0,raw_siblings_unchanged=len(raw_after['functions'])-1,
                processed_allocated_object_equal=True,raw_nontext_and_EH_equal=True,
                compiler=after['edge']['mw'],flags=after['edge']['cflags'],hashes=after['hashes'],
                retired_rules=1,remaining_unit_rules=[r['function'] for r in after['config']['units'][UNIT]],
                limitations=['Not a whole controls.c target-match claim.',
                             'Compiler-internal IR identity mechanism is inferred, not reverse-engineered.',
                             'Source filenames and anonymous pool labels may differ; all allocated bytes and bindings are checked.',
                             'Full Ninja/link checksum gate is separately required.'])


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--before',type=Path)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args()
    if not args.out.resolve().is_relative_to((ROOT/'build').resolve()) or not args.out.name.startswith('r71_controls_'):
        ap.error('output must be lane-prefixed under build/')
    current=capture()
    result=audit(json.loads(args.before.read_text()),current) if args.before else current
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result if args.before else dict(status='BASELINE_CAPTURED',hashes=result['hashes'],artifacts=result['artifacts'])))


if __name__=='__main__':
    main()
