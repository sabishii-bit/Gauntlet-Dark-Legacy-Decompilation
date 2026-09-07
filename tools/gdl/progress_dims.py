#!/usr/bin/env python3
"""The progress DIMENSIONS report: several overlapping numbers, never one.

Restores the postprocessor split line that `configure.py progress` used to
print and that was removed with the memory graph, and adds the two dimensions
AGENTS.md requires alongside it -- manual (unproven, warning-bearing)
exceptions, and actual source-linked coverage. Every dimension prints its own
numerator and denominator, and NONE of them are summed: a byte can be
source-linked AND rule-served AND manually excepted at the same time, so a
single "matched %" would hide exactly the distinctions this file exists for.

    python tools/gdl/progress_dims.py
    python tools/gdl/progress_dims.py --json
    python tools/gdl/progress_dims.py --report R --rules W --edges E

Dimensions, all byte-weighted over report.json's total code:

  STRICT       fuzzy-100 functions whose (unit, function) carries NO rule in
               config/GUNE5D/webfrank.json -- the stock compiler's own bytes.
  EQUIVALENT   fuzzy-100 functions that ARE rule-served: proven equivalent
               modulo allocator/scheduler variance, not stock output. This is
               the whole rule-served set, which is the convention the retired
               line used; the manual subset below is disclosed separately, not
               subtracted, so the two numbers stay comparable across runs.
  MANUAL       the subset of EQUIVALENT whose rule carries an
               `unproven_recolor_audit` anywhere in its body. These are
               explicitly disclosed unproven exceptions and must never be
               reported as machine-proven. Flag detection is `_manual` from
               tools/gdl/build_provenance.py, imported rather than re-copied.
  SOURCE-LINKED  code bytes of units the generator actually links from OUR
               object (`linkage == "source"` in build/<v>/build_edges.json --
               the `Object(Matching, ...)` TUs). This is a LINKAGE dimension:
               a NonMatching TU's fuzzy-100 function still links the extracted
               target object, so its STRICT credit is not linked coverage.

Refusals (exit 2), because a missing input must not read as a zero dimension:
an absent/unreadable report, rule or edge file; a report whose scores or sizes
are malformed (`build_provenance.validate_report`); zero total code bytes; a
webfrank config with no `units` key -- iterating that file's ROOT finds two
keys and zero pins, which reads exactly like "no rules exist" and would
report the entire matched set as STRICT.

IMPORTABLE CORE: dimensions, load_inputs -- pure over parsed data, no build,
no printing; importing this module has no side effects.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.gdl.build_provenance import _manual, validate_report
from tools.gdl.progress import report_unit_key

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parents[2]
REPORT = REPO / "build" / VERSION / "report.json"
RULES = REPO / "config" / VERSION / "webfrank.json"
EDGES = REPO / "build" / VERSION / "build_edges.json"


class Refusal(Exception):
    """A dimension could not be measured; never degrade it to zero."""


def _read_json(path, what):
    path = Path(path)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        raise Refusal(f"missing {what}: {path} ({error.strerror or error})")
    try:
        return json.loads(text)
    except ValueError as error:
        raise Refusal(f"unreadable {what}: {path}: {error}")


def load_inputs(report=None, rules=None, edges=None):
    """(report, rules, edges) parsed, or Refusal. `edges` may be None."""
    report_data = _read_json(report or REPORT, "report.json")
    rules_data = _read_json(rules or RULES, "webfrank.json")
    if not isinstance(rules_data, dict) or "units" not in rules_data:
        raise Refusal(
            "webfrank config has no top-level 'units' key; refusing to report"
            " every matched byte as STRICT")
    edges_path = Path(edges or EDGES)
    edges_data = _read_json(edges_path, "build_edges.json") if edges_path.exists() else None
    return report_data, rules_data, edges_data


def _pins(rules_data):
    """{(unit, function): manual?} over every configured rule."""
    pinned = {}
    for unit, entries in rules_data["units"].items():
        if not isinstance(entries, list):
            raise Refusal(f"webfrank unit {unit!r} is not a rule list")
        for entry in entries:
            if not isinstance(entry, dict) or "function" not in entry:
                raise Refusal(f"webfrank unit {unit!r} has a rule with no function")
            key = (unit, entry["function"])
            pinned[key] = pinned.get(key, False) or _manual(entry)
    return pinned


def _source_linked_units(edges_data):
    """{report unit name: code_size} for units linked from OUR object."""
    linked = {}
    for unit in edges_data.get("units", []):
        name = f"{unit['module']}/{Path(unit['name']).with_suffix('').as_posix()}"
        if unit.get("linkage") == "source":
            linked[name] = int(unit.get("code_size", 0))
    return linked


def dimensions(report_data, rules_data, edges_data=None):
    """Every dimension, with its own numerator and denominator. No sums."""
    validate_report(report_data)
    pinned = _pins(rules_data)
    linked = _source_linked_units(edges_data) if edges_data else None
    total = 0
    rows = {name: {"functions": 0, "bytes": 0}
            for name in ("strict", "equivalent", "manual", "source_linked",
                         "source_linked_matched")}
    seen_pins = set()
    for unit in report_data.get("units", []):
        name = unit.get("name", "")
        key = report_unit_key(name)
        is_linked = linked is not None and name in linked
        for function in unit.get("functions", []):
            size = int(function.get("size", 0))
            total += size
            if is_linked:
                rows["source_linked"]["functions"] += 1
                rows["source_linked"]["bytes"] += size
            if float(function.get("fuzzy_match_percent", 0)) < 100.0:
                continue
            if is_linked:
                rows["source_linked_matched"]["functions"] += 1
                rows["source_linked_matched"]["bytes"] += size
            pin = (key, function.get("name"))
            if pin in pinned:
                seen_pins.add(pin)
                rows["equivalent"]["functions"] += 1
                rows["equivalent"]["bytes"] += size
                if pinned[pin]:
                    rows["manual"]["functions"] += 1
                    rows["manual"]["bytes"] += size
            else:
                rows["strict"]["functions"] += 1
                rows["strict"]["bytes"] += size
    if not total:
        raise Refusal("report.json contains zero code bytes")
    for row in rows.values():
        row["percent"] = 100.0 * row["bytes"] / total
    return {
        "schema_version": 1,
        "total_code_bytes": total,
        "total_functions": sum(len(u.get("functions", []))
                               for u in report_data.get("units", [])),
        "configured_rules": sum(len(v) for v in rules_data["units"].values()),
        "pinned_entries": len(pinned),
        "manual_pinned_entries": sum(1 for v in pinned.values() if v),
        # A pin whose function is not fuzzy-100 earns no EQUIVALENT credit and
        # is not a bug; a pin whose function is absent from the report is a
        # join failure, and naming it is cheaper than a silently low number.
        "pins_without_matched_report_function": sorted(
            "::".join(pin) for pin in set(pinned) - seen_pins),
        "source_linked_available": linked is not None,
        "source_linked_units": len(linked) if linked is not None else None,
        "dimensions": rows,
        "scope": ("Overlapping dimensions over report.json's fuzzy-100 scores."
                  " Not raw-byte, relocation, data or semantic certification,"
                  " and deliberately not summed into one matched percentage."),
    }


def format_lines(result):
    rows = result["dimensions"]
    total = result["total_code_bytes"]

    def line(label, key, note):
        row = rows[key]
        return (f"  {label}: {row['percent']:.2f}% ({row['functions']} fns;"
                f" {row['bytes']} / {total} bytes) -- {note}")

    out = [
        "Progress dimensions (byte-weighted over total code; NOT summable):",
        (f"  Postprocessor split: STRICT matched"
         f" {rows['strict']['percent']:.2f}% ({rows['strict']['functions']} fns)"
         f" + EQUIVALENT {rows['equivalent']['percent']:.2f}%"
         f" ({rows['equivalent']['functions']} fns)"),
        line("STRICT", "strict", "fuzzy-100 with no configured rule"),
        line("EQUIVALENT", "equivalent",
             f"fuzzy-100 rule-served ({result['configured_rules']} rules over"
             f" {result['pinned_entries']} pinned entries)"),
        line("MANUAL EXCEPTION", "manual",
             "subset of EQUIVALENT carrying unproven_recolor_audit;"
             " disclosed, never machine-proven"),
    ]
    if result["source_linked_available"]:
        out.append(line("SOURCE-LINKED", "source_linked",
                        f"code of the {result['source_linked_units']} units"
                        " linked from our object (Object(Matching, ...))"))
        out.append(line("SOURCE-LINKED matched", "source_linked_matched",
                        "fuzzy-100 subset of the above"))
    else:
        out.append("  SOURCE-LINKED: UNAVAILABLE -- no build_edges.json;"
                   " run `python configure.py` (not reported as zero)")
    if result["pins_without_matched_report_function"]:
        out.append("  pins with no fuzzy-100 report function: "
                   + ", ".join(result["pins_without_matched_report_function"]))
    out.append("  Dimensions overlap: a source-linked byte may also be STRICT,"
               " EQUIVALENT or MANUAL. Do not add them.")
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--report", type=Path, help="report.json (default: build/%s/report.json)" % VERSION)
    parser.add_argument("--rules", type=Path, help="webfrank.json (default: config/%s/webfrank.json)" % VERSION)
    parser.add_argument("--edges", type=Path, help="build_edges.json (default: build/%s/build_edges.json)" % VERSION)
    parser.add_argument("--json", action="store_true", help="emit the machine-readable dimensions")
    args = parser.parse_args(argv)
    try:
        result = dimensions(*load_inputs(args.report, args.rules, args.edges))
    except (Refusal, ValueError, KeyError, TypeError) as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print("\n".join(format_lines(result)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
