#!/usr/bin/env python3
"""Build and query the GDL project-memory graph.

Examples:
  python memory_graph/gdlmem.py build
  python memory_graph/gdlmem.py context PlayerMotion
  python memory_graph/gdlmem.py xbox pool_garbage_collect
  python memory_graph/gdlmem.py search "register web topology"
  python memory_graph/gdlmem.py proposals --kind parking_legacy

RESIDUAL-FIRST RETRIEVAL (run 29). Search by what the diff LOOKS LIKE, not
by function name:

  gdlmem.py laws --residual "+1 addi -1 li"   # who else had this residual
  gdlmem.py find --family live-zero-remat     # three LABELLED tiers: the
                                              # verified family, the legacy
                                              # residual_class widening, and
                                              # (with --include-candidates 1)
                                              # quarantined extractor guesses
                                              # — never total the tiers
  gdlmem.py find --capability dataflow-equivalence   # parks waiting on a
                                              # capability, i.e. its payoff
  gdlmem.py laws --query "live zero remat"    # matches id SLUG WORDS and
                                              # webfrank pin `mechanism`
                                              # prose, not just law text

Attempt records carry an optional TOP-LEVEL `residual` object
{signature, family, capability_needed, measured_at} — distinct from the
legacy `attributes.residual` prose, which is never read as structure. Law
records carry optional `falsifier` and `asserted_by`; multi-edit parks carry
`held_fixed`.
`propose-record --template attempt|claim` prints the shapes. Three gates run
on NEW proposals only: a necessity-language law (must/requires/cannot/only)
needs a `falsifier`; a record reclassifying a function postprocessor-class
must quote instruction counts as N/N; a `probed_form` enumerating more than
one edit needs `held_fixed`.

`brief <tu>` now leads with OPEN 10b HYPOTHESES, then vetoed axes,
refutations and webfrank pins with their provenance class. Every number it
prints is read from disk and carries a staleness banner — REMEASURE before
quoting one.

Query subcommands are generated from `memory_graph.core.build_surface_ops()`,
the single registry every consumer (this CLI, the MCP adapter) derives its
surface from. Add a new query op to the registry, not here.
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from memory_graph.core import (
    MemoryGraphError,
    REPO_ROOT,
    accept_records,
    build_database,
    build_surface_ops,
    ensure_database,
    memory_stats,
    prune_attempts,
    record_template,
    register_tool_proposal,
    rename_symbol,
    stage_record_proposal,
)

SPILL_THRESHOLD = 24000


def build_parser() -> tuple[argparse.ArgumentParser, dict[str, object]]:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    parser.add_argument("--db", type=Path, help="override generated SQLite path")
    parser.add_argument("--compact", action="store_true", help="emit compact JSON")
    parser.add_argument("--out", type=Path, default=None,
                        help="write result JSON to this file (UTF-8, no BOM)"
                             " instead of stdout — immune to PowerShell pipe"
                             " re-encoding and Bash/Windows path mismatches")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="atomically rebuild the SQLite graph")
    build.add_argument("--no-legacy", action="store_true", help="exclude ignored legacy notes")
    subparsers.add_parser("ensure", help="create or refresh the graph when appropriate")

    ops = {}
    for op in build_surface_ops():
        sub = subparsers.add_parser(op.name, help=op.doc)
        for param in op.params:
            if param.required:
                sub.add_argument(param.name, help=param.help or None)
            else:
                sub.add_argument(
                    "--" + param.name.replace("_", "-"),
                    dest=param.name,
                    type=param.annotation if param.annotation in (int, str) else str,
                    default=param.default,
                    help=param.help or None,
                )
        ops[op.name] = op

    register_tool = subparsers.add_parser(
        "register-tool", help="write a review-required tool record to memory_graph/inbox"
    )
    register_tool.add_argument("name")
    register_tool.add_argument("--purpose", required=True)
    register_tool.add_argument("--kind", default="external")
    register_tool.add_argument("--source-path")
    register_tool.add_argument("--entrypoint")
    register_tool.add_argument("--status", default="active")
    register_tool.add_argument("--usage", action="append", default=[])
    register_tool.add_argument("--constraint", action="append", default=[])
    register_tool.add_argument("--supersedes")

    propose_record = subparsers.add_parser(
        "propose-record",
        help="validate and stage a structured JSON record for review",
    )
    propose_record.add_argument("json_file", type=Path, nargs="?")
    propose_record.add_argument("--dry-run", action="store_true",
                                help="validate fully but write nothing")
    propose_record.add_argument(
        "--template",
        choices=("attempt", "claim", "evidence", "entity", "edge",
                 "work_claim", "tool"),
        help="print a correctly-shaped skeleton for this kind and exit"
             " (fill <REQUIRED:...>, delete unused <OPTIONAL:...> keys)")

    accept = subparsers.add_parser(
        "accept",
        help="integrator: move inbox records into records/, release claims, "
             "stage the exact paths, and rebuild the graph",
    )
    accept.add_argument("record_ids", nargs="*",
                        help="inbox record ids to accept")
    accept.add_argument("--release", action="append", default=[],
                        help="work_claim id to release (repeatable)")
    accept.add_argument("--any-branch", action="store_true",
                        help="override the integrator-only main-branch guard")

    rename = subparsers.add_parser(
        "rename-symbol",
        help="atomic project-wide symbol rename: collision check, "
             "symbols.txt+src+include, graph-anchor patch, stale .s/.o "
             "cleanup (dry-run unless --apply)",
    )
    rename.add_argument("old_name")
    rename.add_argument("new_name")
    rename.add_argument("--apply", action="store_true",
                        help="execute (default: report the plan)")

    prune = subparsers.add_parser(
        "prune-attempts",
        help="eject attempt records beyond the per-function cap (dry-run "
             "unless --apply); integrator-only, rebuild after applying",
    )
    prune.add_argument("--limit", type=int, default=None,
                       help="override the per-function attempt cap")
    prune.add_argument("--apply", action="store_true",
                       help="delete the ejected files (default: report only)")

    return parser, ops


def main(argv: list[str] | None = None) -> int:
    parser, ops = build_parser()
    args = parser.parse_args(argv)
    root = args.root.resolve()
    database = args.db.resolve() if args.db else None
    try:
        if args.command == "build":
            result = build_database(root, database, include_legacy=not args.no_legacy)
        elif args.command == "ensure":
            path = ensure_database(root, database)
            result = {"database": str(path), "stats": memory_stats(root, path)}
        elif args.command in ops:
            op = ops[args.command]
            values = {p.name: getattr(args, p.name) for p in op.params}
            result = op.call(root, database, **op.clamped(values))
        elif args.command == "register-tool":
            path = register_tool_proposal(
                name=args.name, purpose=args.purpose, tool_kind=args.kind,
                source_path=args.source_path, entrypoint=args.entrypoint,
                status=args.status, usage=args.usage, constraints=args.constraint,
                supersedes=args.supersedes, root=root,
            )
            result = {
                "proposal": str(path),
                "review_state": "pending",
                "next": "review the JSON, then move it from memory_graph/inbox to records",
            }
        elif args.command == "propose-record" and args.template:
            result = record_template(args.template)
        elif args.command == "propose-record":
            if args.json_file is None:
                parser.error("propose-record needs a json_file or --template")
            record = json.loads(args.json_file.read_text(encoding="utf-8-sig"))
            source = args.json_file.resolve()
            inbox_dir = (root / "memory_graph" / "inbox").resolve()
            in_place = source if source.parent == inbox_dir else None
            path = stage_record_proposal(record, root=root, in_place=in_place,
                                         dry_run=args.dry_run)
            result = {
                "proposal": str(path),
                "review_state": "valid (not staged)" if args.dry_run
                                else "pending",
                "next": ("re-run without --dry-run to stage"
                         if args.dry_run else
                         "review the JSON, then move it from"
                         " memory_graph/inbox to records"),
            }
        elif args.command == "accept":
            result = accept_records(
                args.record_ids, release=args.release, root=root,
                allow_any_branch=args.any_branch,
            )
        elif args.command == "rename-symbol":
            result = rename_symbol(args.old_name, args.new_name, root=root,
                                   apply=args.apply)
        elif args.command == "prune-attempts":
            kwargs = {"apply": args.apply}
            if args.limit is not None:
                kwargs["limit"] = args.limit
            result = prune_attempts(root, **kwargs)
        else:
            parser.error(f"unknown command {args.command}")
            return 2
    except (MemoryGraphError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"gdlmem: error: {error}", file=sys.stderr)
        return 1
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(
            json.dumps(result, indent=2, sort_keys=True, default=str),
            encoding="utf-8",
        )
        print(f"wrote {args.out}")
        return 0
    payload = json.dumps(result, indent=None if args.compact else 2,
                         sort_keys=True, default=str)
    # Shell pipes (Bash tool, PowerShell tail) silently truncate large
    # stdout; spill big results to a file and print the pointer instead.
    if len(payload) > SPILL_THRESHOLD:
        spill_dir = root / "build" / "gdlmem_out"
        spill_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
        spill = spill_dir / f"{args.command}-{stamp}.json"
        spill.write_text(payload, encoding="utf-8")
        print(json.dumps({
            "large_output": str(spill),
            "bytes": len(payload),
            "hint": "result exceeds safe stdout size; Read the file"
                    " (do not re-run the query)",
        }, indent=2))
        return 0
    print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
