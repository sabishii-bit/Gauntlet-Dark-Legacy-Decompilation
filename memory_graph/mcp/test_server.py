import sys
import unittest
from pathlib import Path

from mcp import Client

sys.path.insert(0, str(Path(__file__).resolve().parent))
import server  # noqa: E402  (inserts the repo root on import)

from memory_graph.core import build_surface_ops  # noqa: E402


class MemoryMcpSmokeTests(unittest.IsolatedAsyncioTestCase):
    async def test_server_exposes_registry_and_write_tools(self):
        expected = {op.mcp_name for op in build_surface_ops()}
        expected |= {"memory_register_tool", "memory_propose_record"}
        async with Client(server.server) as client:
            result = await client.list_tools()
            names = {tool.name for tool in result.tools}
            self.assertEqual(names, expected)
            # A generated tool must execute end-to-end, not merely list.
            called = await client.call_tool("memory_graph_stats", {})
        self.assertIsNotNone(called)


if __name__ == "__main__":
    unittest.main()
