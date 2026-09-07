"""Manual CalcFrustrumNormals retirement audit; never rewrites an object.

Requires archived complete before objects and fresh Ninja output. Certifies the
retired function's raw bytes/positional relocation identities and preservation
of every other ELF byte. The TU remains NonMatching; this is not a TU flip or
a final-address certificate for its unclaimed data.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.pooldump import dol_read

UNIT = 'game/world/newcam'
FN = 'CalcFrustrumNormals'
ANCHORS = {
    'source.c': 'cf5dc075c6c16ce8d892100042f49c455e0ac34022c96aa681bdff02792c42a7',
    'raw.o': 'c542971959d9c7746eb6981a1d054af1ad4440f7e8bf047592cb704c080dd96e',
    'processed.o': 'afd3146445a8e29ba5cdc0f27d9317c530fc69b61f4a9ab5f42e617f8aaceeaa',
    'target.o': '0bb3197bd6c7a412607e21d7152d5fc2edc4fbe3de3d77ae2b8f0dcfed7eae63',
    'rules.json': 'ccabc054c9dfa0a13b048346add35465aa9a56ed147e8af9ca3a832a68b65dd6',
    'splits.txt': 'afa3a3b5c0ac6b31d43dc3b58b75fc37e094922c16bf6c0abd983e862b48dbf5',
    'symbols.txt': 'a34ae6ab2c480cde4d6ec3a51cb565e05468a74587daf908b2835bb9caef3bf2',
}
COMPILER_SHA = '0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914'
MANIFEST_SHA = 'b16a23c8b199c25668134187ef8da35c9f4cd55e83ec219551bf7e9d044b10ba'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def trusted_manifest(data):
    if sha(data) != MANIFEST_SHA:
        raise ValueError('archived edge/command manifest hash')
    return json.loads(data)


def checked_output(path):
    path = Path(path).resolve()
    if not path.is_relative_to((ROOT / 'build').resolve()) or not path.name.startswith('r79_newcam_'):
        raise ValueError('output must be r79_newcam_-prefixed under build/')
    return path


def verify_envelope(before, after, target_body, offset):
    if len(target_body) != 468 or offset < 0 or offset + 468 > len(before):
        raise ValueError('function extent')
    expected = before[:offset] + target_body + before[offset + 468:]
    if after != expected:
        raise ValueError('raw ELF differs outside the exact function substitution')


def relocations(rows):
    # MWCC stores SDA21 at the immediate halfword; DTK stores its word offset.
    return [[off & ~3 if kind == 109 else off, kind, symbol, addend]
            for off, kind, symbol, addend in rows]


def verify_relocations(actual, target):
    if len(actual) != 11 or relocations(actual) != relocations(target):
        raise ValueError('all eleven positional relocation identities')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', required=True, type=Path)
    parser.add_argument('--out', default='build/r79_newcam_retirement.json', type=Path)
    args = parser.parse_args()
    output = checked_output(args.out)
    if Path.cwd().resolve() != ROOT:
        raise ValueError('run from repository root')
    archive = {}
    for suffix, expected in ANCHORS.items():
        data = (args.baseline_dir / ('r79_newcam_' + suffix)).read_bytes()
        if sha(data) != expected:
            raise ValueError('archive hash ' + suffix)
        archive[suffix] = data
    edge = cv.read_edges()[UNIT]
    manifest = trusted_manifest((args.baseline_dir / 'r79_newcam_manifest.json').read_bytes())
    if edge != manifest['edge'] or edge['mw'] != 'GC/1.2.5':
        raise ValueError('actual Ninja edge changed')
    paths = {'source': ROOT / edge['src'], 'raw': ROOT / edge['body_o'],
             'processed': ROOT / f'build/GUNE5D/src/{UNIT}.o',
             'target': ROOT / f'build/GUNE5D/obj/{UNIT}.o',
             'rules': ROOT / 'config/GUNE5D/webfrank.json',
             'compiler': ROOT / 'build/compilers' / edge['mw'] / 'mwcceppc.exe',
             'splits': ROOT / 'config/GUNE5D/splits.txt',
             'symbols': ROOT / 'config/GUNE5D/symbols.txt'}
    frozen = {key: path.read_bytes() for key, path in paths.items()}
    for key, suffix in [('target', 'target.o'), ('splits', 'splits.txt'), ('symbols', 'symbols.txt')]:
        if frozen[key] != archive[suffix]:
            raise ValueError('held-fixed ' + key)
    if sha(frozen['compiler']) != COMPILER_SHA:
        raise ValueError('compiler hash')
    old_rules = json.loads(archive['rules.json'])['units'][UNIT]
    new_rules = json.loads(frozen['rules'])['units'][UNIT]
    if sum(r['function'] == FN for r in old_rules) != 1 or new_rules != [r for r in old_rules if r['function'] != FN]:
        raise ValueError('exactly one owned rule removal')
    with tempfile.TemporaryDirectory(prefix='r79_newcam_fidelity_', dir=ROOT / 'build') as temporary:
        folder = Path(temporary)
        traces = {}
        for label, source_bytes, expected in [('before', archive['source.c'], archive['raw.o']),
                                               ('after', frozen['source'], frozen['raw'])]:
            source = folder / 'newcam.c'
            source.write_bytes(source_bytes)
            control = dict(edge, src=source.relative_to(ROOT).as_posix(), _command_trace=[])
            obj, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder / (label + '.o'), folder)
            if error or obj is None or obj.read_bytes() != expected:
                raise ValueError(error or 'complete raw ELF fidelity ' + label)
            traces[label] = control['_command_trace']
    elf = Elf(str(paths['raw']))
    syms = [elf.sym(i) for i in range(elf.symcount) if elf.symname(i).decode() == FN]
    if len(syms) != 1 or syms[0][2] != 468:
        raise ValueError('function symbol')
    symbol = syms[0]
    offset = elf.sh[symbol[5]][4] + symbol[1]
    current = canonical(paths['raw'])
    target = canonical(paths['target'])
    target_body = bytes.fromhex(target['functions'][FN]['body'])
    verify_envelope(archive['raw.o'], frozen['raw'], target_body, offset)
    verify_relocations(current['functions'][FN]['relocations'], target['functions'][FN]['relocations'])
    if frozen['processed'] != archive['processed.o']:
        raise ValueError('entire processed ELF changed')
    if dol_read(0x803474A0, 8) != bytes.fromhex('3fe0000000000000'):
        raise ValueError('retail double datum')
    for key, path in paths.items():
        if path.read_bytes() != frozen[key]:
            raise ValueError('input moved during audit: ' + key)
    result = {'status': 'PASS', 'scope': 'raw function exact; whole previous ELF preserved; TU remains NonMatching',
              'function': FN, 'instructions': [117, 117], 'differing_words': 0, 'frame': 152,
              'siblings_preserved': len(current['functions']) - 1, 'relocations': 11,
              'whole_processed_elf_identical': True, 'pool_bytes_unchanged': current['sections']['.sdata2']['size'],
              'hashes': {key: sha(data) for key, data in frozen.items()}, 'fidelity_traces': traces}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in result.items() if key not in ('hashes', 'fidelity_traces')}))
    print('wrote', output.relative_to(ROOT))


if __name__ == '__main__':
    main()
