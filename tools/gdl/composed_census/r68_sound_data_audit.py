"""Certify reconstructed SOUNDS data independently of body-match scores.

Writes only lane-prefixed build artifacts. The current raw Ninja object must
reproduce exactly before any optional retention link. Numeric tables and
literal pointers are compared at explicitly measured retail bases; jump
pointers are priced separately against the target function's original home.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import r67_sound_boundary_probe as r67


def literal_certificate(data, section, offset, address, limit=256):
    """Compare one complete bounded C literal, including its NUL, to retail."""
    if not 0 <= offset < section.size:
        raise ValueError("bank literal points outside its own section")
    available = min(section.size - offset, limit)
    start = section.offset + offset
    source = data[start:start + available]
    end = source.find(b"\0")
    if end < 0:
        raise ValueError("bank literal has no bounded NUL terminator")
    source = source[:end + 1]
    target = r67.sp.dol_read(address, len(source))
    return {"target_address": hex(address), "size_with_nul": len(source),
            "source_hex": source.hex(), "target_hex": target.hex(),
            "equal": source == target}


def data_certificate(path):
    data = path.read_bytes()
    sections = r67.wf._sections(data)
    symbols = r67.wf._symbol_index(data, sections)
    section = next(s for s in sections if s.name == ".data")
    if section.size != 4492:
        raise ValueError("expected complete 4492-byte sound data section")
    ours = bytearray(data[section.offset:section.offset+section.size])
    target = r67.sp.dol_read(0x801232C8, section.size)
    addresses = {name: int(address, 16) for name, address in re.findall(
        r"^(\S+) = \.text:0x([0-9A-Fa-f]+);", (ROOT / "config/GUNE5D/symbols.txt").read_text(), re.M)}
    functions = [s for s in symbols.values() if 0 < s.section_index < len(sections)
                 and sections[s.section_index].name == ".text" and s.size and s.name in addresses]
    relocs = r67.wf._function_text_relocations_full(data, sections, section.index, 0, section.size)
    if set(relocs) != set(range(0x54, 0xD4, 4)) | set(range(0x1088, 0x118C, 4)):
        raise ValueError("expected all 32 bank and 65 jump-table relocation sites")
    bindings, literals = [], []
    for at, (kind, name, addend) in relocs.items():
        symbol = symbols.get(name)
        if symbol is None or not 0 < symbol.section_index < len(sections):
            raise ValueError("data relocation requires an own defined destination")
        pointed = sections[symbol.section_index]
        offset = symbol.value + addend
        if kind != 1:
            raise ValueError(f"unexpected .data relocation {kind}")
        expected = struct.unpack_from(">I", target, at)[0]
        if pointed.name in (".rodata", ".sdata2"):
            if at not in range(0x54, 0xD4, 4):
                raise ValueError("jump-table site points to a literal section")
            base = {".rodata": 0x80114A48, ".sdata2": 0x80348458}[pointed.name]
            value = base + offset
            destination = {"section": pointed.name, "offset": offset}
            literal = literal_certificate(data, pointed, offset, expected)
            literals.append(dict(literal, relocation_offset=at, name=name, addend=addend))
        elif pointed.name == ".text":
            if at not in range(0x1088, 0x118C, 4):
                raise ValueError("bank pointer site points to text")
            owners = [s for s in functions if s.value <= offset < s.value + s.size]
            if len(owners) != 1:
                raise ValueError(f"ambiguous jump destination {name}+{addend}")
            fn = owners[0]
            value = addresses[fn.name] + offset - fn.value
            destination = {"function": fn.name, "offset": offset - fn.value}
        else:
            raise ValueError(f"unexpected pointed section {pointed.name}")
        struct.pack_into(">I", ours, at, value)
        bindings.append({"offset": at, "name": name, "addend": addend,
                         "destination": destination, "ours": hex(value),
                         "target": hex(expected), "equal": value == expected})
    return {"size": len(ours), "target_range": ["0x801232C8", hex(0x801232C8+len(ours))],
            "first_4232_equal": ours[:4232] == target[:4232],
            "literal_contents_equal": len(literals) == 32 and all(r["equal"] for r in literals),
            "literal_contents": literals,
            "all_equal": ours == target, "relocations": bindings,
            "pointer_mismatches": [r for r in bindings if not r["equal"]],
            "numeric_nonrelocated_mismatches": [i for i in range(len(ours)) if ours[i] != target[i]
                and not any(at <= i < at+4 for at in relocs)],
            "relocated_sha256": r67.hashlib.sha256(ours).hexdigest()}


def link_retention(path, folder, edge, source):
    """Actual compiled Turbo is the entry; externals use C link-only stubs.

    No synthetic table getters, forceactive or data patching. This fixture is
    not executable: unrelated externals have minimal C link-only bindings.
    Exception metadata is disabled only here to avoid the full runtime closure.
    The default production compiler output is checked separately.
    """
    raw = path.read_bytes()
    sections = r67.wf._sections(raw)
    symbols = r67.wf._symbol_index(raw, sections)
    known = {name: int(address, 16) for name, address in re.findall(
        r"^(\S+) = \S+:0x([0-9A-Fa-f]+);", (ROOT / "config/GUNE5D/symbols.txt").read_text(), re.M)}
    undefined = [s.name for s in symbols.values() if s.section_index == 0 and s.name]
    missing = [name for name in undefined if name not in known]
    if missing:
        return {"status": "UNRESOLVED", "missing_bindings": missing}
    function_names = set(re.findall(r"^(\S+) = \.text:", (ROOT / "config/GUNE5D/symbols.txt").read_text(), re.M))
    if any(not re.fullmatch(r"[A-Za-z_]\w*", name) for name in undefined):
        return {"status": "UNRESOLVED", "reason": "non-C external symbol"}
    bindings = "\n".join(f"void {name}(void) {{}}" if name in function_names else f"unsigned char {name}[1];" for name in undefined) + "\n"
    binding_source = folder / "r68_sound_link_only_bindings.c"
    binding_source.write_text(bindings, encoding="ascii")
    noeh = dict(edge, cflags=edge["cflags"] + " -Cpp_exceptions off")
    binding_compile = r67.compile_source(noeh, binding_source, binding_source.with_suffix(".o"))
    retention_compile = r67.compile_source(noeh, source, folder / "r68_sound_retention_input.o")
    if not binding_compile["object"] or not retention_compile["object"]:
        return {"status": "UNRESOLVED", "binding_compile": binding_compile, "retention_compile": retention_compile}
    aliases = Path(binding_compile["object"])
    noeh_path = Path(retention_compile["object"])
    unchanged = r67.function_bytes(noeh_path) == r67.function_bytes(path)
    if not unchanged:
        return {"status": "UNRESOLVED", "reason": "exception-off body fidelity changed"}
    linked = folder / "r68_sound_retention.elf"
    lcf = folder / "r68_sound_retention.lcf"
    lcf.write_text((ROOT / "build/GUNE5D/ldscript.lcf").read_text().split("FORCEACTIVE", 1)[0], encoding="ascii")
    cmd = [str(ROOT / "build/compilers/GC/1.3.2/mwldeppc.exe"), "-fp", "hardware", "-nodefaults",
           "-main", "AudioPlayerTurbo", "-lcf", str(lcf), str(noeh_path), str(aliases), "-o", str(linked)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    result = {"command": cmd, "returncode": proc.returncode, "stdout": proc.stdout,
              "stderr": proc.stderr, "external_binding_count": len(undefined),
              "body_fidelity_noeh": unchanged, "binding_compile": binding_compile,
              "retention_compile": retention_compile,
              "scope": "retention only, actual TU and Turbo entry with all 118 bodies unchanged, C link-only external bindings and C++ exceptions disabled, production section layout minus FORCEACTIVE; not executable game"}
    if proc.returncode == 0:
        data = linked.read_bytes()
        sec = r67.wf._sections(data)
        relevant = [s for s in sec if s.name in (".rodata", ".data", ".sdata2")]
        _, banks = r67.stream_table_prefix()
        result["retained_bank_strings"] = sum(any(value["value"].encode()+b"\0" in data[s.offset:s.offset+s.size] for s in relevant) for value in banks)
        result["sections"] = r67.section_inventory(linked)
        result["symbols"] = r67.fndiff.objdump(linked, "-t")
        result["status"] = "PASS" if result["retained_bank_strings"] == 32 and result["sections"].get(".data", {}).get("size") == 4492 else "FAIL"
    else:
        result["status"] = "UNRESOLVED"
    return result


def compare_reference(before, after):
    """All raw words and every positional text relocation, including datums.

    A pre-recovery object has only the 260-byte jump-table data suffix. Its
    data base is therefore 80124350; the reconstructed object's base is
    801232C8. Constants are compared by their own symbol bytes, not anonymous
    names or guessed pool addresses. This is a change-fidelity comparison,
    not target exactness or CFG equivalence proof.
    """
    references = r67.function_bytes(before)
    bodies = r67.function_bytes(after)
    known = r67.fndiff.symbol_addresses()

    def model(path, old):
        data = path.read_bytes()
        sections = r67.wf._sections(data)
        symbols = r67.wf._symbol_index(data, sections)
        expected_size = 260 if old else 4492
        if not any(s.name == ".data" and s.size == expected_size for s in sections):
            raise ValueError(f"reference comparison requires .data size {expected_size}")

        def key(name, addend):
            symbol = symbols.get(name)
            if symbol is None or not 0 < symbol.section_index < len(sections):
                if name in known:
                    return ("address", known[name] + addend)
                return ("external", name, addend)
            section = sections[symbol.section_index]
            offset = symbol.value + addend
            if section.name == ".data":
                return ("address", (0x80124350 if old else 0x801232C8) + offset)
            if section.name == ".rodata" and symbol.size == 0:
                return ("address", 0x80114A48 + offset)
            if section.name in (".rodata", ".sdata2") and symbol.size:
                count = symbol.size - addend
                if count <= 0:
                    raise ValueError("pool relocation beyond symbol")
                return ("datum", data[section.offset+offset:section.offset+offset+count])
            if section.name == ".text" and name in known:
                return ("address", known[name] + addend)
            return ("local", section.name, name, addend)
        return key

    old_key, new_key = model(before, True), model(after, False)
    failures, sites, changed_names = [], 0, 0
    for name in references:
        old = r67.function_relocations(before, name)
        new = r67.function_relocations(after, name)
        if [(r["offset"], r["type"]) for r in old] != [(r["offset"], r["type"]) for r in new]:
            failures.append({"function": name, "reason": "relocation sites/types changed"})
            continue
        for a, b in zip(old, new):
            sites += 1
            changed_names += a["name"] != b["name"]
            ka, kb = old_key(a["name"], a["addend"]), new_key(b["name"], b["addend"])
            equal = ka == kb
            if ka[0] == kb[0] == "datum":
                amount = min(len(ka[1]), len(kb[1]))
                equal = amount > 0 and ka[1][:amount] == kb[1][:amount]
            if not equal:
                failures.append({"function": name, "offset": a["offset"],
                                 "before": a, "after": b, "reason": "binding/datum changed"})
    return {"reference_sha256": r67.fingerprint(before), "functions": len(references),
            "bodies_changed": [n for n, b in references.items() if bodies.get(n) != b],
            "relocation_sites_compared": sites, "relocation_names_changed": changed_names,
            "relocation_failures": failures,
            "scope": "before/after fidelity, same source bodies and resolved static datum/binding; not target exactness"}


def audit(output, reference=None, retention=False):
    output = Path(output).resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or not output.name.startswith("r68_sound") or output.suffix != ".json":
        raise ValueError("output must be build/r68_sound*.json")
    folder = output.parent / (output.stem + "_files")
    folder.mkdir(parents=True, exist_ok=True)
    edge = r67.cv.read_edges()["game/sound/sounds_evt"]
    protected = [ROOT / "build.ninja", ROOT / edge["src"], ROOT / edge["body_o"], ROOT / "config/GUNE5D/webfrank.json"]
    guards = {str(p): r67.fingerprint(p) for p in protected}
    row = r67.compile_source(edge, ROOT / edge["src"], folder / "r68_sound_baseline.o")
    result = {"schema_version": 1, "status": "UNRESOLVED", "edge": edge,
              "baseline": row, "source_pragmas": r67.cv.pragma_inventory(ROOT / edge["src"])}
    if row["sha256"] == r67.fingerprint(ROOT / edge["body_o"]):
        obj = Path(row["object"])
        result.update(raw_baseline_identical=True, sections=r67.section_inventory(obj),
                      data=data_certificate(obj))
        if reference:
            result["reference_comparison"] = compare_reference(Path(reference).resolve(), obj)
        if retention:
            result["retention"] = link_retention(obj, folder, edge, ROOT / edge["src"])
        result["status"] = "PASS" if (result["data"]["first_4232_equal"]
                                      and result["data"]["literal_contents_equal"]) else "FAIL"
        comparison = result.get("reference_comparison", {})
        if comparison.get("bodies_changed") or comparison.get("relocation_failures"):
            result["status"] = "FAIL"
        if retention and result["retention"]["status"] != "PASS":
            result["status"] = "UNRESOLVED"
        result["scope"] = "PASS certifies reconstructed numeric/literal-pointer prefix and all 32 pointed-to literal contents; full-data all_equal and remaining jump-pointer mismatches are separate"
    else:
        result["error"] = "raw Ninja baseline reproduction failed; no certificate or link run"
    result["production_inputs_unchanged"] = guards == {str(p): r67.fingerprint(p) for p in protected}
    if not result["production_inputs_unchanged"]:
        result["status"] = "UNRESOLVED"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("build/r68_sound_data_audit.json"))
    parser.add_argument("--reference", type=Path, help="pre-recovery raw object with only 260-byte .data")
    parser.add_argument("--retention", action="store_true", help="run non-executable real-Turbo-entry retention control")
    args = parser.parse_args()
    result = audit(args.out, args.reference, args.retention)
    print(f"{result['status']}: {args.out}")
    if "data" in result:
        print(f"Typed data prefix exact: {result['data']['first_4232_equal']}; whole data exact: {result['data']['all_equal']}; jump-pointer residuals: {len(result['data']['pointer_mismatches'])}")
        print(f"All 32 pointed-to literal contents exact: {result['data']['literal_contents_equal']}")
    raise SystemExit(0 if result["status"] == "PASS" else 2)


if __name__ == "__main__":
    main()
