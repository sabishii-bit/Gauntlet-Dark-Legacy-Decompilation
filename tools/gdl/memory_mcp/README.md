# GDL Memory MCP

The MCP adapter is isolated from the build dependencies. Run it from the
repository root with its locked `uv` environment:

```sh
uv run --project tools/gdl/memory_mcp python tools/gdl/memory_mcp/server.py
```

Set `GDL_MEMORY_ROOT` when the host starts the server outside this checkout.
The generated database is shared through Git's common directory, so registered
worktrees query the same materialized graph.

Example stdio MCP configuration:

```json
{
  "mcpServers": {
    "gdl-memory": {
      "command": "uv",
      "args": [
        "run",
        "--project",
        "<repo-root>/tools/gdl/memory_mcp",
        "python",
        "<repo-root>/tools/gdl/memory_mcp/server.py"
      ],
      "env": {
        "GDL_MEMORY_ROOT": "<repo-root>"
      }
    }
  }
}
```

Host configuration is machine-local; do not commit absolute paths or API keys.
The CLI remains available when an MCP host is not configured.

The adapter exposes search/context, Xbox symbol/type lookup, tool policy,
migration audit/proposal queries, and two review-gated writes:
`memory_register_tool` and `memory_propose_record`. Writes create JSON only in
`knowledge/memory/inbox/`; they never mutate the generated SQLite database or
accept their own proposal.
