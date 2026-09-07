"""Replay R88's conditional native YUV proof, NOT a TU retirement certificate.

Requires the immutable evidence bundle indexed below. No compile, source edit,
postprocessing, linking, or exception-payload interpretation is performed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory

MANIFEST = 'build/r88_movie_evidence_manifest.json'
MANIFEST_SHA256 = 'a23350c249a6c63cf03de5cddbba06c95e194780484d853d318c99623a84f2b3'
FN = 'fn_800DBE98'
POOL = '430000003f0000003fb374bc3eb0331e3f36d1e13fe2d0e54330000000000000'
BIAS_FIRST_POOL = POOL[-16:] + POOL[:-16]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def verify_manifest(root):
    raw = (root / MANIFEST).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == MANIFEST_SHA256, 'manifest hash')
    entries = json.loads(raw)['files']
    seen = set()
    for row in entries:
        path = (root / row['path']).resolve()
        require(path.is_relative_to(root.resolve()), 'archive path escapes root')
        require(row['path'] not in seen, 'duplicate archive path')
        seen.add(row['path'])
        data = path.read_bytes()
        require(len(data) == row['size'], 'archive size: ' + row['path'])
        require(hashlib.sha256(data).hexdigest() == row['sha256'], 'archive hash: ' + row['path'])
    return len(entries)


def check_functions(before, target, parts):
    joined = {}
    for fs in parts:
        require(not (joined.keys() & fs.keys()), 'duplicate defined function')
        joined.update(fs)
    require(joined.keys() == before.keys(), 'function roster')
    for name, fn in joined.items():
        reference = target[name] if name == FN else before[name]
        for field in ('body', 'size'):
            require(fn[field] == reference[field], name + ' ' + field)
        require(fn['binding'] == before[name]['binding'], name + ' symbol binding')
    return len(joined) - 1


def check_opaque_eh(before, parts):
    for section in ('extab', 'extabindex'):
        require(''.join(ss.get(section, '') for ss in parts) == before.get(section, ''),
                section + ' opaque bytes')


def crossing_entries(rows, cut):
    """All source-order EH ownership inversions crossing a text cut."""
    left = [(name, eh) for name, text, eh in rows if text < cut]
    right = [(name, eh) for name, text, eh in rows if text >= cut]
    return [(ln, rn) for ln, le in left for rn, re in right if le > re]


def target_eh_rows(path):
    elf = Elf(str(path))
    _, relocs = elf.relas('.relaextabindex')
    slots = {}
    for offset, info, addend in relocs:
        sym = elf.sym(info >> 8)
        slots.setdefault(offset // 12, {})[offset % 12] = (
            elf.symname(info >> 8).decode(), sym[1] + addend)
    return [(slot[0][0], 0x800D860C + slot[0][1], 0x800081B8 + slot[8][1])
            for _, slot in sorted(slots.items())]


def audit(root):
    count = verify_manifest(root)
    base = root / 'build/r88_movie_archive'
    before, before_sections = inventory(base / 'r88_movie_raw.o')
    target, _ = inventory(base / 'r88_movie_target.o')
    require(len(before[FN]['body']) // 8 == 53, 'baseline count')
    different = sum(before[FN]['body'][i:i+8] != target[FN]['body'][i:i+8]
                    for i in range(0, len(target[FN]['body']), 8))
    require(different == 9, 'baseline differing words')
    manifests = ['r88_movie_archive/r88_movie_manifest.json',
                 'r88_movie_coarse_v3_archive/r88_movie_coarse_manifest.json',
                 'r88_movie_ehboundary_v4_archive/r88_movie_ehboundary_manifest.json']
    for name in manifests:
        edge = json.loads((root / 'build' / name).read_text())['edge']
        require(edge['mw'] == 'GC/1.2.5', 'compiler directory')
        require(edge['rule'] == 'mwcc_sjis', 'Ninja runner')
    full, full_sections = inventory(base / 'all/r88_movie.o')
    check_functions(before, target, [full])
    check_opaque_eh(before_sections, [full_sections])
    for archive in ('r88_movie_coarse_v3_archive', 'r88_movie_ehboundary_v4_archive'):
        group = root / 'build' / archive
        require((group / 'baseline/r88_movie.o').read_bytes() == (base / 'r88_movie_raw.o').read_bytes(),
                'complete baseline ELF fidelity')
        prefix, ps = inventory(group / 'prefix/r88_movie.o')
        suffix, ss = inventory(group / 'suffix_literal/r88_movie.o')
        check_functions(before, target, [prefix, suffix])
        check_opaque_eh(before_sections, [ps, ss])
        for section, data in before_sections.items():
            if section == '.sdata2':
                require(ps.get(section) == data, archive + ' prefix pool')
            elif section not in ('.text', 'extab', 'extabindex'):
                require(ps.get(section, '') + ss.get(section, '') == data,
                        archive + ' combined ' + section)
        expected = POOL if archive == 'r88_movie_coarse_v3_archive' else BIAS_FIRST_POOL
        require(ss['.sdata2'] == expected, archive + ' exact pool order')
    eh = target_eh_rows(base / 'r88_movie_target.o')
    inversions = crossing_entries(eh, 0x800DB15C)
    require(('fn_800DA60C', '__dla__FPv') in inversions, 'target array-delete inversion')
    require(('__ct__11MoviePlayerFv', '__dl__FPv') in inversions, 'target delete inversion')
    require(not crossing_entries(eh, 0x800DA60C), 'coarser boundary EH order')
    for label, reference in [('prefix', 'prefix'), ('runtime', 'suffix_literal')]:
        actual = root / 'build/r88_movie_final_archive' / label / 'r88_movie.o'
        prior = root / 'build/r88_movie_coarse_v3_archive' / reference / 'r88_movie.o'
        require(inventory(actual) == inventory(prior), 'actual basename inventory')
    return dict(status='PASS_CONDITIONAL_ONLY', indexed_files=count,
                instructions='53/53', baseline_differing_words=9, literal_differing_words=0,
                sibling_bodies_preserved=51, coarse_cut_EH_inversions=inversions,
                EH_legal_cut_pool_order='bias-first, not target float-first',
                production_rule_retired=False,
                limits=['Opaque EH bytes are preserved; payload semantics are not decoded.',
                        'Function bodies and binding strengths here are not a final linked-address proof.',
                        'Archived mapped-relocation controls describe intended datum identities, not NonMatching link placement.',
                        'No production source, split, compiler flag or rule change is certified.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.archive), indent=2))


if __name__ == '__main__':
    main()
