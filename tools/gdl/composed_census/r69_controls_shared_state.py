"""Adjudicate controls' public/private CTL homes and diagnostic target-shape changes.

Reads a faithful declaration-probe report and checks every saved object hash.
Diff scores are diagnostic, not a byte/datum equivalence certificate. No
production source/object is changed, no source-link result is inferred.
"""
import argparse
import difflib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, current_owners, digest
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl import fndiff, datadiff

UNIT, CTL = "game/game/controls", "PlayerControl"


def check_homes(local, public, table):
    if tuple(table[CTL][:3]) != (".bss", 0x80240E30, 240):
        raise ValueError("target CTL identity/extent changed")
    if (local["section"], local["offset"], local["size"], local["binding"], local["bytes"]) != (".bss", 1656, 240, 0, None):
        raise ValueError("expected current private pooled CTL allocation")
    if len(public) != 1 or public[0]["section"] != ".bss" or public[0]["size"] != 240:
        raise ValueError("expected unique current extracted public CTL allocation")


def score(target, ours):
    rows = [s for s in difflib.unified_diff(target, ours, n=0, lineterm="")
            if s[:1] in ("+", "-") and s[:3] not in ("+++", "---")]
    return dict(target_instructions=len(fndiff.instruction_lines(target)),
                ours_instructions=len(fndiff.instruction_lines(ours)),
                nonreloc_diff_rows=fndiff.count_real(rows),
                classification=fndiff.classify_function(target, ours))


def measure(probe):
    if probe.get("status") != "MEASURED" or probe.get("fidelity") is not True:
        raise ValueError("requires faithful measured probe")
    folder = (ROOT / probe["artifacts"]).resolve()
    if not folder.is_relative_to(ROOT / "build"):
        raise ValueError("saved experiment outside build")
    edge = cv.read_edges()[UNIT]
    baseline = probe["variants"]["baseline"]
    if (digest((ROOT / edge["body_o"]).read_bytes()) != baseline["raw_sha256"] or
            digest((ROOT / edge["src"]).read_bytes()) != baseline["source_sha256"]):
        raise ValueError("active raw/source no longer matches probe baseline")
    target = fndiff.parse(ROOT / f"build/GUNE5D/obj/{UNIT}.o")
    scores = {}
    names = ("baseline", "global_reverse_and_ctl_export", "global_reverse_and_bss_all_export")
    parsed = {}
    for label in names:
        path = folder / (label + ".o")
        if digest(path.read_bytes()) != probe["variants"][label]["raw_sha256"]:
            raise ValueError("saved diagnostic object drift: " + label)
        parsed[label] = fndiff.parse(path)
        if label != "baseline" and parsed[label].keys() != parsed["baseline"].keys():
            raise ValueError("diagnostic function roster changed")
        scores[label] = {name: score(t, parsed[label][name]) for name, t in target.items() if name in parsed[label]}
    deltas = {}
    for label in names[1:]:
        deltas[label] = {"improved": [], "worsened": [], "equal": []}
        for name, s in scores[label].items():
            old = scores["baseline"][name]
            delta = s["nonreloc_diff_rows"]-old["nonreloc_diff_rows"]
            deltas[label]["improved" if delta < 0 else "worsened" if delta > 0 else "equal"].append(
                dict(function=name, before=old, after=s))
    target_consumers = []
    for unit in datadiff.parse_splits():
        if not unit.startswith("game/"):
            continue
        object_path = ROOT / "build/GUNE5D/obj" / Path(unit).with_suffix(".o")
        if not object_path.exists():
            raise ValueError("configured game target object unavailable: " + unit)
        # This census asks only which functions carry a named relocation.
        # It neither needs nor certifies arbitrary foreign exception payloads.
        functions, _ = inventory(object_path)
        refs = {fn: [r for r in f["relocations"] if r[2] == CTL]
                for fn, f in functions.items()}
        refs = {fn: r for fn, r in refs.items() if r}
        if refs:
            target_consumers.append(dict(unit=unit, functions=refs))
    graph = json.loads((ROOT / "build/GUNE5D/build_edges.json").read_text())
    if graph.get("schema_version") != 1 or graph.get("ninja_sha256") != digest((ROOT / "build.ninja").read_bytes()):
        raise ValueError("stale actual link graph")
    links = [e for e in graph["edges"] if e["rule"] == "link"]
    if len(links) != 1:
        raise ValueError("expected one actual link")
    extracted = current_owners(links[0]["inputs"], [CTL])[CTL]
    local = object_inventory(ROOT / edge["body_o"])["symbols"][CTL]
    check_homes(local, extracted, fndiff.symbol_table())
    return dict(status="MEASURED", source_private_home=local, target_public_home=extracted[0],
                target_consumers=target_consumers, target_scores=scores, target_score_deltas=deltas,
                clear_player_control_streams={"target": target["ClearPlayerControl"],
                    **{n: p["ClearPlayerControl"] for n, p in parsed.items()}},
                boundary="No visibility-originality, full-source link, runtime execution or compiler-unreachability assertion")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / "build") or not out.name.startswith("r69_controls_"):
        parser.error("output must name build/r69_controls_*.json")
    try:
        result = measure(json.loads(args.probe.read_text()))
    except (OSError, ValueError, KeyError) as error:
        result = dict(status="UNRESOLVED", error=str(error))
    out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(result["status"], result.get("error", "shared-state/target-shape evidence completed"), out)
    return 0 if result["status"] == "MEASURED" else 2


if __name__ == "__main__":
    raise SystemExit(main())
