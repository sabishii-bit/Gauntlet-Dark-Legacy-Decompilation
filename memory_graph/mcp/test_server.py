import unittest
import sys
from pathlib import Path

from mcp import Client

sys.path.insert(0, str(Path(__file__).resolve().parent))
import server


class MemoryMcpSmokeTests(unittest.IsolatedAsyncioTestCase):
    async def test_server_exposes_expected_tools(self):
        async with Client(server.server) as client:
            result = await client.list_tools()
        names = {tool.name for tool in result.tools}
        self.assertEqual(
            names,
            {
                "memory_context",
                "memory_graph_stats",
                "memory_migration_audit",
                "memory_pending_proposals",
                "memory_propose_record",
                "memory_register_tool",
                "memory_search",
                "memory_tool_context",
                "xbox_context",
            },
        )


if __name__ == "__main__":
    unittest.main()
