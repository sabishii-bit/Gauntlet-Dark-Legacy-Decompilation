"""Fresh, read-only reconstruction diagnostics after a successful ninja build.

PASS certifies execution/accounting of these diagnostics, NOT recovered source
or semantic equivalence. The stricter objdiff report remains a SHADOW report;
its demotions are an adjudication queue, never an automatic bug census. Generated
reports/logs stay in a unique directory under the output's parent. No cached
report is accepted as the result of a failed command.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

try:
    from .raw_object import load_graph
except ImportError:
    from raw_object import load_graph

ROOT = Path(__file__).resolve().parents[2]


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def function_index(report):
    if not isinstance(report.get("units"), list) or not report["units"]:
        raise ValueError("missing or empty report unit roster")
    functions, units = {}, set()
    for unit in report["units"]:
        name = unit["name"]
        if name in units:
            raise ValueError(f"duplicate unit: {name}")
        units.add(name)
        for fn in unit.get("functions", []):
            key = (name, fn["name"])
            if key in functions:
                raise ValueError(f"duplicate function: {key}")
            # A missing score is unknown, not an implicit 0 or 100 percent.
            score = fn.get("fuzzy_match_percent")
            if score is not None:
                score = float(score)
                if not math.isfinite(score) or not 0 <= score <= 100:
                    raise ValueError(f"invalid score for {key}")
            metadata = unit.get("metadata", {})
            functions[key] = {
                "score": score, "size": int(fn["size"]),
                "source_linked": metadata.get("complete") is True,
                "categories": metadata.get("progress_categories", []),
            }
    if not functions:
        raise ValueError("empty function roster")
    return functions, units


def compare_reports(normal, shadow):
    """Compare identical populations; unknown scores remain visible."""
    left, left_units = function_index(normal)
    right, right_units = function_index(shadow)
    if left.keys() != right.keys() or left_units != right_units:
        raise ValueError("report populations differ; do not compare percentages")
    for field in ("total_code", "total_data", "total_functions", "total_units"):
        a = normal.get("measures", {}).get(field)
        b = shadow.get("measures", {}).get(field)
        if a is None or b is None or int(a) != int(b):
            raise ValueError(f"missing or inconsistent report total: {field}")
    rows, unknown = [], []
    for (unit, fn), first in sorted(left.items()):
        second = right[(unit, fn)]
        if first["size"] != second["size"] or first["source_linked"] != second["source_linked"]:
            raise ValueError(f"function size/link status changed: {unit}::{fn}")
        a, b = first["score"], second["score"]
        if a is None or b is None:
            unknown.append({"unit": unit, "function": fn,
                            "normal_score": a, "shadow_score": b})
        elif a != b:
            rows.append({"unit": unit, "function": fn, **first,
                         "normal_score": a, "shadow_score": b,
                         "lost_report_100": a == 100 and b < 100,
                         "disposition": "UNRESOLVED"})
    return {
        "schema_version": 1, "status": "PASS",
        "interpretation": "Diagnostic score comparison, not a corrected matching percentage or bug count.",
        "adjudication_status": "UNRESOLVED" if rows or unknown else "NOT_NEEDED",
        "functions_compared": len(left), "scores_changed": len(rows),
        "lost_report_100": sum(row["lost_report_100"] for row in rows),
        "source_linked_demotions": sum(row["lost_report_100"] and row["source_linked"] for row in rows),
        "unknown_scores": unknown, "changes": rows,
        "normal_measures": normal["measures"], "shadow_measures": shadow["measures"],
    }


def input_fingerprints(root):
    """Bind diagnostics to the same graph and compared objects on both sides.

    This detects changes DURING the run; it is not a substitute for a fresh
    build before starting. Hash object contents, not only their timestamps.
    """
    config = root / "objdiff.json"
    graph = load_graph(root, "GUNE5D")
    paths = {config, root / "build.ninja", root / "build/GUNE5D/build_edges.json",
             *(root / name for name in graph["generator_inputs"])}
    document = json.loads(config.read_text(encoding="utf-8"))
    if not document.get("units"):
        raise ValueError("objdiff.json has no comparison units")
    for unit in document["units"]:
        for field in ("target_path", "base_path"):
            if unit.get(field):
                paths.add(root / unit[field])
    return {str(path): sha256(path) for path in sorted(paths)}


def run_stage(name, command, root, folder, artifact=None, require_status=False,
              timeout=300, allowed_returncodes=(0,), artifact_validator=None):
    """Execute once; no old artifact can satisfy a failed or silent process."""
    row = {"name": name, "command": [str(arg) for arg in command], "status": "FAIL"}
    started = time.monotonic()
    if artifact is not None and artifact.exists():
        row["error"] = "refused pre-existing output artifact"
        return row
    try:
        done = subprocess.run(command, cwd=root, capture_output=True, text=True,
                              errors="replace", timeout=timeout)
        log = folder / (name + ".log")
        log.write_text(done.stdout + "\n--- stderr ---\n" + done.stderr,
                       encoding="utf-8")
        row.update(returncode=done.returncode, log=str(log))
        if done.returncode not in allowed_returncodes:
            row["error"] = "command did not complete its required check"
            return row
        if artifact is not None:
            payload = json.loads(artifact.read_text(encoding="utf-8"))
            if not isinstance(payload, dict):
                raise ValueError("artifact is not a JSON object")
            if require_status and (payload.get("schema_version") != 1 or payload.get("status") != "PASS"):
                row.update(status="UNRESOLVED", artifact=str(artifact),
                           error="required tool schema/status is not schema 1 PASS")
                return row
            if artifact_validator is not None:
                row["validated_scope"] = artifact_validator(payload)
                row["tool_reported_status"] = payload.get("status")
            row.update(artifact=str(artifact), artifact_sha256=sha256(artifact))
        row["status"] = "PASS"
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        row["error"] = str(error)
    finally:
        row["seconds"] = round(time.monotonic() - started, 3)
    return row


def validate_exception_control(payload):
    """A positive EH control, not a TU-flip certificate for newcam."""
    if payload.get("schema_version") != 1 or payload.get("status") not in {"PASS", "UNRESOLVED"}:
        raise ValueError("exception control: invalid schema or failed audit")
    rows = payload.get("rows", [])
    if payload.get("units_selected") != 1 or len(rows) != 1 or rows[0].get("unit") != "game/world/newcam.c":
        raise ValueError("exception control: unexpected unit population")
    metadata = rows[0].get("exception_metadata") or {}
    if (metadata.get("schema_version") != 1 or metadata.get("status") != "PASS"
            or not isinstance(metadata.get("target_records"), int)
            or metadata["target_records"] <= 0
            or metadata.get("ours_records") != metadata["target_records"]
            or metadata.get("missing") != [] or metadata.get("changed") != {}
            or metadata.get("extra") != {}):
        raise ValueError("exception control: nonempty function-based equality not established")
    return "newcam exception metadata only; data ownership and whole-TU status remain separate"


def validate_datum_control(payload):
    """Require a complete screen, not a delta-free NonMatching function.

    Recovering enemy's local mbdesc name made the multiset screen compare
    N:mbdesc with A:0x80250E00. That is an adjudication candidate, not failed
    execution or proof of a bad binding. Keep all deltas in the artifact;
    missing/stale objects, unreadable sides and malformed accounting fail.
    This validates measurement only, never datum or relocation equivalence.
    """
    if payload.get("schema_version") != 1 or payload.get("error"):
        raise ValueError("datum control: invalid schema or failed audit")
    selection = payload.get("selection", {})
    rows = payload.get("rows", [])
    if (not isinstance(selection, dict) or not isinstance(rows, list)
            or selection.get("scope") != "explicit-function"
            or selection.get("units_selected") != 1
            or payload.get("discovery_failures") != []
            or len(rows) != 1
            or not isinstance(rows[0], dict)
            or rows[0].get("unit") != "game/enemy/enemy"
            or rows[0].get("function") != "do_enemy_move"):
        raise ValueError("datum control: incomplete or unexpected population")
    expected = dict(functions_selected=1, functions_screened_both=1,
                    raw_equal=0, raw_candidates=0, post_equal=0,
                    post_candidates=0, raw_unreadable=0, post_unreadable=0,
                    disagreements=0, discovery_failures=0)
    row = rows[0]
    states = []
    for name in ("raw", "post"):
        side = row.get(name, {})
        if not isinstance(side, dict) or side.get("error") or not side.get("object"):
            raise ValueError("datum control: unreadable object side")
        for field in ("target_relocs", "ours_relocs"):
            if type(side.get(field)) is not int or side[field] <= 0:
                raise ValueError("datum control: missing/empty relocation population")
        for field in ("target_only", "ours_only"):
            counts = side.get(field)
            if (not isinstance(counts, dict)
                    or any(not isinstance(key, str) or not key
                           or type(value) is not int or value <= 0
                           for key, value in counts.items())):
                raise ValueError("datum control: invalid delta accounting")
            total = side["target_relocs" if field == "target_only" else "ours_relocs"]
            if sum(counts.values()) > total:
                raise ValueError("datum control: delta exceeds relocation population")
        delta = bool(side["target_only"] or side["ours_only"])
        state = "UNRESOLVED" if delta else "PASS"
        if (side.get("verdict") != ("VALUE-DELTA" if delta else "VALUE-EQUAL")
                or side.get("status") != state):
            raise ValueError("datum control: inconsistent side verdict")
        states.append(state)
        expected[name + ("_candidates" if delta else "_equal")] = 1
    expected["disagreements"] = int(any(
        row["raw"][field] != row["post"][field]
        for field in ("target_only", "ours_only")))
    state = "UNRESOLVED" if "UNRESOLVED" in states else "PASS"
    if (payload.get("tally") != expected or row.get("status") != state
            or payload.get("status") != state):
        raise ValueError("datum control: inconsistent complete-screen accounting")
    return ("one enemy function screened on both sides; datum deltas remain "
            "UNRESOLVED review candidates, not a binding/equivalence certificate")


def print_result(result, output):
    """Expose the failed stage in CI even if artifact upload is unavailable."""
    print(f"{result['status']}: reconstruction diagnostics written to {output}")
    if result.get("error"):
        print("ERROR: " + result["error"])
    for stage in result["stages"]:
        print(f"  {stage['name']}: {stage['status']}")
        if stage.get("error"):
            print("    " + stage["error"])
        if stage.get("log") and stage["status"] != "PASS":
            print("    log: " + stage["log"])
        if stage.get("tool_reported_status") == "UNRESOLVED":
            print("    REVIEW UNRESOLVED: " + stage["validated_scope"])
    print("This is not a source-completion or semantic-equivalence certificate.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/GUNE5D/reconstruction_preflight.json")
    parser.add_argument("--smoke-tools", action="store_true",
                        help="also require fresh provenance, full-TU compiler fidelity and a bounded datum CLI control")
    parser.add_argument("--objdiff", type=Path,
                        help="native objdiff-cli executable (defaults to build/tools)")
    args = parser.parse_args(argv)
    args.out = args.out.resolve()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="reconstruction_", dir=args.out.parent))
    result = {"schema_version": 1, "status": "FAIL", "stages": [],
              "scope": "fresh shadow-relocation diagnostics; not source-completion certification",
              "smoke_tools": "REQUESTED" if args.smoke_tools else "NOT_RUN",
              "artifacts_directory": str(folder)}
    try:
        before = input_fingerprints(ROOT)
        result["input_fingerprints"] = before
        binary = args.objdiff or ROOT / "build/tools" / ("objdiff-cli.exe" if os.name == "nt" else "objdiff-cli")
        reports = [folder / "normal.json", folder / "shadow.json"]
        for name, path, override in (("normal_report", reports[0], []),
                                     ("shadow_report", reports[1], ["-c", "functionRelocDiffs=data_value"])):
            stage = run_stage(name, [str(binary), "report", "generate", *override,
                                     "-o", str(path)], ROOT, folder, path)
            result["stages"].append(stage)
        if all(stage["status"] == "PASS" for stage in result["stages"]):
            result["relocation_comparison"] = compare_reports(*[
                json.loads(path.read_text(encoding="utf-8")) for path in reports])
        if args.smoke_tools:
            specifications = [
                ("provenance", "tools/gdl/build_provenance.py", []),
                ("compiler_fidelity", "tools/gdl/composed_census/cv_probe.py",
                 ["game/sys/sysservice", "--axes", "check"]),
            ]
            for name, script, options in specifications:
                path = folder / (name + ".json")
                result["stages"].append(run_stage(
                    name, [sys.executable, script, *options, "--out", str(path)],
                    ROOT, folder, path, require_status=True))
            path = folder / "datum_control.json"
            result["stages"].append(run_stage(
                "datum_control", [sys.executable,
                    "tools/gdl/composed_census/ce_eq_datum_audit.py",
                    "--unit", "game/enemy/enemy", "--function", "do_enemy_move",
                    "--out", str(path)], ROOT, folder, path,
                allowed_returncodes=(0, 2), artifact_validator=validate_datum_control))
            path = folder / "exception_control.json"
            result["stages"].append(run_stage(
                "exception_control", [sys.executable, "tools/gdl/datadiff.py",
                    "game/world/newcam", "--sections", "--out", str(path)],
                ROOT, folder, path, allowed_returncodes=(0, 2),
                artifact_validator=validate_exception_control))
        after = input_fingerprints(ROOT)
        changed = sorted(path for path in before.keys() | after.keys()
                         if before.get(path) != after.get(path))
        result["stages"].append({"name": "input_stability", "status": "FAIL" if changed else "PASS",
                                 "changed_inputs": changed})
        states = {stage["status"] for stage in result["stages"]}
        result["status"] = "FAIL" if "FAIL" in states else "UNRESOLVED" if "UNRESOLVED" in states else "PASS"
    except (OSError, ValueError, KeyError, TypeError) as error:
        result["error"] = str(error)
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print_result(result, args.out)
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
