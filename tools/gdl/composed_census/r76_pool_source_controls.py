"""Manual finite MEMPOOL source controls; production inputs are read-only.

Replays the existing twenty R76 forms, not a compiler-option search. Requires
a fresh built raw object and establishes complete raw baseline fidelity before
trusting any control. Only lane-prefixed scratch outputs under build/ are written.
PASS means the finite replay completed, not target exactness or source exhaustion.
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

UNIT, FN = 'game/audio/mempool', 'pool_garbage_collect'
DECL = '    MemListNode** entries = lbl_8031EB00;\n'
HEAD = '    if ((node = pool->secondary.head) != NULL) {'
SPLIT = '    node = pool->secondary.head;\n    if (node != NULL) {'
BODY_SHA256 = '765f34395ad41a2c3e9de47e4869b27e501abf439cecb516d42fff7d376543a9'


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('source anchor absent or ambiguous: ' + old.strip())
    return text.replace(old, new)


def split_source(text):
    start_marker, end_marker = 's32 pool_garbage_collect(', '\n/* 0x800D54A4'
    if text.count(start_marker) != 1 or text.count(end_marker) != 1:
        raise ValueError('pool procedure boundaries absent or ambiguous')
    start, end = text.index(start_marker), text.index(end_marker)
    if end <= start:
        raise ValueError('pool procedure boundaries reversed')
    return text[:start], text[start:end], text[end:]


def forms(body):
    """Pure generator: baseline plus exactly the twenty already-measured forms."""
    if hashlib.sha256(body.encode('utf-8')).hexdigest() != BODY_SHA256:
        raise ValueError('unreviewed pool body; recorded controls require exact source shape')
    for anchor in (DECL, HEAD, '    s32 i;\n', '    count = 0;\n',
                   's32 (*gapCallback)(MemListNode*, u32)', 'u32 currentEnd;',
                   '    MemListNode* node;', '    return result;'):
        replace_once(body, anchor, anchor)
    out = {'baseline': body}
    out['global_direct'] = re.sub(r'\bentries\b', 'lbl_8031EB00', body.replace(DECL, ''))
    out['const_pointer'] = body.replace('MemListNode** entries =', 'MemListNode** const entries =')
    out['entries_before_index'] = body.replace(DECL, '').replace('    s32 i;\n', DECL+'    s32 i;\n')
    out['entries_assignment_after_count'] = body.replace(DECL, '    MemListNode** entries;\n').replace('    count = 0;\n', '    count = 0;\n    entries = lbl_8031EB00;\n')
    out['entries_assignment_after_node'] = body.replace(DECL, '    MemListNode** entries;\n').replace(HEAD, '    node = pool->secondary.head;\n    entries = lbl_8031EB00;\n    if (node != NULL) {')
    out['head_load_separate'] = body.replace(HEAD, SPLIT)
    def byte_address(text):
        text = text.replace('s32 (*gapCallback)(MemListNode*, u32)', 's32 (*gapCallback)(MemListNode*, u8*)').replace('u32 currentEnd;', 'u8* currentEnd;')
        return re.sub(r'(entries\[0\]|entries\[i\]|node)->flags', r'(u8*)\1->flags', text)
    def int_signature(text):
        return text.replace('s32 pool_garbage_collect(', 'int pool_garbage_collect(').replace('s32 (*gapCallback)', 'int (*gapCallback)')
    out['byte_address'] = byte_address(body)
    out['pdb_int_callback'] = int_signature(body)
    out['pdb_signature_address'] = int_signature(out['byte_address'])
    out['pdb_int_locals'] = out['pdb_signature_address'].replace('s32 result;', 'int result;').replace('s32 count;', 'int count;').replace('s32 i;', 'int i;')
    array = body.replace(DECL, '    MemListNode* (*entries)[] = &lbl_8031EB00;\n')
    out['array_address'] = re.sub(r'\bentries\[', '(*entries)[', array)
    out['array_address_node_split'] = out['array_address'].replace(HEAD, SPLIT)
    scoped = body.replace(HEAD, '    {\n    MemListNode* first;\n    if ((first = pool->secondary.head) != NULL) {')
    scoped = replace_once(scoped, 'entries[count++] = node;\n            node = node->next;', 'entries[count++] = first;\n            first = first->next;')
    scoped = replace_once(scoped, 'while (node != pool->secondary.head);\n    }', 'while (first != pool->secondary.head);\n    }\n    }')
    out['node_scoped_first'] = scoped
    out['byte_address_node_scoped'] = byte_address(scoped)
    out['lifetimes_off'] = '#pragma opt_lifetimes off\n'+body+'\n#pragma opt_lifetimes on\n'
    out['lifetimes_off_node_split'] = '#pragma opt_lifetimes off\n'+body.replace(HEAD, SPLIT)+'\n#pragma opt_lifetimes on\n'
    out['scoped_entries'] = body.replace(DECL, '').replace(HEAD, '    {\n'+DECL+HEAD).replace('    return result;', '    }\n    return result;')
    out['scoped_entries_node_split'] = out['scoped_entries'].replace('    {\n'+DECL+HEAD, '    node = pool->secondary.head;\n    {\n'+DECL+'    if (node != NULL) {')
    out['head_decl_initializer'] = body.replace('    MemListNode* node;', '    MemListNode* node = pool->secondary.head;').replace(HEAD, '    if (node != NULL) {')
    out['literal_element_address'] = body.replace('entries = lbl_8031EB00;', 'entries = &lbl_8031EB00[0];')
    return out


def check_frozen(paths, frozen):
    if paths.keys() != frozen.keys():
        raise ValueError('frozen input roster differs')
    drift = [key for key, path in paths.items() if not path.is_file() or path.read_bytes() != frozen[key]]
    if drift:
        raise ValueError('production inputs changed: ' + ', '.join(drift))


def require_fidelity(obj, error, expected):
    if error or obj is None or not obj.is_file() or obj.read_bytes() != expected:
        raise ValueError(error or 'complete raw baseline fidelity failed')


def candidate_error(obj, error):
    if error:
        return error
    if obj is None:
        return 'FAIL: compiler returned no candidate object'
    try:
        if not obj.is_file():
            return 'FAIL: candidate object is missing or not a file'
        obj.read_bytes()
    except OSError as failure:
        return 'FAIL: candidate object is unreadable: '+str(failure)
    return None


def probe_status(rows):
    return 'PASS' if rows and all(not row.get('error') and 'inventory' in row for row in rows.values()) else 'FAIL'


def compare_inventory(before, current, target):
    """Keep body changes distinct from layout/relocation/other metadata changes.

    Canonical is the shipped R71 whole-object inventory: anonymous symbols
    normalize only to their OWN section/offset/type/size; STT_FILE is excluded.
    Target body equality below is not a target-relocation or DOL certificate.
    """
    a = bytes.fromhex(current['functions'][FN]['body'])
    b = bytes.fromhex(target['functions'][FN]['body'])
    if len(a) % 4 or len(b) % 4:
        raise ValueError('non-word-aligned function body')
    words = [dict(offset=i, ours=a[i:i+4].hex(), target=b[i:i+4].hex())
             for i in range(0, len(a), 4) if a[i:i+4] != b[i:i+4]] if len(a) == len(b) else None
    names = before['functions'].keys() | current['functions'].keys()
    changed = sorted(n for n in names if before['functions'].get(n) != current['functions'].get(n))
    changed_bodies = sorted(n for n in names if before['functions'].get(n, {}).get('body') != current['functions'].get(n, {}).get('body'))
    def metadata(row):
        return {k: v for k, v in row.items() if k != 'body'} if row is not None else None
    changed_metadata = sorted(n for n in names if metadata(before['functions'].get(n)) != metadata(current['functions'].get(n)))
    expected = copy.deepcopy(before)
    baseline_size = len(bytes.fromhex(before['functions'][FN]['body']))
    same_extent = len(a) == baseline_size
    if same_extent:
        expected['functions'][FN]['body'] = a.hex()
        buf = bytearray.fromhex(expected['sections']['.text']['bytes'])
        off = before['functions'][FN]['offset']
        if off < 0 or off+len(a) > len(buf):
            raise ValueError('baseline function outside text')
        buf[off:off+len(a)] = a
        expected['sections']['.text']['bytes'] = buf.hex()
    section_names = before['sections'].keys() | current['sections'].keys()
    return dict(target_insns=len(b)//4, ours_insns=len(a)//4,
                differing_words=len(words) if words is not None else None, words=words,
                siblings_metadata_equal=same_extent and expected == current,
                changed_functions=changed, changed_bodies=changed_bodies,
                changed_function_metadata=changed_metadata,
                changed_nontext_sections=sorted(n for n in section_names if n != '.text' and before['sections'].get(n) != current['sections'].get(n)),
                frame=(-int.from_bytes(a[10:12], 'big')) & 0xffff if len(a) >= 12 else None)


def output_directory(path=None):
    if path is None:
        return Path(tempfile.mkdtemp(prefix='r76_pool_controls_', dir=ROOT/'build'))
    path = Path(path).resolve()
    if not path.is_relative_to((ROOT/'build').resolve()) or not path.name.startswith('r76_pool_'):
        raise ValueError('output directory must be lane-prefixed beneath build/')
    if path.exists():
        raise ValueError('output directory already exists; refusing artifact overwrite')
    path.mkdir(parents=True, exist_ok=False)
    return path


def replay(out=None):
    if Path.cwd().resolve() != ROOT or cv.REPO.resolve() != ROOT:
        raise ValueError('run from this tool repository root')
    edge = cv.read_edges()[UNIT]
    paths = dict(source=ROOT/edge['src'], raw=ROOT/edge['body_o'],
                 processed=ROOT/f'build/GUNE5D/src/{UNIT}.o', target=ROOT/f'build/GUNE5D/obj/{UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json', ninja=ROOT/'build.ninja',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe', types=ROOT/'include/types.h')
    # Current mempool source includes only types.h, which has no nested includes.
    for name in ('sjiswrap.exe', 'dtk.exe'):
        path = ROOT/'build/tools'/name
        if path.is_file():
            paths[name] = path
    for name, path in (('driver', Path(__file__)), ('cv_probe', Path(cv.__file__)),
                       ('canonical', ROOT/'tools/gdl/composed_census/r71_controls_retirement_audit.py'),
                       ('inventory', ROOT/'tools/gdl/composed_census/r68_aux_ownership_audit.py'),
                       ('elf_reader', ROOT/'tools/fix_exception_objects.py')):
        paths[name] = path
    frozen = {k: p.read_bytes() for k, p in paths.items()}
    text = frozen['source'].decode('utf-8').replace('\r\n', '\n')
    if re.findall(r'^#include\s+(.+)$', text, re.M) != ['"types.h"'] or re.search(r'^\s*#\s*include\b', frozen['types'].decode('utf-8'), re.M):
        raise ValueError('unreviewed include dependency closure')
    prefix, body, suffix = split_source(text)
    variants = forms(body)
    folder = output_directory(out)
    for key in ('raw', 'processed', 'target', 'config', 'source'):
        (folder/('r76_pool_'+key+paths[key].suffix)).write_bytes(frozen[key])
    before, wanted = canonical(paths['raw']), canonical(paths['target'])
    result = dict(schema_version=1, status='RUNNING', unit=UNIT, function=FN, edge=edge,
                  artifacts=str(folder.relative_to(ROOT)), paths={k: str(p.relative_to(ROOT)) for k,p in paths.items()},
                  hashes={k: hashlib.sha256(v).hexdigest() for k,v in frozen.items()},
                  baseline=before, target=wanted, probes={},
                  limits='Twenty recorded forms only; no source-unreachability, target relocation, rule retirement or DOL certification.')
    output = folder/'r76_pool_results.json'
    try:
        for name, newbody in variants.items():
            check_frozen(paths, frozen)
            work = folder/('r76_pool_'+name)
            work.mkdir()
            src = work/'mempool.c'
            src.write_text(prefix+newbody+suffix, encoding='utf-8')
            trial = dict(edge, src=str(src.relative_to(ROOT)), _command_trace=[])
            obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], work/'r76_pool.o', work)
            check_frozen(paths, frozen)
            error = candidate_error(obj, error)
            if name == 'baseline':
                require_fidelity(obj, error, frozen['raw'])
                result['baseline_fidelity'] = True
            row = dict(error=error, trace=trial['_command_trace'], source=str(src.relative_to(ROOT)))
            result['probes'][name] = row
            if obj is not None and not error:
                inv = canonical(obj)
                row.update(inventory=inv, whole_raw_identical=obj.read_bytes() == frozen['raw'],
                           **compare_inventory(before, inv, wanted))
                print(name, f"T{row['target_insns']}/O{row['ours_insns']}",
                      'words='+str(row['differing_words']), 'metadata='+str(row['siblings_metadata_equal']),
                      'body_changes='+','.join(row['changed_bodies']), flush=True)
            else:
                print(name, 'COMPILE_FAILURE', error, flush=True)
            output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
        result['status'] = probe_status(result['probes'])
    except Exception as error:
        result.update(status='UNRESOLVED', failure=str(error))
        raise
    finally:
        try:
            check_frozen(paths, frozen)
            result['inputs_frozen'] = True
        except Exception as error:
            result.update(status='UNRESOLVED', failure=str(error), inputs_frozen=False)
            raise
        finally:
            output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    return result, output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, help='new build/r76_pool_* output directory; never overwritten')
    args = parser.parse_args(argv)
    result, output = replay(args.out)
    print(result['status'], output)
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
