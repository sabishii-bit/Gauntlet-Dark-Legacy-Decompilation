"""Bounded joint header/option controls for exception-runtime reconstruction.

Actual Ninja commands and complete raw controls are mandatory. All generated
files stay in build/. COMPLETE means a finite measurement, not a match or a
proof of source-unreachability. No production source or objects are changed.
Header pragma pairs that do not move output are INCONCLUSIVE about sensitivity.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import r73_runtime_source_probe as base

OPTIONS = {
    'base': '',
    'deferred': '-inline deferred',
    'noauto': '-inline noauto',
    'deferred_noauto': '-inline deferred,noauto',
    'nopool': '-pool off',
}


def variants(unit, source, header):
    """Return explicit (source, header, extra flags) triples without mutation."""
    prior = base.forms(unit, source, header)
    result = {}
    for name in ('overlay_control', 'inline_what', 'unified_inline', 'unified_decl'):
        body, hdr = prior[name]
        for option, flags in OPTIONS.items():
            result[name + '__' + option] = body, hdr, flags
    for name in ('overlay_control', 'inline_what', 'unified_inline'):
        body, hdr = prior[name]
        for pragma in ('force_active', 'defer_codegen'):
            for setting in ('on', 'off'):
                selected = base.replace_once(hdr, 'namespace std {',
                    '#pragma ' + pragma + ' ' + setting + '\nnamespace std {')
                selected = base.replace_once(selected, '} // namespace std',
                    '} // namespace std\n#pragma ' + pragma + ' reset')
                result[name + '__header_' + pragma + '_' + setting] = body, selected, ''
    return result


def summarize(rows):
    """Pair option-sensitive controls; inert header pragmas remain inconclusive."""
    keys = ('missing_functions', 'extra_functions', 'changed_bodies',
            'rodata_hex', 'small_data', 'allocated_geometry', 'relocations', 'functions')
    baseline = rows['overlay_control__base']
    changed = {}
    for label, row in rows.items():
        changed[label] = [k for k in keys if row[k] != baseline[k]]
    sensitivity = {
        option: bool(changed['overlay_control__' + option])
        for option in OPTIONS if option != 'base'
    }
    return {'option_control_changed': sensitivity,
            'changed_fact_categories': changed,
            'pragma_warning': 'No output change does not establish pragma sensitivity or source impossibility.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if os.name != 'nt' or not output.is_relative_to(ROOT / 'build') or output.suffix != '.json':
        parser.error('Native Windows only; --out must name a JSON artifact inside this checkout build/')
    edges = base.cv.read_edges()
    header_path = ROOT / 'include/NMWException.h'
    protected = {header_path: header_path.read_bytes()}
    folder = Path(tempfile.mkdtemp(prefix='r74_runtime_emission_', dir=ROOT / 'build'))
    result = {'schema_version': 1, 'status': 'UNRESOLVED',
              'artifacts': folder.relative_to(ROOT).as_posix(), 'units': {}}
    for unit in base.UNITS:
        edge = edges[unit]
        source_path, raw_path = ROOT / edge['src'], ROOT / edge['body_o']
        compiler_path = ROOT / 'build/compilers' / edge['mw'] / 'mwcceppc.exe'
        protected.update({p: p.read_bytes() for p in (source_path, raw_path, compiler_path)})
        source, header = source_path.read_text(), header_path.read_text()
        specs = variants(unit, source, header)
        specs = {'actual_control': (source, header, ''),
                 'overlay_sentinel': (source, '#error ' + base.SENTINEL + '\n', ''), **specs}
        facts = base.object_facts(raw_path)
        rows = {}
        for label, (body, hdr, flags) in specs.items():
            directory = folder / unit.split('/')[-1] / label
            directory.mkdir(parents=True)
            trial = dict(edge, _command_trace=[])
            if label != 'actual_control':
                overlay = directory / 'NMWException.h'
                overlay.write_text(hdr, encoding='utf-8')
                scratch = directory / source_path.name
                scratch.write_text(base.overlay_include(body, overlay.relative_to(ROOT)), encoding='utf-8')
                trial['src'] = str(scratch.relative_to(ROOT))
            obj, error = base.cv.compile_with(trial, edge['mw'], edge['cflags'] + ' ' + flags,
                                               directory / 'output.o', directory)
            trace = trial['_command_trace']
            if label == 'overlay_sentinel':
                if not error or base.SENTINEL not in ''.join(t.get('stdout', '') + t.get('stderr', '') for t in trace):
                    raise ValueError('header-selection sentinel did not refuse as expected')
                rows[label] = {'expected_refusal': error, 'trace': trace}
                continue
            if error or not obj:
                raise ValueError(error or 'missing output')
            if label in ('actual_control', 'overlay_control__base') and obj.read_bytes() != protected[raw_path]:
                raise ValueError('complete raw compiler fidelity failed: ' + unit + '/' + label)
            rows[label] = dict(base.observe(obj, facts), trace=trace, extra_flags=flags)
        summary = summarize({k: v for k, v in rows.items() if '__' in k})
        result['units'][unit] = {'rows': rows, 'summary': summary}
        print(unit, 'controls=PASS variants=' + str(len(rows) - 2),
              'raw_functions=' + str(len(facts[0])), summary['option_control_changed'], flush=True)
    if any(path.read_bytes() != data for path, data in protected.items()):
        raise ValueError('production source/raw object changed')
    result['input_hashes'] = {p.relative_to(ROOT).as_posix(): hashlib.sha256(data).hexdigest()
                              for p, data in protected.items()}
    result['status'] = 'COMPLETE'
    result['scope'] = 'No full-link trials; object emission evidence only. No necessity claim.'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('COMPLETE', output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
