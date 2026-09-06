"""Bounded source/home-emission experiments for the two exception runtime TUs.

No production writes or new rewrite modes. Actual-source and explicit-header
overlay controls must reproduce each complete raw compiler object. A failing
header sentinel proves the overlay is used (MWCC can otherwise search include/
before the scratch source's directory). Finite failures are not source
impossibility proofs. Full-link trials retain every other configured input.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_symbol_binding_probe import object_facts
from tools.gdl.atree_exports import read_symbols
from tools.gdl.raw_object import load_graph

UNITS = ('Runtime.PPCEABI.H/NMWException', 'Runtime.PPCEABI.H/ExceptionPPC')
HANDLERS = ('static terminate_handler thandler = dthandler;\n'
            'static unexpected_handler uhandler = duhandler;\n')
WHAT = 'const char* exception::what() const { return "exception"; }'
SENTINEL = 'R73_HEADER_OVERLAY_ACTIVE'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('expected exactly one source anchor: ' + before)
    return text.replace(before, after)


def forms(unit, source, header):
    """Explicit, independent source controls; no types or compiler flags change."""
    start = header.index('#ifdef NMWEXCEPTION_CPP')
    end = header.index('#endif', start) + len('#endif')
    destructor = header[start:end]
    inline_what = replace_once(header, 'virtual const char* what() const;',
                              'virtual const char* what() const { return "exception"; }')
    result = {'control': (source, header), 'overlay_control': (source, header),
              'implicit_overlay_refusal': (source, '#error ' + SENTINEL + '\n'),
              'overlay_refusal': (source, '#error ' + SENTINEL + '\n'),
              'inline_what': (source, inline_what),
              'unified_inline': (source, inline_what.replace(destructor, 'virtual ~exception() throw() {}')),
              'unified_decl': (source, header.replace(destructor, 'virtual ~exception() throw();'))}
    if unit == UNITS[0]:
        late = replace_once(source, HANDLERS, '')
        late = replace_once(late, WHAT, WHAT + '\n\n' + HANDLERS.rstrip())
        result['late_handlers'] = (late, header)
        result['reverse_late_handlers'] = (
            replace_once(late, HANDLERS, '\n'.join(HANDLERS.strip().splitlines()[::-1]) + '\n'), header)
        result['inline_what_late_handlers'] = (late, inline_what)
        for label in ('inline_what', 'unified_inline', 'inline_what_late_handlers'):
            body, hdr = result[label]
            body = replace_once(body, WHAT, '')
            if label == 'unified_inline':
                body = replace_once(body, 'exception::~exception() throw() {}', '')
            result[label] = body, hdr
    return result


def overlay_include(source, relative_header):
    return replace_once(source, '#include "NMWException.h"',
                        '#include "' + Path(relative_header).as_posix() + '"')


def observe(path, baseline):
    facts = object_facts(path)
    return dict(sha256=digest(path),
                missing_functions=sorted(baseline[0].keys() - facts[0].keys()),
                extra_functions=sorted(facts[0].keys() - baseline[0].keys()),
                changed_bodies=sorted(n for n in baseline[0].keys() & facts[0].keys()
                                      if baseline[0][n]['body'] != facts[0][n]['body']),
                rodata_hex=facts[1].get('.rodata'),
                small_data=[dict(name=s.name, offset=s.value, size=s.size, binding=s.bind)
                            for s in read_symbols(path) if s.section == '.sdata'],
                allocated_geometry=facts[2], relocations=facts[3], functions=facts[0])


def run(argv):
    p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True,
                       errors='replace', timeout=120)
    return dict(argv=argv, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)


def link_trials(folder, graph, objects):
    links = [e for e in graph['edges'] if e['rule'] == 'link']
    if len(links) != 1 or links[0]['outputs'] != ['build/GUNE5D/main.elf']:
        raise ValueError('expected unique default linker edge')
    link = links[0]
    command = run(['ninja', '-t', 'compdb', 'link'])
    if command['exit_code']:
        raise ValueError('cannot read actual linker command')
    compdb = json.loads(command['stdout'])
    if len(compdb) != 1:
        raise ValueError('ambiguous linker command')
    argv = [t.strip('"') for t in shlex.split(compdb[0]['command'], posix=False)]
    if not argv[0].lower().endswith('mwldeppc.exe') or argv.count('-o') != 1:
        raise ValueError('unsupported linker command')
    if any(t in {'-map', '&&', ';', '|', '>'} for t in argv):
        raise ValueError('linker command has additional side effects')
    oi = argv.index('-o') + 1
    ri = [i for i, t in enumerate(argv) if t.startswith('@')]
    if len(ri) != 1:
        raise ValueError('expected one linker response file')
    baseline_elf = (ROOT / 'build/GUNE5D/main.elf').read_bytes()
    baseline_dol = (ROOT / 'build/GUNE5D/main.dol').read_bytes()
    protected = {p: digest(ROOT / p) for p in link['inputs']}
    rows = {}
    for label in ('control', 'raw', 'inline_what', 'unified_decl'):
        inputs = list(link['inputs'])
        if label != 'control':
            key = 'build/GUNE5D/src/' + UNITS[1] + '.o'
            if inputs.count(key) != 1:
                raise ValueError('runtime input does not resolve uniquely')
            inputs[inputs.index(key)] = str(objects['control' if label == 'raw' else label])
        rsp = folder / (label + '.rsp')
        rsp.write_text('\n'.join('"' + p + '"' for p in inputs) + '\n', encoding='ascii')
        trial = list(argv)
        output = folder / (label + '.elf')
        trial[oi], trial[ri[0]] = str(output), '@' + str(rsp)
        row = run(trial)
        if row['exit_code'] == 0:
            if label == 'control' and output.read_bytes() != baseline_elf:
                raise ValueError('unchanged full-link control differs')
            dol = folder / (label + '.dol')
            conversion = run(['build/tools/dtk.exe', 'elf2dol', str(output), str(dol)])
            if conversion['exit_code']:
                raise ValueError('ELF conversion failed')
            data = dol.read_bytes()
            row.update(dol_equal=data == baseline_dol, dol_size=len(data),
                       differing_file_bytes=sum(a != b for a, b in zip(data, baseline_dol))
                       + abs(len(data) - len(baseline_dol)))
        elif label == 'control':
            raise ValueError('unchanged full-link control failed')
        rows[label] = row
    if any(digest(ROOT / p) != h for p, h in protected.items()):
        raise ValueError('production linker input changed')
    if (ROOT / 'build/GUNE5D/main.elf').read_bytes() != baseline_elf or (ROOT / 'build/GUNE5D/main.dol').read_bytes() != baseline_dol:
        raise ValueError('production link output changed')
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / 'build') or output.suffix != '.json':
        parser.error('--out must be a JSON artifact in this checkout build directory')
    if os.name != 'nt':
        parser.error('this actual native-link experiment currently supports Windows only')
    graph = load_graph(ROOT, 'GUNE5D')
    edges = cv.read_edges()
    header_path = ROOT / 'include/NMWException.h'
    frozen = {header_path: header_path.read_bytes()}
    folder = Path(tempfile.mkdtemp(prefix='r73_runtime_source_', dir=ROOT / 'build'))
    result = dict(schema_version=1, status='UNRESOLVED', artifacts=str(folder.relative_to(ROOT)),
                  disclaimer='Finite source controls, not necessity or semantic equivalence proof.', compiles={})
    all_objects = {}
    for unit in UNITS:
        edge = edges[unit]
        source_path, raw_path = ROOT / edge['src'], ROOT / edge['body_o']
        frozen.update({p: p.read_bytes() for p in (source_path, raw_path)})
        baseline = object_facts(raw_path)
        source, header = source_path.read_text(), header_path.read_text()
        objects = {}
        for label, (body, hdr) in forms(unit, source, header).items():
            directory = folder / unit.split('/')[-1] / label
            directory.mkdir(parents=True)
            trial = dict(edge, _command_trace=[])
            if label != 'control':
                overlay = directory / 'NMWException.h'
                overlay.write_text(hdr, encoding='utf-8')
                scratch = directory / source_path.name
                selected_body = body if label == 'implicit_overlay_refusal' else overlay_include(body, overlay.relative_to(ROOT))
                scratch.write_text(selected_body, encoding='utf-8')
                trial['src'] = str(scratch.relative_to(ROOT))
            obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], directory / 'output.o', directory)
            row = dict(error=error, trace=trial['_command_trace'])
            if label == 'overlay_refusal':
                if not error or SENTINEL not in ''.join(r.get('stdout', '') + r.get('stderr', '') for r in trial['_command_trace']):
                    raise ValueError('header sentinel did not cause the expected refusal')
            else:
                if error or not obj:
                    raise ValueError(error or 'missing compiler object')
                if label in ('control', 'overlay_control', 'implicit_overlay_refusal') and obj.read_bytes() != frozen[raw_path]:
                    raise ValueError('complete raw compiler fidelity failed: ' + unit + '/' + label)
                row.update(observe(obj, baseline))
                objects[label] = obj
            result['compiles'][unit + '/' + label] = row
            print(unit, label, error or ('changed bodies: ' + repr(row['changed_bodies'])), flush=True)
        all_objects[unit] = objects
    result['links'] = link_trials(folder, graph, all_objects[UNITS[1]])
    if any(p.read_bytes() != data for p, data in frozen.items()):
        raise ValueError('production source/raw input changed')
    result['status'] = 'COMPLETE'
    result['input_hashes'] = {str(p.relative_to(ROOT)): hashlib.sha256(data).hexdigest()
                              for p, data in frozen.items()}
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('COMPLETE', {k: (v['exit_code'], v.get('dol_equal'), v.get('differing_file_bytes'))
                       for k, v in result['links'].items()}, output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
