"""Measure btext's six small-global allocations, not whole-TU equivalence.

The fresh compiler control must reproduce the active raw Ninja object. A
before/after comparison permits only the two intended .sbss symbol offsets
to move; all allocated bytes, relocations, functions and EH remain fixed.
Ownership is counted over current ordered link inputs, not stale objects.
An ownership substitution inventory is NOT an executed source link.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r68_btext_bss_audit import obligation as large_bss_obligation
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, current_owners
from tools.gdl import datadiff, fndiff
from tools.gdl.exception_metadata import compare_exception_records

UNIT = "game/ui/btext"
RUNS = {
    ".sdata": (0x80343BB8, ("scroll_level_msg", "DrawStringScale", "OldStringScale")),
    ".sbss": (0x803443D8, ("shadow_color", "gDrawTextY", "gLineSpacing")),
}
NAMES = tuple(n for _, names in RUNS.values() for n in names)


def section_obligation(snapshot, section_name, table, retail):
    base, names = RUNS[section_name]
    initialized = section_name == ".sdata"
    section = snapshot["sections"].get(section_name)
    expected = dict(type=1 if initialized else 8, flags=3, size=12,
                    alignment=8, bytes=retail.hex() if initialized else None)
    if section != expected or snapshot["relocations"].get(section_name):
        raise ValueError("complete section shape/bytes/relocations differ: " + section_name)
    objects = [s for s in snapshot["symbols"] if s["section"] == section_name and s["type"] == 1]
    if sorted(s["name"] for s in objects) != sorted(names):
        raise ValueError("missing, duplicate or additional small object")
    rows, mismatches = [], []
    for index, name in enumerate(names):
        symbol = next(s for s in objects if s["name"] == name)
        if tuple(table[name][:3]) != (section_name, base + index * 4, 4):
            raise ValueError("unexpected target object identity/extent: " + name)
        if (symbol["size"], symbol["binding"], symbol["other"]) != (4, 1, 0):
            raise ValueError("unexpected compiled object metadata: " + name)
        if symbol["value"] not in (0, 4, 8):
            raise ValueError("small object outside complete section")
        if symbol["value"] != index * 4:
            mismatches.append(name)
        rows.append(dict(name=name, source_offset=symbol["value"], target_offset=index*4,
                         target_address=hex(base + index*4), size=4))
    if sorted(s["value"] for s in objects) != [0, 4, 8]:
        raise ValueError("overlap/gap in small-global allocation")
    return dict(status="EXACT" if not mismatches else "LAYOUT_MISMATCH",
                section=section_name, start=hex(base), end=hex(base+12), extent=12,
                alignment=8, terminal_padding=0, initialized=initialized,
                bytes=retail.hex() if initialized else None, objects=rows,
                offset_mismatches=mismatches)


def preservation(before, after):
    """Only the endpoint positions of the three-element .sbss run may move."""
    a, b = before["snapshot"], after["snapshot"]
    if not a.get("fidelity") or not b.get("fidelity") or not a.get("functions"):
        raise ValueError("missing faithful nonempty control")
    for key in ("unit", "compiler", "flags", "sections", "relocations", "functions"):
        if a[key] != b[key]:
            raise ValueError("unexpected whole-TU change: " + key)
    allowed = {"shadow_color": (8, 0), "gLineSpacing": (0, 8)}
    normalized = []
    for symbol in b["symbols"]:
        symbol = dict(symbol)
        if symbol["name"] in allowed and symbol["section"] == ".sbss":
            old, new = allowed[symbol["name"]]
            original = [s for s in a["symbols"] if s["name"] == symbol["name"] and s["section"] == ".sbss"]
            if len(original) != 1 or original[0]["value"] != old or symbol["value"] != new:
                raise ValueError("unexpected endpoint movement")
            symbol["value"] = old
        normalized.append(symbol)
    key = lambda s: json.dumps(s, sort_keys=True)
    if sorted(a["symbols"], key=key) != sorted(normalized, key=key):
        raise ValueError("unexpected symbol change")
    for key in ("functions", "sections", "exception_records"):
        if before["postprocessed"][key] != after["postprocessed"][key]:
            raise ValueError("postprocessed change: " + key)
    post_symbols = {n: dict(s) for n, s in after["postprocessed"]["symbols"].items()}
    for name, (old, new) in allowed.items():
        if (post_symbols[name]["section"] != ".sbss" or post_symbols[name]["offset"] != new
                or before["postprocessed"]["symbols"][name]["offset"] != old):
            raise ValueError("unexpected postprocessed endpoint movement")
        post_symbols[name]["offset"] = old
    if post_symbols != before["postprocessed"]["symbols"]:
        raise ValueError("unexpected postprocessed symbol change")
    if before["raw_eh"] != after["raw_eh"]:
        raise ValueError("raw exception metadata changed")
    return dict(status="PASS", functions=len(a["functions"]), raw_bodies_and_relocations_unchanged=True,
                initialized_and_nobits_sections_unchanged=True, exception_records_unchanged=True,
                allowed_symbol_offsets=allowed)


def measure():
    snapshot = capture(UNIT)
    raw = object_inventory(ROOT / snapshot["raw_object"])
    post = object_inventory(ROOT / f"build/GUNE5D/src/{UNIT}.o")
    target_path = f"build/GUNE5D/obj/{UNIT}.o"
    target = object_inventory(ROOT / target_path)
    table = fndiff.symbol_table()
    retail = datadiff.dol_read(RUNS[".sdata"][0], 12)
    if retail is None or len(retail) != 12:
        raise ValueError("retail initialized range unavailable")
    obligations = [section_obligation(snapshot, name, table, retail) for name in RUNS]
    graph = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
    if graph.get("schema_version") != 1 or graph.get("ninja_sha256") != hashlib.sha256((ROOT / "build.ninja").read_bytes()).hexdigest():
        raise ValueError("stale or unsupported build graph")
    links = [e for e in graph["edges"] if e["rule"] == "link"]
    if len(links) != 1 or links[0]["inputs"].count(target_path) != 1:
        raise ValueError("expected one current NonMatching fallback")
    inputs = links[0]["inputs"]
    owners = current_owners(inputs, NAMES)
    if any(len(v) != 1 for v in owners.values()):
        raise ValueError("current link lacks unique small-global owners")
    substituted = [f"build/GUNE5D/src/{UNIT}.o" if p == target_path else p for p in inputs]
    alternate = current_owners(substituted, NAMES)
    claims = datadiff.parse_splits()[UNIT + ".c"]
    for name, (base, _) in RUNS.items():
        if name not in claims:
            continue
        if claims[name] != (base, base + 12):
            raise ValueError("claim includes foreign state or incorrect extent")
        section = dict(target["sections"][name])
        relocs = section.pop("relocations")
        extracted = dict(sections={name: section}, relocations={name: relocs}, symbols=[
            dict(name=n, section=s["section"], type=1, value=s["offset"], size=s["size"],
                 binding=s["binding"], other=0) for n, s in target["symbols"].items()])
        if section_obligation(extracted, name, table, retail)["status"] != "EXACT":
            raise ValueError("extracted claim does not reproduce target allocation")
        if any(len(alternate[n]) != 1 or alternate[n][0]["object"] != f"build/GUNE5D/src/{UNIT}.o"
               for n in RUNS[name][1]):
            raise ValueError("claimed source substitution lacks unique compiled owner")
    refs = {n: [dict(function=fn, offset=off, relocation_type=typ, addend=add)
                for fn, f in target["functions"].items()
                for off, typ, sym, add in f["relocations"] if sym == n] for n in NAMES}
    if any(not v for v in refs.values()):
        raise ValueError("target function references missing for small global")
    return dict(schema_version=1, status="MEASURED", snapshot=snapshot, obligations=obligations,
                large_bss=large_bss_obligation(snapshot, table), raw_eh=raw["exception_records"],
                postprocessed=post, target_eh=compare_exception_records(target["exception_records"], raw["exception_records"]),
                target_references=refs, claims=claims, current_owners=owners,
                source_substituted_owners=alternate,
                source_substituted_duplicates=[n for n, v in alternate.items() if len(v) > 1],
                boundary="Named datum ownership only; not a whole-TU match or executed source-link certificate")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--require-exact", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / "build") or not out.name.startswith("r69_btext_") or out.suffix != ".json":
        parser.error("output must name build/**/r69_btext_*.json")
    result = dict(status="UNRESOLVED")
    try:
        # Canonicalize tuple-valued ELF relocation rows before comparing with
        # a persisted JSON snapshot (JSON has arrays, not Python tuples).
        result = json.loads(json.dumps(measure()))
        if args.before:
            result["preservation"] = preservation(json.loads(args.before.read_text()), result)
        if args.require_exact and any(r["status"] != "EXACT" for r in result["obligations"]):
            raise ValueError("small-global allocation not exact")
    except (OSError, ValueError, KeyError) as error:
        result.update(status="UNRESOLVED", error=str(error))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("error", [(r["section"], r["status"]) for r in result.get("obligations", [])]), out)
    return 0 if result["status"] == "MEASURED" else 2


if __name__ == "__main__":
    raise SystemExit(main())
