"""Manual full-TU certificate for fn_800C72DC native rule retirement.

Requires an immutable pre-change snapshot made with actual Ninja ELF fidelity.
No source/config mutation, no automatic rule removal, no blanket name masking.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r71_controls_retirement_audit import canonical

UNIT, FN = 'game/pb/pb_texture', 'fn_800C72DC'
BASELINE_SHA = '33f3b50b65e20caa2a78ef42c464e68a01eeb6680bec7bc823aafb6f9d3d412d'
# Immutable r89-pbtexture-20260907-340eee6c5, r89_texture_joint/before.
# Manifest SHA256: 3c1057eab1063dd6b2981447043bfcafe1d8adb62ed1e80cd5325b7d2fe78e61.
BASELINE_HASHES = {
    'raw.o': BASELINE_SHA,
    'processed.o': '5664042bb2596663f45c0426c433d32b55c2cebd6e609ae42c988dd73925c090',
    'webfrank.json': 'a5830158d76cfe22879891459aae644784ae458ac4997e4bfc7c880eaf689ebf',
    'target.o': '1c1f65dcf65904ca15740772e81a34bd03015ea5414a9bd024322071cf3be728',
}


def verify_inputs(baseline, current_target):
    """Refuse altered evidence before compiling or interpreting any ELF."""
    verified = {}
    for name, expected in BASELINE_HASHES.items():
        actual = hashlib.sha256((baseline/name).read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError('baseline snapshot hash differs: '+name)
        verified[name] = actual
    if hashlib.sha256(current_target.read_bytes()).hexdigest() != BASELINE_HASHES['target.o']:
        raise ValueError('current target object hash differs')
    return verified


def verify(before, after, target, old_rules, new_rules):
    expected = copy.deepcopy(old_rules)
    rules = expected['units'][UNIT]
    removed = [r for r in rules if r['function'] == FN]
    if len(removed) != 1:
        raise ValueError('snapshot must carry exactly one selected rule')
    expected['units'][UNIT] = [r for r in rules if r['function'] != FN]
    if expected['units'][UNIT] != new_rules['units'].get(UNIT):
        raise ValueError('TU configuration differs by more than the selected rule')
    if {k:v for k,v in old_rules.items() if k != 'units'} != {k:v for k,v in new_rules.items() if k != 'units'}:
        raise ValueError('global postprocessor options changed')
    if before['processed'] != after['processed']:
        raise ValueError('processed full allocated object changed')
    old, new = before['raw'], after['raw']
    if set(old['functions']) != set(new['functions']) or len(new['functions']) != 19:
        raise ValueError('function roster changed or empty')
    for name in old['functions']:
        if name != FN and old['functions'][name] != new['functions'][name]:
            raise ValueError('raw sibling changed: '+name)
    for key in ('symbols', 'all_symbols', 'exception_records'):
        if old[key] != new[key]:
            raise ValueError('raw metadata changed: '+key)
    for name, section in old['sections'].items():
        if name != '.text' and section != new['sections'].get(name):
            raise ValueError('nontext section changed: '+name)
    actual, wanted = new['functions'][FN], target['functions'][FN]
    if actual['size'] != 260 or actual['body'] != wanted['body']:
        raise ValueError('selected raw body differs from 65-word target')
    def relocations(rows):
        # MWCC encodes SDA21 relocation offset at halfword; dtk at word.
        return sorted((off & ~3 if kind == 109 else off, kind, name, add)
                      for off, kind, name, add in rows)
    if len(actual['relocations']) != 8 or relocations(actual['relocations']) != relocations(wanted['relocations']):
        raise ValueError('named positional target relocation bindings differ')
    return dict(status='PASS', instructions=[65,65], differing_words=0,
                raw_siblings_unchanged=18, named_target_relocations=8,
                configuration_scope=UNIT,
                foreign_unit_retirements='explicitly outside certificate scope',
                processed_allocated_object_equal=True,
                raw_nontext_symbols_EH_equal=True, retired_rules=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--before', required=True)
    ap.add_argument('--out', default='build/r89_retirement_certificate')
    args = ap.parse_args()
    folder = (ROOT/args.out).resolve()
    if not folder.is_relative_to((ROOT/'build').resolve()):
        raise ValueError('output must remain under build')
    folder.mkdir(parents=True, exist_ok=True)
    baseline = Path(args.before).resolve()
    target_path = ROOT/f'build/GUNE5D/obj/{UNIT}.o'
    input_hashes = verify_inputs(baseline, target_path)
    edge = cv.read_edges()[UNIT]
    trace = dict(edge, _command_trace=[])
    raw = ROOT/edge['body_o']
    obj, error = cv.compile_with(trace, edge['mw'], edge['cflags'], folder/'pb_texture.o', folder)
    if error or obj is None or obj.read_bytes() != raw.read_bytes():
        raise ValueError(error or 'fresh actual-Ninja full ELF fidelity failed')
    old = {k: canonical(baseline/(k+'.o')) for k in ('raw','processed')}
    new = dict(raw=canonical(raw),processed=canonical(ROOT/f'build/GUNE5D/src/{UNIT}.o'))
    target = canonical(target_path)
    result = verify(old,new,target,json.loads((baseline/'webfrank.json').read_text()),
                    json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text()))
    result.update(compiler=edge['mw'],flags=edge['cflags'],baseline_raw_sha256=BASELINE_SHA,
                  baseline_sha256=input_hashes, target_sha256=input_hashes['target.o'],
                  raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
                  current_fresh_full_ELF_fidelity=True)
    (folder/'certificate.json').write_text(json.dumps(result,indent=2))
    (folder/'inventory.json').write_text(json.dumps(new,indent=2))
    (folder/'edge.json').write_text(json.dumps(trace,indent=2))
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
