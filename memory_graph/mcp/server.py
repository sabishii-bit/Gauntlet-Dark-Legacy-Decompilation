#!/usr/bin/env python3
"""MCP adapter for the deterministic GDL project-memory graph.

Read tools are generated from `memory_graph.core.build_surface_ops()`, the
single registry the CLI also derives its query surface from. Add new query
operations to the registry, not here; only the review-gated write tools are
defined explicitly below.
"""

from __future__ import annotations

import inspect
import os
import sys
from pathlib import Path
from typing import Any

GDL_REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(GDL_REPO_ROOT))

from mcp.server import MCPServer  # noqa: E402

from memory_graph.core import (  # noqa: E402
    REPO_ROOT,
    build_surface_ops,
    ensure_database,
    register_tool_proposal,
    stage_record_proposal,
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


def _register_surface_tools() -> None:
    for op in build_surface_ops():
        def impl(_op=op, **kwargs: Any) -> Any:
            return _op.call(ROOT, None, **_op.clamped(kwargs))

        parameters = []
        annotations: dict[str, Any] = {"return": Any}
        for param in op.params:
            annotation = param.annotation if param.required else param.annotation | None
            parameters.append(
                inspect.Parameter(
                    param.name,
                    inspect.Parameter.KEYWORD_ONLY,
                    default=inspect.Parameter.empty if param.required else param.default,
                    annotation=annotation,
                )
            )
            annotations[param.name] = annotation
        impl.__name__ = op.mcp_name
        impl.__qualname__ = op.mcp_name
        impl.__doc__ = op.doc
        impl.__signature__ = inspect.Signature(parameters, return_annotation=Any)
        impl.__annotations__ = annotations
        server.tool(name=op.mcp_name)(impl)


_register_surface_tools()


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
