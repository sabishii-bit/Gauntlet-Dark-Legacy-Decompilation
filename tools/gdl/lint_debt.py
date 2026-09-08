#!/usr/bin/env python3
"""Read-only ledger over the reconstruction linter's JSON report.

    python .vscode/lint/fakematch_lint.py src/game --out build/fakematch_lint.json
    python tools/gdl/lint_debt.py                       # per-TU + per-family
    python tools/gdl/lint_debt.py --baseline build/lint_run61.json
    python tools/gdl/lint_debt.py --top 10 --json --out build/c_debt.json

This tool NEVER edits the linter, its rules or its policy, never runs a
scan, and never decides whether a finding is real. It reads one report (and
optionally an older one) and answers the question a run report has to
answer: where is the review debt, and did it move?

WHAT A ROW MEANS. A finding is a REVIEW CANDIDATE, not a proven fakematch;
the report says so itself and this ledger repeats it. Counting them is
useful for direction and for spotting a TU that a change made worse. It is
not a quality score, and driving a count down by rewriting findings
mechanically is explicitly forbidden by AGENTS.md.

OPEN vs REVIEWED EXCEPTION. The report's own `by_rule`/`by_file` count only
UNSUPPRESSED rows, while `findings` carries every row with a `suppressed`
flag. Both numbers are needed -- a TU that waived 40 findings with reasons
has not got smaller, it has been reviewed -- so every table here prints
`open` and `exc` (reviewed exceptions) side by side, derived from `findings`
alone. The derived open totals are cross-checked against the report's own
`by_rule`/`by_file`, and a disagreement is reported rather than hidden.

MOVEMENT IS MEASURED TWICE, ON PURPOSE. A finding's `fingerprint` is
sha256 over (path, rule, enclosing scope, normalized excerpt, ~6 tokens
either side, directive) -- NOT its line. That survives code moving down a
file, which is what makes it the right identity for an exception in
`.vscode/lint/fakematch_lint.toml`. It does NOT survive an edit to the
NEIGHBOURING tokens: an untouched finding whose neighbour changed reads as
one resolved plus one new. So the ledger reports both

  * COUNT movement per (TU, rule), the conservative measure, and
  * FINGERPRINT movement, which is what names the functions involved,

and says which is which instead of merging them. Measured at 20ea60f02 over
the live report (18120 findings): every fingerprint was distinct, but this
tool does multiset arithmetic anyway, so a duplicate can never make a delta
lie.

FRESHNESS IS MEASURED, NOT ASSUMED. The report records the sha256 of every
file it scanned. `--freshness` (default on) re-hashes those files and says
how many have changed since, and which scoped files the report never saw at
all. A stale report is still printed -- with its staleness stated -- because
refusing would hide the numbers a lane already has.

IMPORTABLE CORE: tu_rows, family_rows, compare, finding_counts, key_of,
freshness_rows, format_tu_table, format_family_table, format_movement --
pure over dicts already read (freshness_rows hashes the paths it is given).
"""
import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_REPORT = REPO / "build" / "fakematch_lint.json"
SCAN_COMMAND = ("python .vscode/lint/fakematch_lint.py src/game"
                " --out build/fakematch_lint.json")


class ReportUnavailable(RuntimeError):
    """A missing or unusable report, never an empty successful ledger."""


def load_report(path):
    """Read one report, or REFUSE naming the command that writes it."""
    path = Path(path)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise ReportUnavailable(
            "no lint report at %s (%s).\nGenerate one first:\n    %s"
            % (path, error.strerror or error, SCAN_COMMAND)) from error
    except ValueError as error:
        raise ReportUnavailable("malformed lint report %s: %s"
                                % (path, error)) from error
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        raise ReportUnavailable(
            "unsupported lint report schema in %s: %r (expected 1)"
            % (path, None if not isinstance(data, dict)
               else data.get("schema_version")))
    if not isinstance(data.get("findings"), list):
        raise ReportUnavailable("lint report %s carries no findings list"
                                % path)
    for row in data["findings"]:
        for field in ("rule", "path", "line", "fingerprint", "suppressed"):
            if field not in row:
                raise ReportUnavailable(
                    "lint report %s has a finding without %r: %r"
                    % (path, field, sorted(row)))
    return data


def key_of(row):
    """The (TU, rule) bucket a finding is counted in."""
    return (str(row["path"]).replace("\\", "/"), str(row["rule"]))


def finding_counts(report):
    """{(path, rule): [open, suppressed]} over every row in the report."""
    counts = {}
    for row in report["findings"]:
        slot = counts.setdefault(key_of(row), [0, 0])
        slot[int(bool(row.get("suppressed")))] += 1
    return counts


def _rule_mix(rows):
    return " ".join("%s:%d" % (rule, count) for rule, count
                    in sorted(Counter(rows).items()))


def tu_rows(report):
    """[{path, open, errors, warnings, exceptions, rules}] sorted by debt."""
    table = {}
    for row in report["findings"]:
        path, rule = key_of(row)
        entry = table.setdefault(path, {"path": path, "open": 0, "errors": 0,
                                        "warnings": 0, "exceptions": 0,
                                        "_open_rules": [],
                                        "_exception_rules": []})
        if row.get("suppressed"):
            entry["exceptions"] += 1
            entry["_exception_rules"].append(rule)
            continue
        entry["open"] += 1
        entry["_open_rules"].append(rule)
        if row.get("severity") == "warning":
            entry["warnings"] += 1
        else:
            entry["errors"] += 1
    rows = []
    for entry in table.values():
        entry["rules"] = _rule_mix(entry.pop("_open_rules"))
        entry["exception_rules"] = _rule_mix(entry.pop("_exception_rules"))
        rows.append(entry)
    rows.sort(key=lambda entry: (-entry["open"], -entry["exceptions"],
                                 entry["path"]))
    return rows


def family_rows(report):
    """[{rule, open, errors, warnings, exceptions, files, summary}]."""
    guidance = (report.get("remediation_guidance") or {}).get("rules") or {}
    table = {}
    for row in report["findings"]:
        rule = str(row["rule"])
        entry = table.setdefault(rule, {"rule": rule, "open": 0, "errors": 0,
                                        "warnings": 0, "exceptions": 0,
                                        "files": set(), "top_scopes": []})
        if row.get("suppressed"):
            entry["exceptions"] += 1
            continue
        entry["open"] += 1
        entry["files"].add(key_of(row)[0])
        entry["top_scopes"].append(row.get("scope") or "<file>")
        if row.get("severity") == "warning":
            entry["warnings"] += 1
        else:
            entry["errors"] += 1
    rows = []
    for entry in table.values():
        entry["files"] = len(entry["files"])
        entry["top_scopes"] = [name for name, _ in
                               Counter(entry.pop("top_scopes")).most_common(3)]
        summary = (guidance.get(entry["rule"]) or {}).get("summary") or ""
        entry["summary"] = summary
        rows.append(entry)
    rows.sort(key=lambda entry: (-entry["open"], entry["rule"]))
    return rows


def cross_check(report):
    """[str] disagreements between our totals and the report's own counts."""
    notes = []
    counts = finding_counts(report)
    open_by_rule = Counter()
    open_by_file = Counter()
    for (path, rule), (open_count, _exceptions) in counts.items():
        open_by_rule[rule] += open_count
        open_by_file[path] += open_count
    for label, ours, theirs in (("by_rule", open_by_rule,
                                 report.get("by_rule") or {}),
                                ("by_file", open_by_file,
                                 report.get("by_file") or {})):
        mismatched = {key for key in set(ours) | set(theirs)
                      if int(ours.get(key, 0)) != int(theirs.get(key, 0))}
        if mismatched:
            notes.append("%s disagrees with the report for %d key(s): %s"
                         % (label, len(mismatched),
                            ", ".join(sorted(mismatched)[:5])))
    stated = report.get("unsuppressed")
    derived = sum(open_by_rule.values())
    if isinstance(stated, int) and stated != derived:
        notes.append("report says unsuppressed=%d, findings carry %d"
                     % (stated, derived))
    return notes


def freshness_rows(report, root=REPO):
    """{changed, missing, unscanned, scanned} for the report's own hashes."""
    root = Path(root)
    hashes = report.get("source_sha256") or {}
    changed, missing = [], []
    for name, expected in sorted(hashes.items()):
        path = root / name
        try:
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError:
            missing.append(name)
            continue
        if actual != expected:
            changed.append(name)
    scope = report.get("source_scope")
    unscanned = []
    if scope:
        base = root / scope
        if base.is_dir():
            for path in sorted(base.rglob("*")):
                if path.suffix.lower() not in (".c", ".cpp"):
                    continue
                name = path.relative_to(root).as_posix()
                if name not in hashes:
                    unscanned.append(name)
    return {"scanned": len(hashes), "changed": changed, "missing": missing,
            "unscanned": unscanned}


def compare(baseline, current):
    """Movement between two reports: counts first, fingerprints second.

    `counts` is per (path, rule) and is the conservative measure. `new` and
    `resolved` are multiset differences over fingerprints, carrying the rows
    themselves so a caller can name the functions involved.
    """
    before, after = finding_counts(baseline), finding_counts(current)
    counts = []
    for key in sorted(set(before) | set(after)):
        was_open, was_exc = before.get(key, (0, 0))
        now_open, now_exc = after.get(key, (0, 0))
        if (was_open, was_exc) == (now_open, now_exc):
            continue
        counts.append({"path": key[0], "rule": key[1],
                       "open_before": was_open, "open_after": now_open,
                       "open_delta": now_open - was_open,
                       "exceptions_before": was_exc,
                       "exceptions_after": now_exc,
                       "exceptions_delta": now_exc - was_exc})
    counts.sort(key=lambda row: (-abs(row["open_delta"]), row["path"],
                                 row["rule"]))

    def by_fingerprint(report):
        table = {}
        for row in report["findings"]:
            table.setdefault(row["fingerprint"], []).append(row)
        return table

    old_rows, new_rows = by_fingerprint(baseline), by_fingerprint(current)
    new, resolved = [], []
    for fingerprint, rows in new_rows.items():
        extra = len(rows) - len(old_rows.get(fingerprint, ()))
        new.extend(rows[:extra] if extra > 0 else ())
    for fingerprint, rows in old_rows.items():
        extra = len(rows) - len(new_rows.get(fingerprint, ()))
        resolved.extend(rows[:extra] if extra > 0 else ())
    order = (lambda row: (str(row.get("path")), str(row.get("rule")),
                          int(row.get("line") or 0)))
    new.sort(key=order)
    resolved.sort(key=order)
    return {"counts": counts, "new": new, "resolved": resolved,
            "open_before": sum(v[0] for v in before.values()),
            "open_after": sum(v[0] for v in after.values()),
            "exceptions_before": sum(v[1] for v in before.values()),
            "exceptions_after": sum(v[1] for v in after.values())}


def _scope_summary(rows, limit=6):
    """`fn (n)` for the functions a set of findings sits in."""
    counted = Counter(str(row.get("scope") or "<file>") for row in rows)
    return ", ".join("%s (%d)" % (name, count)
                     for name, count in counted.most_common(limit))


def format_tu_table(rows, top=None):
    out = ["PER-TU DEBT (open = unsuppressed review candidates,"
           " exc = reviewed exceptions)",
           "  %-46s %6s %6s %5s %5s  %s"
           % ("translation unit", "open", "err", "warn", "exc", "rules")]
    shown = rows if top is None else rows[:top]
    for row in shown:
        out.append("  %-46s %6d %6d %5d %5d  %s"
                   % (row["path"], row["open"], row["errors"],
                      row["warnings"], row["exceptions"], row["rules"]))
    if top is not None and len(rows) > top:
        rest = rows[top:]
        out.append("  ... %d more TU(s), %d open finding(s) between them"
                   % (len(rest), sum(row["open"] for row in rest)))
    return out


def format_family_table(rows):
    out = ["PER-FAMILY DEBT",
           "  %-7s %7s %6s %5s %5s %6s  %s"
           % ("rule", "open", "err", "warn", "exc", "files", "busiest scopes")]
    for row in rows:
        out.append("  %-7s %7d %6d %5d %5d %6d  %s"
                   % (row["rule"], row["open"], row["errors"],
                      row["warnings"], row["exceptions"], row["files"],
                      ", ".join(row["top_scopes"])))
    return out


def format_movement(movement, top=None):
    out = ["MOVEMENT vs BASELINE",
           "  open findings %d -> %d (%+d);  reviewed exceptions %d -> %d (%+d)"
           % (movement["open_before"], movement["open_after"],
              movement["open_after"] - movement["open_before"],
              movement["exceptions_before"], movement["exceptions_after"],
              movement["exceptions_after"] - movement["exceptions_before"])]
    rows = movement["counts"]
    if not rows:
        out.append("  no (TU, rule) bucket changed count")
    else:
        rose = [row for row in rows if row["open_delta"] > 0]
        fell = [row for row in rows if row["open_delta"] < 0]
        out.append("  %d bucket(s) ROSE, %d FELL (count movement, the"
                   " conservative measure)" % (len(rose), len(fell)))
        for label, group in (("ROSE", rose), ("FELL", fell)):
            for row in (group if top is None else group[:top]):
                out.append("    %-4s %-40s %-6s %4d -> %-4d (%+d)  exc %d -> %d"
                           % (label, row["path"], row["rule"],
                              row["open_before"], row["open_after"],
                              row["open_delta"], row["exceptions_before"],
                              row["exceptions_after"]))
    out.append("  FINGERPRINT movement: %d new, %d resolved  (a fingerprint"
               " covers the finding's NEIGHBOURING tokens, so edited"
               % (len(movement["new"]), len(movement["resolved"])))
    out.append("   surroundings can retire and re-raise the same finding;"
               " trust the counts above for size, this for names)")
    by_tu = {}
    for row in movement["new"]:
        by_tu.setdefault(str(row.get("path")), []).append(row)
    for path in sorted(by_tu, key=lambda name: -len(by_tu[name])):
        rows = by_tu[path]
        out.append("    NEW  %-40s %3d in %s"
                   % (path, len(rows), _scope_summary(rows)))
    return out


def render(report, path, *, top=None, baseline=None, baseline_path=None,
           freshness=True, root=REPO):
    out = ["LINT DEBT LEDGER  (read-only; the linter, its rules and its"
           " policy are never touched here)",
           "  report      %s" % Path(path),
           "  scope       %s   files scanned %s   engine %s"
           % (report.get("source_scope"), report.get("files_scanned"),
              report.get("engine")),
           "  rules       %s" % ", ".join(report.get("rules_selected") or []),
           "  identity    rules %s  policy %s  guidance %s"
           % (str(report.get("rules_sha256"))[:12],
              str(report.get("policy_sha256"))[:12],
              str(report.get("guidance_sha256"))[:12]),
           "  totals      %s finding(s): %s open (%s error, %s warning),"
           " %s reviewed exception(s)"
           % (len(report["findings"]), report.get("unsuppressed"),
              report.get("errors"), report.get("warnings"),
              report.get("suppressed")),
           "  caveat      %s" % report.get("interpretation"),
           "  parse recovery regions: %d (coverage limit, not a clean bill)"
           % len(report.get("parse_recovery") or [])]
    for note in cross_check(report):
        out.append("  CROSS-CHECK  %s" % note)
    if freshness:
        fresh = freshness_rows(report, root)
        state = ("current" if not fresh["changed"] and not fresh["missing"]
                 else "STALE")
        out.append("  freshness   %s: %d of %d scanned file(s) changed since"
                   " the scan, %d missing, %d scoped file(s) never scanned"
                   % (state, len(fresh["changed"]), fresh["scanned"],
                      len(fresh["missing"]), len(fresh["unscanned"])))
        for name in (fresh["changed"] + fresh["missing"])[:5]:
            out.append("    changed/missing: %s" % name)
        if fresh["changed"] or fresh["missing"]:
            out.append("    re-scan before quoting these numbers:  %s"
                       % SCAN_COMMAND)
    out.append("")
    out.extend(format_family_table(family_rows(report)))
    out.append("")
    out.extend(format_tu_table(tu_rows(report), top))
    if baseline is not None:
        out.append("")
        out.append("  baseline    %s" % Path(baseline_path))
        out.extend(format_movement(compare(baseline, report), top))
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("report", nargs="?", default=str(DEFAULT_REPORT),
                        help="lint report JSON (default: %s)"
                             % DEFAULT_REPORT.relative_to(REPO).as_posix())
    parser.add_argument("--baseline", help="an older report to compare against")
    parser.add_argument("--top", type=int, default=15,
                        help="rows per table section (0 = all)")
    parser.add_argument("--no-freshness", action="store_true",
                        help="skip re-hashing the scanned sources")
    parser.add_argument("--json", action="store_true",
                        help="emit the ledger as JSON instead of a table")
    parser.add_argument("--out", type=Path, help="write the JSON ledger here")
    args = parser.parse_args(argv)
    top = None if not args.top else args.top
    try:
        report = load_report(args.report)
        baseline = load_report(args.baseline) if args.baseline else None
    except ReportUnavailable as error:
        print("LINT DEBT REFUSED: %s" % error)
        return 2
    if args.json or args.out:
        payload = {"schema_version": 1, "report": str(args.report),
                   "scope": report.get("source_scope"),
                   "totals": {"findings": len(report["findings"]),
                              "open": report.get("unsuppressed"),
                              "errors": report.get("errors"),
                              "warnings": report.get("warnings"),
                              "exceptions": report.get("suppressed")},
                   "cross_check": cross_check(report),
                   "families": family_rows(report),
                   "units": tu_rows(report)}
        if not args.no_freshness:
            payload["freshness"] = freshness_rows(report)
        if baseline is not None:
            movement = compare(baseline, report)
            payload["baseline"] = str(args.baseline)
            payload["movement"] = {
                "open_before": movement["open_before"],
                "open_after": movement["open_after"],
                "exceptions_before": movement["exceptions_before"],
                "exceptions_after": movement["exceptions_after"],
                "counts": movement["counts"],
                "new": [{"path": row.get("path"), "rule": row.get("rule"),
                         "line": row.get("line"), "scope": row.get("scope"),
                         "fingerprint": row.get("fingerprint")}
                        for row in movement["new"]],
                "resolved": [{"path": row.get("path"), "rule": row.get("rule"),
                              "line": row.get("line"), "scope": row.get("scope"),
                              "fingerprint": row.get("fingerprint")}
                             for row in movement["resolved"]]}
        text = json.dumps(payload, indent=1)
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(text + "\n", encoding="utf-8")
            print("wrote %s" % args.out)
        if args.json:
            print(text)
        return 0
    for line in render(report, args.report, top=top, baseline=baseline,
                       baseline_path=args.baseline,
                       freshness=not args.no_freshness):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
