"""Baseline-faithful, whole-object A/B for explicit stale-symbol repairs.

Both old and corrected source forms are compiled with the actual Ninja edge.
PASS proves this bounded edit changes only the listed relocation names, not
instructions, initialized sections, BSS extents, or exception data. Target
relocations and existing linked providers are reported separately: this is
neither a TU-flip certificate nor an all-source-link certificate.
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
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.gdl.atree_exports import read_symbols

CASES = {
    "pb-diag": {
        "unit": "game/pb/pb_diag",
        "names": {"lbl_8023D000": "atree_scroll", "lbl_8023D180": "whichatree"},
        "occurrences": 2,
        "provider": "build/GUNE5D/src/game/anim/atree.o",
    },
    "scroll-mode": {
        "unit": "game/ui/btext",
        "names": {"gScrollModes": "gScrollModes_80343BB0"},
        "occurrences": 2,
        "provider": "build/GUNE5D/obj/auto_09_80343B18_sdata.o",
    },
}


def forms(source, names, occurrences):
    """Refuse mixed/absent inputs; identifier boundaries prevent suffix edits."""
    states = []
    for old, new in names.items():
        counts = [len(re.findall(rb"\b" + name.encode() + rb"\b", source))
                  for name in (old, new)]
        if counts not in ([occurrences, 0], [0, occurrences]):
            raise ValueError(f"ambiguous identifier occurrences for {old}: {counts}")
        states.append("old" if counts[0] else "corrected")
    if len(set(states)) != 1:
        raise ValueError("mixed source state")
    active = states[0]
    other = source
    for old, new in names.items():
        before, after = (old, new) if active == "old" else (new, old)
        other = re.sub(rb"\b" + before.encode() + rb"\b", after.encode(), other)
    return active, {active: source, "corrected" if active == "old" else "old": other}


def object_facts(path):
    elf = Elf(str(path))
    functions, payloads = inventory(path)
    sections = {elf.names[i]: (h[1], h[2], h[5], h[8])
                for i, h in enumerate(elf.sh) if h[2] & 2}
    relocs = []
    for i, h in enumerate(elf.sh):
        if h[1] != 4:
            continue
        _, rows = elf.relas(elf.names[i])
        relocs.extend((elf.names[h[7]], off, info & 255,
                       elf.symname(info >> 8).decode(), add)
                      for off, info, add in rows)
    return functions, payloads, sections, sorted(relocs)


def renamed_relocations(rows, names):
    return sorted((*r[:3], names.get(r[3], r[3]), r[4]) for r in rows)


def compare(old, corrected, names):
    a, pa, sa, ra = old
    b, pb, sb, rb = corrected
    if a.keys() != b.keys():
        raise ValueError("function roster changed")
    for name in a:
        for key in ("body", "offset", "size", "binding"):
            if a[name][key] != b[name][key]:
                raise ValueError(f"{name}: {key} changed")
    if pa != pb or sa != sb:
        raise ValueError("allocated section content/layout changed")
    if renamed_relocations(ra, names) != rb:
        raise ValueError("relocations changed beyond the explicit identifier mapping")
    changed = [dict(section=r[0], offset=r[1], type=r[2], old=r[3],
                    corrected=names[r[3]], addend=r[4]) for r in ra if r[3] in names]
    if {r["old"] for r in changed} != names.keys():
        raise ValueError("requested identifier change has no relocation witness")
    return {"function_count": len(a), "unchanged_function_bodies": len(a),
            "unchanged_allocated_section_layout": True,
            "unchanged_initialized_sections": True, "changed_relocations": changed}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=CASES)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or output.suffix != ".json":
        parser.error("--out must be a JSON artifact inside this checkout's build")
    case = CASES[args.case]
    result = dict(schema_version=1, status="UNRESOLVED", case=args.case, compiles={})
    try:
        edge = cv.read_edges()[case["unit"]]
        source_path = ROOT / edge["src"]
        raw_path = ROOT / edge["body_o"]
        source, raw = source_path.read_bytes(), raw_path.read_bytes()
        active, variants = forms(source, case["names"], case["occurrences"])
        folder = Path(tempfile.mkdtemp(prefix="r68_binding_", dir=ROOT / "build"))
        result.update(active_form=active, artifacts=str(folder.relative_to(ROOT)),
                      source_sha256=hashlib.sha256(source).hexdigest(),
                      raw_sha256=hashlib.sha256(raw).hexdigest(), raw_object=edge["body_o"])
        for label in (active, "corrected" if active == "old" else "old"):
            trial = dict(edge, _command_trace=[])
            if label != active:
                scratch = folder / source_path.name
                scratch.write_bytes(variants[label])
                trial["src"] = str(scratch.relative_to(ROOT))
            obj, error = cv.compile_with(trial, trial["mw"], trial["cflags"], folder / (label + ".o"), folder)
            result["compiles"][label] = dict(trace=trial["_command_trace"], error=error)
            if error or not obj:
                raise ValueError(error or "no compiler output")
            if label == active and obj.read_bytes() != raw:
                raise ValueError("fresh actual-source control differs from raw Ninja object")
        result.update(compare(object_facts(folder / "old.o"), object_facts(folder / "corrected.o"), case["names"]))
        target = ROOT / "build/GUNE5D/obj" / (case["unit"] + ".o")
        target_facts = object_facts(target)
        result["target_relocations"] = [r for r in target_facts[3] if r[3] in case["names"].values()]
        if {r[3] for r in result["target_relocations"]} != set(case["names"].values()):
            raise ValueError("target object does not witness every proposed corrected symbol")
        provider = ROOT / case["provider"]
        exports = [s for s in read_symbols(provider) if s.name in case["names"].values()
                   and s.section is not None and s.bind == "STB_GLOBAL"]
        result["providers"] = [dict(path=case["provider"], name=s.name, section=s.section,
                                    offset=s.value, size=s.size, binding=s.bind) for s in exports]
        if sorted(s.name for s in exports) != sorted(case["names"].values()):
            raise ValueError("provider does not define exactly one global for every corrected name")
        if source_path.read_bytes() != source or raw_path.read_bytes() != raw:
            raise ValueError("production source/raw changed during experiment")
        result.update(status="PASS", baseline_fidelity=True, protected_unchanged=True)
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("error", f"{result.get('function_count')} raw bodies unchanged"), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
