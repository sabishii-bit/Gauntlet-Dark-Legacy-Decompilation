"""Manual certificate for the approved MBNewNode source-only retirement.

Requires the R76 replay's archived baseline and report. This is not a build
hook, source transformer, or mod-build gate. The linked DOL gate is separate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census import r76_tree_helper_reconstruction as prior
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical
from tools.gdl.raw_object import resolve_object


def tokens(source):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S).split()


def check_source(before, after):
    if tokens(prior.reconstruct(before, 'shared')) != tokens(after):
        raise ValueError('source is not precisely the reviewed shared-helper factoring')


def check_rules(before, after):
    expected = json.loads(json.dumps(before))
    rules = expected['units'].pop(prior.UNIT, None)
    if not rules or len(rules) != 1 or rules[0]['function'] != prior.FN:
        raise ValueError('expected exactly one archived MBNewNode rule')
    if expected != after:
        raise ValueError('rule changes exceed retirement of mb_tree edge')


def certify(before, after, target, processed):
    result = prior.audit(before, after, target, processed)
    result.update(status='SOURCE_NATIVE_RETIREMENT', source_and_config_modified=True,
                  rules_retired=1,
                  policy='User approved documented shared-helper compatibility factoring on 2026-09-06; original provenance unproven.')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', type=Path, required=True)
    parser.add_argument('--before-report', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=Path('build/r77_tree_retirement.json'))
    args = parser.parse_args(argv)
    if not args.out.resolve().is_relative_to((ROOT/'build').resolve()):
        parser.error('output must stay below build/')
    base, report = args.baseline_dir, json.loads(args.before_report.read_text())
    edge = cv.read_edges()[prior.UNIT]
    selected = resolve_object(prior.UNIT, root=ROOT, view='compiler')
    # resolve_object returns a descriptive selector, not freshness evidence.
    raw = selected.path
    if raw.resolve() != (ROOT / edge['body_o']).resolve():
        raise ValueError('compiler selector disagrees with parsed Ninja edge')
    paths = dict(source=ROOT/edge['src'], raw=raw,
                 target=ROOT/f'build/GUNE5D/obj/{prior.UNIT}.o',
                 processed=ROOT/f'build/GUNE5D/src/{prior.UNIT}.o',
                 config=ROOT/'config/GUNE5D/webfrank.json', ninja=ROOT/'build.ninja',
                 compiler=ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe', types=ROOT/'include/types.h')
    frozen = {k:p.read_bytes() for k,p in paths.items()}
    check_source((base/'baseline/mb_tree.c').read_text(), frozen['source'].decode())
    check_rules(json.loads((base/'r76_tree_rules.json').read_text()), json.loads(frozen['config']))
    for key in ('mw', 'cflags', 'rule', 'command_template', 'extab_padding'):
        if edge.get(key) != report['edge'].get(key):
            raise ValueError('compiler edge drift: '+key)
    for key in ('compiler', 'target', 'types'):
        if hashlib.sha256(frozen[key]).hexdigest() != report['hashes'][key]:
            raise ValueError('baseline input drift: '+key)
    folder = Path(tempfile.mkdtemp(prefix='r77_tree_certificate_', dir=ROOT/'build'))
    trial = dict(edge, _command_trace=[])
    obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], folder/'mb_tree.o', folder)
    if error or obj is None or obj.read_bytes() != frozen['raw']:
        raise ValueError(error or 'fresh complete compiler-object fidelity failed')
    before, target, processed = [canonical(base/f'r76_tree_{name}.o') for name in ('raw', 'target', 'processed')]
    if target != canonical(paths['target']) or processed != canonical(paths['processed']):
        raise ValueError('target or final allocated object changed since baseline')
    result = certify(before, canonical(obj), target, processed)
    if any(p.read_bytes() != frozen[k] for k,p in paths.items()):
        raise ValueError('production inputs drifted during certificate')
    result.update(baseline_fidelity=True, compiler=edge['mw'], edge=edge,
                  command_trace=trial['_command_trace'], inputs_frozen=True,
                  baseline=str(base), object_sha256=hashlib.sha256(frozen['raw']).hexdigest())
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('command_trace','edge')}))


if __name__ == '__main__':
    main()
