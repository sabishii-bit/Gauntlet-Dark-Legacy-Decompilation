"""Bounded tower initialized-data certificate and current-input ownership audit.

PASS proves the complete 488-byte data run, not a TU flip or an executed link.
The nine existing typed arrays are checked against retail at their named homes,
including the two alignment bytes and all 37 compiler-generated jump pointers.
No source, compiler, pin or split is changed. Optional Xbox evidence is read-only.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import r68_aux_ownership_audit as aux
from tools.gdl.composed_census import r68_sound_data_recovery as pdb_reader
from tools.gdl.composed_census import r67_sound_boundary_probe as compile_helper

UNIT = "game/world/tower"
BASE, END = 0x80124C70, 0x80124E58
TABLES = (
    ("lbl_80124C70", 0x00, 36), ("lbl_80124C94", 0x24, 72),
    ("lbl_80124CDC", 0x6C, 12), ("lbl_80124CE8", 0x78, 42),
    ("crystal_order", 0xA4, 56), ("lbl_80124D4C", 0xDC, 56),
    ("lbl_80124D84", 0x114, 16), ("lbl_80124D94", 0x124, 12),
    ("lbl_80124DA0", 0x130, 36),
)


def data_obligation(source, retail, addresses):
    """Check every byte and relocation; an exact prefix alone cannot pass."""
    section = source["sections"].get(".data")
    if not section or (section["type"], section["alignment"], section["size"]) != (1, 8, 488):
        raise ValueError("expected complete initialized .data extent 488, alignment 8")
    raw = bytes.fromhex(section["bytes"])
    if len(raw) != 488 or len(retail) != 488:
        raise ValueError("unexpected data byte extent")
    globals_ = {n: s for n, s in source["symbols"].items()
                if s["section"] == ".data" and s["binding"] == 1}
    if set(globals_) != {name for name, _, _ in TABLES}:
        raise ValueError("named global coverage differs")
    for name, offset, size in TABLES:
        s = globals_[name]
        target_size = 44 if name == "lbl_80124CE8" else size
        if ((s["offset"], s["size"], s["bytes"]) != (offset, size, raw[offset:offset+size].hex())
                or addresses[name][:3] != (".data", BASE + offset, target_size)):
            raise ValueError("named array layout differs: " + name)
    if raw[:340] != retail[:340] or raw[0xA2:0xA4] != b"\0\0":
        raise ValueError("array bytes or two-byte inter-array alignment differ")
    local_data = {n: s for n, s in source["symbols"].items()
                  if s["section"] == ".data" and s["binding"] == 0}
    if len(local_data) != 1 or next(iter(local_data.values()))["offset"] != 340:
        raise ValueError("expected one local jump-table suffix")
    jump = next(iter(local_data.values()))
    if jump["size"] != 148 or jump["bytes"] != raw[340:].hex():
        raise ValueError("jump-table extent differs")
    relocs = section["relocations"]
    if len(relocs) != 37 or {r[0] for r in relocs} != set(range(340, 488, 4)):
        raise ValueError("expected every one of 37 jump-pointer relocation sites")
    resolved, bindings = bytearray(raw), []
    fn = source["functions"]["TowerCheckMessages"]
    target_fn = addresses["TowerCheckMessages"]
    if target_fn[0] != ".text" or fn["size"] != target_fn[2]:
        raise ValueError("jump owner size/section differs")
    for at, kind, name, addend in relocs:
        if (kind != 1 or name != "TowerCheckMessages" or addend % 4
                or not 0 <= addend < fn["size"]):
            raise ValueError("unsupported or out-of-bounds jump destination")
        pointer = target_fn[1] + addend
        expected = struct.unpack_from(">I", retail, at)[0]
        if pointer != expected:
            raise ValueError(f"jump pointer differs at +0x{at:X}")
        struct.pack_into(">I", resolved, at, pointer)
        bindings.append(dict(offset=at, function=name, addend=addend, target=hex(expected)))
    if resolved != retail:
        raise ValueError("complete relocated data differs")
    return dict(status="PASS", range=[hex(BASE), hex(END)], alignment=8,
                size=488, named_arrays=9, array_bytes=338, inter_array_zero_alignment=2,
                jump_table_bytes=148, jump_relocations=bindings,
                relocated_sha256=aux.digest(resolved), target_sha256=aux.digest(retail),
                scope="complete data equality at verified named homes, not code equivalence or linked reachability")


def pdb_context(path):
    data = path.read_bytes()
    streams = pdb_reader.pdb_streams(data)
    describe = pdb_reader.describe_types(streams[2])
    dbi, at, modules = streams[3], 64, []
    modsize = struct.unpack_from("<I", dbi, 24)[0]
    while at < 64 + modsize:
        stream = struct.unpack_from("<H", dbi, at + 34)[0]
        size = struct.unpack_from("<I", dbi, at + 36)[0]
        end = dbi.index(b"\0", at + 64)
        name = dbi[at + 64:end].decode("latin1")
        end2 = dbi.index(b"\0", end + 1)
        at = (end2 + 4) & ~3
        if name.upper().endswith(("TOWER.OBJ", "ITEMS.OBJ")):
            rows, procs = pdb_reader.sound_symbols(streams[stream][:size], describe)
            modules.append(dict(module=name, procedure_count=len(procs), data=rows))
    if len(modules) != 2:
        raise ValueError("expected TOWER and ITEMS PDB modules")
    return dict(pdb_sha256=aux.digest(data), modules=modules,
                scope="Xbox names/types/scopes only; crystal_order occurs in ITEMS, not proof of GC defining TU")


def audit(output, before=None, pdb=None):
    output = output.resolve()
    if (not output.is_relative_to((ROOT / "build").resolve())
            or not output.name.startswith("r69_tower_") or output.suffix != ".json"):
        raise ValueError("output must be build/r69_tower_*.json")
    folder = output.parent / (output.stem + "_files")
    folder.mkdir(parents=True, exist_ok=True)
    result = dict(schema_version=1, status="UNRESOLVED", scope=__doc__)
    edge = aux.cv.read_edges()[UNIT]
    raw_path = ROOT / edge["body_o"]
    protected = [raw_path, ROOT / edge["src"], ROOT / "build.ninja",
                 ROOT / "config/GUNE5D/webfrank.json", ROOT / "config/GUNE5D/splits.txt"]
    guards = {p.relative_to(ROOT).as_posix(): aux.digest(p.read_bytes()) for p in protected}
    compiled = compile_helper.compile_source(edge, ROOT / edge["src"], folder / "r69_tower_control.o")
    if compiled["error"] or compiled["sha256"] != aux.digest(raw_path.read_bytes()):
        raise ValueError(compiled["error"] or "raw Ninja baseline fidelity failed")
    raw = json.loads(json.dumps(aux.object_inventory(raw_path)))
    target = aux.object_inventory(ROOT / f"build/GUNE5D/obj/{UNIT}.o")
    result.update(compiler_edge=edge, compiler=compiled, baseline_fidelity=True,
                  source_sha256=guards[edge["src"]], raw=raw, target=target,
                  postprocessed=aux.object_inventory(ROOT / f"build/GUNE5D/src/{UNIT}.o"),
                  source_pragmas=aux.cv.pragma_inventory(ROOT / edge["src"]))
    table = aux.fndiff.symbol_table()
    retail = aux.datadiff.dol_read(BASE, END - BASE)
    if retail is None:
        raise ValueError("retail data range unavailable")
    result["data_obligation"] = data_obligation(raw, retail, table)
    result["exception_comparison"] = aux.compare_exception_records(target["exception_records"], raw["exception_records"])
    result["claimed_sections"] = aux.datadiff.parse_splits()[UNIT + ".c"]
    if ".data" in result["claimed_sections"]:
        if result["claimed_sections"][".data"] != (BASE, END):
            raise ValueError("claimed data range differs")
        target_data = target["sections"].get(".data")
        if not target_data or (target_data["size"], target_data["alignment"]) != (488, 8):
            raise ValueError("target data claim extent/alignment differs")
    snapshot = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
    if snapshot.get("schema_version") != 1 or snapshot.get("ninja_sha256") != guards["build.ninja"]:
        raise ValueError("current build snapshot is stale or unsupported")
    links = [e for e in snapshot["edges"] if e["rule"] == "link"]
    if len(links) != 1:
        raise ValueError("expected one current link edge")
    inputs = links[0]["inputs"]
    target_path, source_path = f"build/GUNE5D/obj/{UNIT}.o", f"build/GUNE5D/src/{UNIT}.o"
    if inputs.count(target_path) != 1:
        raise ValueError("expected one NonMatching tower fallback input")
    trial_inputs = [source_path if p == target_path else p for p in inputs]
    names = [n for n, _, _ in TABLES]
    result["current_link_owners"] = aux.current_owners(inputs, names)
    result["selected_source_owners"] = aux.current_owners(trial_inputs, names)
    result["selected_source_duplicates"] = [n for n, owners in result["selected_source_owners"].items() if len(owners) > 1]
    result["selected_source_missing"] = [n for n, owners in result["selected_source_owners"].items() if not owners]
    if ".data" in result["claimed_sections"] and (result["selected_source_duplicates"] or result["selected_source_missing"]):
        raise ValueError("claimed data still has duplicate or missing selected-source owners")
    if before:
        old = json.loads(before.read_text())
        result["raw_function_changes"] = aux.changed_functions(old["raw"]["functions"], raw["functions"])
        result["raw_sections_unchanged"] = old["raw"]["sections"] == raw["sections"]
        result["raw_eh_unchanged"] = old["raw"]["exception_records"] == raw["exception_records"]
        if (any(result["raw_function_changes"].values()) or not result["raw_sections_unchanged"]
                or not result["raw_eh_unchanged"]):
            raise ValueError("raw code, relocations, sections or EH changed from reference")
    if pdb:
        result["pdb"] = pdb_context(pdb)
    result["production_inputs_unchanged"] = guards == {p.relative_to(ROOT).as_posix(): aux.digest(p.read_bytes()) for p in protected}
    if not result["production_inputs_unchanged"]:
        raise ValueError("production inputs changed during audit")
    result["status"] = "PASS"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("build/r69_tower_audit.json"))
    parser.add_argument("--before", type=Path)
    parser.add_argument("--pdb", type=Path)
    args = parser.parse_args()
    result = audit(args.out, args.before, args.pdb)
    print(f"PASS: all 488 data bytes and 37 jump pointers exact; {len(result['raw']['functions'])} raw functions inventoried")
    print(f"Selected-source duplicate names: {result['selected_source_duplicates']}; missing: {result['selected_source_missing']}")
    print(f"Not a TU flip or executed link: {args.out}")


if __name__ == "__main__":
    main()
