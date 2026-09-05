"""Check btext's complete typed BSS run and its actual link owners.

BSS has no stored DOL bytes. This audit requires exact named target addresses,
sizes, source offsets, alignment, complete section coverage and no relocations.
The fresh Ninja compiler control covers the entire TU, not just the BSS run.
PASS is an ownership measurement, not source originality or a TU flip.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools/gdl"))
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r68_aux_ownership_audit import current_owners, object_inventory
from tools.gdl import fndiff, datadiff

UNIT = "game/ui/btext"
BASE = 0x8023EAE0
END = 0x8023F7E8
OBJECTS = (("font_info", 0x38), ("gTextWorkBuf", 0x800),
           ("gScrollMsgList", 0x88), ("gStringMsgList", 0x44),
           ("gTextFormatBuf", 0x404))


def obligation(snapshot, target_symbols):
    section = snapshot["sections"].get(".bss")
    if section != dict(type=8, flags=3, size=END-BASE, alignment=8, bytes=None):
        raise ValueError("unexpected complete BSS section shape")
    if snapshot["relocations"].get(".bss"):
        raise ValueError("unexpected BSS relocation")
    symbols = [s for s in snapshot["symbols"] if s["section"] == ".bss" and s["type"] == 1]
    if sorted(s["name"] for s in symbols) != sorted(n for n, _ in OBJECTS):
        raise ValueError("BSS has missing, duplicate or additional objects")
    rows, offset = [], 0
    for name, size in OBJECTS:
        symbol = next(s for s in symbols if s["name"] == name)
        expected = (".bss", BASE + offset, size)
        if tuple(target_symbols[name][:3]) != expected:
            raise ValueError("target address/size mismatch: " + name)
        if (symbol["value"], symbol["size"], symbol["binding"]) != (offset, size, 1):
            raise ValueError("source offset/size/binding mismatch: " + name)
        rows.append(dict(name=name, offset=offset, size=size, target=hex(BASE + offset)))
        offset += size
    if offset != END-BASE:
        raise ValueError("BSS coverage incomplete")
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / "build") or output.suffix != ".json":
        parser.error("output must be JSON under build")
    result = dict(schema_version=1, status="UNRESOLVED")
    try:
        snapshot = capture(UNIT)
        result.update(raw_sha256=snapshot["raw_sha256"], source_sha256=snapshot["source_sha256"],
                      fidelity=snapshot["fidelity"], functions=len(snapshot["functions"]),
                      objects=obligation(snapshot, fndiff.symbol_table()),
                      base=hex(BASE), end=hex(END), extent=END-BASE,
                      claim=datadiff.parse_splits()[UNIT+".c"].get(".bss"))
        graph = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
        if graph.get("schema_version") != 1 or graph.get("ninja_sha256") != hashlib.sha256((ROOT / "build.ninja").read_bytes()).hexdigest():
            raise ValueError("stale or unsupported graph snapshot")
        links = [e for e in graph["edges"] if e["rule"] == "link"]
        if len(links) != 1:
            raise ValueError("expected one actual linker edge")
        inputs = links[0]["inputs"]
        target = f"build/GUNE5D/obj/{UNIT}.o"
        if inputs.count(target) != 1:
            raise ValueError("expected one NonMatching btext fallback")
        names = [n for n, _ in OBJECTS]
        owners = current_owners(inputs, names)
        if any(len(o) != 1 for o in owners.values()):
            raise ValueError("default link lacks exactly one owner per datum")
        source_inputs = [f"build/GUNE5D/src/{UNIT}.o" if p == target else p for p in inputs]
        alternate = current_owners(source_inputs, names)
        if result["claim"] is not None:
            if result["claim"] != (BASE, END):
                raise ValueError("claimed BSS boundaries differ from measured run")
            extracted = object_inventory(ROOT / target)
            section = dict(extracted["sections"][".bss"])
            relocations = section.pop("relocations")
            target_snapshot = dict(sections={".bss": section}, relocations={".bss": relocations},
                symbols=[dict(name=n, section=s["section"], type=1, value=s["offset"],
                              size=s["size"], binding=s["binding"]) for n, s in extracted["symbols"].items()])
            result["claimed_objects"] = obligation(target_snapshot, fndiff.symbol_table())
            if any(len(o) != 1 for o in alternate.values()):
                raise ValueError("claimed BSS lacks exactly one source-substituted owner")
        result.update(current_link_owners=owners, source_substituted_owners=alternate,
                      source_substituted_duplicate_names=[n for n, o in alternate.items() if len(o) > 1],
                      status="PASS")
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(result["status"], result.get("error", f"complete {END-BASE}-byte BSS run"), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
