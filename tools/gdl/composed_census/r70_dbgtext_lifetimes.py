"""Controlled, full-TU dbgtext variable-identity experiments.

No production file or configured option is changed. PASS establishes that the
experiment completed, not that source matches. Every candidate is compared to
a fresh byte-identical Ninja raw baseline, including siblings, sections and EH.
Lexical scopes by themselves are not claims of changed dataflow liveness.
"""
import argparse
from collections import Counter
import difflib
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import tempfile
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools/gdl"))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, changed_functions
from tools.gdl.composed_census import wf_word_diff as words
from tools.gdl import fndiff, fnasm, savedregs
from tools.fix_exception_objects import Elf

UNIT = "game/pb/dbgtext"
FUNCTION = "fn_800C03E0"
REVIEWED_LOOP = (0x7C, 0x1C4)
LOOP_SHA256 = {
    "target": "97087ce6a41ac18a47fca948e4e6935b1650f2d3691a7680c3587a40f263d2e1",
    "raw": "d90f5e21e4ca7ed1db10d74c3bf0d774fe26f22e2a4c1616ae81dcef61d94e4c",
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalize(data):
    return json.loads(json.dumps(data))


def reviewed_loop_witness(target, raw):
    """Fixed reviewed interval, NOT a general CFG or liveness verifier.

    All instructions from the zero definition through the back edge are
    bound, including calls, branch arms and intervening writes. The target
    counter is r16 and the raw counter is r22; BOTH are callee-saved. A
    different source/target interval requires a new review, not this witness.
    """
    start, end = REVIEWED_LOOP
    for label, body in (("target", target), ("raw", raw)):
        if digest(body[start:end]) != LOOP_SHA256[label]:
            raise ValueError(label + " differs from fixed reviewed loop witness")
    return {
        "kind": "fixed-manually-reviewed-target-and-raw-witness",
        "function_offset_range": [hex(start), hex(end)],
        "interval_sha256": LOOP_SHA256,
        "counter_sites": [
            {"offset": "0x7c", "target": "li r16,0", "raw": "li r22,0"},
            {"offset": "0x1b0", "target": "addi r16,r16,1", "raw": "addi r22,r22,1"},
            {"offset": "0x1b4", "target": "cmpwi r16,74", "raw": "cmpwi r22,74"},
            {"offset": "0x1c0", "both": "blt +0x8c"},
        ],
        "different_role": "T+0xd8 li r0,0 feeds T+0xe4 slwi r4,r0,3: x=0 call argument, not row counter",
        "conclusion": "This raw row counter is not a promotion of the target's volatile zero argument.",
        "limit": "No whole-function register pairing, semantic equivalence or source-unreachability proof.",
    }


def selected_savedregs_pairs(target_rows, raw_rows):
    """Preserve current tool output at the inspected sites, without endorsing it."""
    return [dict(label=label, target=t, raw=o, verdict=verdict, note=note)
            for label, t, o, verdict, note in savedregs.lifetime_pairs(target_rows, raw_rows)
            if (t and t[0] in (0x7C, 0x1B0)) or (o and o[0] in (0x7C, 0x1B0))]


def canonical_relocations(rows):
    # EMB_SDA21's relocation names the whole instruction; MWCC puts its
    # record at +2 while the extractor places it at +0. Other offsets matter.
    return [(off & ~3 if kind == 109 else off, kind, name, add)
            for off, kind, name, add in rows]


def canonical_sections(path, current):
    elf = Elf(path)
    local = {}
    for index in range(elf.symcount):
        symbol = elf.sym(index)
        name = elf.symname(index).decode()
        if name.startswith("@") and 0 < symbol[5] < len(elf.sh):
            local[name] = (elf.names[symbol[5]], symbol[1])
    result = {}
    for name, section in current["sections"].items():
        if name == ".text":
            continue
        value = dict(section)
        value["relocations"] = [(off, kind, local.get(symbol, symbol), add)
                                for off, kind, symbol, add in section["relocations"]]
        result[name] = value
    return result


def split_body(source):
    marker = "s32 fn_800C03E0(s32 mode)\n{"
    start = source.index(marker)
    end = source.index("\n/* Latch a graph slot:", start)
    return source[:start], source[start:end], source[end:]


def mode_region(body, mode):
    start = body.index(f"    if (lbl_8034475C == {mode}) {{")
    opening = body.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (body[end] == "{") - (body[end] == "}")
        end += 1
    return start, opening, end


def per_mode(body, name, typename, modes):
    for mode in modes:
        start, opening, end = mode_region(body, mode)
        region = re.sub(r"\b" + name + r"\b", name + str(mode), body[opening+1:end])
        body = body[:opening+1] + f"\n        {typename} {name}{mode};" + region + body[end:]
    return body


def source_forms(source):
    # These are fixed source experiments, not generic rewriting rules. Refuse
    # future source drift rather than silently measuring a replacement no-op.
    expected = {
        "    u32* tblA = lbl_802C45CC;": 1,
        "    char* fmts = lbl_80116450;": 1,
        "    DbgRow* tblB = lbl_80127DE8;": 1,
        "    u32 div;": 1,
        "    u32 scale;": 1,
        "    u32 shift = 10;": 1,
        "extern s32 dbgTextActive;": 1,
        "extern s32 dbgTextColor;": 1,
        "extern s32 dbgTextLine;": 1,
        # TIMING recovery typed the registered samples; only the separate
        # fixed debug-cell loop still has the old word-array representation.
        "cell[3] = cell[2] = cell[1] = cell[0] = 0;": 1,
        "base[i].last_frame = base[i].current = base[i].count = base[i].frame = 0;": 1,
        "void fn_800C031C(TimerSample* base, TimerDesc* arg1, struct MBBlit** arg2, s32 count)": 1,
    }
    for anchor, count in expected.items():
        if source.count(anchor) != count:
            raise ValueError("fixed source-form anchor changed: " + anchor)
    prefix, body, suffix = split_body(source)
    forms = {"baseline": body}
    for name, typename, modes in (
            ("i", "s32", (3, 2, 5, 4)),
            ("quad", "void*", (3, 2, 4)),
            ("scale", "u32", (3, 2, 4)),
            ("j", "s32", (3, 2, 1, 4))):
        forms[name + "_per_mode"] = per_mode(body, name, typename, modes)
    combined = per_mode(body, "i", "s32", (3, 2, 5, 4))
    combined = per_mode(combined, "quad", "void*", (3, 2, 4))
    forms["i_quad_joint"] = combined
    for mode in (3, 2, 5, 4):
        forms[f"i_mode{mode}"] = per_mode(body, "i", "s32", (mode,))
    # Separate row iteration from the mode-3 graph-grid position loop.
    start = body.index("#define DBGROW3")
    end = body.index("#undef DBGROW3") + len("#undef DBGROW3")
    row_index = re.sub(r"\bi\b", "rowIndex", body[start:end])
    forms["mode3_row_counter"] = (body[:start] + row_index + body[end:]).replace(
        "    if (lbl_8034475C == 3) {", "    if (lbl_8034475C == 3) {\n        s32 rowIndex;")
    forms["quad_per_allocation"] = body.replace("quad = MBNewTempQuad();", "void* quad = MBNewTempQuad();")
    # Existing pointer identities and expression order are unchanged; unlink
    # declaration order from initializer evaluation order as ordinary C allows.
    declarations = "    u32* tblA = lbl_802C45CC;\n    char* fmts = lbl_80116450;\n    DbgRow* tblB = lbl_80127DE8;"
    uninitialized = "    u32* tblA;\n    char* fmts;\n    DbgRow* tblB;"
    split = body.replace(declarations, uninitialized).replace(
        "    (void)mode;", "    (void)mode;\n    tblA = lbl_802C45CC;\n    fmts = lbl_80116450;\n    tblB = lbl_80127DE8;")
    forms["pointers_split_assignment"] = split
    forms["pointers_fmts_first"] = split.replace(uninitialized, "    char* fmts;\n    u32* tblA;\n    DbgRow* tblB;")
    forms["pointers_fmts_last"] = split.replace(uninitialized, "    u32* tblA;\n    DbgRow* tblB;\n    char* fmts;")
    # Each source value stays within signed range: div is an unsigned >>10
    # result or 1000000, scale is 4882, shift is 10. Numerators are unsigned,
    # preserving unsigned division and shift semantics under these declarations.
    forms["signed_divisor"] = body.replace("    u32 div;", "    s32 div;")
    forms["signed_scale"] = body.replace("    u32 scale;", "    s32 scale;")
    forms["signed_shift"] = body.replace("    u32 shift = 10;", "    s32 shift = 10;")
    result = {name: prefix + value + suffix for name, value in forms.items()}
    # A source-ownership control, not authorization to claim a data range.
    # These three scalar values have ordinary definitions and real consumers;
    # no synthetic owner struct, padding or address alias is introduced.
    defined = source
    for name in ("dbgTextActive", "dbgTextColor", "dbgTextLine"):
        defined = defined.replace("extern s32 " + name + ";", "s32 " + name + ";")
    result["init_defined_state"] = defined
    bind_start = source.index("void fn_800C031C(TimerSample* base")
    bind_end = source.index("\nvoid fn_800C0394", bind_start)
    bind = source[bind_start:bind_end]
    typed_bind = bind.replace("u32* cell;", "DbgGraphCell* cell;").replace(
        "cell = (u32*)((char*)", "cell = (DbgGraphCell*)((char*)").replace(
        "cell[3] = cell[2] = cell[1] = cell[0] = 0;", "cell->acc = cell->unk8 = cell->unk4 = cell->unk0 = 0;")
    result["bind_typed_cell"] = source[:bind_start] + typed_bind + source[bind_end:]
    split_loop = bind.index("    for (i = 0; i < 24;")
    split_bind = bind[:split_loop] + re.sub(r"\bi\b", "fixedIndex", bind[split_loop:])
    split_bind = split_bind.replace("    s32 i;", "    s32 i;\n    s32 fixedIndex;")
    result["bind_distinct_counter"] = source[:bind_start] + split_bind + source[bind_end:]
    # arg1 is a descriptor pointer, not an integer value. The old signedness
    # control is no longer admissible after recovering the registration API.
    for name in ("dbgTextActive", "dbgTextColor", "dbgTextLine"):
        result["init_int_" + name] = source.replace("extern s32 " + name + ";", "extern int " + name + ";")
    int_state = source
    for name in ("dbgTextActive", "dbgTextColor", "dbgTextLine"):
        int_state = int_state.replace("extern s32 " + name + ";", "extern int " + name + ";")
    result["init_int_state"] = int_state
    result["divisor_unsigned_int"] = prefix + body.replace("    u32 div;", "    unsigned int div;") + suffix
    result["cursors_int"] = prefix + body.replace("    s32 qline;", "    int qline;").replace("    s32 line;", "    int line;") + suffix
    opening = body.index("{")
    result["locals_int"] = prefix + body[:opening] + re.sub(r"\bs32\b", "int", body[opening:]) + suffix
    result["bind_int_counter"] = source[:bind_start] + bind.replace("    s32 i;", "    int i;") + source[bind_end:]
    result["color_unsigned_int"] = source.replace("extern s32 dbgTextColor;", "extern unsigned int dbgTextColor;")
    result["defined_int_state"] = int_state
    for name in ("dbgTextActive", "dbgTextColor", "dbgTextLine"):
        result["defined_int_state"] = result["defined_int_state"].replace("extern int " + name + ";", "int " + name + ";")
    if any(value == source for name, value in result.items() if name != "baseline"):
        raise ValueError("a source-form candidate became a textual no-op")
    return result


def completed_status(candidates):
    """PASS means requested compiles completed, never means raw exact."""
    return "PASS" if candidates and all(not row["error"] and "inventory" in row
                                        for row in candidates.values()) else "FAIL"


def measure(path, baseline, target, target_lines, baseline_sections):
    current = normalize(object_inventory(path))
    cf = current["functions"][FUNCTION]
    tf = target["functions"][FUNCTION]
    ours = bytes.fromhex(cf["body"])
    retail = bytes.fromhex(tf["body"])
    lines = fndiff.parse(path)[FUNCTION]
    details = io.StringIO()
    with redirect_stdout(details):
        fndiff.ops_diff(FUNCTION, target_lines, lines)
        fndiff.clean_diff(FUNCTION, target_lines, lines, path)
    tc, oc = Counter(fndiff.opcodes(target_lines)), Counter(fndiff.opcodes(lines))
    rows = [(i//4, int.from_bytes(ours[i:i+4], "big"), int.from_bytes(retail[i:i+4], "big"))
            for i in range(0, len(ours), 4) if ours[i:i+4] != retail[i:i+4]] if len(ours) == len(retail) else []
    nontext = canonical_sections(path, current)
    return dict(inventory=current, target_instructions=len(retail)//4,
                current_instructions=len(ours)//4, raw_word_differences=len(rows) if len(ours)==len(retail) else None,
                decode_counts=dict(words.decode_counts(rows)),
                mnemonic_divergence=words.mnemonic_divergence(ours, retail) if len(ours)==len(retail) else None,
                target_only_opcodes=dict(tc-oc), ours_only_opcodes=dict(oc-tc),
                frame=fndiff.frame_size(lines), raw_body_sha256=digest(ours),
                target_relocations_equal=canonical_relocations(cf["relocations"]) == canonical_relocations(tf["relocations"]),
                raw_target_words={name: sum(a[i:i+4] != b[i:i+4] for i in range(0, len(a), 4)) if len(a)==len(b) else None
                    for name in current["functions"] if name in target["functions"]
                    for a, b in [(bytes.fromhex(current["functions"][name]["body"]), bytes.fromhex(target["functions"][name]["body"]))]},
                changes_vs_baseline=changed_functions(baseline["functions"], current["functions"]),
                nontext_equal=nontext == baseline_sections,
                exception_records_equal=current["exception_records"] == baseline["exception_records"],
                details=details.getvalue(), assembly=lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--forms", default="baseline,i_per_mode,quad_per_mode,scale_per_mode,i_quad_joint",
                        help="comma-separated named controls, or all (finite registered set)")
    parser.add_argument("--out", type=Path, default=ROOT / "build/r70_dbgtext_lifetimes.json")
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / "build") or not output.name.startswith("r70_dbgtext_"):
        parser.error("--out must be build/r70_dbgtext_*.json")
    result = dict(schema_version=1, status="UNRESOLVED", scope=__doc__, candidates={})
    try:
        edge = cv.read_edges()[UNIT]
        source_path = ROOT / edge["src"]
        source_bytes = source_path.read_bytes()
        raw_path = ROOT / edge["body_o"]
        target_path = ROOT / f"build/GUNE5D/obj/{UNIT}.o"
        folder = Path(tempfile.mkdtemp(prefix="r70_dbgtext_lifetimes_", dir=ROOT / "build"))
        result.update(artifacts=folder.relative_to(ROOT).as_posix(), compiler_edge=edge,
                      source_sha256=digest(source_bytes), raw_sha256=digest(raw_path.read_bytes()),
                      compiler_sha256=digest((ROOT / "build/compilers" / edge["mw"] / "mwcceppc.exe").read_bytes()),
                      pragma_inventory=cv.pragma_inventory(edge["src"]))
        trace = []
        control = dict(edge, _command_trace=trace)
        obj, error = cv.compile_with(control, edge["mw"], edge["cflags"], folder / "r70_dbgtext_control.o", folder)
        result["control_trace"] = trace
        if error or not obj or obj.read_bytes() != raw_path.read_bytes():
            raise ValueError(error or "fresh Ninja baseline fidelity failed")
        result["baseline_fidelity"] = True
        baseline = normalize(object_inventory(obj))
        baseline_sections = canonical_sections(obj, baseline)
        target = normalize(object_inventory(target_path))
        result.update(baseline=baseline, target=target)
        result["reviewed_loop_witness"] = reviewed_loop_witness(
            bytes.fromhex(target["functions"][FUNCTION]["body"]),
            bytes.fromhex(baseline["functions"][FUNCTION]["body"]))
        t_rows, _, t_error = fnasm.parse_fn(UNIT, FUNCTION, ours=False)
        o_rows, _, o_error = fnasm.parse_fn(UNIT, FUNCTION, ours=True, raw=True)
        if t_error or o_error:
            raise ValueError(t_error or o_error)
        result["savedregs_inspected_pairs"] = selected_savedregs_pairs(t_rows, o_rows)
        target_lines = fndiff.parse(target_path)[FUNCTION]
        forms = source_forms(source_bytes.decode("utf-8").replace("\r\n", "\n"))
        requested = list(forms) if args.forms == "all" else args.forms.split(",")
        for label in requested:
            if label not in forms:
                raise ValueError("unknown form " + label)
            scratch = folder / ("r70_dbgtext_" + label + ".c")
            scratch.write_text(forms[label], encoding="utf-8")
            (folder / ("r70_dbgtext_" + label + ".diff")).write_text("".join(difflib.unified_diff(
                forms["baseline"].splitlines(True), forms[label].splitlines(True), fromfile="baseline", tofile=label)), encoding="utf-8")
            trial = dict(edge, src=scratch.relative_to(ROOT).as_posix(), _command_trace=[])
            obj, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / ("r70_dbgtext_"+label+".o"), folder)
            row = dict(error=error, trace=trial["_command_trace"], source=scratch.relative_to(ROOT).as_posix(),
                       source_sha256=digest(scratch.read_bytes()))
            result["candidates"][label] = row
            if error or not obj:
                print(label + ": COMPILE FAILED " + str(error))
                continue
            row.update(measure(obj, baseline, target, target_lines, baseline_sections))
            print(f"{label}: T{row['target_instructions']}/O{row['current_instructions']} words={row['raw_word_differences']} frame={row['frame']} reloc={row['target_relocations_equal']} nontext={row['nontext_equal']} EH={row['exception_records_equal']} changes={row['changes_vs_baseline']['body']}")
        if source_path.read_bytes() != source_bytes:
            raise ValueError("production source changed during scratch experiments")
        result["status"] = completed_status(result["candidates"])
    except (OSError, ValueError, KeyError) as exc:
        result["reason"] = str(exc)
        print("UNRESOLVED: " + str(exc))
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("wrote " + output.relative_to(ROOT).as_posix())
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
