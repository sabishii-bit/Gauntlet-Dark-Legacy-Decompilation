"""Manual AudioPlayerEatSFX native-retirement proof; no object rewriting.

Fresh whole-TU before/after Ninja compiler fidelity, exact target body and
positional relocation identities, and complete canonical allocated-object
preservation (including symbols/data/EH). No whole-TU target-match claim:
sounds_evt remains NonMatching. A full linked Ninja checksum is also required.
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

UNIT, FN = 'game/sound/sounds_evt', 'AudioPlayerEatSFX'
ANCHORS = {
    'source.c': '5ded355f82bd1a34bdd6b237b9aebfaccd43b5c6ce0e2c809ab7b80a46083c99',
    'raw.bin': '739423affa59968e5559f08d8659c916e1439cea9e71fdc287ffbfa926159e8d',
    'processed.bin': '75ee88f038871af8aa96ea66ebe0c2076797aceb72ce823f6cb972637d8010a9',
    'target.bin': 'f5e8f443b2059772ff0057f5cb777126bdfe19a701bd8f9e7a4924d42d499282',
    'rules.bin': '03a2856dfaf90aeba3f16a3562abdc25b2832b294540e92dca9fc15d4266bbb3',
    'symbols.bin': 'a34ae6ab2c480cde4d6ec3a51cb565e05468a74587daf908b2835bb9caef3bf2',
}
MANIFEST_SHA = '7c6f1626921c265feb701872dde8a7671d4ba33d0ac1b917f4786d0edf516244'
COMPILER_SHA = '0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(before, after, processed_before, target):
    old_fn = before['functions'][FN]
    actual = after['functions'][FN]
    wanted = target['functions'][FN]
    require(old_fn['size'] == actual['size'] == wanted['size'] == 272, '68-instruction extent')
    require(actual['body'] == wanted['body'], 'raw target body')
    def relocations(rows):
        return [[off & ~3 if kind == 109 else off, kind, name, add]
                for off, kind, name, add in rows]
    require(len(actual['relocations']) == 12 and
            relocations(actual['relocations']) == relocations(wanted['relocations']),
            'all twelve positional relocation identities')
    expected = copy.deepcopy(before)
    expected['functions'][FN]['body'] = wanted['body']
    offset = old_fn['offset'] * 2
    text = expected['sections']['.text']['bytes']
    expected['sections']['.text']['bytes'] = text[:offset] + wanted['body'] + text[offset + 544:]
    require(expected == after, 'canonical raw envelope outside exact function body')
    require(after == processed_before, 'canonical prior processed object')
    return dict(status='PASS', instructions=[68, 68], raw_words=0,
                siblings_preserved=len(after['functions'])-1, relocations=12,
                allocated_bytes_symbols_relocations_EH_preserved=True,
                limitations=['STT_FILE and anonymous pool labels are normalized, not a whole ELF byte-identity assertion.',
                             'No data/pool/split claims changed; not a whole sounds_evt target certificate.',
                             'Full Ninja checksum is a separate required gate.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/r81_sounds_audit.json')
    args = parser.parse_args()
    output = args.out.resolve()
    require(output.is_relative_to(ROOT / 'build') and output.name.startswith('r81_'), 'lane-prefixed build output required')
    archive = {}
    for suffix, expected in ANCHORS.items():
        path = args.baseline_dir / ('r81_before_' + suffix)
        require(sha(path.read_bytes()) == expected, 'archive hash ' + suffix)
        archive[suffix] = path
    manifest_bytes = (args.baseline_dir / 'r81_results.json').read_bytes()
    require(sha(manifest_bytes) == MANIFEST_SHA, 'original edge manifest hash')
    old_edge = json.loads(manifest_bytes)['edge']
    edge = cv.read_edges()[UNIT]
    for key in ('mw', 'cflags', 'rule', 'command_template', 'extab_padding'):
        require(old_edge[key] == edge[key], 'compiler edge changed: ' + key)
    require(edge['mw'] == 'GC/1.2.5' and not edge['raw'], 'stock direct compiler edge required')
    paths = dict(source=ROOT / edge['src'], raw=ROOT / edge['body_o'],
                 target=ROOT / f'build/GUNE5D/obj/{UNIT}.o',
                 symbols=ROOT / 'config/GUNE5D/symbols.txt',
                 compiler=ROOT / 'build/compilers' / edge['mw'] / 'mwcceppc.exe',
                 rules=ROOT / 'config/GUNE5D/webfrank.json')
    frozen = {key: path.read_bytes() for key, path in paths.items()}
    require(sha(frozen['compiler']) == COMPILER_SHA, 'compiler bytes')
    for key in ('target', 'symbols'):
        require(frozen[key] == archive[key + '.bin'].read_bytes(), 'held-fixed ' + key)
    old_rules = json.loads(archive['rules.bin'].read_bytes())['units'][UNIT]
    require(len(old_rules) == 1 and old_rules[0]['function'] == FN, 'exactly one old owned rule')
    require(UNIT not in json.loads(frozen['rules'])['units'], 'owned WebFrank edge not retired')
    traces = {}
    with tempfile.TemporaryDirectory(prefix='r81_sounds_fidelity_', dir=ROOT / 'build') as temporary:
        folder = Path(temporary)
        for name, source_bytes, expected in (
                ('before', archive['source.c'].read_bytes(), archive['raw.bin'].read_bytes()),
                ('after', frozen['source'], frozen['raw'])):
            source = folder / 'sounds_evt.c'
            source.write_bytes(source_bytes)
            control = dict(edge, src=source.relative_to(ROOT).as_posix(), _command_trace=[])
            obj, error = cv.compile_with(control, edge['mw'], edge['cflags'], folder / (name + '.o'), folder)
            require(not error and obj is not None, str(error))
            require(obj.read_bytes() == expected, 'complete raw ELF fidelity: ' + name)
            traces[name] = control['_command_trace']
    result = verify(canonical(archive['raw.bin']), canonical(paths['raw']),
                    canonical(archive['processed.bin']), canonical(paths['target']))
    require(all(path.read_bytes() == frozen[key] for key, path in paths.items()), 'input changed during audit')
    result.update(hashes={key: sha(data) for key, data in frozen.items()}, traces=traces,
                  compiler=edge['mw'], flags=edge['cflags'])
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in result.items() if key not in ('traces', 'flags')}))


if __name__ == '__main__':
    main()
