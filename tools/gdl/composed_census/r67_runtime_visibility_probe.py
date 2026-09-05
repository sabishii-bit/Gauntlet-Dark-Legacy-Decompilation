"""Scratch full-TU static/global A/B for three existing mb_blit helpers.

Uses actual Ninja compiler flags and requires a byte-identical fresh raw
baseline. Changes no production source or object. The only variant is removal
of static from each helper's declaration and definition in a scratch copy.
No flags, bodies, order, data or pragmas change; this is not a TU flip claim.
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
from tools.fix_exception_objects import Elf
from tools.gdl.composed_census import cv_probe as cv

HELPERS = ("mbInitBlitEntry", "mbBlitProject", "mbBlitSetupVerts")


def variant_source(source):
    for name in HELPERS:
        pattern = rb"(?m)^static (?=(?:u32|void) " + name.encode() + rb"\()"
        source, count = re.subn(pattern, b"", source)
        if count != 2:
            raise ValueError(f"{name}: expected exactly one static declaration and definition")
    return source


def inventory(path):
    elf = Elf(str(path))
    functions = {}
    for i in range(elf.symcount):
        symbol = elf.sym(i)
        if symbol[3] & 15 != 2 or not symbol[5] or not symbol[2]:
            continue
        section = elf.sh[symbol[5]]
        name = elf.symname(i).decode()
        relocations = []
        for ri, rh in enumerate(elf.sh):
            if rh[1] != 4 or rh[7] != symbol[5]:
                continue
            _, entries = elf.relas(elf.names[ri])
            relocations.extend((off - symbol[1], info & 255, elf.symname(info >> 8).decode(), add)
                               for off, info, add in entries if symbol[1] <= off < symbol[1] + symbol[2])
        body = bytes(elf.data[section[4] + symbol[1]:section[4] + symbol[1] + symbol[2]])
        functions[name] = dict(offset=symbol[1], size=symbol[2], binding=symbol[3] >> 4,
                               body=body.hex(), relocations=relocations)
    sections = {elf.names[i]: bytes(elf.data[h[4]:h[4] + h[5]]).hex()
                for i, h in enumerate(elf.sh) if h[2] & 2 and h[1] != 8}
    return functions, sections


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/r67_runtime_visibility.json")
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or output.suffix != ".json" or not output.name.startswith("r67_runtime_"):
        parser.error("--out must name r67_runtime_*.json under this checkout's build/")
    result = dict(status="UNRESOLVED", schema_version=1, scope=__doc__, compiles={})
    try:
        edge = cv.read_edges()["game/mb/mb_blit"]
        source_path = ROOT / edge["src"]
        source = source_path.read_bytes()
        raw = (ROOT / edge["body_o"]).read_bytes()
        variant = variant_source(source)
        folder = Path(tempfile.mkdtemp(prefix="r67_runtime_visibility_", dir=ROOT / "build"))
        scratch = folder / source_path.name
        scratch.write_bytes(variant)
        result.update(artifacts=str(folder.relative_to(ROOT)), raw_object=edge["body_o"],
                      source_sha256=hashlib.sha256(source).hexdigest(),
                      variant_source_sha256=hashlib.sha256(variant).hexdigest())
        for label in ("baseline", "global"):
            trial = dict(edge, _command_trace=[])
            if label == "global":
                trial["src"] = str(scratch.relative_to(ROOT))
            obj, error = cv.compile_with(trial, trial["mw"], trial["cflags"], folder / (label + ".o"), folder)
            result["compiles"][label] = dict(trace=trial["_command_trace"], error=error)
            if not obj or error:
                raise ValueError(error or "no compiler output")
            if label == "baseline" and obj.read_bytes() != raw:
                raise ValueError("fresh baseline differs from active raw compiler object")
        a, sa = inventory(folder / "baseline.o")
        b, sb = inventory(folder / "global.o")
        target, _ = inventory(ROOT / "build/GUNE5D/obj/game/mb/mb_blit.o")
        result.update(baseline_fidelity=True, baseline_sha256=hashlib.sha256(raw).hexdigest(),
                      function_count=len(a), identical_function_set=a.keys() == b.keys(),
                      changed_function_bodies=sorted(n for n in a.keys() & b.keys() if a[n]["body"] != b[n]["body"]),
                      changed_function_relocations=sorted(n for n in a.keys() & b.keys() if a[n]["relocations"] != b[n]["relocations"]),
                      changed_function_positions=sorted(n for n in a.keys() & b.keys() if a[n]["offset"] != b[n]["offset"]),
                      changed_allocated_sections=sorted(n for n in sa.keys() | sb.keys() if sa.get(n) != sb.get(n)),
                      helpers={n: {label: {k: v for k, v in table[n].items() if k not in ("body", "relocations")}
                                   for label, table in (("baseline", a), ("global", b), ("target", target))} for n in HELPERS})
        if source_path.read_bytes() != source or (ROOT / edge["body_o"]).read_bytes() != raw:
            raise ValueError("production source/raw object changed during measurement")
        result.update(protected_unchanged=True, status="PASS")
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("error", "bounded visibility experiment completed"), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
