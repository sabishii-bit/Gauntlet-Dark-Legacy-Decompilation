"""Controls small-BSS reconstruction evidence, with the large-BSS boundary open.

Fresh actual-Ninja raw control, complete scalar/array allocation, current
ordered-link names and TU-wide preservation are separate obligations. A
substituted ownership inventory is not an executed source link or TU match.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, current_owners
from tools.gdl.composed_census.r69_controls_layout_probe import layout
from tools.gdl import datadiff, fndiff
from tools.gdl.exception_metadata import compare_exception_records

UNIT = "game/game/controls"
BASE, END = 0x803445D8, 0x80344628
OBJECTS = tuple(("ctrls_initialized" if a == 0x803445F0 else f"lbl_{a:08X}", a,
                 8 if a in (0x80344608, 0x80344610) else 4)
                for a in (0x803445D8, 0x803445DC, 0x803445E0, 0x803445E4, 0x803445E8,
                          0x803445EC, 0x803445F0, 0x803445F4, 0x803445F8, 0x803445FC,
                          0x80344600, 0x80344604, 0x80344608, 0x80344610, 0x80344618,
                          0x8034461C, 0x80344620))


def allocation(snapshot, table, extracted=False):
    section = snapshot["sections"].get(".sbss")
    extent = 80 if extracted else 76
    if section != dict(type=8, flags=3, size=extent, alignment=8, bytes=None):
        raise ValueError("unexpected complete small-BSS allocation")
    if snapshot["relocations"].get(".sbss"):
        raise ValueError("BSS relocation cannot be ignored")
    symbols = [s for s in snapshot["symbols"] if s["section"] == ".sbss" and s["type"] == 1]
    if sorted(s["name"] for s in symbols) != sorted(n for n, _, _ in OBJECTS):
        raise ValueError("small-BSS roster has missing, duplicate or extra object")
    rows = []
    for name, address, scalar_size in OBJECTS:
        target_size = 8 if name == "lbl_80344620" else scalar_size
        if tuple(table[name][:3]) != (".sbss", address, target_size):
            raise ValueError("target allocation identity/extent differs: " + name)
        symbol = next(s for s in symbols if s["name"] == name)
        size = target_size if extracted else scalar_size
        if (symbol["value"], symbol["size"], symbol["binding"], symbol["other"]) != (address-BASE, size, 1, 0):
            raise ValueError("compiled allocation offset/size/linkage differs: " + name)
        rows.append(dict(name=name, offset=address-BASE, size=size, target=hex(address), target_size=target_size))
    return dict(status="EXACT", source_extent=76, target_extent=80, terminal_alignment_extent=4,
                alignment=8, NOBITS=True, initialized_bytes=None, objects=rows)


def preservation(before, after):
    a, b = before["snapshot"], after["snapshot"]
    if not a.get("fidelity") or not b.get("fidelity") or not a.get("functions"):
        raise ValueError("missing faithful nonempty compiler control")
    for key in ("unit", "compiler", "flags", "functions", "relocations"):
        if a[key] != b[key]:
            raise ValueError("unexpected raw TU change: " + key)
    for key in a["sections"].keys() | b["sections"].keys():
        if key != ".sbss" and a["sections"].get(key) != b["sections"].get(key):
            raise ValueError("changed other allocated section: " + key)
    for section, size in ((a["sections"][".sbss"], 80), (b["sections"][".sbss"], 76)):
        if section != dict(type=8, flags=3, size=size, alignment=8, bytes=None):
            raise ValueError("unexpected BSS section transition")
    names = {n for n, _, _ in OBJECTS} | {"lbl_80344624"}
    allowed = lambda s: s["section"] == ".sbss" and s["type"] == 1 and s["name"] in names
    canonical = lambda seq: sorted(json.dumps(s, sort_keys=True) for s in seq if not allowed(s))
    if canonical(a["symbols"]) != canonical(b["symbols"]):
        raise ValueError("unexpected unrelated raw symbol change")
    if len([s for s in a["symbols"] if allowed(s)]) != 18:
        raise ValueError("unexpected baseline small-object roster")
    expected_names = {s["name"] for s in a["symbols"] if allowed(s)}
    if expected_names != names:
        raise ValueError("unexpected baseline small-object names")
    for key in ("functions", "exception_records"):
        if before["postprocessed"][key] != after["postprocessed"][key]:
            raise ValueError("postprocessed change: " + key)
    for key in before["postprocessed"]["sections"].keys() | after["postprocessed"]["sections"].keys():
        x, y = before["postprocessed"]["sections"].get(key), after["postprocessed"]["sections"].get(key)
        if key == ".sbss":
            if dict(x, size=76) != y:
                raise ValueError("unexpected postprocessed BSS transition")
        elif x != y:
            raise ValueError("postprocessed allocated section changed")
    old_symbols = {n: s for n, s in before["postprocessed"]["symbols"].items() if n not in names}
    new_symbols = {n: s for n, s in after["postprocessed"]["symbols"].items() if n not in names}
    if old_symbols != new_symbols or before["raw_eh"] != after["raw_eh"]:
        raise ValueError("other symbols or raw EH changed")
    return dict(status="PASS", functions=len(a["functions"]), raw_and_postprocessed_bodies_relocations_unchanged=True,
                other_allocations_symbols_and_eh_unchanged=True, removed_unused_nominal_pad="lbl_80344624")


def measure():
    snap = capture(UNIT)
    raw = object_inventory(ROOT / snap["raw_object"])
    post = object_inventory(ROOT / f"build/GUNE5D/src/{UNIT}.o")
    target_path = f"build/GUNE5D/obj/{UNIT}.o"
    target = object_inventory(ROOT / target_path)
    table = fndiff.symbol_table()
    result = dict(snapshot=snap, raw_eh=raw["exception_records"], postprocessed=post,
                  large_bss_layout=layout(raw, ".bss", table), small_bss_layout=layout(raw, ".sbss", table),
                  target_eh=compare_exception_records(target["exception_records"], raw["exception_records"]),
                  large_bss_boundary="Local arrays preserve raw pooling but at least public CTL is separately extracted; no large-BSS ownership closure claimed")
    try:
        result["small_bss_allocation"] = allocation(snap, table)
    except ValueError as error:
        result["small_bss_allocation"] = dict(status="OPEN", reason=str(error))
    graph = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
    if graph.get("schema_version") != 1 or graph.get("ninja_sha256") != hashlib.sha256((ROOT / "build.ninja").read_bytes()).hexdigest():
        raise ValueError("stale or unsupported actual build graph")
    links = [e for e in graph["edges"] if e["rule"] == "link"]
    if len(links) != 1 or links[0]["inputs"].count(target_path) != 1:
        raise ValueError("expected one controls NonMatching fallback")
    names = [n for n, _, _ in OBJECTS]
    inputs = links[0]["inputs"]
    owners = current_owners(inputs, names)
    if any(len(o) != 1 for o in owners.values()):
        raise ValueError("default link lacks unique small-state owner")
    alternate = current_owners([f"build/GUNE5D/src/{UNIT}.o" if p == target_path else p for p in inputs], names)
    claims = datadiff.parse_splits()[UNIT+".c"]
    if ".sbss" in claims:
        if claims[".sbss"] != (BASE, END):
            raise ValueError("unexpected target small-state claim")
        section = dict(target["sections"][".sbss"])
        relocs = section.pop("relocations")
        target_snap = dict(sections={".sbss": section}, relocations={".sbss": relocs}, symbols=[
            dict(name=n, section=s["section"], value=s["offset"], size=s["size"], binding=s["binding"],
                 type=1, other=0) for n, s in target["symbols"].items()])
        result["extracted_allocation"] = allocation(target_snap, table, extracted=True)
        if any(len(o) != 1 or o[0]["object"] != f"build/GUNE5D/src/{UNIT}.o" for o in alternate.values()):
            raise ValueError("claimed small-state substitution lacks one compiled owner")
    result.update(status="MEASURED", current_owners=owners, source_substituted_owners=alternate,
                  source_substituted_duplicates=[n for n, o in alternate.items() if len(o) > 1], claims=claims)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--require-exact", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / "build") or not out.name.startswith("r69_controls_"):
        parser.error("output must name build/r69_controls_*.json")
    result = dict(status="UNRESOLVED")
    try:
        result = json.loads(json.dumps(measure()))
        if args.before:
            result["preservation"] = preservation(json.loads(args.before.read_text()), result)
        if args.require_exact and result["small_bss_allocation"]["status"] != "EXACT":
            raise ValueError("small-state allocation remains open")
    except (OSError, ValueError, KeyError) as error:
        result.update(status="UNRESOLVED", error=str(error))
    out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(result["status"], result.get("error", result.get("small_bss_allocation", {}).get("status")), out)
    return 0 if result["status"] == "MEASURED" else 2


if __name__ == "__main__":
    raise SystemExit(main())
