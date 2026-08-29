#!/usr/bin/env python3
"""MCP adapter for the deterministic GDL project-memory graph."""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Any

GDL_TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(GDL_TOOLS))

from mcp.server import MCPServer  # noqa: E402

from memory_graph.core import (  # noqa: E402
    REPO_ROOT,
    ensure_database,
    memory_audit,
    memory_stats,
    migration_proposals,
    register_tool_proposal,
    stage_record_proposal,
    search_memory,
    symbol_context,
    tool_context,
    xbox_symbol_context,
)


ROOT = Path(os.environ.get("GDL_MEMORY_ROOT", REPO_ROOT)).resolve()
server = MCPServer(
    "GDL Memory Graph",
    instructions=(
        "Query memory_context before starting work on a GDL function or TU. "
        "Treat legacy notes, migration proposals, and automatic Xbox exact-name "
        "links as evidence candidates, not verified truth. GameCube target data "
        "and accepted evidence-backed records remain authoritative."
    ),
)


@server.tool()
def memory_graph_stats() -> dict[str, Any]:
    """Return graph build metadata and row counts."""
    database = ensure_database(ROOT)
    return {"database": str(database), **memory_stats(ROOT, database)}


@server.tool()
def memory_search(query: str, limit: int = 20) -> dict[str, Any]:
    """Search accepted records, legacy evidence, symbols, and entities."""
    return search_memory(query, root=ROOT, limit=max(1, min(limit, 100)))


@server.tool()
def memory_context(symbol: str, document_limit: int = 12) -> dict[str, Any]:
    """Assemble GameCube, Xbox, claim, parking, and legacy context for a symbol."""
    return symbol_context(
        symbol, root=ROOT, document_limit=max(1, min(document_limit, 50))
    )


@server.tool()
def xbox_context(query: str, limit: int = 20, neighbor_radius: int = 4) -> dict[str, Any]:
    """Search Xbox PDB symbols/types and show source-order module neighbors."""
    return xbox_symbol_context(
        query,
        root=ROOT,
        limit=max(1, min(limit, 100)),
        radius=max(0, min(neighbor_radius, 20)),
    )


@server.tool()
def memory_tool_context(query: str, limit: int = 20) -> dict[str, Any]:
    """Return reviewed usage policy, discovered tools, and legacy provenance."""
    return tool_context(query, root=ROOT, limit=max(1, min(limit, 100)))


@server.tool()
def memory_migration_audit(duplicate_limit: int = 100) -> dict[str, Any]:
    """Report migration coverage and duplicates without modifying source notes."""
    return memory_audit(
        root=ROOT, duplicate_limit=max(1, min(duplicate_limit, 500))
    )


@server.tool()
def memory_pending_proposals(kind: str | None = None, limit: int = 100) -> list[dict[str, Any]]:
    """List unreviewed legacy law/parking proposals."""
    return migration_proposals(
        root=ROOT, kind=kind, limit=max(1, min(limit, 500))
    )


@server.tool()
def memory_register_tool(
    name: str,
    purpose: str,
    tool_kind: str = "external",
    source_path: str | None = None,
    entrypoint: str | None = None,
    usage: list[str] | None = None,
    constraints: list[str] | None = None,
    supersedes: str | None = None,
) -> dict[str, str]:
    """Stage a new or updated tool record in the review-required inbox."""
    path = register_tool_proposal(
        name=name,
        purpose=purpose,
        tool_kind=tool_kind,
        source_path=source_path,
        entrypoint=entrypoint,
        usage=usage or (),
        constraints=constraints or (),
        supersedes=supersedes,
        root=ROOT,
    )
    return {
        "proposal": str(path),
        "review_state": "pending",
        "next": "Integrator reviews the JSON before moving it into records.",
    }


@server.tool()
def memory_propose_record(record: dict[str, Any]) -> dict[str, str]:
    """Validate and stage any supported structured record for review."""
    path = stage_record_proposal(record, root=ROOT)
    return {
        "proposal": str(path),
        "review_state": "pending",
        "next": "Integrator reviews the JSON before moving it into records.",
    }


def main() -> None:
    ensure_database(ROOT)
    server.run()


if __name__ == "__main__":
    main()
