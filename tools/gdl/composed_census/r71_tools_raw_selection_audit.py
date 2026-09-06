"""Compare active raw selectors to actual Ninja compile edges; no build or edit.

PASS certifies path selection for this graph, not source freshness, semantics
or whole-object matching. A supplied retired object is only read: it proves
the historical wrong-selection consequence without reinstalling that object.
Run after configure and ninja. All generated artifacts live under build/.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/gdl'))
sys.path.insert(0, str(ROOT / 'tools/gdl/composed_census'))
import raw_object
import fnasm
import fndiff
import probe
import cn_analyze
import cv_probe


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--retired-object', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT/'build/r71_tools_selection.json')
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to(ROOT/'build') or not out.name.startswith('r71_tools'):
        parser.error('--out must be under build/ with r71_tools basename')
    result = dict(status='UNRESOLVED', scope=__doc__, units=[], refused_units=[], failures=[])
    try:
        graph = raw_object.load_graph(ROOT, 'GUNE5D')
        result.update(mode='editable' if graph['non_matching'] else 'matching',
                      ninja_sha256=sha(ROOT/'build.ninja'),
                      snapshot_sha256=sha(ROOT/'build/GUNE5D/build_edges.json'))
        edges = cv_probe.read_edges()
        owners = {fndiff.unit_key(row['name']): row for row in graph['units']
                  if row.get('source_object')}
        source_units = set(owners)
        producers = {output: edge['rule'] for edge in graph['edges']
                     for output in edge['outputs']}
        result['active_source_units'] = len(source_units)
        result['parsed_compile_units'] = len(edges)
        result['source_units_without_compile_edge'] = sorted(source_units - edges.keys())
        result['compile_units_without_source_owner'] = sorted(edges.keys() - source_units)
        # cv_probe deliberately inventories MWCC, not assembler sources.
        # Name every omission and exercise its refusal, never silently drop it.
        for unit in result['source_units_without_compile_edge']:
            producer = producers.get(owners[unit]['source_object'])
            try:
                selected = raw_object.resolve_object(unit, root=ROOT, require_exists=False)
                result['failures'].append('unparsed source unexpectedly resolved: ' + unit)
            except raw_object.RawObjectError as error:
                result['refused_units'].append(dict(unit=unit, producer=producer,
                    expected_non_compiler=producer == 'as', error=str(error)))
            if producer != 'as':
                result['failures'].append(unit)
        result['failures'].extend(result['compile_units_without_source_owner'])
        for unit, edge in sorted(edges.items()):
            try:
                selected = raw_object.resolve_object(unit, root=ROOT, require_exists=False)
            except raw_object.RawObjectError as error:
                result['refused_units'].append(dict(unit=unit, error=str(error)))
                result['failures'].append(unit)
                continue
            row = dict(unit=unit, selected=selected.relative, ninja_compile=edge['body_o'],
                       pipeline=selected.pipeline, built=selected.path.is_file())
            row['equal'] = row['selected'] == row['ninja_compile']
            result['units'].append(row)
            if not row['equal']:
                result['failures'].append(unit)
        result['pipeline_counts'] = dict(Counter(' -> '.join(row['pipeline']) for row in result['units']))
        # The R70 retirement seed: inspect every consumer with the phantom
        # file present if the caller has installed it in this worktree.
        unit, function = 'game/g3d/sndvoice', 'sndVoiceUpdateAll'
        target = cn_analyze.load(cn_analyze.target_object(unit), function)[3]
        paths = dict(fnasm=fnasm.raw_obj_path(unit),
                     fndiff=fndiff.ours_object_path(unit, raw=True)[0],
                     probe=probe.raw_object_target(unit), cn_analyze=cn_analyze.our_object(unit)[0])
        if args.retired_object:
            paths['retired_reference_NOT_SELECTED'] = args.retired_object
        rows = {}
        for name, path in paths.items():
            body = cn_analyze.load(path, function)[3]
            rows[name] = dict(path=str(path), object_sha256=sha(path),
                target_instructions=len(target)//4, current_instructions=len(body)//4,
                raw_words=(sum(body[i:i+4] != target[i:i+4] for i in range(0,len(body),4))
                           if len(body) == len(target) else None))
        result['retirement_seed'] = rows
        result['status'] = 'FAIL' if result['failures'] else 'PASS'
    except (OSError, ValueError, KeyError) as error:
        result['failures'].append(str(error))
    out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({key:value for key,value in result.items() if key not in ('scope','units')}, indent=2))
    print(f'wrote {out}; compared {len(result["units"])} compile edges')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
