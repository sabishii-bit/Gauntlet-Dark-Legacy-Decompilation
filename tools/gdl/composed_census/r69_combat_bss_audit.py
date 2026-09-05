"""Combat BSS ownership and bounded zero-initialization control.

PASS certifies the eight names/extents and inventories actual linker inputs;
it does NOT certify source/retail allocation-order equality or a TU match.
Scratch zero initializers test a natural storage form, never modify source.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import r68_aux_ownership_audit as aux
from tools.gdl.composed_census import r67_sound_boundary_probe as compiler
from tools.gdl.composed_census import r68_sound_data_recovery as pdb_reader

UNIT = "game/game/combat"
BASE, END = 0x80240560, 0x802407B8
OBJECTS = (
    ("pmissile_sfxidx", 0, 20), ("WeapThrowFx", 20, 80),
    ("WeapHoldFxTree", 100, 80), ("FamiliarSpit", 180, 16),
    ("PhoenixTree", 196, 4), ("FamiliarTree", 200, 32),
    ("EnemyMissileTree", 232, 336), ("PlayerMissileTreeInfo", 568, 32),
)


def allocation_obligation(source, addresses):
    """Prove complete identity/extent coverage, explicitly separating layout."""
    expected = {name for name, _, _ in OBJECTS}
    actual = {n for n, s in source["symbols"].items() if s["section"] in (".bss", ".sbss")}
    if actual != expected:
        raise ValueError("unexpected BSS name coverage")
    section_layout, rows = {}, []
    for name, offset, size in OBJECTS:
        s = source["symbols"][name]
        if (s["size"], s["binding"], s["bytes"]) != (size, 1, None):
            raise ValueError("source extent/linkage/type differs: " + name)
        if addresses[name][:3] != (".bss", BASE + offset, size):
            raise ValueError("target identity/extent differs: " + name)
        section_layout.setdefault(s["section"], []).append((s["offset"], size))
        rows.append(dict(name=name, target_address=hex(BASE+offset), size=size,
                         source_section=s["section"], source_offset=s["offset"],
                         layout_equal=s["section"] == ".bss" and s["offset"] == offset))
    for name, spans in section_layout.items():
        section = source["sections"][name]
        if (section["type"], section["alignment"], section["bytes"], section["relocations"]) != (8, 8, None, []):
            raise ValueError("unexpected BSS section kind/alignment/content")
        position = 0
        for offset, size in sorted(spans):
            if offset != position or offset % 4:
                raise ValueError("overlap or unidentified BSS allocation")
            position += size
        if position != section["size"]:
            raise ValueError("unidentified section tail")
    if sum(s["size"] for s in rows) != END - BASE:
        raise ValueError("target range coverage differs")
    return dict(status="PASS", named_extent_coverage_exact=True,
                source_target_layout_equal=all(s["layout_equal"] for s in rows),
                source_sections={n: source["sections"][n]["size"] for n in section_layout},
                target_range=[hex(BASE), hex(END)], target_extent=END-BASE,
                objects=rows, scope="identity and zero-allocation extents, not initialized bytes or identical allocation order")


def reference_sites(inputs):
    """Enumerate real relocation users over only the current link inputs."""
    names = {n for n, _, _ in OBJECTS}
    rows = {n: [] for n in names}
    for path in inputs:
        elf = aux.Elf(ROOT / path)
        for i, h in enumerate(elf.sh):
            if h[1] != 4:
                continue
            _, relocs = elf.relas(elf.names[i])
            for offset, info, addend in relocs:
                name = elf.symname(info >> 8).decode()
                if name in names:
                    rows[name].append(dict(object=path, section=elf.names[h[7]],
                                           offset=offset, type=info & 255, addend=addend))
    return rows


def zero_form(source):
    """Change only the eight tentative definitions to explicit zero values."""
    pattern = rb"(?m)^(s32|void\*|MissileTreeInfo) (" + b"|".join(n.encode() for n, _, _ in OBJECTS) + rb")(\[[^;]+)?;\r?$"
    matches = list(re.finditer(pattern, source))
    if len(matches) != 8 or {m[2].decode() for m in matches} != {n for n, _, _ in OBJECTS}:
        raise ValueError("unexpected tentative definition roster")
    return re.sub(pattern, lambda m: m[0].rstrip(b"\r")[:-1] + (b" = {0};" if m[3] else b" = 0;") +
                  (b"\r" if m[0].endswith(b"\r") else b""), source)


def pdb_context(path):
    data = path.read_bytes()
    streams = pdb_reader.pdb_streams(data)
    describe = pdb_reader.describe_types(streams[2])
    dbi, at, modules = streams[3], 64, []
    limit = 64 + struct.unpack_from("<I", dbi, 24)[0]
    while at < limit:
        stream = struct.unpack_from("<H", dbi, at + 34)[0]
        size = struct.unpack_from("<I", dbi, at + 36)[0]
        end = dbi.index(b"\0", at + 64)
        name = dbi[at+64:end].decode("latin1")
        end2 = dbi.index(b"\0", end+1)
        at = (end2+4) & ~3
        if name.upper().endswith("COMBAT.OBJ"):
            rows, procs = pdb_reader.sound_symbols(streams[stream][:size], describe)
            modules.append(dict(module=name, data=rows, procedure_count=len(procs)))
    if len(modules) != 1:
        raise ValueError("expected one Xbox COMBAT module")
    return dict(sha256=aux.digest(data), modules=modules, scope="Xbox data names/types, not GC layout or defining-TU proof")


def audit(output, before=None, probe=False, pdb=None):
    output = output.resolve()
    if (not output.is_relative_to(ROOT / "build") or not output.name.startswith("r69_combat_")
            or output.suffix != ".json"):
        raise ValueError("output must be build/r69_combat_*.json")
    folder = output.parent / (output.stem + "_files")
    folder.mkdir(parents=True, exist_ok=True)
    edge = aux.cv.read_edges()[UNIT]
    raw_path, source_path = ROOT / edge["body_o"], ROOT / edge["src"]
    protected = [raw_path, source_path, ROOT / "build.ninja", ROOT / "config/GUNE5D/webfrank.json", ROOT / "config/GUNE5D/splits.txt"]
    guards = {str(p.relative_to(ROOT)): aux.digest(p.read_bytes()) for p in protected}
    compiled = compiler.compile_source(edge, source_path, folder / "r69_combat_control.o")
    if compiled["error"] or compiled["sha256"] != aux.digest(raw_path.read_bytes()):
        raise ValueError(compiled["error"] or "fresh Ninja baseline differs")
    raw = json.loads(json.dumps(aux.object_inventory(raw_path)))
    target = aux.object_inventory(ROOT / f"build/GUNE5D/obj/{UNIT}.o")
    result = dict(schema_version=1, status="PASS", scope=__doc__, compiler_edge=edge,
                  compiler=compiled, baseline_fidelity=True, raw=raw, target=target,
                  source_sha256=aux.digest(source_path.read_bytes()),
                  source_pragmas=aux.cv.pragma_inventory(source_path),
                  allocation_obligation=allocation_obligation(raw, aux.fndiff.symbol_table()),
                  target_eh_comparison=aux.compare_exception_records(target["exception_records"], raw["exception_records"]))
    snapshot = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
    if snapshot.get("schema_version") != 1 or snapshot.get("ninja_sha256") != aux.digest((ROOT / "build.ninja").read_bytes()):
        raise ValueError("current link snapshot is stale")
    links = [e for e in snapshot["edges"] if e["rule"] == "link"]
    fallback = f"build/GUNE5D/obj/{UNIT}.o"
    if len(links) != 1 or links[0]["inputs"].count(fallback) != 1:
        raise ValueError("expected one NonMatching combat fallback")
    inputs = links[0]["inputs"]
    substituted = [f"build/GUNE5D/src/{UNIT}.o" if p == fallback else p for p in inputs]
    names = [n for n, _, _ in OBJECTS]
    result["current_owners"] = aux.current_owners(inputs, names)
    result["selected_source_owners"] = aux.current_owners(substituted, names)
    result["selected_source_duplicates"] = [n for n, rows in result["selected_source_owners"].items() if len(rows) > 1]
    result["reference_sites"] = reference_sites(inputs)
    result["claimed_sections"] = aux.datadiff.parse_splits()[UNIT + ".c"]
    claim = result["claimed_sections"].get(".bss")
    if claim is not None and (claim != (BASE, END) or result["selected_source_duplicates"]):
        raise ValueError("claimed BSS range or selected-source ownership differs")
    if any(len(rows) != 1 for rows in result["current_owners"].values()):
        raise ValueError("current link has missing or ambiguous BSS definition")
    if any(not rows for rows in result["selected_source_owners"].values()):
        raise ValueError("selected-source ownership loses a definition")
    if probe:
        trial_source = folder / "r69_combat_zero.c"
        trial_source.write_bytes(zero_form(source_path.read_bytes()))
        trial = compiler.compile_source(edge, trial_source, folder / "r69_combat_zero.o")
        result["zero_initialization_probe"] = dict(compiler=trial, held_fixed="All eight types/linkages, all function bodies, flags, pragmas and includes unchanged")
        if not trial["error"]:
            current = aux.object_inventory(Path(trial["object"]))
            result["zero_initialization_probe"].update(
                storage={n: current["symbols"][n] for n in names},
                functions_changed=aux.changed_functions(raw["functions"], current["functions"]),
                eh_unchanged=raw["exception_records"] == current["exception_records"],
                section_sizes={n: s["size"] for n, s in current["sections"].items()},
                initialized_sections_changed=[n for n, s in raw["sections"].items() if s["type"] != 8 and s != json.loads(json.dumps(current["sections"].get(n)))])
    if pdb:
        result["pdb"] = pdb_context(pdb)
    if before:
        old = json.loads(before.read_text())
        result["raw_function_changes"] = aux.changed_functions(old["raw"]["functions"], raw["functions"])
        result["raw_sections_unchanged"] = old["raw"]["sections"] == raw["sections"]
        result["raw_eh_unchanged"] = old["raw"]["exception_records"] == raw["exception_records"]
        if (any(result["raw_function_changes"].values()) or not result["raw_sections_unchanged"] or not result["raw_eh_unchanged"]):
            raise ValueError("source raw code, relocations, sections or EH changed")
    result["protected_unchanged"] = guards == {str(p.relative_to(ROOT)): aux.digest(p.read_bytes()) for p in protected}
    if not result["protected_unchanged"]:
        raise ValueError("production input changed during measurement")
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--probe", action="store_true")
    parser.add_argument("--pdb", type=Path)
    args = parser.parse_args()
    result = audit(args.out, args.before, args.probe, args.pdb)
    print("PASS: eight identities/extents cover 600 bytes; source-target layout equal:", result["allocation_obligation"]["source_target_layout_equal"])
    print("Selected-source duplicate definitions:", result["selected_source_duplicates"])
    print("Not a TU flip or executed link:", args.out)


if __name__ == "__main__":
    main()
