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
    paths = {config, root / "build.ninja", root / "config/GUNE5D/webfrank.json"}
    document = json.loads(config.read_text(encoding="utf-8"))
    if not document.get("units"):
        raise ValueError("objdiff.json has no comparison units")
    for unit in document["units"]:
        for field in ("target_path", "base_path"):
            if unit.get(field):
                paths.add(root / unit[field])
    return {str(path): sha256(path) for path in sorted(paths)}


def run_stage(name, command, root, folder, artifact=None, require_status=False,
              timeout=300):
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
        if done.returncode:
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
            row.update(artifact=str(artifact), artifact_sha256=sha256(artifact))
        row["status"] = "PASS"
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        row["error"] = str(error)
    finally:
        row["seconds"] = round(time.monotonic() - started, 3)
    return row


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
                ("datum_control", "tools/gdl/composed_census/ce_eq_datum_audit.py",
                 ["--unit", "game/enemy/enemy", "--function", "do_enemy_move"]),
            ]
            for name, script, options in specifications:
                path = folder / (name + ".json")
                result["stages"].append(run_stage(
                    name, [sys.executable, script, *options, "--out", str(path)],
                    ROOT, folder, path, require_status=True))
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
    print(f"{result['status']}: reconstruction diagnostics written to {args.out}")
    print("This is not a source-completion or semantic-equivalence certificate.")
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
