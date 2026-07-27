#!/usr/bin/env python3
"""Report and validate reconstruction progress independently of byte matching.

The default matching build must remain hash-identical, so reconstructed units
use Object(Equivalent, ...): their original objects are linked normally, while
`configure.py --non-matching` links the reconstructed source.

Statuses in config/GUNE5D/semantic_progress.toml:

  reconstructed          Every target function is represented in source.
  structurally_verified  Additionally, instruction order and all non-register
                         operands agree (exact/relocation/register-only).
  native_ready           Reserved for units exercised by a native backend/test.

Usage:
  python tools/gdl/semantic_progress.py
  python tools/gdl/semantic_progress.py --classify
  python tools/gdl/semantic_progress.py --check
  python tools/gdl/semantic_progress.py --check --verify-builds
"""

import argparse
from collections import Counter
import json
import re
import subprocess
import sys
import tomllib
from pathlib import Path

from fndiff import classify_function, instruction_lines, parse

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
MANIFEST = REPO / "config" / VERSION / "semantic_progress.toml"
REPORT = REPO / "build" / VERSION / "report.json"
CONFIGURE = REPO / "configure.py"
ALLOWED_STATUSES = {"reconstructed", "structurally_verified", "native_ready"}
VERIFIED_CLASSES = {
    "EXACT",
    "RELOCATION_ONLY",
    "REGISTER_ONLY",
    "BASE_ONLY",
}


def load_manifest():
    data = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise ValueError(f"{MANIFEST}: unsupported schema_version")
    return data.get("units", {})


def configured_state(config_text, unit):
    pattern = re.compile(
        r'Object\(\s*(Matching|NonMatching|Equivalent)\s*,\s*"'
        + re.escape(unit)
        + r'"',
        re.DOTALL,
    )
    match = pattern.search(config_text)
    return match.group(1) if match else None


def source_path(unit):
    return REPO / "src" / unit


def object_paths(unit):
    stem = re.sub(r"\.(?:c|cpp)$", "", unit)
    return (
        REPO / "build" / VERSION / "obj" / f"{stem}.o",
        REPO / "build" / VERSION / "src" / f"{stem}.o",
    )


def build_source_object(unit):
    _, base = object_paths(unit)
    result = subprocess.run(["ninja", str(base.relative_to(REPO))], cwd=REPO)
    if result.returncode:
        raise RuntimeError(f"failed to build {unit}")


def classify_unit(unit, build=True):
    if build:
        build_source_object(unit)
    target_path, base_path = object_paths(unit)
    if not target_path.exists() or not base_path.exists():
        raise FileNotFoundError(f"missing target/base object for {unit}")

    target = parse(target_path)
    base = parse(base_path)
    names = list(target)
    names.extend(name for name in base if name not in target)
    rows = []
    for name in names:
        target_lines = target.get(name)
        base_lines = base.get(name)
        if target_lines is None:
            category = "BASE_ONLY"
            target_count = 0
            base_count = len(instruction_lines(base_lines))
        elif base_lines is None:
            category = "TARGET_ONLY"
            target_count = len(instruction_lines(target_lines))
            base_count = 0
        else:
            category = classify_function(target_lines, base_lines)
            target_count = len(instruction_lines(target_lines))
            base_count = len(instruction_lines(base_lines))
        rows.append((name, category, target_count, base_count))
    return rows


def report_units():
    if not REPORT.exists():
        return {}
    data = json.loads(REPORT.read_text(encoding="utf-8"))
    return {
        unit.get("name", "").removeprefix("main/"): unit
        for unit in data.get("units", [])
    }


def validate_entry(unit, entry, state, rows):
    errors = []
    status = entry.get("status")
    if status not in ALLOWED_STATUSES:
        errors.append(f"invalid status {status!r}")
    if state not in {"Equivalent", "Matching"}:
        errors.append(f"configure.py uses {state or 'no Object entry'}, expected Equivalent/Matching")
    if not source_path(unit).is_file():
        errors.append("source file is missing")
    if not entry.get("summary"):
        errors.append("summary is required")
    if not entry.get("evidence"):
        errors.append("at least one evidence item is required")

    categories = {category for _, category, _, _ in rows}
    if "TARGET_ONLY" in categories:
        errors.append("one or more target functions are absent from the source object")
    if status in {"structurally_verified", "native_ready"}:
        risky = sorted(categories - VERIFIED_CLASSES)
        if risky:
            errors.append("verified status has unresolved classes: " + ", ".join(risky))
    return errors


def format_classes(rows):
    counts = Counter(category for _, category, _, _ in rows)
    order = [
        "EXACT",
        "RELOCATION_ONLY",
        "REGISTER_ONLY",
        "SCHEDULE_CANDIDATE",
        "OPERAND_DIFF",
        "STRUCTURAL",
        "TARGET_ONLY",
        "BASE_ONLY",
    ]
    labels = {
        "EXACT": "exact",
        "RELOCATION_ONLY": "reloc",
        "REGISTER_ONLY": "regs",
        "SCHEDULE_CANDIDATE": "schedule?",
        "OPERAND_DIFF": "operands",
        "STRUCTURAL": "structural",
        "TARGET_ONLY": "missing",
        "BASE_ONLY": "helpers",
    }
    return " ".join(
        f"{labels[category]}:{counts[category]}"
        for category in order
        if counts[category]
    )


def run_command(command):
    result = subprocess.run(command, cwd=REPO)
    if result.returncode:
        raise RuntimeError("command failed: " + " ".join(command))


def verify_source_linkage(units):
    """Ensure every manifest unit is an input to the integration link."""
    ninja_text = (REPO / "build.ninja").read_text(encoding="utf-8")
    marker = f"build build\\{VERSION}\\main.elf: link"
    start = ninja_text.find(marker)
    end = ninja_text.find("\n  ldflags =", start)
    if start < 0 or end < 0:
        raise RuntimeError("could not locate the main link edge in build.ninja")
    link_edge = ninja_text[start:end].replace("\\", "/")
    missing = []
    for unit in units:
        stem = re.sub(r"\.(?:c|cpp)$", "", unit)
        expected = f"build/{VERSION}/src/{stem}.o"
        if expected not in link_edge:
            missing.append(unit)
    if missing:
        raise RuntimeError(
            "Equivalent source objects are absent from the integration link: "
            + ", ".join(missing)
        )
    print(f"verified {len(units)} Equivalent source object(s) on the link edge")


def verify_builds(units):
    """Verify both link modes and always leave the canonical configuration active."""
    print("== canonical matching build ==")
    run_command([sys.executable, "configure.py"])
    run_command(["ninja"])
    try:
        print("== Equivalent-source integration build ==")
        run_command([sys.executable, "configure.py", "--non-matching"])
        verify_source_linkage(units)
        run_command(["ninja"])
    finally:
        print("== restore and recheck canonical build ==")
        run_command([sys.executable, "configure.py"])
        run_command(["ninja"])


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--classify", action="store_true",
                        help="build and classify every function in each listed unit")
    parser.add_argument("--check", action="store_true",
                        help="validate manifest, configure state, objects, and status claims")
    parser.add_argument("--no-build", action="store_true",
                        help="with --classify/--check, use existing source objects")
    parser.add_argument("--verify-builds", action="store_true",
                        help="verify canonical SHA, Equivalent-source link, then canonical SHA again")
    parser.add_argument("--grep", metavar="TEXT", help="filter unit paths")
    args = parser.parse_args()
    if args.verify_builds and not args.check:
        parser.error("--verify-builds requires --check")

    try:
        units = load_manifest()
    except (OSError, ValueError, tomllib.TOMLDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    config_text = CONFIGURE.read_text(encoding="utf-8")
    reports = report_units()
    run_classification = args.classify or args.check
    failures = 0
    shown = 0
    status_counts = Counter()
    semantic_functions = 0
    semantic_bytes = 0
    verified_functions = 0
    verified_bytes = 0

    for unit, entry in units.items():
        if args.grep and args.grep.lower() not in unit.lower():
            continue
        shown += 1
        status = entry.get("status", "?")
        status_counts[status] += 1
        state = configured_state(config_text, unit)
        rows = []
        try:
            if run_classification:
                rows = classify_unit(unit, build=not args.no_build)
        except (OSError, RuntimeError) as error:
            print(f"ERROR {unit}: {error}", file=sys.stderr)
            failures += 1
            continue

        report_name = re.sub(r"\.(?:c|cpp)$", "", unit)
        report = reports.get(report_name, {})
        measures = report.get("measures", {})
        matched = int(measures.get("matched_functions", 0))
        total = int(measures.get("total_functions", 0))
        fuzzy = float(measures.get("fuzzy_match_percent", 0.0))
        suffix = f" | {format_classes(rows)}" if rows else ""
        print(
            f"{entry.get('status', '?'):<23} {unit:<34} "
            f"match {matched:>2}/{total:<2} fuzzy {fuzzy:6.2f}% "
            f"link {state or '?'}{suffix}"
        )
        if rows:
            unit_functions = sum(1 for _, _, target_count, _ in rows if target_count)
            unit_bytes = sum(target_count * 4 for _, _, target_count, _ in rows)
            semantic_functions += unit_functions
            semantic_bytes += unit_bytes
            if status in {"structurally_verified", "native_ready"}:
                verified_functions += unit_functions
                verified_bytes += unit_bytes

        if args.check:
            errors = validate_entry(unit, entry, state, rows)
            for error in errors:
                print(f"  ERROR: {error}")
            failures += len(errors)

    if args.verify_builds and not failures:
        try:
            verify_builds(units)
        except RuntimeError as error:
            print(f"ERROR: {error}", file=sys.stderr)
            failures += 1

    totals = ""
    if run_classification:
        totals = (
            f" | {semantic_functions} functions / {semantic_bytes} text bytes reconstructed"
            f" | {verified_functions} functions / {verified_bytes} bytes structurally verified"
        )
    states = ", ".join(f"{name}:{count}" for name, count in sorted(status_counts.items()))
    print(
        f"--- {shown} semantic units ({states}){totals}"
        f" | {failures} validation error(s) ---"
    )
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
