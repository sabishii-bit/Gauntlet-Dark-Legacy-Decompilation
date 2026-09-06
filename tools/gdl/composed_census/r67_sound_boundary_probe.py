"""Controlled sound-TU boundary experiments; never changes production inputs.

Run from the repository root after a matching build. Generated C, objects and
JSON stay under build/. Results are finite experiments, not historical TU proof.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/gdl"))
sys.path.insert(0, str(ROOT))
import fndiff
import matchtool
import webfrank as wf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census import sp_blob_census as sp

UNITS = ("game/sound/sounds_evt", "game/sound/sounds")


def fingerprint(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compile_source(edge, source, output):
    edge = dict(edge, src=str(source), _command_trace=[])
    obj, error = cv.compile_with(edge, edge["mw"], edge["cflags"], output, output.parent)
    return {"object": str(obj) if obj else None, "error": error,
            "commands": edge["_command_trace"],
            "sha256": fingerprint(obj) if obj else None}


def section_inventory(path):
    data = path.read_bytes()
    return {s.name: {"size": s.size, "sha256": hashlib.sha256(
                data[s.offset:s.offset+s.size]).hexdigest()}
            for s in wf._sections(data) if s.name in (".text", ".rodata", ".data", ".sdata2", "extab", "extabindex")}


def function_bytes(path):
    data = Path(path).read_bytes()
    sections = wf._sections(data)
    return {s.name: data[sections[s.section_index].offset+s.value:
                        sections[s.section_index].offset+s.value+s.size]
            for s in wf._symbols(data, sections)
            if 0 < s.section_index < len(sections) and sections[s.section_index].name == ".text"
            and s.size and not s.name.startswith("...")}


def function_relocations(path, name):
    data = Path(path).read_bytes()
    sections = wf._sections(data)
    symbol = wf._find_symbol(data, sections, name)
    symbols = wf._symbol_index(data, sections)
    relocs = wf._function_text_relocations_full(
        data, sections, symbol.section_index, symbol.value, symbol.value+symbol.size)
    return [{"offset": hex(at), "type": kind, "name": label, "addend": addend,
             "own_section": sections[symbols[label].section_index].name
                 if label in symbols and 0 < symbols[label].section_index < len(sections) else None,
             "own_offset": symbols[label].value+addend
                 if label in symbols and 0 < symbols[label].section_index < len(sections) else None}
            for at, (kind, label, addend) in relocs.items()]


def link_control(edge, folder):
    """Final-link controls with the configured linker and ordinary portable C."""
    edge = dict(edge, cflags=edge["cflags"] + " -Cpp_exceptions off")
    forms = {
        "literal_a": 'const char* left(void) { return "R67 duplicate literal"; }\n',
        "literal_b": 'const char* right(void) { return "R67 duplicate literal"; }\n',
        "shared_a": 'const char shared[] = "R67 duplicate literal";\nconst char* left(void) { return shared; }\n',
        "shared_b": 'extern const char shared[];\nconst char* right(void) { return shared; }\n',
        "entry": 'extern const char *left(void), *right(void);\nvolatile const char* results[2];\nvoid __start(void) { results[0] = left(); results[1] = right(); }\n',
        "retained_module": 'char letters[16] = "abcdefghijk";\nchar* names[2] = {"R67 unused filename one", "R67 unused filename two"};\nint flags[2] = {1, 1};\nchar getletter(int n) { return letters[n]; }\n',
        "retained_entry": 'extern char getletter(int);\nvolatile int result;\nvoid __start(void) { result = getletter(0); }\n',
        "pooled_module": 'char letters[16] = "abcdefghijk";\nchar* names[32] = {"R67 unused filename one", "R67 unused filename two"};\nint flags[32] = {1, 1, 1, 1};\nchar getletter(int n) { return letters[n] + flags[n]; }\n',
        "static_pooled_module": 'static char letters[16] = "abcdefghijk";\nstatic char* names[32] = {"R67 unused filename one", "R67 unused filename two"};\nstatic int flags[32] = {1, 1, 1, 1};\nchar getletter(int n) { return letters[n] + flags[n]; }\n',
        "static_pooled_three_module": 'static char letters[16] = "abcdefghijk";\nstatic char* names[32] = {"R67 unused filename one", "R67 unused filename two"};\nstatic int flags[32] = {1, 1, 1, 1};\nstatic int tail[16] = {2, 2, 2, 2};\nchar getletter(int n) { return letters[n] + flags[n] + tail[n]; }\n',
    }
    forms["static_no_pool"] = forms["static_pooled_three_module"]
    forms["typed_island"] = typed_island_source() + (
        'char getletter(int n) { return letter[n] + pantab[n] + elev_desc[n][0]; }\n')
    compiled = {}
    for label, source in forms.items():
        src = folder / (label + ".c")
        src.write_text(source, encoding="utf-8")
        variant_edge = dict(edge, cflags=edge["cflags"] + " -pool off") if label == "static_no_pool" else edge
        compiled[label] = compile_source(variant_edge, src, folder / (label + ".o"))
    ninja = (ROOT / "build.ninja").read_text(encoding="utf-8")
    mw = re.search(r"^mw_version = (.+)$", ninja, re.M).group(1).strip().replace("\\", "/")
    ldflags = re.search(r"^ldflags = (.+)$", ninja, re.M).group(1).strip()
    linker = ROOT / "build/compilers" / mw / "mwldeppc.exe"
    result = {"compiler": edge["mw"], "linker": mw, "linker_sha256": fingerprint(linker),
              "scope": "same configured compiler/linker and global link flags; synthetic fixtures explicitly disable C++ exception metadata, default section layout and synthetic entry (not game LCF)",
              "compiles": compiled, "links": {}}
    import shlex
    for label in ("literal", "shared", "unused_exported_data", "unused_between_live_globals", "unused_between_live_statics", "unused_three_statics", "unused_pool_off", "typed_island"):
        output = folder / (label + ".elf")
        if label == "unused_exported_data":
            names = ("retained_module", "retained_entry")
        elif label == "unused_between_live_globals":
            names = ("pooled_module", "retained_entry")
        elif label == "unused_between_live_statics":
            names = ("static_pooled_module", "retained_entry")
        elif label == "unused_three_statics":
            names = ("static_pooled_three_module", "retained_entry")
        elif label == "unused_pool_off":
            names = ("static_no_pool", "retained_entry")
        elif label == "typed_island":
            names = ("typed_island", "retained_entry")
        else:
            names = (label+"_a", label+"_b", "entry")
        if any(not compiled[k]["object"] for k in names):
            result["links"][label] = {"status": "UNRESOLVED", "error": "compile failed"}
            continue
        command = [str(linker)] + shlex.split(ldflags) + [compiled[k]["object"] for k in names] + ["-o", str(output)]
        proc = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, errors="replace")
        row = {"command": command, "returncode": proc.returncode, "stdout": proc.stdout, "stderr": proc.stderr}
        if proc.returncode == 0 and output.exists():
            data = output.read_bytes()
            sections = wf._sections(data)
            relevant = [s for s in sections if s.name in (".rodata", ".data", ".sdata2")]
            row.update(status="PASS", linked_literal_copies=sum(
                data[s.offset:s.offset+s.size].count(b"R67 duplicate literal\0") for s in relevant),
                unused_filename_copies=sum(data[s.offset:s.offset+s.size].count(b"R67 unused filename") for s in relevant),
                listing=fndiff.objdump(output, "-d"), input_listing=fndiff.objdump(Path(compiled[names[0]]["object"]), "-dr"),
                input_symbols=fndiff.objdump(Path(compiled[names[0]]["object"]), "-t"), sha256=fingerprint(output))
            if label == "typed_island":
                row["datum_reconstruction"] = typed_island_check(Path(compiled[names[0]]["object"]))
                _, table = stream_table_prefix()
                row["retained_bank_strings"] = sum(any(
                    value["value"].encode()+b"\0" in data[s.offset:s.offset+s.size]
                    for s in relevant) for value in table)
        else:
            row["status"] = "FAIL"
        result["links"][label] = row
    return result


def literal_prefix_census():
    """The prefix obligation includes data relocations, not just text xrefs."""
    base, end = 0x80114A48, 0x80114A48+348
    entries = []
    at = base
    while at < end:
        value = sp.pool_entry(at)
        if value is None:
            raise ValueError(f"unreadable literal prefix at {at:#x}")
        entries.append({"address": hex(at), "offset": at-base, "value": value})
        at += (len(value) + 4) & ~3
    labels = {"lbl_" + row["address"][2:].upper() for row in entries}
    hits = []
    config = json.loads((ROOT / "build/GUNE5D/config.json").read_text(encoding="utf-8"))
    current_asm = [ROOT / "build/GUNE5D/asm" / Path(row["object"]).relative_to(
        "build/GUNE5D/obj").with_suffix(".s") for row in config["units"]]
    missing = [str(path.relative_to(ROOT)) for path in current_asm if not path.exists()]
    for path in sorted(path for path in current_asm if path.exists()):
        owner = None
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            start = re.match(r"\.(?:obj|fn) (\S+),", line)
            if start:
                owner = start.group(1)
            if any(label in line for label in labels) and not line.startswith((".obj", ".endobj")):
                hits.append({"path": str(path.relative_to(ROOT)), "line": number, "owner": owner, "text": line.strip()})
    return {"start": hex(base), "end": hex(end), "entries": entries, "references": hits,
            "missing_current_asm": missing,
            "note": "Current config.json units only, including initialized data; stale asm is excluded. Symbol labels are extractor names, not original names; missing_current_asm limits coverage."}


def stream_table_prefix():
    # Actual 32-pointer table at +0x10 inside extractor symbol lbl_8012330C.
    # The diagnostic name is explicitly synthetic. No source/ownership claim
    # is integrated merely because emitting these real data changes codegen.
    pointer_bytes = sp.dol_read(0x8012331C, 32*4)
    addresses = struct.unpack(">32I", pointer_bytes)
    strings = [sp.pool_entry(address) for address in addresses]
    if any(value is None for value in strings):
        raise ValueError("stream table contains a non-string pointer")
    source = "char* r67_stream_names[32] = {\n" + ",\n".join(
        "    " + json.dumps(value) for value in strings) + "\n};\n"
    return source, [{"address": hex(a), "value": s} for a, s in zip(addresses, strings)]


def typed_island_source():
    """Actual data values; five independent array types corroborated by PDB."""
    pantab = struct.unpack(">5i", sp.dol_read(0x801232C8, 20))
    descriptions = sp.dol_read(0x801232DC, 48)
    letter = sp.dol_read(0x8012330C, 16).split(b"\0", 1)[0].decode("ascii")
    _, banks = stream_table_prefix()
    music = struct.unpack(">32i", sp.dol_read(0x8012339C, 128))
    return (
        "static int pantab[5] = {" + ", ".join(map(str, pantab)) + "};\n"
        + "static char elev_desc[6][8] = {" + ", ".join(json.dumps(
            descriptions[i:i+8].split(b"\0", 1)[0].decode("ascii")) for i in range(0, 48, 8)) + "};\n"
        + "static char letter[16] = " + json.dumps(letter) + ";\n"
        + "static char* MovieBanks[32] = {" + ", ".join(json.dumps(r["value"]) for r in banks) + "};\n"
        + "static int MovieMusic[32] = {" + ", ".join(map(str, music)) + "};\n")


def typed_island_check(path):
    """Resolve this isolated fixture's own pointers at measured retail bases.

    A datum/layout certificate, deliberately not an LCF/integration certificate.
    Every pointer resolves to its own defined symbol in a known section.
    """
    data = path.read_bytes()
    sections = wf._sections(data)
    symbols = wf._symbol_index(data, sections)
    section = next(s for s in sections if s.name == ".data")
    ours = bytearray(data[section.offset:section.offset+section.size])
    relocs = wf._function_text_relocations_full(data, sections, section.index, 0, section.size)
    if section.size != 0x154 or set(relocs) != set(range(0x54, 0xD4, 4)):
        raise ValueError("typed island needs 340 bytes and all 32 expected pointer sites")
    bases = {".rodata": 0x80114A48, ".sdata2": 0x80348458}
    rows = []
    for at, (kind, name, addend) in relocs.items():
        symbol = symbols.get(name)
        if kind != 1 or symbol is None or not 0 < symbol.section_index < len(sections):
            raise ValueError("typed island needs defined R_PPC_ADDR32 pointers")
        pointed = sections[symbol.section_index]
        if pointed.name not in bases:
            raise ValueError("typed island pointer outside known literal sections")
        offset = symbol.value+addend
        value = bases[pointed.name] + offset
        struct.pack_into(">I", ours, at, value)
        rows.append({"offset": at, "name": name, "section": pointed.name,
                     "section_offset": offset, "relocated_address": hex(value)})
    target = sp.dol_read(0x801232C8, 0x154)
    pool_checks = {s.name: data[s.offset:s.offset+s.size] == sp.dol_read(bases[s.name], s.size)
                   for s in sections if s.name in bases}
    return {"equal_at_retail_bases": bytes(ours) == target, "bytes": len(ours),
            "pointer_count": len(rows), "pointer_bindings": rows,
            "literal_sections_equal_at_retail_bases": pool_checks,
            "data_sha256": hashlib.sha256(ours).hexdigest(),
            "scope": "five independent arrays, complete bytes plus 32 own-section pointer bindings at measured retail section bases; no claim that production LCF places them there"}


def boss_literals(source):
    source, n = re.subn(r"\bformats \+ (\d+)\b", lambda m: json.dumps(
        sp.pool_entry(0x80114A48 + int(m.group(1)))), source)
    if n != 19:
        raise ValueError(f"expected 19 boss format uses, found {n}")
    decl = "    register char* formats = lbl_80114A48;\n"
    if source.count(decl) != 1:
        raise ValueError("boss format local declaration drifted")
    return source.replace(decl, "")


def rodata_prefix_check(path):
    data = Path(path).read_bytes()
    section = next(s for s in wf._sections(data) if s.name == ".rodata")
    ours = data[section.offset:section.offset+section.size]
    target = sp.dol_read(0x80114A48, 524)
    length = min(len(ours), len(target))
    first = next((i for i in range(length) if ours[i] != target[i]), None)
    return {"compared": length, "target_extent": len(target), "ours_extent": len(ours),
            "first_difference": first, "whole_524_byte_prefix_equal": first is None and length == len(target),
            "scope": "literal bytes/individual 4-byte padding from stream-table prefix through final boss format; not linked-address proof"}


def compare_roster(reference, candidate):
    scores = matchtool.score(reference, candidate)
    return {"scope": "normalized instruction/relocation-name score; not datum proof",
            "functions": len(reference),
            "normalized_equal": sum(v in ("OK", "OK~") for v in scores.values()),
            "changed": {k: v for k, v in scores.items() if v not in ("OK", "OK~")}}


def merge_sources(evt, sound, reset=False):
    # Two existing conflicting return declarations are unused by these callers.
    # Correcting the declarations exposes the same function definitions;
    # separate-file controls below quantify this seam independently of merging.
    evt = evt.replace("extern int AudioWithName(int id, int pidx, f32 vol, int s4, int s5);",
                      "extern void AudioWithName(int id, int pidx, f32 vol, int s4, int s5);")
    evt = evt.replace("extern void sndFxPlay3DTracked(int soundId, int pos, int p2, int flags);",
                      "extern int sndFxPlay3DTracked(int soundId, int pos, int p2, int flags);")
    seam = "\n#undef offsetof\n"
    if reset:
        seam += "#pragma optimization_level reset\n#pragma peephole reset\n#pragma scheduling reset\n"
    return evt, sound, evt + seam + sound


def run(out):
    # This tool generates many C/object siblings. Keep all of them in its
    # own build namespace, never a production source/report directory.
    out = Path(out).resolve()
    if not out.is_relative_to((ROOT / "build").resolve()) or not out.name.startswith("r67_sound") or out.suffix != ".json":
        raise ValueError("output must be a build/r67_sound*.json artifact")
    out.parent.mkdir(parents=True, exist_ok=True)
    folder = out.parent / (out.stem + "_files")
    folder.mkdir(parents=True, exist_ok=True)
    edges = cv.read_edges()
    protected = [ROOT / "build.ninja", ROOT / "config/GUNE5D/webfrank.json"]
    protected += [ROOT / edges[u]["src"] for u in UNITS]
    protected += [ROOT / edges[u]["body_o"] for u in UNITS]
    before = {str(p.relative_to(ROOT)): fingerprint(p) for p in protected}
    result = {"schema_version": 1, "status": "UNRESOLVED", "baselines": {},
              "variants": {}, "source_merge_seams": [
                  "evt AudioWithName unused return int -> void",
                  "evt sndFxPlay3DTracked unused return void -> int",
                  "undef offsetof at seam", "optional reset of three leaked optimization pragmas"]}
    raw = {}
    raw_by_unit = {}
    raw_bytes = {}
    targets = {}
    for unit in UNITS:
        edge = edges[unit]
        row = compile_source(edge, ROOT / edge["src"], folder / (Path(unit).name + "_baseline.o"))
        row["ninja_raw_sha256"] = fingerprint(ROOT / edge["body_o"])
        row["baseline_identical"] = row["sha256"] == row["ninja_raw_sha256"]
        result["baselines"][unit] = row
        if not row["baseline_identical"]:
            result["error"] = "raw baseline reproduction failed; no variants run"
            break
        raw_by_unit[unit] = matchtool.parse(Path(row["object"]))
        raw.update(raw_by_unit[unit])
        raw_bytes.update(function_bytes(Path(row["object"])))
        targets.update(matchtool.parse(ROOT / "build/GUNE5D/obj" / (unit + ".o")))
    else:
        texts = [(ROOT / edges[u]["src"]).read_text(encoding="utf-8") for u in UNITS]
        evt, sound, merged = merge_sources(*texts)
        forms = [("evt_declaration_control", evt, UNITS[0]),
                 ("sound_declaration_control", sound, UNITS[1]),
                 ("merged_inherited_pragmas", merged, None),
                 ("merged_reset_pragmas", merge_sources(*texts, reset=True)[2], None)]
        prefix, table = stream_table_prefix()
        result["stream_table"] = {"address": "0x8012331c", "entries": table,
            "scope": "real 32 target pointer data, diagnostic-only global name; historical ownership unresolved"}
        forms += [("evt_stream_table_control", prefix + evt, UNITS[0]),
                  ("evt_stream_table_boss_literals", prefix + boss_literals(evt), UNITS[0]),
                  ("merged_stream_table_boss_literals", prefix + boss_literals(merged), None)]
        for label, source, unit in forms:
            src = folder / (label + ".c")
            src.write_text(source, encoding="utf-8")
            row = compile_source(edges[unit or UNITS[0]], src, folder / (label + ".o"))
            result["variants"][label] = row
            if row["object"]:
                parsed = matchtool.parse(Path(row["object"]))
                names = set(raw_by_unit[unit]) if unit else set(raw)
                reference = {k: v for k, v in raw.items() if k in names}
                row["versus_separate_raw"] = compare_roster(reference, parsed)
                row["versus_target"] = compare_roster(
                    {k: v for k, v in targets.items() if k in names}, parsed)
                row["sections"] = section_inventory(Path(row["object"]))
                if "stream_table" in label:
                    row["rodata_prefix"] = rodata_prefix_check(Path(row["object"]))
                    row["boss_relocations"] = function_relocations(Path(row["object"]), "AudioSetupBossStreams")
                words = function_bytes(Path(row["object"]))
                checked = {k: v for k, v in raw_bytes.items() if k in names}
                row["raw_function_bytes_vs_separate"] = {"compared": len(checked),
                    "equal": sum(words.get(k) == v for k, v in checked.items()),
                    "changed": [k for k, v in checked.items() if words.get(k) != v],
                    "extra_names": sorted(set(words) - names),
                    "scope": "unrelocated function bytes; relocation payloads remain separately unproved"}
        result["baseline_vs_target"] = compare_roster(targets, raw)
        result["link_controls"] = link_control(edges[UNITS[0]], folder)
        result["literal_prefix"] = literal_prefix_census()
        result["status"] = "PASS" if (
            all(r["object"] for r in result["variants"].values())
            and all(r["status"] == "PASS" for r in result["link_controls"]["links"].values())) else "UNRESOLVED"
    result["protected_inputs_unchanged"] = all(fingerprint(p) == before[str(p.relative_to(ROOT))] for p in protected)
    result["protected_sha256"] = before
    if not result["protected_inputs_unchanged"]:
        result["status"] = "FAIL"
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": result["status"], "baseline_vs_target": result.get("baseline_vs_target"),
                      "variants": {k: {"error": v["error"], "versus_separate_raw": v.get("versus_separate_raw"),
                                       "versus_target": v.get("versus_target")}
                                   for k, v in result["variants"].items()},
                      "protected_inputs_unchanged": result["protected_inputs_unchanged"], "out": str(out)}, indent=2))
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("build/r67_sound_boundary.json"))
    args = parser.parse_args()
    raise SystemExit(run(args.out.resolve()))
