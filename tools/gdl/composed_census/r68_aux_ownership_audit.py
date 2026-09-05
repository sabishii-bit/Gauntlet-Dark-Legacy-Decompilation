"""Bounded auxscreen data-ownership evidence, not a whole-TU match certificate.

Freshly reproduces the raw Ninja compiler object, inventories every function,
allocated section and relocation, and resolves named definitions over CURRENT
ordered linker inputs (never stale objects left on disk after a resplit).
An optional prior report checks whole-function and EH preservation. Reports
retain the open target differences; PASS means the measurement completed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools/gdl"))
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.gdl import datadiff, fndiff
from tools.gdl.exception_metadata import exception_records, compare_exception_records

UNIT = "game/ui/auxscreen"
INITIALIZED = ("map_fade_a", "map_fade_b", "map_load_len", "map_load_step",
               "wiz_exit_min", "WizDelayGoldLeft", "WizDelayNoGold")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def object_inventory(path):
    elf = Elf(path)
    functions, _ = inventory(path)
    for function in functions.values():
        function["relocations"] = [list(row) for row in function["relocations"]]
    sections = {}
    for i, h in enumerate(elf.sh):
        if not h[2] & 2:
            continue
        relocs = []
        for ri, rh in enumerate(elf.sh):
            if rh[1] == 9 and rh[7] == i:
                raise ValueError("implicit-addend relocations are unsupported")
            if rh[1] == 4 and rh[7] == i:
                _, entries = elf.relas(elf.names[ri])
                relocs.extend((off, info & 255, elf.symname(info >> 8).decode(), add)
                              for off, info, add in entries)
        sections[elf.names[i]] = dict(size=h[5], alignment=h[8], type=h[1], flags=h[2],
                                      bytes=None if h[1] == 8 else bytes(elf.data[h[4]:h[4]+h[5]]).hex(),
                                      relocations=relocs)
    symbols = {}
    for i in range(elf.symcount):
        s = elf.sym(i)
        if s[3] & 15 != 1 or not 0 < s[5] < len(elf.sh):
            continue
        name = elf.symname(i).decode()
        h = elf.sh[s[5]]
        symbols[name] = dict(section=elf.names[s[5]], offset=s[1], size=s[2],
                             binding=s[3] >> 4,
                             bytes=None if h[1] == 8 else bytes(elf.data[h[4]+s[1]:h[4]+s[1]+s[2]]).hex())
    return dict(functions=functions, sections=sections, symbols=symbols,
                exception_records=exception_records(bytes(elf.data)))


def initialized_obligation(source, target_bytes, expected_offsets):
    """Prove the complete emitted run, alignment and zero terminal slack.

    Exactly seven four-byte globals are expected. A prefix equality alone
    cannot pass: every emitted byte is covered by one named definition, no
    relocation can hide a datum, and only four zero alignment bytes may trail.
    """
    section = source["sections"].get(".sdata")
    if section is None or section["type"] != 1 or section["alignment"] != 8:
        raise ValueError("expected initialized .sdata with alignment 8")
    if section["relocations"]:
        raise ValueError("unexpected initialized-state relocation")
    actual = bytes.fromhex(section["bytes"])
    if section["size"] != 28 or len(actual) != 28 or len(target_bytes) != 32:
        raise ValueError("unexpected initialized-state extent")
    if actual != target_bytes[:28] or target_bytes[28:] != b"\0" * 4:
        raise ValueError("initialized bytes or alignment slack differ")
    emitted = {n: s for n, s in source["symbols"].items() if s["section"] == ".sdata"}
    if set(emitted) != set(INITIALIZED) or set(expected_offsets) != set(INITIALIZED):
        raise ValueError("initialized-state named coverage differs")
    for index, name in enumerate(INITIALIZED):
        symbol = emitted[name]
        if (symbol["offset"] != index * 4 or expected_offsets[name] != index * 4
                or symbol["size"] != 4 or symbol["binding"] != 1
                or symbol["bytes"] != actual[index*4:index*4+4].hex()):
            raise ValueError("initialized-state symbol position/type differs: " + name)
    return dict(status="PASS", source_extent=28, target_claim_extent=32,
                alignment=8, terminal_zero_slack=4, relocations=0,
                source_sha256=digest(actual), target_sha256=digest(target_bytes))


def changed_functions(before, after):
    if set(before) != set(after):
        raise ValueError("function roster changed")
    return {key: sorted(n for n in before if before[n][key] != after[n][key])
            for key in ("offset", "size", "body", "relocations", "binding")}


def current_owners(inputs, names):
    owners = {name: [] for name in names}
    for path in inputs:
        elf = Elf(ROOT / path)
        for i in range(elf.symcount):
            s = elf.sym(i)
            name = elf.symname(i).decode()
            if (name not in owners or s[3] & 15 != 1 or s[3] >> 4 != 1
                    or not 0 < s[5] < len(elf.sh)):
                continue
            owners[name].append(dict(object=path, section=elf.names[s[5]],
                                     offset=s[1], size=s[2]))
    return owners


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--before", type=Path)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / "build") or not output.name.startswith("r68_aux_"):
        parser.error("output must be build/r68_aux_*.json")
    result = dict(schema_version=1, status="UNRESOLVED", scope=__doc__)
    try:
        edge = cv.read_edges()[UNIT]
        raw_path = ROOT / edge["body_o"]
        raw = raw_path.read_bytes()
        source = (ROOT / edge["src"]).read_bytes()
        folder = Path(tempfile.mkdtemp(prefix="r68_aux_fidelity_", dir=ROOT / "build"))
        trace = []
        trial = dict(edge, _command_trace=trace)
        obj, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / "control.o", folder)
        result.update(compiler_trace=trace, raw_path=edge["body_o"], source_sha256=digest(source))
        if error or not obj or obj.read_bytes() != raw:
            raise ValueError(error or "fresh raw-object fidelity failed")
        result.update(baseline_fidelity=True, raw_sha256=digest(raw),
                      raw=object_inventory(raw_path),
                      postprocessed=object_inventory(ROOT / f"build/GUNE5D/src/{UNIT}.o"),
                      target=object_inventory(ROOT / f"build/GUNE5D/obj/{UNIT}.o"))
        result["target_eh_comparison"] = compare_exception_records(
            result["target"]["exception_records"], result["raw"]["exception_records"])
        table = fndiff.symbol_table()
        base, end = 0x80343B50, 0x80343B70
        expected = {}
        for name in INITIALIZED:
            section, address, size = table[name][:3]
            if section != ".sdata" or size not in (4, 8):
                raise ValueError("unexpected target symbol metadata")
            expected[name] = address - base
        target_bytes = datadiff.dol_read(base, end - base)
        if target_bytes is None:
            raise ValueError("retail initialized-state range unavailable")
        result["initialized_obligation"] = initialized_obligation(result["raw"], target_bytes, expected)
        result["claimed_sections"] = datadiff.parse_splits()[UNIT + ".c"]
        snapshot = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
        if snapshot.get("schema_version") != 1 or snapshot.get("ninja_sha256") != digest((ROOT / "build.ninja").read_bytes()):
            raise ValueError("active build snapshot is stale or unsupported")
        links = [e for e in snapshot["edges"] if e["rule"] == "link"]
        if len(links) != 1:
            raise ValueError("expected one current link")
        names = set(result["raw"]["symbols"]) & set(table)
        names |= {"good_wiz_enabled", "good_wiz_exit_timer"}
        result["current_link_owners"] = current_owners(links[0]["inputs"], names)
        target_path = f"build/GUNE5D/obj/{UNIT}.o"
        if links[0]["inputs"].count(target_path) != 1:
            raise ValueError("expected exactly one NonMatching auxscreen fallback input")
        source_inputs = [f"build/GUNE5D/src/{UNIT}.o" if p == target_path else p
                         for p in links[0]["inputs"]]
        result["aux_source_substituted_owners"] = current_owners(source_inputs, names)
        result["aux_source_substituted_duplicate_names"] = sorted(
            n for n, owners in result["aux_source_substituted_owners"].items() if len(owners) > 1)
        if any(len(result["current_link_owners"][n]) != 1 for n in INITIALIZED):
            raise ValueError("initialized state lacks exactly one current link owner")
        if args.before:
            before = json.loads(args.before.read_text())
            result["raw_function_changes"] = changed_functions(before["raw"]["functions"], result["raw"]["functions"])
            result["postprocessed_function_changes"] = changed_functions(before["postprocessed"]["functions"], result["postprocessed"]["functions"])
            result["raw_eh_unchanged"] = before["raw"]["exception_records"] == result["raw"]["exception_records"]
        if raw_path.read_bytes() != raw or (ROOT / edge["src"]).read_bytes() != source:
            raise ValueError("production source or raw object changed during measurement")
        result.update(status="PASS", protected_unchanged=True)
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("error", "measurement completed; not a TU flip"), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
