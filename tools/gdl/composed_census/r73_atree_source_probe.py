"""Finite scratch-only atree source retirement experiments; PASS is not exhaustion."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory
from tools.gdl.composed_census.r71_pad_identity_probe import differences, words, replace_once
from tools.gdl import fndiff

UNIT = "game/anim/atree"
PINS = ("DoAnimateTreeFrame", "fn_8001267C", "AtreeInitSub", "fn_80011BBC")


def sha(value):
    return hashlib.sha256(value).hexdigest()


def canonical_relocations(inv, rows, eh_index=False):
    """Anonymous names may change only at the SAME private initialized location.

    This is layout-preservation evidence, not arbitrary equal-value semantics.
    Section payload equality is checked separately by finalized_comparison.
    """
    result = []
    for offset, kind, name, addend in rows:
        identity = name
        if name.startswith("@"):
            symbol = inv["symbols"].get(name)
            if (not symbol or symbol["binding"] != 0
                    or symbol["section"] not in (("extab",) if eh_index else (".rodata", ".sdata2"))
                    or symbol["bytes"] is None
                    or inv["sections"][symbol["section"]]["relocations"]
                    or not 0 <= addend < symbol["size"]):
                raise ValueError("unreviewed anonymous relocation: " + name)
            identity = (symbol["section"], symbol["offset"], symbol["size"], symbol["bytes"])
        result.append((offset, kind, identity, addend))
    return result


def finalized_comparison(before, after):
    changes = differences(before, after)
    if changes["missing"] or changes["extra"] or not before["functions"]:
        raise ValueError("nonempty exact function roster required")
    relocs_equal = all(canonical_relocations(before, before["functions"][n]["relocations"])
                       == canonical_relocations(after, after["functions"][n]["relocations"])
                       for n in before["functions"])
    if set(before["sections"]) != set(after["sections"]):
        raise ValueError("allocated section roster differs")
    section_changes = []
    for name, section in before["sections"].items():
        a, b = dict(section), dict(after["sections"][name])
        a["relocations"] = canonical_relocations(before, a["relocations"], name == "extabindex")
        b["relocations"] = canonical_relocations(after, b["relocations"], name == "extabindex")
        if a != b:
            section_changes.append(name)
    exports = ("sAtreeZero", "sAtreeFrameRoundBias", "sAtreeDummyName", "natreelists",
               "atree_handles", "atree_scroll", "whichatree")
    export_changes = [n for n in exports if before["symbols"].get(n) != after["symbols"].get(n)]
    named_before = {n:s for n,s in before["symbols"].items() if not n.startswith("@")}
    named_after = {n:s for n,s in after["symbols"].items() if not n.startswith("@")}
    checks = dict(roster=not changes["missing"] and not changes["extra"] and bool(before["functions"]),
                  bodies=not changes["body"], geometry=not changes["offset"] and not changes["size"],
                  function_visibility=not changes["binding"], relocation_identities=relocs_equal,
                  allocated_sections=not section_changes, named_exports=not export_changes,
                  all_named_objects=named_before == named_after,
                  exception_records=before["exception_records"] == after["exception_records"])
    return dict(status="PASS" if all(checks.values()) else "FAIL", checks=checks,
                function_count=len(before["functions"]), changes=changes,
                section_changes=section_changes, export_changes=export_changes)


def source_export_obligation(raw, target):
    """Certify the two source-owned exports, not every exporter or text rule."""
    for name, section, size in (("sAtreeZero", ".sdata2", 4), ("natreelists", ".sbss", 4)):
        a, b = raw["symbols"].get(name), target["symbols"].get(name)
        if (not a or not b or a["binding"] != 1 or a["section"] != section or b["section"] != section
                or a["size"] != size or a["offset"] != b["offset"]
                or b["size"] != size or a["bytes"] != b["bytes"]):
            raise ValueError("source export differs: " + name)
    pool, retail = raw["sections"][".sdata2"], target["sections"][".sdata2"]
    if (pool["size"], retail["size"], pool["alignment"], retail["alignment"],
            pool["relocations"], retail["relocations"]) != (22, 24, 8, 8, [], []):
        raise ValueError("reviewed pool extent/alignment differs")
    if pool["bytes"] + "0000" != retail["bytes"]:
        raise ValueError("pool bytes or terminal zero slack differ")
    anonymous_zero = [n for n,s in raw["symbols"].items() if n.startswith("@")
        and s["section"] == ".sdata2" and s["size"] == 4 and s["bytes"] == "00000000"]
    if anonymous_zero:
        raise ValueError("anonymous zero alias dependence remains")
    bss, retail_bss = raw["sections"][".sbss"], target["sections"][".sbss"]
    for key in ("size", "alignment", "type", "bytes", "relocations"):
        if bss[key] != retail_bss[key]:
            raise ValueError("small BSS differs: " + key)
    return dict(status="PASS", source_exports={n:raw["symbols"][n] for n in ("sAtreeZero", "natreelists")},
                source_pool_bytes=22, target_pool_claim_bytes=24, terminal_zero_slack=2,
                remaining_promotions=["atree_handles", "atree_scroll", "whichatree"],
                text_rules_retired=0)


def forms(source):
    literal = "result = AnimateTreeFrame(0.0f, info, sequence, frame, frame);"
    named = "result = AnimateTreeFrame(sAtreeZero, info, sequence, frame, frame);"
    marker = "const f64 sAtreeFrameRoundBias = 0.5;"
    original = source
    if "const f32 sAtreeZero = 0.0f;" in source:
        original = replace_once(original, "const f32 sAtreeZero = 0.0f;\n", "")
        original = replace_once(original, named, literal)
        original = replace_once(original, "\ns32 natreelists;", "\nstatic s32 natreelists;")
    late = replace_once(original, marker, "const f32 sAtreeZero = 0.0f;\n" + marker)
    yield "scratch_control", source
    yield "legacy_anonymous_control", original
    yield "late_zero_literal_held", late
    yield "late_zero_named", replace_once(late, literal, named)
    yield "public_natreelists", replace_once(original, "static s32 natreelists;", "s32 natreelists;")
    yield "late_zero_public_natreelists", replace_once(replace_once(late, literal, named),
        "static s32 natreelists;", "s32 natreelists;")
    yield "existing_inline_helper", replace_once(original,
        "DoSeqTexModsInPlace(obj, frame, seq);", "DoSeqTexMods(obj, frame, seq);")


def run(selected):
    edge = cv.read_edges()[UNIT]
    if edge["mw"] != "GC/1.2.5" or edge["rule"] != "mwcc_sjis":
        raise ValueError("reviewed compiler/runner changed")
    paths = [ROOT / edge["src"], ROOT / edge["body_o"],
             ROOT / f"build/GUNE5D/obj/{UNIT}.o", ROOT / f"build/GUNE5D/src/{UNIT}.o",
             ROOT / "config/GUNE5D/webfrank.json", ROOT / "build.ninja",
             ROOT / "build/compilers/GC/1.2.5/mwcceppc.exe"]
    protected = {p: p.read_bytes() for p in paths}
    source = protected[paths[0]].decode("utf-8")
    variants = list(forms(source))
    if selected - {name for name, _ in variants}:
        raise ValueError("unknown form: " + repr(selected))
    folder = Path(tempfile.mkdtemp(prefix="r73_atree_", dir=ROOT / "build"))
    for name, path in zip(("baseline.c", "baseline_raw.o", "target.o", "baseline_processed.o"), paths):
        (folder / name).write_bytes(protected[path])
    trace = dict(edge, _command_trace=[])
    control, error = cv.compile_with(trace, edge["mw"], edge["cflags"], folder / "actual_control.o", folder)
    if error or control.read_bytes() != protected[paths[1]]:
        raise ValueError("actual-edge whole-object fidelity: " + str(error))
    baseline, target = object_inventory(paths[1]), object_inventory(paths[2])
    processed = object_inventory(paths[3])
    target_lines = fndiff.parse(paths[2])
    report = dict(schema_version=1, status="PASS", unit=UNIT, edge=edge,
                  protected={str(p.relative_to(ROOT)): sha(v) for p, v in protected.items()},
                  command=trace["_command_trace"], baseline=baseline, target=target,
                  processed=processed, variants=[])
    for name, text in variants:
        if selected and name not in selected and name != "scratch_control":
            continue
        trial_dir = folder / name
        trial_dir.mkdir()
        candidate = trial_dir / "atree.c"
        candidate.write_text(text, encoding="utf-8", newline="\n")
        trial = dict(edge, src=str(candidate.relative_to(ROOT)), _command_trace=[])
        obj, error = cv.compile_with(trial, edge["mw"], edge["cflags"], trial_dir / "atree.o", folder)
        row = dict(name=name, source=str(candidate.relative_to(ROOT)), source_sha256=sha(candidate.read_bytes()),
                   commands=trial["_command_trace"], error=error)
        if error:
            raise ValueError(error)
        inv = object_inventory(obj)
        lines = fndiff.parse(obj)
        row.update(inventory=inv, function_changes=differences(baseline, inv),
                   object=str(obj.relative_to(ROOT)), entire_raw_equal=obj.read_bytes() == protected[paths[1]],
                   object_sha256=sha(obj.read_bytes()), pins={},
                   sdata2_extent=inv["sections"][".sdata2"]["size"],
                   sdata2_prefix_exact=(inv["sections"][".sdata2"]["size"] == 22
                     and target["sections"][".sdata2"]["size"] == 24
                     and inv["sections"][".sdata2"]["bytes"] + "0000" == target["sections"][".sdata2"]["bytes"]),
                   eh_equal=baseline["exception_records"] == inv["exception_records"])
        for fn in PINS:
            diff = [x for x in difflib.unified_diff(target_lines[fn], lines[fn], lineterm="", n=0)
                    if x[:1] in "+-" and not x.startswith(("+++", "---"))]
            row["pins"][fn] = dict(insns=inv["functions"][fn]["size"]//4,
                target_insns=target["functions"][fn]["size"]//4, real=fndiff.count_real(diff),
                words=words(target["functions"][fn]["body"], inv["functions"][fn]["body"]),
                frame=fndiff.frame_size(lines[fn]), disassembly=lines[fn])
        if name == "scratch_control" and not row["entire_raw_equal"]:
            raise ValueError("scratch-source whole-object fidelity failed")
        report["variants"].append(row)
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(name, "bodies", row["function_changes"]["body"], "sdata2 prefix exact", row["sdata2_prefix_exact"],
              "EH", row["eh_equal"], "pins", {n:(r["insns"], r["real"], len(r["words"])) for n,r in row["pins"].items()}, flush=True)
    for path, data in protected.items():
        if path.read_bytes() != data:
            raise ValueError("protected file changed: " + str(path))
    report["protected_unchanged"] = True
    (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Report:", folder / "report.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--forms", default="late_zero_literal_held,late_zero_named")
    parser.add_argument("--compare-finalized", nargs=2, type=Path)
    parser.add_argument("--source-exports", nargs=2, type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if args.compare_finalized or args.source_exports:
        inputs = args.compare_finalized or args.source_exports
        function = finalized_comparison if args.compare_finalized else source_export_obligation
        result = function(*(object_inventory(p) for p in inputs))
        if args.out:
            args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        raise SystemExit(0 if result["status"] == "PASS" else 1)
    else:
        run(set(args.forms.split(",")))
