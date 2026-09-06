"""Replay two experimental MBNewNode source reconstructions without editing src.

These are explicit compiler-compatibility candidates, not recovered original
source or approved retirements. Both duplicate-inline and shared-body-macro
forms are retained for review. This manual tool has no build edge, performs no
object rewriting, and never changes WebFrank rules. A PASS establishes exact
allocated output relative to the reviewed baseline, not original provenance.

IMPORTABLE CORE: reconstruct, audit -- pure over source text/object inventories.
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

UNIT, FN = 'game/mb/mb_tree', 'MBNewNode'
SITES = (0x10, 0x14, 0x18, 0x28, 0x2c, 0x34, 0x38, 0x3c, 0x44,
         0x5c, 0x90, 0x98, 0x9c, 0xa4, 0xa8, 0xb0, 0xb4, 0xc8,
         0xd4, 0xdc, 0xe8, 0xf4, 0xf8)


def function(source, signature):
    found = list(re.finditer(re.escape(signature) + r'\n\{.*?\n\}', source, re.S))
    if len(found) != 1:
        raise ValueError('expected unique original definition: ' + signature)
    return found[0].group()


def reconstruct(source, form):
    if form not in ('duplicate', 'shared'):
        raise ValueError('unknown reconstruction form')
    if 'MBCreateNodeInline' in source or 'MBNodeInsertInline' in source:
        raise ValueError('expected pre-reconstruction source')
    new = function(source, 'MBTreeNode* MBNewNode(MBTreeNode* parent, const f32* matrix, s32 type)')
    create = function(source, 'MBTreeNode* MBCreateNode(void)')
    insert = function(source, 'void MBNodeInsert(MBTreeNode* node, MBTreeNode* parent)')
    if insert.count('MBNodeAppend(node, head);') != 2:
        raise ValueError('expected both existing insertion append sites')
    allocation = new[new.index('    if ((node = lbl_80344EE0)'):new.index('\n    if (node != 0)')]
    insertion = new[new.index('        node->parent = parent;'):new.index('\n    }\n    return node;')]
    caller = new.replace(allocation, '    node = MBCreateNodeInline();\n').replace(
        insertion, '        MBNodeInsertInline(node, parent);')
    if form == 'duplicate':
        creation_helper = create.replace('MBTreeNode* MBCreateNode', 'static inline MBTreeNode* MBCreateNodeInline')
        insertion_helper = insert.replace('void MBNodeInsert', 'static inline void MBNodeInsertInline').replace(
            'MBNodeAppend(node, head);', 'MBNodeLastSibling(head)->next = node;')
        return source.replace(new, creation_helper + '\n\n' + insertion_helper + '\n\n' + caller)

    def macro(name, body):
        return '#define ' + name + ' \\\n' + ' \\\n'.join(body.splitlines()) + '\n'

    creation_macro = macro('MB_NODE_CREATE_BODY', create[create.index('\n{')+1:])
    insertion_macro = macro('MB_NODE_INSERT_BODY(append_statement)', insert[insert.index('\n{')+1:].replace(
        'MBNodeAppend(node, head);', 'append_statement;'))
    creation_helper = 'static inline MBTreeNode* MBCreateNodeInline(void)\nMB_NODE_CREATE_BODY'
    insertion_helper = ('static inline void MBNodeInsertInline(MBTreeNode* node, MBTreeNode* parent)\n'
                        'MB_NODE_INSERT_BODY(MBNodeLastSibling(head)->next = node)')
    result = source.replace(new, creation_macro + '\n' + insertion_macro + '\n' +
                            creation_helper + '\n\n' + insertion_helper + '\n\n' + caller)
    result = result.replace(create, 'MBTreeNode* MBCreateNode(void)\nMB_NODE_CREATE_BODY')
    return result.replace(insert, 'void MBNodeInsert(MBTreeNode* node, MBTreeNode* parent)\n'
                          'MB_NODE_INSERT_BODY(MBNodeAppend(node, head))\n\n'
                          '#undef MB_NODE_CREATE_BODY\n#undef MB_NODE_INSERT_BODY')


def audit(before, after, target, processed):
    old, wanted = before['functions'][FN], target['functions'][FN]
    a, b = bytes.fromhex(old['body']), bytes.fromhex(wanted['body'])
    if old['size'] != 272 or wanted['size'] != 272 or len(a) != 272 or len(b) != 272:
        raise ValueError('expected 68/68 instruction bodies')
    sites = tuple(i for i in range(0, 272, 4) if a[i:i+4] != b[i:i+4])
    if sites != SITES:
        raise ValueError('baseline differs from reviewed 23-word residual')
    expected = copy.deepcopy(before)
    expected['functions'][FN]['body'] = wanted['body']
    text = bytearray.fromhex(expected['sections']['.text']['bytes'])
    off = old['offset']
    text[off:off+272] = b
    expected['sections']['.text']['bytes'] = text.hex()
    if after != expected:
        raise ValueError('changes exceed MBNewNode target words: sibling/layout/data/EH/symbol/relocation drift')
    if after != processed:
        raise ValueError('candidate is not identical to baseline processed allocated object')
    return dict(status='RAW_EXACT_EXPERIMENT', function=FN, target_instructions=68,
                candidate_instructions=68, differing_words_before=23, differing_words_after=0,
                unchanged_siblings=len(before['functions'])-1, entire_allocated_object_equal=True,
                source_and_config_modified=False, rules_retired=0,
                policy='Explicit compiler-compatibility factoring; user review pending, not original-source provenance.',
                limitation='Relocation/datum bindings preserved from verified baseline; not a new image-wide binding audit or linked-DOL gate.')


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-dir', type=Path, help='archived baseline with baseline/mb_tree.c and r76_tree_{raw,target,processed}.o')
    p.add_argument('--out', type=Path, default=Path('build/r76_tree_helper_reconstruction.json'))
    args = p.parse_args(argv)
    if not args.out.resolve().is_relative_to((ROOT/'build').resolve()):
        p.error('output must stay under build/')
    edge = cv.read_edges()[UNIT]
    paths = dict(source=ROOT/edge['src'], raw=ROOT/edge['body_o'],
                 target=ROOT/f'build/GUNE5D/obj/{UNIT}.o', processed=ROOT/f'build/GUNE5D/src/{UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json', ninja=ROOT/'build.ninja',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe', types=ROOT/'include/types.h')
    frozen = {k:v.read_bytes() for k,v in paths.items()}
    folder = Path(tempfile.mkdtemp(prefix='r76_tree_helper_', dir=ROOT/'build'))
    base = args.baseline_dir
    if base is None:
        base = folder
        (base/'baseline').mkdir()
        (base/'baseline/mb_tree.c').write_bytes(frozen['source'])
        for name in ('raw', 'target', 'processed'):
            (base/f'r76_tree_{name}.o').write_bytes(frozen[name])
        (base/'r76_tree_rules.json').write_bytes(frozen['config'])
    source = (base/'baseline/mb_tree.c').read_text()
    before, target, processed = [canonical(base/f'r76_tree_{name}.o') for name in ('raw','target','processed')]
    if target != canonical(paths['target']) or processed != canonical(paths['processed']):
        raise ValueError('archive target/processed inventory no longer matches this checkout baseline')
    results, traces = [], {}
    for form in ('baseline', 'duplicate', 'shared'):
        trial_dir = folder/form
        trial_dir.mkdir(exist_ok=True)
        src = trial_dir/'mb_tree.c'
        src.write_text(source if form == 'baseline' else reconstruct(source, form), encoding='utf-8')
        trial = dict(edge, src=str(src), _command_trace=[])
        obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], trial_dir/'r76_tree.o', trial_dir)
        if error or obj is None:
            raise ValueError(error or 'missing candidate object')
        traces[form] = trial['_command_trace']
        if form == 'baseline':
            if obj.read_bytes() != (base/'r76_tree_raw.o').read_bytes():
                raise ValueError('complete raw Ninja baseline fidelity failed')
            continue
        result = audit(before, canonical(obj), target, processed)
        result.update(form=form, source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),
                      object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest())
        results.append(result)
    if any(path.read_bytes() != frozen[name] for name,path in paths.items()):
        raise ValueError('production inputs changed during experiment')
    report = dict(schema_version=1, results=results, edge=edge, trace=traces,
                  baseline=str(base), artifacts=str(folder.relative_to(ROOT)),
                  hashes={k:hashlib.sha256(v).hexdigest() for k,v in frozen.items()})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(results=results, artifacts=report['artifacts'])))
    print(args.out)


if __name__ == '__main__':
    main()
