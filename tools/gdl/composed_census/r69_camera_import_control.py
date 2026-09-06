"""Camera import-name diagnostic using fresh raw whole-TU controls.

The mapping is deliberately explicit and excludes DiffRate: its caller needs
a separate argument-value investigation, not only a name repair. No object is patched.
Target call multiplicity/type/addend and whole-TU identity are checked by the
R68 control; ABI meaning requires the separately recorded target inspection.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r68_interfaces_import_control import audit
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory

UNIT = 'game/world/camera'
MAPPING = {
    'MoveCam_walk': 'MoveCam_walk_8002A024',
    'ProcCamera': 'ProcCamera_8002E548',
    'StandardCamera': 'StandardCamera_8002B828',
    'calc_cam_pyr': 'calc_cam_pyr_8002A97C',
    'cam_orient_to': 'cam_orient_to_80029E8C',
    'get_cam_wpos': 'get_cam_wpos_8002ABE0',
}


def provider_check(functions):
    for name in MAPPING.values():
        if name not in functions or functions[name]['binding'] != 1:
            raise ValueError('missing GLOBAL provider: ' + name)


def calls(functions, names):
    result = []
    for name, fn in functions.items():
        body = bytes.fromhex(fn['body'])
        for offset, kind, symbol, addend in fn['relocations']:
            if symbol in names:
                result.append(dict(caller=name, offset=offset, kind=kind,
                                   symbol=symbol, addend=addend,
                                   preceding_words=[dict(offset=i, word=body[i:i+4].hex())
                                                    for i in range(max(0, offset-24), offset, 4)]))
    return result


def run(before=None):
    current = capture(UNIT)
    target, _ = inventory(ROOT / 'build/GUNE5D/obj/game/world/camera.o')
    provider, _ = inventory(ROOT / 'build/GUNE5D/src/game/game/.postprocess/body/combat.o')
    provider_check(provider)
    result = audit(before, current, MAPPING, target) if before else dict(status='PASS')
    result['current'] = current
    result['source_calls'] = calls(current['functions'], set(MAPPING) | set(MAPPING.values()) | {'DiffRate'})
    result['target_calls'] = calls(target, set(MAPPING.values()) | {'camera_orbit_update', 'DiffRate_8002951C'})
    result['providers'] = {name: {k: provider[name][k] for k in ('binding', 'size', 'offset')} for name in MAPPING.values()}
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / 'build') or not output.name.startswith('r69_camera_') or output.suffix != '.json':
        parser.error('--out must name build/**/r69_camera_*.json')
    try:
        before = json.loads(args.before.read_text())['current'] if args.before else None
        result = run(before)
    except (OSError, ValueError, KeyError) as error:
        result = dict(status='UNRESOLVED', error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('current', 'source_calls', 'target_calls', 'import_renames')}, indent=2))
    print('written', output)
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
