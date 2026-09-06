"""Recover and certify combat's real three-float launch-offset datum.

Retail consumer instructions, all twelve datum bytes, and adjacent objects are
checked independently. --probe compiles a scratch typed definition, never edits
production source. This is not a whole-function or whole-.data certificate.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import r68_aux_ownership_audit as aux
from tools.gdl.composed_census import r67_sound_boundary_probe as compiler

UNIT, FUNCTION = "game/game/combat", "PlayerStartMissile"
NAME, ADDRESS = "lbl_8011A1A8", 0x8011A1A8
VALUES = (0.0, -0.5, -1.25)
TABLES = ((0, 60, 0x80118D5C, "screen_limitation"),
          (60, 44, 0x80118D98, "screen_limitation"),
          (104, 52, 0x80118DC4, "screen_limitation"),
          (156, 96, 0x8011A1C0, "EnemyStartMissile"))


def target_obligation(code, neighborhood):
    if len(code) != 0x758 or len(neighborhood) != 0x48:
        raise ValueError("unexpected retail code or neighborhood extent")
    hi, lo, disp = (struct.unpack_from(">I", code, offset)[0] for offset in (0x48, 0x50, 0xC4))
    if (hi >> 16, lo >> 16, disp >> 16) != (0x3CA0, 0x3885, 0x3864):
        raise ValueError("retail base/consumer opcode or register changed")
    signed = lambda word: (word & 65535) - (65536 if word & 32768 else 0)
    base = ((hi & 65535) << 16) + signed(lo)
    address = (base + signed(disp)) & 0xFFFFFFFF
    if base != 0x80118DF8 or address != ADDRESS:
        raise ValueError("retail computed datum address differs")
    datum = neighborhood[0x30:0x3C]
    if datum != struct.pack(">3f", *VALUES):
        raise ValueError("retail three-float bytes differ")
    colors = struct.unpack_from(">4I", neighborhood, 0)
    intensities = struct.unpack_from(">8I", neighborhood, 0x10)
    following = struct.unpack_from(">3f", neighborhood, 0x3C)
    return dict(status="PASS", base=hex(base), address=hex(address), size=12,
                values=list(VALUES), bytes=datum.hex(), sha256=aux.digest(datum),
                consumer_instructions={hex(k): hex(struct.unpack_from(">I", code, k)[0]) for k in (0x48, 0x50, 0xC4)},
                neighborhood_range=["0x8011A178", "0x8011A1C0"],
                preceding_colors=list(colors), preceding_intensities=list(intensities), following_vector=list(following),
                caveat="neighbor interpretations are evidence; only the twelve-byte launch datum is reconstructed here")


def typed_form(source):
    old = b"extern u8 lbl_8011A1A8[];"
    use = b"MulVecMat3((f32*)lbl_8011A1A8, aim, playerView->mat);"
    if source.count(old) != 1 or source.count(use) != 1:
        raise ValueError("unexpected source declaration/consumer")
    return source.replace(old, b"f32 lbl_8011A1A8[3] = {0.0f, -0.5f, -1.25f};").replace(
        use, b"MulVecMat3(lbl_8011A1A8, aim, playerView->mat);")


def source_obligation(current, retail):
    datum = current["symbols"].get(NAME)
    if not datum or (datum["section"], datum["size"], datum["binding"]) != (".data", 12, 1):
        raise ValueError("expected one real exported three-float definition")
    if bytes.fromhex(datum["bytes"]) != retail:
        raise ValueError("compiled datum differs from retail")
    section = current["sections"][".data"]
    start = datum["offset"]
    if section["type"] != 1 or bytes.fromhex(section["bytes"])[start:start+12] != retail:
        raise ValueError("compiled section does not contain exact datum")
    if any(start <= r[0] < start + 12 for r in section["relocations"]):
        raise ValueError("unexpected relocation within float datum")
    consumer = current["functions"][FUNCTION]
    refs = [list(r) for r in consumer["relocations"] if r[2] == NAME]
    if refs != [[0xE2, 6, NAME, 0], [0xE6, 4, NAME, 0]]:
        raise ValueError("source launch consumer does not bind the recovered datum")
    body = bytes.fromhex(consumer["body"])
    if body[0xE0:0xE8] != bytes.fromhex("3c60000038630000"):
        raise ValueError("source launch consumer register/opcode differs")
    return dict(status="PASS", section=".data", offset=start, size=12,
                bytes=datum["bytes"], sha256=aux.digest(retail), consumer_relocations=refs)


def delta_obligation(before, current, retail):
    changes = aux.changed_functions(before["functions"], current["functions"])
    if any(changes.values()) or before["exception_records"] != current["exception_records"]:
        raise ValueError("raw function or exception metadata changed")
    if set(before["sections"]) != set(current["sections"]):
        raise ValueError("allocated section roster changed")
    for name, section in before["sections"].items():
        if name != ".data" and section != current["sections"][name]:
            raise ValueError("unrelated allocated section changed: " + name)
    old, new = before["sections"][".data"], current["sections"][".data"]
    for key in set(old) - {"bytes", "size"}:
        if old[key] != new.get(key):
            raise ValueError("existing data metadata or relocations changed")
    if (new["size"] != old["size"] + 12 or
            bytes.fromhex(new["bytes"]) != bytes.fromhex(old["bytes"]) + retail):
        raise ValueError("expected unchanged data prefix and only twelve appended bytes")
    if NAME in before["symbols"] or current["symbols"][NAME]["offset"] != old["size"]:
        raise ValueError("datum is not a newly appended definition")
    if before["symbols"] != {n: s for n, s in current["symbols"].items() if n != NAME}:
        raise ValueError("existing object symbol identity/extent changed")
    source_obligation(current, retail)
    return dict(status="PASS", function_changes=changes, eh_unchanged=True,
                existing_data_prefix_size=old["size"], only_appended_datum_size=12,
                other_sections_unchanged=True, existing_object_symbols_unchanged=True)


def jump_table_context(raw, target, addresses, read=None):
    """Pair all existing table entries; do not equate unequal code offsets."""
    read = read or aux.datadiff.dol_read
    relocs = raw["sections"][".data"]["relocations"]
    if len(relocs) != 63 or {r[0] for r in relocs} != set(range(0, 252, 4)):
        raise ValueError("expected exactly sixty-three generated pointer sites")
    rows = []
    for offset, size, address, function in TABLES:
        symbols = [(n, s) for n, s in raw["symbols"].items() if s["section"] == ".data"
                   and (s["offset"], s["size"], s["binding"]) == (offset, size, 0)]
        if len(symbols) != 1:
            raise ValueError("source local-table extent differs")
        symbol = symbols[0][0]
        target_name = f"jumptable_{address:08X}"
        if addresses[target_name][:3] != (".data", address, size):
            raise ValueError("target table metadata differs")
        source_refs = [list(r) for r in raw["functions"][function]["relocations"] if r[2] == symbol]
        target_refs = [list(r) for r in target["functions"][function]["relocations"] if r[2] == target_name]
        if any(sorted((r[1], r[3]) for r in refs) != [(4, 0), (6, 0)] for refs in (source_refs, target_refs)):
            raise ValueError("table is not bound by its expected consumer")
        contents = read(address, size)
        if contents is None or len(contents) != size:
            raise ValueError("retail table bytes unavailable")
        entries = []
        for site, kind, owner, addend in sorted(r for r in relocs if offset <= r[0] < offset + size):
            target_pointer = struct.unpack_from(">I", contents, site-offset)[0]
            target_offset = target_pointer - addresses[function][1]
            if (kind != 1 or owner != function or addend % 4 or target_offset % 4
                    or not 0 <= addend < raw["functions"][function]["size"]
                    or not 0 <= target_offset < addresses[function][2]):
                raise ValueError("jump destination owner, alignment or extent differs")
            entries.append(dict(index=(site-offset)//4, raw_function_offset=addend,
                                target_function_offset=target_offset, delta=addend-target_offset))
        rows.append(dict(raw_symbol=symbol, raw_offset=offset, size=size, function=function,
                         target_range=[hex(address), hex(address+size)], source_consumer=source_refs,
                         target_consumer=target_refs, entries=entries,
                         exact_relative_destinations=sum(e["delta"] == 0 for e in entries)))
    return dict(status="PASS", raw_table_bytes=252, table_pointer_count=63, tables=rows,
                raw_data_not_one_contiguous_retail_run=True,
                caveat="Table cases are paired by the verified consumer and order; differing destination offsets remain code-layout debt, not a semantic-equivalence proof")


def audit(output, before=None, probe=False):
    output = output.resolve()
    if (not output.is_relative_to(ROOT / "build") or not output.name.startswith("r69_combat_")
            or output.suffix != ".json"):
        raise ValueError("output must be build/r69_combat_*.json")
    folder = output.parent / (output.stem + "_files")
    folder.mkdir(parents=True, exist_ok=True)
    edge = aux.cv.read_edges()[UNIT]
    source_path, raw_path = ROOT / edge["src"], ROOT / edge["body_o"]
    protected = [source_path, raw_path, ROOT / "config/GUNE5D/webfrank.json", ROOT / "config/GUNE5D/splits.txt", ROOT / "build.ninja"]
    guards = {str(p.relative_to(ROOT)): aux.digest(p.read_bytes()) for p in protected}
    control = compiler.compile_source(edge, source_path, folder / "r69_combat_control.o")
    if control["error"] or control["sha256"] != aux.digest(raw_path.read_bytes()):
        raise ValueError(control["error"] or "fresh Ninja raw baseline differs")
    addresses = aux.fndiff.symbol_table()
    at = addresses[FUNCTION][1]
    code = aux.datadiff.dol_read(at, addresses[FUNCTION][2])
    neighborhood = aux.datadiff.dol_read(0x8011A178, 0x48)
    if code is None or neighborhood is None:
        raise ValueError("retail consumer/data unavailable")
    target = target_obligation(code, neighborhood)
    raw = json.loads(json.dumps(aux.object_inventory(raw_path)))
    result = dict(schema_version=1, status="PASS", scope=__doc__, compiler=control,
                  target_obligation=target, raw=raw, source_sha256=aux.digest(source_path.read_bytes()),
                  source_pragmas=aux.cv.pragma_inventory(source_path))
    extracted = aux.object_inventory(ROOT / f"build/GUNE5D/obj/{UNIT}.o")
    result["jump_table_context"] = jump_table_context(raw, extracted, addresses)
    if probe:
        path = folder / "r69_combat_vector.c"
        path.write_bytes(typed_form(source_path.read_bytes()))
        trial = compiler.compile_source(edge, path, folder / "r69_combat_vector.o")
        if trial["error"]:
            raise ValueError(trial["error"])
        candidate = json.loads(json.dumps(aux.object_inventory(Path(trial["object"]))))
        result["probe"] = dict(compiler=trial, inventory=candidate,
            datum_obligation=source_obligation(candidate, neighborhood[0x30:0x3C]),
            delta_obligation=delta_obligation(raw, candidate, neighborhood[0x30:0x3C]),
            changes=aux.changed_functions(raw["functions"], candidate["functions"]),
            eh_unchanged=raw["exception_records"] == candidate["exception_records"],
            changed_sections=[n for n, s in raw["sections"].items() if candidate["sections"].get(n) != s])
    if NAME in raw["symbols"]:
        result["source_obligation"] = source_obligation(raw, neighborhood[0x30:0x3C])
    if before:
        old = json.loads(before.read_text())
        result["raw_function_changes"] = aux.changed_functions(old["raw"]["functions"], raw["functions"])
        result["raw_eh_unchanged"] = old["raw"]["exception_records"] == raw["exception_records"]
        result["changed_sections"] = [n for n, s in old["raw"]["sections"].items() if raw["sections"].get(n) != s]
        result["delta_obligation"] = delta_obligation(old["raw"], raw, neighborhood[0x30:0x3C])
    if guards != {str(p.relative_to(ROOT)): aux.digest(p.read_bytes()) for p in protected}:
        raise ValueError("production inputs changed during audit")
    result["protected_unchanged"] = True
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()
    result = audit(args.out, args.before, args.probe)
    print("PASS: retail computed address and all three float values proven", args.out)
    if "probe" in result:
        print("Scratch full-TU changes:", result["probe"]["changes"])
        print("EH unchanged:", result["probe"]["eh_unchanged"])
    if "raw_function_changes" in result:
        print("Production raw changes:", result["raw_function_changes"])


if __name__ == "__main__":
    main()
