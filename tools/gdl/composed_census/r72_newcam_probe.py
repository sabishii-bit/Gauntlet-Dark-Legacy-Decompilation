"""Finite, scratch-only newcam source experiments with full raw-object fidelity.

PASS means the requested measurements ran, never source exhaustion or matching.
No production source, flags, pins, or target objects are modified.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory
from tools.gdl.composed_census.r71_pad_identity_probe import differences, words
from tools.gdl import fndiff, slotdiff

UNIT = "game/world/newcam"
FUNCTION = "fn_8006DC64"
TARGET_ORDER = ["fn_8006DC2C", FUNCTION, "fn_8006DF34", "UpdateCam", "fn_8006E654",
                "CurTransmitterBlink", "StdCamReturn", "StdCamFreeze", "CalcDist",
                "CalcFrustrumNormals", "fn_8006F16C", "fn_8006F418", "GetPlayerAvgPos",
                "CamGetPlayerAvgPos", "fn_8006FBAC", "fn_8006FCDC", "fn_8006FE30",
                "fn_8006FF1C", "fn_80070144", "CamLookInDir", "DebugCamControlInputs",
                "DebugCamInit", "CamReset"]


def definition_order_forms(source):
    """Move intact definitions after necessary declarations; preserve scoped pragmas.

    The same-order control separates declaration/helper visibility from order.
    This is a bounded lexical transformation of this source, not a general C parser.
    """
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source, flags=re.S)
    spans = []
    for name in TARGET_ORDER:
        pattern = r'(?m)^([A-Za-z_][^;{}]*\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*)\{'
        found = list(re.finditer(pattern, masked))
        if len(found) != 1:
            raise ValueError('definition extraction ambiguous: ' + name)
        match = found[0]
        depth, stop = 1, match.end()
        while depth and stop < len(masked):
            depth += (masked[stop] == '{') - (masked[stop] == '}')
            stop += 1
        if depth:
            raise ValueError('unterminated definition: ' + name)
        spans.append((match.start(), stop, name, source[match.start():stop],
                      source[match.start():match.end()-1].strip() + ';'))
    spans.sort()
    settings = {"fn_8006FF1C": "opt_propagation", "GetPlayerAvgPos": "opt_propagation",
                "fn_8006DF34": "opt_propagation", "fn_8006E654": "opt_propagation",
                "CamReset": "dont_inline"}
    directives = list(re.finditer(r'(?m)^#pragma\s+(\w+)\s+(\w+)\s*$', source))
    if len(directives) != 10:
        raise ValueError('reviewed pragma inventory changed')
    for start, stop, name, body, declaration in spans:
        active = {'opt_propagation': 'reset', 'dont_inline': 'off'}
        for directive in directives:
            if directive.start() >= start:
                break
            key, value = directive.groups()
            if key not in active:
                raise ValueError('unreviewed pragma: ' + key)
            active[key] = value
        expected = {'opt_propagation': 'reset', 'dont_inline': 'off'}
        if name in settings:
            expected[settings[name]] = 'on' if name == 'CamReset' else 'off'
        if active != expected:
            raise ValueError('pragma association changed: ' + name)
    definitions = re.findall(r'(?m)^#(?:define|undef)\s+(\w+)', source)
    if definitions != ['NC_DOT', 'NC_DOT']:
        raise ValueError('reviewed macro inventory changed')
    preamble = source
    for start, stop, name, body, declaration in reversed(spans):
        preamble = preamble[:start] + declaration + preamble[stop:]
    originals = {n: body for _, _, n, body, _ in spans}
    def assemble(order, prefix=preamble, bodies=originals):
        parts = [prefix]
        for name in order:
            pragma = settings.get(name)
            if name == "CalcDist":
                macro_start = source.index('#define NC_DOT')
                macro_end = source.index('\n\n', macro_start)
                parts.append(source[macro_start:macro_end])
            if pragma:
                parts.append('#pragma ' + pragma + (' on' if pragma == 'dont_inline' else ' off'))
            parts.append(bodies[name])
            if pragma:
                parts.append('#pragma ' + pragma + (' off' if pragma == 'dont_inline' else ' reset'))
            if name == "CalcDist":
                parts.append('#undef NC_DOT')
        return '\n\n'.join(parts) + '\n'
    # Production-quality presentation of the same experiment: keep nearby
    # function documentation with its definition, not orphaned in the preamble.
    clean_prefix = source
    documented = dict(originals)
    removals = []
    for start, stop, name, body, declaration in spans:
        prefix = source[:start]
        comments = list(re.finditer(r'/\*.*?\*/', prefix, re.S))
        remove_start = start
        if comments:
            comment = comments[-1]
            between = re.sub(r'(?m)^#pragma[^\n]*', '', prefix[comment.end():])
            if not between.strip():
                remove_start = comment.start()
                documented[name] = comment[0] + '\n' + body
        removals.append((remove_start, stop))
    for start, stop in reversed(removals):
        clean_prefix = clean_prefix[:start] + clean_prefix[stop:]
    clean_prefix = re.sub(r'(?m)^#pragma[^\n]*\n?', '', clean_prefix)
    clean_prefix = re.sub(r'(?m)^#define NC_DOT[^\n]*\n[^\n]*\n', '', clean_prefix)
    clean_prefix = re.sub(r'(?m)^#undef NC_DOT[^\n]*\n?', '', clean_prefix)
    clean_prefix = re.sub(r'\n{3,}', '\n\n', clean_prefix).rstrip()
    declarations = '\n'.join(s[4] for s in spans)
    clean_prefix += '\n\n/* TU-local declarations; definitions follow retail code order. */\n' + declarations
    return [("declaration_hoist_control", assemble([s[2] for s in spans])),
            ("target_definition_order", assemble(TARGET_ORDER)),
            ("clean_hoist_control", assemble([s[2] for s in spans], clean_prefix, documented)),
            ("clean_target_order", assemble(TARGET_ORDER, clean_prefix, documented))]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def normalized_relocations(inventory):
    """Keep named identity; resolve renamed private pool entries by full datum.

    This checks use bindings, not runtime ownership of external objects. Only
    non-relocated compiler-private initialized data may use the datum identity.
    """
    result = {}
    for name, function in inventory["functions"].items():
        rows = []
        for offset, kind, symbol, addend in function["relocations"]:
            identity = ["named", symbol]
            if symbol.startswith('@'):
                datum = inventory["symbols"].get(symbol)
                if not datum or datum["bytes"] is None:
                    raise ValueError("unresolved private relocation: " + symbol)
                if datum.get('binding') != 0 or datum['section'] not in ('.sdata2', '.rodata'):
                    raise ValueError('private datum must be local readonly pool: ' + symbol)
                section = inventory["sections"][datum["section"]]
                if section["relocations"]:
                    raise ValueError("private pointer-bearing section unsupported")
                if not 0 <= addend < datum["size"]:
                    raise ValueError("private datum addend outside object")
                identity = ["private-datum", datum["section"], datum["size"], datum["bytes"]]
            rows.append([offset, kind, identity, addend])
        result[name] = rows
    return result


def validate_selection(selected, available):
    if selected and selected - set(available):
        raise ValueError("unknown forms: " + ','.join(sorted(selected - set(available))))


def validate_target_order(target):
    actual = sorted(target["functions"], key=lambda n: target["functions"][n]["offset"])
    if actual != TARGET_ORDER:
        raise ValueError("reviewed target function sequence changed")
    return actual


def variants(source):
    start = source.index("    positionDeltaX = player->pos[0] + pt->x;")
    end = source.index("    outputY = pt->y;", start)
    forms = {
        "scratch_control": source[start:end],
        "bg_d_control": """    positionDeltaX = player->pos[0] + pt->x;
    positionDeltaY = player->pos[1] + pt->y - cameraY;
    positionDeltaZ = player->pos[2] + pt->z - cameraZ;
    deltaX = player->clip_pos.x + pt->x;
    deltaY = player->clip_pos.y + pt->y - cameraY;
    deltaZ = player->clip_pos.z + pt->z - cameraZ;
    positionDeltaX -= cameraX;
    deltaX -= cameraX;
""",
        "interleaved_joint": """    positionDeltaX = player->pos[0] + pt->x;
    deltaX = player->clip_pos.x + pt->x;
    positionDeltaY = player->pos[1] + pt->y - cameraY;
    deltaY = player->clip_pos.y + pt->y - cameraY;
    positionDeltaX -= cameraX;
    positionDeltaZ = player->pos[2] + pt->z - cameraZ;
    deltaX -= cameraX;
    deltaZ = player->clip_pos.z + pt->z - cameraZ;
""",
    }
    return ([(name, source[:start] + block + source[end:]) for name, block in forms.items()]
            + definition_order_forms(source))


def run(selected=None):
    edge = cv.read_edges()[UNIT]
    if edge["mw"] != "GC/1.2.5" or edge["rule"] != "mwcc_sjis":
        raise ValueError("configured compiler/runner changed")
    paths = [ROOT / edge["src"], ROOT / edge["body_o"],
             ROOT / f"build/GUNE5D/obj/{UNIT}.o", ROOT / f"build/GUNE5D/src/{UNIT}.o",
             ROOT / "config/GUNE5D/webfrank.json", ROOT / "build.ninja",
             ROOT / "build/compilers/GC/1.2.5/mwcceppc.exe"]
    protected = {p: p.read_bytes() for p in paths}
    source_path, raw_path, target_path = paths[:3]
    forms = variants(protected[source_path].decode("utf-8"))
    validate_selection(selected, [name for name, source in forms])
    folder = Path(tempfile.mkdtemp(prefix="r72_newcam_", dir=ROOT / "build"))
    for name, path in (("baseline.c", source_path), ("baseline_raw.o", raw_path),
                       ("target.o", target_path), ("baseline_processed.o", paths[3])):
        (folder / name).write_bytes(protected[path])
    trace = dict(edge, _command_trace=[])
    control, error = cv.compile_with(trace, edge["mw"], edge["cflags"], folder / "actual_control.o", folder)
    if error or control.read_bytes() != protected[raw_path]:
        raise ValueError("actual-command whole-object fidelity failed: " + str(error))
    baseline, target = object_inventory(raw_path), object_inventory(target_path)
    measured_order = validate_target_order(target)
    baseline_relocs = normalized_relocations(baseline)
    report = dict(schema_version=1, status="PASS", source_exhaustion=False,
                  unit=UNIT, edge=edge, baseline_command=trace["_command_trace"],
                  protected={str(p.relative_to(ROOT)): sha(v) for p, v in protected.items()},
                  baseline=baseline, target=target, target_order=measured_order, variants=[])
    target_lines = fndiff.parse(target_path)[FUNCTION]
    for name, source in forms:
        if selected and name not in selected and name != "scratch_control":
            continue
        trial_dir = folder / name
        trial_dir.mkdir()
        candidate = trial_dir / source_path.name
        candidate.write_text(source, encoding="utf-8", newline="\n")
        trial = dict(edge, src=str(candidate.relative_to(ROOT)), _command_trace=[])
        output, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / (name + ".o"), folder)
        row = dict(name=name, source=str(candidate.relative_to(ROOT)), source_sha256=sha(candidate.read_bytes()),
                   commands=trial["_command_trace"], error=error, status="FAIL" if error else "PASS")
        if not error:
            inv = object_inventory(output)
            lines = fndiff.parse(output)[FUNCTION]
            diff = [s for s in difflib.unified_diff(target_lines, lines, lineterm="", n=0)
                    if s[:1] in "+-" and not s.startswith(("+++", "---"))]
            row.update(object=str(output.relative_to(ROOT)), object_sha256=sha(output.read_bytes()),
                       entire_raw_equal=output.read_bytes() == protected[raw_path],
                       instruction_count=inv["functions"][FUNCTION]["size"] // 4,
                       real=fndiff.count_real(diff),
                       target_words=words(target["functions"][FUNCTION]["body"], inv["functions"][FUNCTION]["body"]),
                       function_changes=differences(baseline, inv), inventory=inv, disassembly=lines,
                       nontext_equal={k:v for k,v in baseline["sections"].items() if k != ".text"}
                           == {k:v for k,v in inv["sections"].items() if k != ".text"},
                       eh_equal=baseline["exception_records"] == inv["exception_records"],
                       frame=fndiff.frame_size(lines), slots=slotdiff.slot_map(lines))
            row["target_geometry"] = {key: [n for n in TARGET_ORDER
                if target["functions"][n][key] != inv["functions"][n][key]]
                for key in ("offset", "size")}
            named, sections = fndiff.object_sections(output)
            row["candidate_sdata2_exact"] = sections.get('.sdata2') == fndiff.dol_read(0x80347448, 0xc8)
            normalized = normalized_relocations(inv)
            row["normalized_relocations"] = normalized
            row["normalized_relocations_equal"] = normalized == baseline_relocs
            row["relocation_rename_witnesses"] = [dict(function=fn, before=a, after=b, identity=c)
                for fn in baseline["functions"]
                for a, b, c in zip(baseline["functions"][fn]["relocations"],
                                   inv["functions"][fn]["relocations"], normalized[fn]) if a != b]
            cmd = [str(ROOT / "build/tools/objdiff-cli.exe"), "diff", "-1", str(target_path),
                   "-2", str(output), "-o", str(trial_dir / "objdiff.json"), FUNCTION]
            done = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
            row["objdiff_command"] = cmd
            row["objdiff_exit"] = done.returncode
            if done.returncode:
                raise ValueError("objdiff failed: " + done.stderr)
        report["variants"].append(row)
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if name == "scratch_control" and (error or not row["entire_raw_equal"]):
            raise ValueError("scratch source control does not reproduce whole raw object")
        print(name, row["status"], "insns", row.get("instruction_count"), "real", row.get("real"),
              "words", len(row.get("target_words", [])), "frame", row.get("frame"), flush=True)
    for path, data in protected.items():
        if path.read_bytes() != data:
            raise ValueError("protected production file changed: " + str(path))
    report["protected_unchanged"] = True
    report["status"] = "FAIL" if any(v["status"] != "PASS" for v in report["variants"]) else "PASS"
    (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Report:", folder / "report.json")
    return report


def verify_retained(archive, output):
    """Verify the installed order repair against its archived old compiler output."""
    edge = cv.read_edges()[UNIT]
    archived = json.loads((archive / 'report.json').read_text(encoding='utf-8'))
    if any(edge[k] != archived['edge'][k] for k in ('mw', 'cflags', 'rule', 'command_template')):
        raise ValueError('retained compiler command differs from baseline')
    compiler = ROOT / 'build/compilers' / edge['mw'] / 'mwcceppc.exe'
    if sha(compiler.read_bytes()) != archived['protected'][str(compiler.relative_to(ROOT))]:
        raise ValueError('retained compiler binary differs from baseline')
    target_path = ROOT / f'build/GUNE5D/obj/{UNIT}.o'
    raw_path = ROOT / edge['body_o']
    processed_path = ROOT / f'build/GUNE5D/src/{UNIT}.o'
    previous = object_inventory(archive / 'baseline_raw.o')
    previous_processed = object_inventory(archive / 'baseline_processed.o')
    current = object_inventory(raw_path)
    processed = object_inventory(processed_path)
    target = object_inventory(target_path)
    folder = Path(tempfile.mkdtemp(prefix='r72_newcam_verify_', dir=ROOT / 'build'))
    traced = dict(edge, _command_trace=[])
    control, error = cv.compile_with(traced, edge['mw'], edge['cflags'], folder / 'control.o', folder)
    if error or control.read_bytes() != raw_path.read_bytes():
        raise ValueError('retained compiler fidelity failed: ' + str(error))
    changes = differences(previous, current)
    processed_changes = differences(previous_processed, processed)
    order = validate_target_order(target)
    checks = dict(exact_function_rosters=set(previous['functions']) == set(current['functions'])
                  == set(processed['functions']) == set(target['functions']),
                  all_raw_bodies_preserved=not changes['body'],
                  all_processed_bodies_preserved=not processed_changes['body'],
                  raw_relocation_bindings_preserved=normalized_relocations(previous) == normalized_relocations(current),
                  processed_relocation_bindings_preserved=normalized_relocations(previous_processed) == normalized_relocations(processed),
                  exact_geometry=all(current['functions'][n][k] == target['functions'][n][k]
                                     for n in order for k in ('offset', 'size')),
                  exact_sdata2=current['sections']['.sdata2']['bytes'] == fndiff.dol_read(0x80347448, 200).hex(),
                  exact_rodata=current['sections']['.rodata']['bytes'] == fndiff.dol_read(0x80113808, 38).hex(),
                  no_bss=not any(n in current['sections'] for n in ('.bss', '.sbss')),
                  exact_eh_payloads=all(current['sections'][n]['bytes'] == target['sections'][n]['bytes']
                                        for n in ('extab', 'extabindex')),
                  exact_eh_records=current['exception_records'] == target['exception_records'])
    def index_relocations(inv):
        rows = []
        for off, kind, name, addend in inv['sections']['extabindex']['relocations']:
            if name in inv['functions']:
                key = ['function', name, inv['functions'][name]['offset'] + addend]
            else:
                symbol = inv['symbols'][name]
                key = ['section', symbol['section'], symbol['offset'] + addend]
            rows.append([off, kind, key])
        return rows
    ours_index, target_index = index_relocations(current), index_relocations(target)
    checks['exact_eh_index_bindings'] = ours_index == target_index
    result = dict(schema_version=1, status='PASS' if all(checks.values()) else 'FAIL',
                  scope='Source-order and full pool/EH witness, not full TU matching or a source-linked DOL proof',
                  checks=checks, command=traced['_command_trace'], archive=str(archive),
                  source_sha256=sha((ROOT / edge['src']).read_bytes()), raw_sha256=sha(raw_path.read_bytes()),
                  raw_changes=changes, processed_changes=processed_changes,
                  eh_index_bindings=ours_index, target_eh_index_bindings=target_index)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(status=result['status'], checks=checks, report=str(output))))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--forms", help="comma-separated finite form names")
    parser.add_argument('--verify-retained', type=Path, help='archived pre-repair experiment directory')
    parser.add_argument('--out', type=Path, default=Path('build/r72_newcam_retained.json'))
    args = parser.parse_args()
    if args.forms and args.verify_retained:
        parser.error('--forms and --verify-retained are mutually exclusive')
    result = (verify_retained(args.verify_retained, args.out) if args.verify_retained else
              run(set(args.forms.split(",")) if args.forms else None))
    raise SystemExit(0 if result['status'] == 'PASS' else 1)
