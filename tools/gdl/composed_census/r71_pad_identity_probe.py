"""Bounded full-TU source-identity experiments for control-pad rule retirement.

Uses the current raw Ninja edge and refuses variants until both the original
source command and relocated scratch-source control reproduce every ELF byte.
Outputs are diagnostic source candidates, never production patches or a proof
of source exhaustion. The archived processed object is a shape reference; the
target's resolved gPadManager instruction payloads remain separately visible.
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
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory
from tools.gdl import fndiff, slotdiff

UNIT = "game/g3d/gcontrolpads"
FUNCTION = "G3DReadControlPadStates"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("source precondition differs: " + repr(old))
    return text.replace(old, new, 1)


def variants(source):
    start = source.index("void " + FUNCTION + "(void)")
    end = source.index("#pragma opt_propagation reset", start)
    body = source[start:end]
    results = [("scratch_path_control", "identical complete source; path sensitivity control", source)]

    def add(name, rationale, changed):
        results.append((name, rationale, source[:start] + changed + source[end:]))

    manager = replace_once(body, "    u8 unused[8];", "    u8 unused[8];\n    GPADMANAGER* manager;")
    manager = replace_once(manager, "    maskA = 0;", "    manager = &gPadManager;\n    maskA = 0;")
    manager = manager.replace("gPadManager.", "manager->")
    add("named_manager", "symbolic manager object identity, not literal target address", manager)
    manager_late = replace_once(manager, "    manager = &gPadManager;\n", "")
    manager_late = replace_once(manager_late, "    manager->count = maskA;", "    manager = &gPadManager;\n    manager->count = maskA;")
    add("named_manager_at_use", "same symbolic identity introduced at first manager use", manager_late)
    result = replace_once(body, "    u8 unused[8];", "    u8 unused[8];\n    PADStatus* status;")
    result = replace_once(result, "    gPadManager.status = G3DGetPadStatusBuffer();",
                          "    status = G3DGetPadStatusBuffer();\n    gPadManager.status = status;")
    add("named_call_result", "name actual status-buffer call result; retain loop cell reloads", result)
    scoped = replace_once(body, "    s8 err;\n    u32 bit;\n", "")
    scoped = replace_once(scoped, "        bit = PAD_CHAN0_BIT >> i;\n        err = gPadManager.status[i].err;",
                          "        u32 bit = PAD_CHAN0_BIT >> i;\n        s8 err = gPadManager.status[i].err;")
    add("iteration_local_values", "mbBlitPadTest same-loop source uses iteration-local bit/error identities", scoped)
    mb_source = (ROOT / "src/game/mb/mb_blit.c").read_text(encoding="utf-8")
    mb_start = mb_source.index("void mbBlitPadTest(s32* manager) {")
    mb_end = mb_source.index("void mbBlitStaticInit", mb_start)
    existing = "static inline " + mb_source[mb_start:mb_end]
    wrapper = "void " + FUNCTION + "(void)\n{\n    mbBlitPadTest((s32*)&gPadManager);\n}\n"
    add("existing_mb_helper_inline", "DIAGNOSTIC ONLY: copied existing mbBlitPadTest definition, provisional shared-header lineage", existing + wrapper)
    typed = replace_once(body, "void " + FUNCTION + "(void)", "static inline void mbBlitPadTest(GPADMANAGER* manager)")
    typed = typed.replace("gPadManager.", "manager->")
    typed_wrapper = wrapper.replace("(s32*)&gPadManager", "&gPadManager")
    add("typed_mb_helper_inline", "DIAGNOSTIC ONLY: existing manager-loop identity adapted to target-proven typed fields", typed + typed_wrapper)
    typed_order = replace_once(typed, "    u32 maskB;\n    u32 maskA;", "    u32 maskA;\n    u32 maskB;")
    add("typed_mb_declaration_order", "DIAGNOSTIC ONLY: helper masks declared disconnected/present as existing mbBlitPadTest", typed_order + typed_wrapper)
    mb_typed = replace_once(existing, "s32* manager", "GPADMANAGER* manager")
    mb_typed = replace_once(mb_typed, "manager[0] = 0;", "manager->count = 0;")
    mb_typed = replace_once(mb_typed, "manager[2] = (s32)G3DGetPadStatusBuffer();", "manager->status = G3DGetPadStatusBuffer();")
    mb_typed = replace_once(mb_typed, "((s8*)manager[2])[i * 12 + 10]", "manager->status[i].err")
    mb_typed = replace_once(mb_typed, "manager[manager[0] + 3] = i;", "manager->map[manager->count] = i;")
    mb_typed = replace_once(mb_typed, "manager[0]++;", "manager->count++;")
    add("existing_mb_typed_fields", "DIAGNOSTIC ONLY: existing helper initializer/scope chronology, target-proven typed fields", mb_typed + typed_wrapper)
    flattened = replace_once(mb_typed, "static inline void mbBlitPadTest(GPADMANAGER* manager)", "void " + FUNCTION + "(void)")
    flattened = flattened.replace("manager->", "gPadManager.")
    add("existing_mb_flattened_control", "OPPOSITE CONTROL: identical typed helper chronology expanded as the caller body; separates inline identity from source chronology", flattened)
    add("typed_mb_auto_inline", "DIAGNOSTIC ONLY: same typed helper under configured auto-inlining without explicit inline keyword", typed_order.replace("static inline void", "static void") + typed_wrapper)
    member = replace_once(mb_typed, "static inline void mbBlitPadTest(GPADMANAGER* manager)",
                          "inline void GPADMANAGER::ReadControlPadStates(void)")
    member = member.replace("manager->", "")
    member_wrapper = ('#pragma cplusplus on\n' + member + 'extern "C" void ' + FUNCTION
                      + '(void)\n{\n    gPadManager.ReadControlPadStates();\n}\n#pragma cplusplus off\n')
    member_source = source[:start] + member_wrapper + source[end:]
    member_source = replace_once(member_source, "typedef struct GPADMANAGER {", "#pragma cplusplus on\ntypedef struct GPADMANAGER {")
    member_source = replace_once(member_source, "} GPADMANAGER;", "    void ReadControlPadStates(void);\n} GPADMANAGER;\n#pragma cplusplus off")
    results.append(("member_this_context", "DIAGNOSTIC ONLY: Xbox-proven ReadControlPadStates member identity with GC-proven fields; scoped C++ parsing", member_source))
    inherited = replace_once(member_source, "typedef struct GPADMANAGER {\n    /* 0x00 */ int count;\n    /* 0x04 */ int unk04;",
                             "struct gcontrolpad {};\nstruct gcontrolpadmanager_c {\n    int count;\n    gcontrolpad pControllers[4];\n};\ntypedef struct GPADMANAGER : public gcontrolpadmanager_c {")
    results.append(("member_inherited_prefix", "DIAGNOSTIC ONLY: PDB base-class count + four 1-byte controller objects, GC constructor verifies prefix", inherited))
    member_pointer = replace_once(inherited, "    gPadManager.ReadControlPadStates();",
                                  "    GPADMANAGER* manager = &gPadManager;\n    manager->ReadControlPadStates();")
    results.append(("member_caller_pointer", "DIAGNOSTIC ONLY: named symbolic manager belongs to original C wrapper, not inline body", member_pointer))
    member_reference = replace_once(inherited, "    gPadManager.ReadControlPadStates();",
                                    "    GPADMANAGER& manager = gPadManager;\n    manager.ReadControlPadStates();")
    results.append(("member_caller_reference", "DIAGNOSTIC ONLY: actual C++ reference identity for the known singleton manager", member_reference))
    base_methods = replace_once(inherited, "    gcontrolpad pControllers[4];",
                                 "    gcontrolpad pControllers[4];\n"
                                 "    void RemoveAllActiveControllers() { count = 0; }\n"
                                 "    int GetActivePadCount() { return count; }\n"
                                 "    int AddController() { return count++; }")
    reset_method = replace_once(base_methods, "    count = 0;", "    RemoveAllActiveControllers();")
    results.append(("member_reset_method", "DIAGNOSTIC ONLY: PDB-proven base-class reset method; simple body inferred from target reset", reset_method))
    count_methods = replace_once(base_methods, "            map[count] = i;\n            count++;",
                                 "            map[GetActivePadCount()] = i;\n            AddController();")
    results.append(("member_count_methods", "DIAGNOSTIC ONLY: PDB-proven base-class read/add methods; unused AddController return not claimed recovered", count_methods))
    joint = replace_once(count_methods, "    count = 0;", "    RemoveAllActiveControllers();")
    results.append(("member_all_base_methods", "DIAGNOSTIC ONLY: joint reset/count/add method contexts; controls isolate each branch", joint))
    return results


def differences(before, after):
    common = before["functions"].keys() & after["functions"].keys()
    return dict(missing=sorted(before["functions"].keys() - common),
                extra=sorted(after["functions"].keys() - common),
                **{key: [n for n in sorted(common)
                  if before["functions"][n][key] != after["functions"][n][key]]
            for key in ("offset", "size", "body", "relocations", "binding")})


def words(reference, actual):
    a, b = bytes.fromhex(reference), bytes.fromhex(actual)
    return [{"offset": hex(i), "reference": a[i:i+4].hex(), "actual": b[i:i+4].hex()}
            for i in range(0, max(len(a), len(b)), 4) if a[i:i+4] != b[i:i+4]]


FRAME_PAIRS = {0x0c: ("9421ffe0", "9421ffe8"), 0x10: ("bfa10014", "bfa1000c"),
               0xac: ("bba10014", "bba1000c"), 0xb0: ("80010024", "8001001c"),
               0xb4: ("38210020", "38210018")}


def partial_shape_witness(processed, candidate):
    """Prove only this finite counterexample, explicitly not complete matching."""
    changes = differences(processed, candidate)
    expected_changes = dict(missing=[], extra=[], offset=[], size=[], body=[FUNCTION],
                            relocations=[], binding=[])
    if changes != expected_changes:
        raise ValueError("candidate differs outside the one function body/expected roster")
    p, c = processed["functions"][FUNCTION], candidate["functions"][FUNCTION]
    if p["size"] != 192 or len(bytes.fromhex(p["body"])) != 192 or len(bytes.fromhex(c["body"])) != 192:
        raise ValueError("unexpected 48-instruction extent")
    actual = {int(row["offset"], 16): (row["reference"], row["actual"])
              for row in words(p["body"], c["body"])}
    if actual != FRAME_PAIRS:
        raise ValueError("not the measured five-frame-word-only counterexample")
    if (not processed["exception_records"]
            or processed["exception_records"] != candidate["exception_records"]):
        raise ValueError("decoded EH metadata is empty or differs")
    a = {n: s for n, s in processed["sections"].items() if n != ".text"}
    b = {n: s for n, s in candidate["sections"].items() if n != ".text"}
    if not a or a.keys() != b.keys():
        raise ValueError("allocated nontext section roster differs")
    aliases = {}
    for name in a:
        for key in a[name]:
            if key == "relocations" and name == "extabindex":
                # These exact differing names are decoded and resolved by the
                # existing exception_metadata core above; do not wildcard them
                # in text/data or silently equate unknown metadata formats.
                aliases[name] = dict(before=a[name][key], after=b[name][key])
            elif a[name][key] != b[name][key]:
                raise ValueError("allocated nontext content/metadata differs: " + name + ":" + key)
    return dict(status="PASS", full_match=False, production_rule_retired=False,
                target_current_instructions="48/48", remaining_frame_words=sorted(FRAME_PAIRS),
                meaning="Every previous rule-controlled code choice is source-reachable here; frame24 still differs from target32.",
                exception_symbol_spelling=aliases)


def bound_target_address(target, candidate, symbol_text):
    matches = re.findall(r"^gPadManager = \.bss:0x([0-9A-Fa-f]+);.*size:0x1C\b", symbol_text, re.M)
    if len(matches) != 1:
        raise ValueError("manager target definition is missing or ambiguous")
    address = int(matches[0], 16)
    function = candidate["functions"][FUNCTION]
    expected = [[6, 6, "gPadManager", 0], [26, 4, "gPadManager", 0],
                [36, 10, "G3DGetPadStatusBuffer", 0]]
    if function["relocations"] != expected:
        raise ValueError("candidate positional manager/call bindings differ")
    target_body = bytes.fromhex(target["functions"][FUNCTION]["body"])
    actual_body = bytes.fromhex(function["body"])
    for offset, immediate in ((4, ((address + 0x8000) >> 16) & 0xffff), (24, address & 0xffff)):
        a, t = int.from_bytes(actual_body[offset:offset+4], "big"), int.from_bytes(target_body[offset:offset+4], "big")
        if a & 0xffff or (a | immediate) != t:
            raise ValueError("resolved target manager instruction payload is not the named binding")
    return dict(symbol="gPadManager", address=hex(address), addend=0,
                candidate_relocations=expected, target_payload_offsets=[4, 24],
                scope="instruction binding only; not a fresh linker/runtime ownership proof")


def run():
    edge = cv.read_edges()[UNIT]
    if edge["mw"] != "GC/1.2.5n" or edge["rule"] != "mwcc_sjis":
        raise ValueError("compiler identity/edge changed; re-adjudicate experiment")
    raw_path = ROOT / edge["body_o"]
    processed_path = ROOT / f"build/GUNE5D/src/{UNIT}.o"
    target_path = ROOT / f"build/GUNE5D/obj/{UNIT}.o"
    source_path = ROOT / edge["src"]
    protected = {p: p.read_bytes() for p in (
        raw_path, processed_path, target_path, source_path, ROOT / "build.ninja",
        ROOT / "config/GUNE5D/webfrank.json", ROOT / "include/types.h",
        ROOT / "include/dolphin/pad.h", ROOT / "include/game/g3dpad.h", ROOT / "src/game/mb/mb_blit.c",
        ROOT / "config/GUNE5D/symbols.txt", ROOT / "research/xbox_symbols/misc.h",
        ROOT / "build/compilers/GC/1.2.5n/mwcceppc.exe")}
    folder = Path(tempfile.mkdtemp(prefix="r71_pad_identity_", dir=ROOT / "build"))
    for name, path in (("baseline.c", source_path), ("baseline_raw.o", raw_path),
                       ("baseline_processed.o", processed_path), ("target.o", target_path)):
        (folder / name).write_bytes(protected[path])
    baseline = object_inventory(raw_path)
    processed = object_inventory(processed_path)
    target = object_inventory(target_path)
    trace = dict(edge, _command_trace=[])
    control, error = cv.compile_with(trace, edge["mw"], edge["cflags"], folder / "actual_control.o", folder)
    if error or control is None or control.read_bytes() != protected[raw_path]:
        raise ValueError("actual-command full-object fidelity failed: " + str(error))
    report = dict(schema_version=1, unit=UNIT, status="PASS", source_exhaustion=False,
                  baseline_command=trace["_command_trace"], edge=edge,
                  archive=str(folder.relative_to(ROOT)),
                  protected={str(p.relative_to(ROOT)): sha(data) for p, data in protected.items()},
                  baseline=baseline, processed=processed, target=target, variants=[])
    target_lines = fndiff.parse(target_path)[FUNCTION]
    for name, rationale, source in variants(protected[source_path].decode("utf-8")):
        candidate_folder = folder / name
        candidate_folder.mkdir()
        candidate_path = candidate_folder / source_path.name
        candidate_path.write_text(source, encoding="utf-8", newline="\n")
        trial = dict(edge, src=str(candidate_path.relative_to(ROOT)), _command_trace=[])
        output, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / (name + ".o"), folder)
        row = dict(name=name, rationale=rationale, source_sha256=sha(candidate_path.read_bytes()),
                   source=str(candidate_path.relative_to(ROOT)), commands=trial["_command_trace"],
                   pragmas=cv.pragma_inventory(trial["src"]),
                   status="FAIL" if error else "PASS", error=error)
        if not error:
            actual = object_inventory(output)
            row.update(object=str(output.relative_to(ROOT)), object_sha256=sha(output.read_bytes()),
                       entire_raw_object_equal=output.read_bytes() == protected[raw_path],
                       entire_processed_object_equal=output.read_bytes() == protected[processed_path],
                       function_changes=differences(baseline, actual),
                       processed_function_changes=differences(processed, actual),
                       instruction_count=actual["functions"][FUNCTION]["size"] // 4,
                       processed_word_differences=words(processed["functions"][FUNCTION]["body"], actual["functions"][FUNCTION]["body"]),
                       target_word_differences=words(target["functions"][FUNCTION]["body"], actual["functions"][FUNCTION]["body"]),
                       nontext_equal={n: s for n, s in baseline["sections"].items() if n != ".text"}
                           == {n: s for n, s in actual["sections"].items() if n != ".text"},
                       eh_equal=baseline["exception_records"] == actual["exception_records"],
                       inventory=actual)
            lines = fndiff.parse(output)[FUNCTION]
            row["disassembly"] = lines
            row["stack_witness"] = dict(target_frame=fndiff.frame_size(target_lines),
                                         actual_frame=fndiff.frame_size(lines),
                                         target_slots=slotdiff.slot_map(target_lines),
                                         actual_slots=slotdiff.slot_map(lines),
                                         target_save_set=slotdiff.save_set(target_lines),
                                         actual_save_set=slotdiff.save_set(lines))
            if name == "existing_mb_typed_fields":
                row["partial_shape_witness"] = partial_shape_witness(processed, actual)
                row["bound_target_address"] = bound_target_address(target, actual, (ROOT / "config/GUNE5D/symbols.txt").read_text())
        report["variants"].append(row)
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if name == "scratch_path_control" and (error or not row["entire_raw_object_equal"]):
            raise ValueError("scratch-source-path full-object fidelity failed; report=" + str(folder / "report.json"))
        print(name, row["status"], row.get("instruction_count"),
              len(row.get("processed_word_differences", [])), "processed word differences")
    for path, data in protected.items():
        if path.read_bytes() != data:
            raise ValueError("protected production input/output changed: " + str(path))
    report["protected_unchanged"] = True
    if any(row["status"] != "PASS" for row in report["variants"]):
        report["status"] = "FAIL"
    report_path = folder / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Report:", report_path)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    raise SystemExit(0 if run()["status"] == "PASS" else 1)
