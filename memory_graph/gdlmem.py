#!/usr/bin/env python3
"""Build and query the GDL project-memory graph.

Examples:
  python memory_graph/gdlmem.py build
  python memory_graph/gdlmem.py context PlayerMotion
  python memory_graph/gdlmem.py xbox pool_garbage_collect
  python memory_graph/gdlmem.py search "register web topology"
  python memory_graph/gdlmem.py proposals --kind parking_legacy
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
    default_database_path,
    ensure_database,
    memory_stats,
    memory_audit,
    migration_proposals,
    search_memory,
    register_tool_proposal,
    stage_record_proposal,
    symbol_context,
    validate_records,
    tool_context,
    xbox_symbol_context,
)


def _print(value: object, compact: bool) -> None:
    print(json.dumps(value, indent=None if compact else 2, sort_keys=True, default=str))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    parser.add_argument("--db", type=Path, help="override generated SQLite path")
    parser.add_argument("--compact", action="store_true", help="emit compact JSON")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="atomically rebuild the SQLite graph")
    build.add_argument("--no-legacy", action="store_true", help="exclude ignored legacy notes")

    subparsers.add_parser("ensure", help="create or refresh the graph when appropriate")
    subparsers.add_parser("stats", help="show graph counts and metadata")
    subparsers.add_parser("validate", help="validate durable and inbox JSON records")
    subparsers.add_parser("audit", help="report duplicates and migration coverage without deleting anything")

    search = subparsers.add_parser("search", help="search documents, symbols, and entities")
    search.add_argument("query")
    search.add_argument("--limit", type=int, default=20)

    context = subparsers.add_parser("context", help="assemble context for a GameCube symbol")
    context.add_argument("symbol")
    context.add_argument("--document-limit", type=int, default=12)

    xbox = subparsers.add_parser("xbox", help="search Xbox PDB symbols and module neighbors")
    xbox.add_argument("query")
    xbox.add_argument("--limit", type=int, default=20)
    xbox.add_argument("--radius", type=int, default=4)

    tool = subparsers.add_parser("tool", help="show registered tool guidance and source evidence")
    tool.add_argument("query")
    tool.add_argument("--limit", type=int, default=20)

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

    proposals = subparsers.add_parser("proposals", help="list unreviewed migration proposals")
    proposals.add_argument("--kind")
    proposals.add_argument("--state", default="pending")
    proposals.add_argument("--limit", type=int, default=100)

    args = parser.parse_args(argv)
    root = args.root.resolve()
    database = args.db.resolve() if args.db else None
    try:
        if args.command == "build":
            result = build_database(root, database, include_legacy=not args.no_legacy)
        elif args.command == "ensure":
            path = ensure_database(root, database)
            result = {"database": str(path), "stats": memory_stats(root, path)}
        elif args.command == "stats":
            ensure_database(root, database)
            result = {"database": str(database or default_database_path(root)), **memory_stats(root, database)}
        elif args.command == "validate":
            result = validate_records(root)
        elif args.command == "audit":
            result = memory_audit(root=root, db_path=database)
        elif args.command == "search":
            result = search_memory(args.query, root=root, db_path=database, limit=args.limit)
        elif args.command == "context":
            result = symbol_context(
                args.symbol, root=root, db_path=database,
                document_limit=args.document_limit,
            )
        elif args.command == "xbox":
            result = xbox_symbol_context(
                args.query, root=root, db_path=database,
                limit=args.limit, radius=args.radius,
            )
        elif args.command == "tool":
            result = tool_context(args.query, root=root, db_path=database, limit=args.limit)
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
            record = json.loads(args.json_file.read_text(encoding="utf-8"))
            path = stage_record_proposal(record, root=root)
            result = {
                "proposal": str(path),
                "review_state": "pending",
                "next": "review the JSON, then move it from memory_graph/inbox to records",
            }
        elif args.command == "proposals":
            result = migration_proposals(
                root=root, db_path=database, kind=args.kind,
                state=args.state, limit=args.limit,
            )
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
