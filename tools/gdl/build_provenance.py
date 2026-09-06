"""Build-input provenance, not an exactness or semantic proof.

The generator records the same edges it emits to Ninja. This reader binds that
snapshot to build.ninja, asks Ninja for expanded commands and discovered
dependencies, then hashes the current artifacts. It never builds or patches.
IMPORTABLE CORE: report_accounting, compiler_class, parse_ninja_deps -- pure over parsed data.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools import ninja_syntax

SCHEMA_VERSION = 1
COMPILE_RULES = {"mwcc", "mwcc_sjis", "mwcc_extab", "mwcc_sjis_extab",
                 "mwcc_pch", "mwcc_pch_sjis"}
TRANSFORM_RULES = {"webfrank", "webfrank_globalize_atree", "p6frank", "frank",
                   "globalize_atree", "fix_exception_object"}


def key(path):
    return str(path).replace("\\", "/")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class RecordingWriter(ninja_syntax.Writer):
    """Transparent recorder; serialization still belongs to Ninja's writer."""
    def __init__(self, output):
        super().__init__(output)
        self.edges = []
        self.rules = {}
        self.units = []

    def rule(self, name, command, **kwargs):
        self.rules[name] = {"command_template": command}
        return super().rule(name, command, **kwargs)

    def build(self, outputs, rule, inputs=None, **kwargs):
        row = {"rule": rule}
        serialized = {}
        for name, values in (("outputs", outputs), ("inputs", inputs),
                             ("implicit", kwargs.get("implicit")),
                             ("order_only", kwargs.get("order_only")),
                             ("implicit_outputs", kwargs.get("implicit_outputs"))):
            serialized[name] = ninja_syntax.serialize_paths(values)
            row[name] = [key(p) for p in serialized[name]]
        row["variables"] = {k: " ".join(ninja_syntax.serialize_paths(v))
                            for k, v in dict(kwargs.get("variables") or {}).items()}
        self.edges.append(row)
        kwargs.update({name: serialized[name] for name in
                       ("implicit", "order_only", "implicit_outputs")})
        kwargs["variables"] = row["variables"]
        return super().build(serialized["outputs"], rule, serialized["inputs"], **kwargs)

    def write_snapshot(self, path, *, version, non_matching, compilers,
                       linker_version, ninja_path):
        result = {"schema_version": SCHEMA_VERSION, "version": version,
                  "non_matching": non_matching, "compilers": key(compilers),
                  "linker_version": linker_version, "ninja": key(ninja_path),
                  "ninja_sha256": sha256("build.ninja"), "rules": self.rules,
                  "edges": self.edges, "units": self.units,
                  "generator_inputs": {key(p): sha256(p) for p in
                      ["configure.py", "tools/project.py", "tools/gdl/build_provenance.py",
                       f"config/{version}/webfrank.json", f"config/{version}/p6frank.json",
                       f"config/{version}/config.yml", f"config/{version}/splits.txt",
                       f"config/{version}/symbols.txt"]}}
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        Path(path).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


def compiler_class(version):
    version = key(version)
    if version in {"GC/1.2.5n", "GC/1.2.5s", "GC/1.2.5e"}:
        return "derived_configured"
    if version in {"GC/1.2.5", "GC/1.1p1"}:
        return "stock_configured"
    return "unknown"


def parse_ninja_deps(text):
    """Parse Ninja's own deps listing; absence and stale state stay visible."""
    rows = {}
    current = None
    for line in text.splitlines():
        match = re.match(r"^(.*): #deps (\d+), deps mtime .* \(([^)]+)\)$", line)
        if match:
            current = {"paths": [], "declared_count": int(match[2]), "state": match[3]}
            rows[key(match[1])] = current
        elif line.startswith("    ") and current is not None:
            current["paths"].append(key(line[4:]))
        elif line:
            current = None
    return rows


def _manual(rule):
    if isinstance(rule, dict):
        return bool(rule.get("unproven_recolor_audit")) or any(_manual(v) for v in rule.values())
    if isinstance(rule, list):
        return any(_manual(v) for v in rule)
    return False


def postprocessor_functions(unit, edges, webfrank, p6frank):
    result = {}
    rules = {edge["rule"] for edge in edges}
    if rules & {"webfrank", "webfrank_globalize_atree"}:
        if unit not in webfrank.get("units", {}):
            raise ValueError("WebFrank build edge has no unit rules: " + unit)
        for rule in webfrank["units"][unit]:
            name = rule["function"]
            old = result.setdefault(name, {"kind": "webfrank_guarded_declared", "rule_count": 0})
            old["rule_count"] += 1
            if _manual(rule):
                old["kind"] = "manual_exception"
    if "p6frank" in rules:
        rule = p6frank.get("units", {}).get(unit)
        if not rule:
            raise ValueError("P6Frank build edge has no unit rule: " + unit)
        result[rule["function"]] = {"kind": "p6frank_guarded_declared", "rule_count": 1}
    return result


def _nonnegative_int(value, context):
    if isinstance(value, bool) or not re.fullmatch(r"\d+", str(value)):
        raise ValueError("invalid nonnegative integer for " + context)
    return int(value)


def validate_report(report, units=None):
    """Reject malformed or stale score inventories before credit is counted."""
    names = set()
    total = 0
    for unit in report["units"]:
        name = unit["name"]
        if name in names:
            raise ValueError("duplicate report unit: " + name)
        names.add(name)
        fn_names = set()
        unit_size = 0
        for fn in unit.get("functions", []):
            fn_name = fn["name"]
            if fn_name in fn_names:
                raise ValueError("duplicate report function: " + name + "::" + fn_name)
            fn_names.add(fn_name)
            size = _nonnegative_int(fn.get("size", 0), name + "::" + fn_name)
            score = fn.get("fuzzy_match_percent", 0)
            if isinstance(score, bool) or not math.isfinite(float(score)) or not 0 <= float(score) <= 100:
                raise ValueError("invalid report score: " + name + "::" + fn_name)
            unit_size += size
        if units is not None and name in units and unit_size != units[name]["code_size"]:
            raise ValueError(f"report/generator code total differs for {name}: {unit_size}/{units[name]['code_size']}")
        total += unit_size
    if units is not None and names != set(units):
        raise ValueError(f"report/generator roster differs: extra={sorted(names - set(units))} missing={sorted(set(units) - names)}")
    return total


def report_accounting(report, units):
    """Disjoint *report-score* tiers; linkage remains an independent dimension."""
    validate_report(report)
    tiers = {}
    total = 0
    for unit in report.get("units", []):
        # This is the exact generator name, not suffix matching.
        provenance = units.get(unit["name"])
        for fn in unit.get("functions", []):
            size = int(fn.get("size", 0))
            total += size
            if float(fn.get("fuzzy_match_percent", 0)) < 100:
                continue
            category = "unknown"
            if provenance:
                category = provenance.get("function_postprocessors", {}).get(
                    fn["name"], {}).get("kind", provenance.get("raw_text_class", "raw_text_" + provenance["compiler_class"]))
            tier = tiers.setdefault(category, {"functions": 0, "bytes": 0,
                                               "linked_source_functions": 0, "linked_source_bytes": 0})
            tier["functions"] += 1
            tier["bytes"] += size
            if provenance and provenance.get("linkage") == "source":
                tier["linked_source_functions"] += 1
                tier["linked_source_bytes"] += size
    for row in tiers.values():
        row["percent_of_total_code"] = 100 * row["bytes"] / total if total else None
    return {"scope": "100% function scores in the existing objdiff report; NOT raw-byte, relocation, data or semantic certification",
            "total_code_bytes": total, "tiers": tiers}


def collect_manifest(root, build_dir="build", version="GUNE5D"):
    root = Path(root).resolve()
    out_dir = root / build_dir / version
    failures = []
    unknowns = []
    artifacts = {}

    def artifact(path):
        path = Path(path)
        absolute = path if path.is_absolute() else root / path
        try:
            label = key(absolute.relative_to(root))
        except ValueError:
            label = key(absolute)
        if label not in artifacts:
            item = {"path": label, "exists": absolute.is_file()}
            if item["exists"]:
                item.update(sha256=sha256(absolute), size=absolute.stat().st_size)
            else:
                unknowns.append("missing artifact: " + label)
            artifacts[label] = item
        return label

    def load(path):
        artifact(path)
        return json.loads(Path(path).read_text(encoding="utf-8"))

    def ninja(*args):
        command = [snapshot.get("ninja", "ninja"), *args]
        proc = subprocess.run(command, cwd=root, text=True, capture_output=True)
        if proc.returncode:
            raise ValueError(f"Ninja query failed ({proc.returncode}): {args}: {proc.stderr}")
        return proc.stdout

    result = {"schema_version": SCHEMA_VERSION, "version": version,
              "status": "UNRESOLVED", "execution_status": "FAIL",
              "scope": "Current artifact/input provenance; no recompilation, proof replay or source equivalence assertion",
              "failures": failures, "unresolved": unknowns, "artifacts": artifacts,
              "units": [], "links": [], "known_limitations": []}
    try:
        snapshot_path = out_dir / "build_edges.json"
        snapshot = load(snapshot_path)
        if snapshot["schema_version"] != SCHEMA_VERSION:
            raise ValueError("Unsupported build_edges schema; reconfigure")
        if sha256(root / "build.ninja") != snapshot["ninja_sha256"]:
            raise ValueError("build.ninja differs from generator snapshot; reconfigure")
        artifact(root / "build.ninja")
        for path, expected in snapshot["generator_inputs"].items():
            artifact(path)
            if sha256(root / path) != expected:
                raise ValueError("generator input changed since configure: " + path)
        result["non_matching"] = snapshot["non_matching"]
        result["snapshot_sha256"] = sha256(snapshot_path)
        command_rows = json.loads(ninja("-t", "compdb", *snapshot["rules"]))
        commands = {key(row["output"]): row["command"] for row in command_rows}
        if len(command_rows) != len(commands):
            raise ValueError("duplicate output in Ninja compdb")
        dependencies = parse_ninja_deps(ninja("-t", "deps"))
        edges = snapshot["edges"]
        target_bound = [edge for edge in edges if edge["rule"] in {
            "webfrank", "webfrank_globalize_atree", "p6frank", "frank",
            "fix_exception_object", "fix_exception_objects", "retail_dol"}]
        result["editable_postprocess_isolation"] = {
            "mode": "editable" if snapshot["non_matching"] else "matching",
            "target_bound_edge_count": len(target_bound),
            "status": ("FAIL" if target_bound else "PASS") if snapshot["non_matching"] else "NOT_APPLICABLE",
            "scope": "Target-bound edge presence only; source-only completeness and runtime behavior need separate tests"}
        if snapshot["non_matching"] and target_bound:
            failures.append("editable build contains target-bound transformations")
        inplace = {path: edge for edge in edges if edge["rule"] == "fix_exception_objects"
                   for path in edge["inputs"]}
        by_output = {key(output): edge for edge in edges for output in edge["outputs"]}
        wf = load(root / "config" / version / "webfrank.json")
        p6 = load(root / "config" / version / "p6frank.json")
        report = load(out_dir / "report.json")
        expected_units = {key(Path(row["module"]) / Path(row["name"]).with_suffix("")): row
                          for row in snapshot["units"]}
        if len(expected_units) != len(snapshot["units"]):
            raise ValueError("duplicate generator unit")
        validate_report(report, expected_units)
        report_units = {row["name"]: row for row in report["units"]}
        unit_map = {}
        selected = []
        for entry in snapshot["units"]:
            row = dict(entry)
            row["report_name"] = key(Path(row["module"]) / Path(row["name"]).with_suffix(""))
            row["compiler_class"] = "unknown"
            row["function_postprocessors"] = {}
            row["pipeline"] = []
            row["metadata_operations"] = []
            row["compatibility_scaffolding"] = {"status": "UNRESOLVED", "reason": "Not mechanically inferred from source spelling"}
            if row.get("linked_object"):
                selected.append(row["linked_object"])
                artifact(row["linked_object"])
            if row.get("extracted_object"):
                artifact(row["extracted_object"])
            source_object = row.get("source_object")
            stage_edges = []
            seen = set()

            def visit(output):
                if output in seen:
                    return
                seen.add(output)
                edge = by_output.get(output)
                if not edge:
                    return
                if edge["rule"] in TRANSFORM_RULES:
                    for source in edge["inputs"]:
                        visit(source)
                stage_edges.append(edge)

            if source_object:
                visit(source_object)
            for edge in stage_edges:
                output = edge["outputs"][0]
                stage = {"rule": edge["rule"], "inputs": [artifact(p) for p in edge["inputs"]],
                         "outputs": [artifact(p) for p in edge["outputs"]],
                         "command": commands.get(output), "variables": edge["variables"]}
                stage["implicit_files"] = []
                stage["implicit_directories"] = []
                for dependency in edge["implicit"]:
                    for variable, value in edge["variables"].items():
                        dependency = dependency.replace("$" + variable, value)
                    if (root / dependency).is_dir():
                        stage["implicit_directories"].append(dependency)
                    else:
                        stage["implicit_files"].append(artifact(dependency))
                if not isinstance(stage["command"], str) or not stage["command"].strip():
                    failures.append("Ninja did not expand command for " + output)
                if edge["rule"] in COMPILE_RULES:
                    compiler_version = key(edge["variables"].get("mw_version", ""))
                    stage["compiler"] = artifact(Path(snapshot["compilers"]) / compiler_version / "mwcceppc.exe")
                    stage["compiler_version"] = compiler_version
                    stage["compiler_class"] = compiler_class(compiler_version)
                    stage["identity_basis"] = "Configured archive label plus observed hash; independent release authentication not performed"
                    if "/profile/" not in output:
                        row["compiler_class"] = stage["compiler_class"]
                    deps = dependencies.get(output)
                    stage["dependency_status"] = "UNRESOLVED"
                    stage["dependencies"] = []
                    stage["ninja_dependency_state"] = deps["state"] if deps else "MISSING"
                    if deps:
                        stage["dependencies"] = [artifact(p) for p in deps["paths"]]
                    if deps and deps["state"] == "VALID" and len(deps["paths"]) == deps["declared_count"]:
                        stage["dependency_status"] = "PASS"
                    elif deps and deps["state"] == "STALE" and output in inplace and len(deps["paths"]) == deps["declared_count"]:
                        result["known_limitations"].append("Ninja dependency mtime STALE after declared in-place exception runtime rewrite: " + output)
                    else:
                        unknowns.append("missing/stale Ninja dependency set: " + output)
                    stage["dependency_basis"] = "Ninja's last successful compile depfile; current hashes are not a replay of that compile"
                    source = root / edge["inputs"][0]
                    if source.is_file():
                        stage["pragma_observations"] = [{"line": i, "text": line.strip()}
                            for i, line in enumerate(source.read_text(encoding="utf-8", errors="replace").splitlines(), 1)
                            if re.match(r"^\s*#\s*pragma\b", line)]
                    stage["pragma_scope_status"] = "UNRESOLVED: lexical observations, not preprocessed/effective per-function options"
                    if "extab" in edge["rule"]:
                        row["metadata_operations"].append("dtk extab clean (inside compile command; pre-clean object not retained)")
                    stage["raw_compiler_output_retained"] = "extab" not in edge["rule"] and output not in inplace
                    if output in inplace:
                        row["raw_text_class"] = "exception_runtime_rewrite_declared"
                        row["metadata_operations"].append("fix_exception_objects rewrites text/data/relocations/EH in place; pre-fix raw object unavailable")
                        stage["in_place_transform"] = inplace[output]
                if edge["rule"] in {"globalize_atree", "webfrank_globalize_atree"}:
                    row["metadata_operations"].append("atree symbol visibility/rename (target-independent metadata operation)")
                if edge["rule"] == "fix_exception_object":
                    row["raw_text_class"] = "exception_runtime_rewrite_declared"
                    row["metadata_operations"].append("Matching-only, hash-guarded exception runtime layout rewrite; separate raw compiler object retained")
                if edge["rule"] == "as":
                    row["raw_text_class"] = "assembly_source"
                if edge["rule"] == "frank":
                    row["raw_text_class"] = "legacy_frank_unclassified"
                row["pipeline"].append(stage)
            unit_key = key(Path(row["name"]).with_suffix(""))
            row["function_postprocessors"] = postprocessor_functions(unit_key, stage_edges, wf, p6)
            report_row = report_units.get(row["report_name"])
            if report_row is None:
                failures.append("selected unit missing from report: " + row["report_name"])
            row["report_measures"] = report_row.get("measures", {}) if report_row else None
            if row["configured"] and not row["configured_completed"] and report_row:
                row["closure_inputs"] = {
                    "function_roster_basis": "Existing objdiff report order; not recovered source order",
                    "functions": [{"name": fn["name"], "size": int(fn.get("size", 0)),
                                   "report_fuzzy_percent": fn.get("fuzzy_match_percent")}
                                  for fn in report_row.get("functions", [])],
                    "review_needed": ["source order/inlining", "prototypes and ABI", "literal pools and relocations",
                                      "data/BSS ownership", "weak/helper symbols", "exception records",
                                      "effective flags and source/header pragmas"],
                }
            row["reconstruction_verification"] = {dimension: "UNRESOLVED" for dimension in
                ("text", "relocations", "initialized_data", "bss_common", "exception_metadata", "source_semantics")}
            unit_map[row["report_name"]] = row
            result["units"].append(row)
        actual_link_inputs = []
        for edge in edges:
            if edge["rule"] != "link":
                continue
            actual_link_inputs.extend(edge["inputs"])
            output = edge["outputs"][0]
            if not isinstance(commands.get(output), str) or not commands[output].strip():
                failures.append("Ninja did not expand link command for " + output)
            result["links"].append({"output": artifact(output), "inputs": [artifact(p) for p in edge["inputs"]],
                "command": commands.get(output),
                "linker": artifact(Path(snapshot["compilers"]) / snapshot["linker_version"] / "mwldeppc.exe")})
        # DOL-only today. Multi-step RELs reuse inputs; do not certify by set equality.
        if len(result["links"]) != 1:
            unknowns.append("link reconciliation requires exactly one DOL link (REL chains not yet modelled)")
            result["link_reconciliation"] = {"status": "UNRESOLVED"}
        else:
            same = selected == actual_link_inputs
            result["link_reconciliation"] = {"status": "PASS" if same else "FAIL",
                "selected_count": len(selected), "ninja_input_count": len(actual_link_inputs),
                "ordered_inputs_equal": same}
            if not same:
                failures.append("add_unit selection does not reconcile to ordered Ninja link inputs")
        result["report_accounting"] = report_accounting(report, unit_map)
        result["inventory"] = {"linkage": dict(Counter(row["linkage"] for row in result["units"])),
            "compiler_configuration": dict(Counter(row["compiler_class"] for row in result["units"] if row["source_object"])),
            "auto_units": [{"name": row["name"], "object": row["linked_object"],
                "section_name_from_dtk_label": "." + row["name"].rsplit("_", 1)[-1],
                "code_size": row["code_size"], "data_size": row["data_size"], "disposition": "UNRESOLVED ownership; not necessarily missing C"}
                for row in result["units"] if row["autogenerated"]],
            "unfinished_units": [row["name"] for row in result["units"]
                if row["configured"] and not row["configured_completed"]]}
        result["post_link_edges"] = [{**edge, "command": commands.get(edge["outputs"][0]),
            "input_artifacts": [artifact(p) for p in edge["inputs"]],
            "implicit_artifacts": [artifact(p) for p in edge["implicit"]],
            "output_artifacts": [artifact(p) for p in edge["outputs"]]}
            for edge in edges if edge["rule"] in {"elf2dol", "retail_dol", "fix_exception_objects"}]
        for edge in result["post_link_edges"]:
            if not isinstance(edge["command"], str) or not edge["command"].strip():
                failures.append("Ninja did not expand transformation command for " + edge["outputs"][0])
        result["reconstruction_status"] = "UNRESOLVED"
        result["execution_status"] = "FAIL" if failures else "PASS"
        result["status"] = "FAIL" if failures else "UNRESOLVED" if unknowns else "PASS"
        result["limitations"] = ["PASS certifies snapshot/query accounting only, not matching or provenance of the original game source.",
            "Current input hashes do not prove artifacts were built from those hashes; run a clean build for that claim.",
            "Guarded declarations are not proof replays. Raw text categories may still have metadata transformations.",
            "Report 100% uses its configured relocation scoring; no report tier certifies raw byte equality.",
            "BSS is allocated storage, not stored DOL payload. Auto data ownership remains unadjudicated."]
    except (OSError, ValueError, KeyError, TypeError) as error:
        failures.append(str(error))
        result["status"] = "FAIL"
    return result


def print_report_accounting(accounting):
    print("  Report-score provenance (100% scores; NOT raw-byte/relocation certification):")
    for name, row in sorted(accounting["tiers"].items()):
        percent = row["percent_of_total_code"]
        print(f"    {name}: {percent:.2f}% ({row['functions']} fns; {row['bytes']} bytes)")
    print("  Linkage is independent; guarded declarations are not new proof replays.")
    print("  Snapshot PASS is not compiler identity, dependency freshness or source correctness certification.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--version", default="GUNE5D")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    result = collect_manifest(args.root, args.build_dir, args.version)
    output = args.out or args.root / args.build_dir / args.version / "build_provenance.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{result['status']} execution={result['execution_status']}: {output}")
    print(f"  units={len(result['units'])} failures={len(result['failures'])} unresolved={len(result['unresolved'])}")
    for error in result["failures"]:
        print("  FAIL: " + error)
    return 1 if result["execution_status"] == "FAIL" else 2 if result["status"] == "UNRESOLVED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
