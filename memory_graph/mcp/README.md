# GDL Memory MCP

The MCP adapter is isolated from the build dependencies. Run it from the
repository root with its locked `uv` environment:

```sh
uv run --project memory_graph/mcp python memory_graph/mcp/server.py
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
        "<repo-root>/memory_graph/mcp",
        "python",
        "<repo-root>/memory_graph/mcp/server.py"
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

Read tools are **generated** from `memory_graph.core.build_surface_ops()` —
the same registry the CLI derives its query subcommands from — so the two
surfaces cannot drift: adding an operation to the registry exposes it in both
places with no adapter edit. Only the two review-gated writes are defined
explicitly: `memory_register_tool` and `memory_propose_record`. Writes create
JSON only in `memory_graph/inbox/`; they never mutate the generated SQLite
database or accept their own proposal.

`test_server.py` (run inside this project's uv environment) verifies the
generated tools both list and execute through a real MCP client, and
`tools/gdl/tests/test_memory_graph.py` verifies the CLI exposes exactly the
registry.
