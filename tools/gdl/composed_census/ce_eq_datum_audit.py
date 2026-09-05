"""Compare raw and postprocessed relocation-datum multisets through fndiff.

Default scope: unique WebFrank-pinned functions (rule chains count once).
--image selects functions of non-complete, non-auto report units. A bounded
control is --unit game/enemy/enemy --function do_enemy_move. Requires a fresh
successful build; this read-only tool does not build or certify freshness.

VALUE-DELTA is a review candidate, NEVER proof of a source-value defect.
Naming, reference multiplicity and pointer-table representation can produce
deltas; transposed operands can preserve a multiset. VALUE-EQUAL therefore
means this screen found no delta, not byte/semantic/relocation equivalence.

JSON schema_version=1: PASS (exit 0) means complete, delta-free screening;
UNRESOLVED (exit 2) means candidates, absent functions/objects or stale-object
markers; FAIL (exit 1) means invalid input or a failed analysis operation.
Both sides of every selected function and every discovery failure are kept.
--objdump PATH overrides the executable for the shared fndiff core, useful
in the private Linux build environment. No private input is copied to output.
"""
import argparse
import json
import os
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "gdl"))
import fndiff  # noqa: E402

SCHEMA_VERSION = 1


def parsed(path):
    """Use the shared, file-identity-cached parser; never cache failed reads."""
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"missing object: {path}")
    warning = fndiff.stale_object_warning(path)
    if warning:
        raise ValueError(warning)
    return fndiff.parse(path)


def function_key(functions, name, object_path=None):
    """Mirror fndiff's unique dtk suffix normalization without fuzzy pairing."""
    if name in functions:
        return name
    if not name.startswith("fn_"):
        normalized = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
        if normalized != name and normalized in functions and object_path is not None:
            # A made-up address suffix must not select another real function
            # merely because the parser normalized that function's name.
            names = {line.split()[-1] for line in fndiff.objdump(object_path, "-t").splitlines()
                     if line.split()}
            if name in names:
                return normalized
    raise KeyError(f"function absent from parsed object: {name}")


def screen_against(unit, fn, bobj):
    """Screen caller-chosen OUR object through the shared datum core."""
    tobj = ROOT / "build" / "GUNE5D" / "obj" / f"{unit}.o"
    tfns, bfns = parsed(tobj), parsed(bobj)
    return fndiff.datum_screen_from_lines(
        tfns[function_key(tfns, fn, tobj)], bfns[function_key(bfns, fn, bobj)],
        tobj, bobj)


def object_paths(unit, requires_raw=False, edges=None):
    final = ROOT / "build" / "GUNE5D" / "src" / f"{unit}.o"
    body = final.parent / ".postprocess" / "body" / final.name
    if edges is not None:
        if unit not in edges:
            raise FileNotFoundError(f"no active compile edge for {unit}")
        raw = (ROOT / edges[unit]["body_o"].replace("\\", "/")).resolve()
        if not raw.is_relative_to(ROOT):
            raise ValueError("raw object edge is outside this checkout")
        # Use what this graph compiles, not an old body left by another mode.
        return final, raw, raw != final
    # A pinned TU requires the raw body even if it went missing. Falling
    # back to final would turn a failed raw audit into apparent success.
    raw = body if requires_raw or body.is_file() else final
    return final, raw, raw == body


def raw_object(unit):
    """Legacy path helper; production audit supplies explicit pin knowledge."""
    _final, raw, has_raw = object_paths(unit)
    return raw, has_raw


def pinned_functions(config):
    if not isinstance(config, dict):
        raise ValueError("webfrank config must be an object")
    units = config.get("units", config)
    if not isinstance(units, dict):
        raise ValueError("webfrank units must be an object")
    pins = []
    rule_count = 0
    for unit, rules in units.items():
        if not isinstance(rules, list):
            raise ValueError(f"rules for {unit} must be a list")
        for rule in rules:
            if not isinstance(rule, dict) or not isinstance(rule.get("function"), str):
                raise ValueError(f"invalid rule in {unit}")
            pins.append((fndiff.unit_key(unit), rule["function"]))
            rule_count += 1
    return sorted(set(pins)), rule_count


def select_functions(pins, image=False, unit=None, function=None):
    """Return (unique roster, discovery failures, selection details)."""
    if unit:
        units = [fndiff.unit_key(unit)]
        scope = "explicit-function" if function else "explicit-unit"
    elif image:
        report = json.loads((ROOT / "build/GUNE5D/report.json").read_text(encoding="utf-8"))
        if not isinstance(report, dict) or not isinstance(report.get("units"), list):
            raise ValueError("report.json has no units list")
        units = []
        for row in report["units"]:
            if not isinstance(row, dict) or not isinstance(row.get("name"), str):
                raise ValueError("malformed unit in report.json")
            metadata = row.get("metadata", {})
            if not isinstance(metadata, dict):
                raise ValueError("malformed unit metadata in report.json")
            if not metadata.get("complete") and not metadata.get("auto_generated"):
                units.append(fndiff.unit_key(row["name"].removeprefix("main/")))
        scope = "non-complete-non-auto-report-units"
    else:
        return pins, [], {"scope": "unique-webfrank-functions",
                          "units_selected": len({u for u, _ in pins})}
    roster, failures = [], []
    for name in sorted(set(units)):
        if function:
            roster.append((name, function))
            continue
        target = ROOT / "build/GUNE5D/obj" / f"{name}.o"
        try:
            functions = parsed(target)
            if not functions:
                raise ValueError("target object contains no parsed functions")
            roster.extend((name, fn) for fn in functions)
        except (FileNotFoundError, KeyError, ValueError) as exc:
            failures.append({"unit": name, "status": "UNRESOLVED", "error": str(exc)})
        except (OSError, RuntimeError, SystemExit) as exc:
            failures.append({"unit": name, "status": "FAIL", "error": str(exc)})
    return sorted(set(roster)), failures, {"scope": scope,
                                          "units_selected": len(set(units))}


def screen_side(unit, fn, obj):
    side = {"object": str(Path(obj).relative_to(ROOT))}
    try:
        result = screen_against(unit, fn, obj)
        side.update(result)
        side["status"] = "PASS" if result["verdict"] == "VALUE-EQUAL" else "UNRESOLVED"
        side["interpretation"] = ("no datum-multiset delta found" if side["status"] == "PASS"
                                  else "review candidate, not a proven source defect")
    except (FileNotFoundError, KeyError, ValueError) as exc:
        side.update(status="UNRESOLVED", error=str(exc))
    except (OSError, RuntimeError, SystemExit) as exc:
        side.update(status="FAIL", error=str(exc))
    return side


def audit(pins, rule_count, roster, discovery, selection, edges=None):
    pinned_units = {unit for unit, _fn in pins}
    pin_set = set(pins)
    tally = Counter({key: 0 for key in (
        "functions_selected", "functions_screened_both", "raw_equal", "raw_candidates",
        "post_equal", "post_candidates", "raw_unreadable", "post_unreadable",
        "disagreements", "discovery_failures")})
    rows = []
    for unit, fn in roster:
        try:
            final, raw, has_raw = object_paths(unit, unit in pinned_units, edges)
        except (FileNotFoundError, ValueError) as exc:
            discovery = [*discovery, {"unit": unit, "function": fn,
                                     "status": "UNRESOLVED", "error": str(exc)}]
            tally["functions_selected"] += 1
            tally["raw_unreadable"] += 1
            tally["post_unreadable"] += 1
            continue
        raw_result = screen_side(unit, fn, raw)
        post_result = screen_side(unit, fn, final)
        tally["functions_selected"] += 1
        both = True
        for prefix, result in (("raw", raw_result), ("post", post_result)):
            if "verdict" not in result:
                tally[f"{prefix}_unreadable"] += 1
                both = False
            else:
                tally[f"{prefix}_equal" if result["verdict"] == "VALUE-EQUAL"
                      else f"{prefix}_candidates"] += 1
        if both:
            tally["functions_screened_both"] += 1
            if any(raw_result[key] != post_result[key]
                   for key in ("target_only", "ours_only")):
                tally["disagreements"] += 1
        statuses = {raw_result["status"], post_result["status"]}
        rows.append({"unit": unit, "function": fn, "pinned": (unit, fn) in pin_set,
                     "has_raw_body": has_raw, "raw": raw_result, "post": post_result,
                     "status": "FAIL" if "FAIL" in statuses else
                     "UNRESOLVED" if "UNRESOLVED" in statuses else "PASS"})
    tally["discovery_failures"] = len(discovery)
    statuses = {row["status"] for row in rows + discovery}
    status = ("FAIL" if "FAIL" in statuses else "UNRESOLVED"
              if "UNRESOLVED" in statuses or not roster else "PASS")
    return {"schema_version": SCHEMA_VERSION, "status": status,
            "scope_limit": "datum multisets only; equal does not prove operand binding or semantics",
            "selection": {**selection, "rule_entries": rule_count,
                          "unique_pinned_functions": len(pins),
                          "selected_pinned_functions": sum((u, f) in pin_set for u, f in roster)},
            "tally": dict(tally), "discovery_failures": discovery, "rows": rows}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=str(ROOT / "build/GUNE5D/ce_eq_datum_audit.json"))
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--image", action="store_true")
    selection.add_argument("--unit")
    parser.add_argument("--function")
    parser.add_argument("--objdump", type=Path)
    args = parser.parse_args(argv)
    if args.function and not args.unit:
        parser.error("--function requires --unit")
    fndiff.OBJDUMP = args.objdump or ROOT / "build/binutils" / (
        "powerpc-eabi-objdump.exe" if os.name == "nt" else "powerpc-eabi-objdump")
    try:
        from cv_probe import read_edges
        config = json.loads((ROOT / "config/GUNE5D/webfrank.json").read_text(encoding="utf-8"))
        pins, rule_count = pinned_functions(config)
        roster, discovery, selected = select_functions(pins, args.image, args.unit, args.function)
        result = audit(pins, rule_count, roster, discovery, selected, read_edges())
    except (OSError, ValueError, RuntimeError, SystemExit) as exc:
        result = {"schema_version": SCHEMA_VERSION, "status": "FAIL", "error": str(exc)}
    output = Path(args.out)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"DATUM AUDIT {result['status']}: {result.get('selection', {})}")
    print(json.dumps(result.get("tally", {}), sort_keys=True))
    for row in result.get("rows", []):
        if row["status"] != "PASS":
            print(f"  {row['status']} {row['unit']}::{row['function']} "
                  f"raw={row['raw'].get('verdict', row['raw']['status'])} "
                  f"post={row['post'].get('verdict', row['post']['status'])}")
    if "error" in result:
        print(result["error"])
    print(f"wrote {output}")
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
