#!/usr/bin/env python3
"""Build and query the GDL project-memory graph.

Examples:
  python memory_graph/gdlmem.py build
  python memory_graph/gdlmem.py context PlayerMotion
  python memory_graph/gdlmem.py xbox pool_garbage_collect
  python memory_graph/gdlmem.py search "register web topology"
  python memory_graph/gdlmem.py proposals --kind parking_legacy

Query subcommands are generated from `memory_graph.core.build_surface_ops()`,
the single registry every consumer (this CLI, the MCP adapter) derives its
surface from. Add a new query op to the registry, not here.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from memory_graph.core import (
    MemoryGraphError,
    REPO_ROOT,
    build_database,
    build_surface_ops,
    ensure_database,
    memory_stats,
    prune_attempts,
    register_tool_proposal,
    stage_record_proposal,
)


def _print(value: object, compact: bool) -> None:
    print(json.dumps(value, indent=None if compact else 2, sort_keys=True, default=str))


def build_parser() -> tuple[argparse.ArgumentParser, dict[str, object]]:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    parser.add_argument("--db", type=Path, help="override generated SQLite path")
    parser.add_argument("--compact", action="store_true", help="emit compact JSON")
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
    propose_record.add_argument("json_file", type=Path)

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
        elif args.command == "propose-record":
            record = json.loads(args.json_file.read_text(encoding="utf-8-sig"))
            path = stage_record_proposal(record, root=root)
            result = {
                "proposal": str(path),
                "review_state": "pending",
                "next": "review the JSON, then move it from memory_graph/inbox to records",
            }
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
    _print(result, args.compact)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
