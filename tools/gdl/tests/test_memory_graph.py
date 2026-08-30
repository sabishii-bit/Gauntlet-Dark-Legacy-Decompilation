import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from memory_graph.core import (  # noqa: E402
    MemoryGraphError,
    attempt_staleness,
    build_database,
    ensure_database,
    memory_audit,
    register_tool_proposal,
    search_memory,
    stage_record_proposal,
    symbol_context,
    tool_context,
    validate_records,
    xbox_struct_layout,
    xbox_symbol_context,
)


class MemoryGraphTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "config/GUNE5D").mkdir(parents=True)
        (self.root / "memory_graph/legacy").mkdir(parents=True)
        (self.root / "research/xbox_symbols").mkdir(parents=True)
        (self.root / "memory_graph/records").mkdir(parents=True)
        (self.root / "memory_graph/inbox").mkdir(parents=True)
        (self.root / "tools/gdl").mkdir(parents=True)

        (self.root / "config/GUNE5D/symbols.txt").write_text(
            "foo = .text:0x80001000; // type:function size:0x20 scope:global\n"
            "bar = .text:0x80001020; // type:function size:0x10 scope:global\n",
            encoding="utf-8",
        )
        (self.root / "config/GUNE5D/splits.txt").write_text(
            "game/example.c:\n\t.text start:0x80001000 end:0x80001030\n",
            encoding="utf-8",
        )
        (self.root / "research/xbox_symbols/functions_by_module.txt").write_text(
            "== .\\Release\\EXAMPLE.OBJ (.\\Release\\EXAMPLE.OBJ)\n"
            "[0001:00000010] 20 G foo\n"
            "[0001:00000030] 10 L helper\n",
            encoding="utf-8",
        )
        (self.root / "research/xbox_symbols/shell3D.pdb").write_bytes(b"test-pdb")
        (self.root / "research/xbox_symbols/xbox_structs.tsv").write_text(
            "S\tExample\t16\tgame\nF\t0\t4\tfirst\nF\t4\t4\tsecond\nF\t12\t4\tthird\n",
            encoding="utf-8",
        )
        parked = "# legacy list\nfoo # allocator residual\n"
        (self.root / "memory_graph/legacy/PARKED.txt").write_text(parked, encoding="utf-8")
        (self.root / "memory_graph/legacy/duplicate.md").write_text(parked, encoding="utf-8")
        (self.root / "memory_graph/legacy/duplicate2.md").write_text(parked, encoding="utf-8")
        (self.root / "memory_graph/legacy/matching-playbook.md").write_text(
            "# Playbook\n\n## Example compiler law\n\nA verified-looking legacy paragraph.\n",
            encoding="utf-8",
        )
        (self.root / "tools/gdl/example.py").write_text(
            '\"\"\"Example discovered tool.\"\"\"\n', encoding="utf-8"
        )
        (self.root / "memory_graph/records/project.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "entity.project.test",
                    "kind": "entity",
                    "entity_type": "project",
                    "key": "project:test",
                    "name": "Test project",
                }
            ),
            encoding="utf-8",
        )
        (self.root / "memory_graph/records/tool.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "tool.example.v1",
                    "kind": "tool",
                    "tool_key": "tool:example",
                    "name": "Example",
                    "tool_kind": "test",
                    "status": "active",
                    "purpose": "Exercise the tool registry.",
                    "usage": ["Run it."],
                    "constraints": ["Test only."],
                }
            ),
            encoding="utf-8",
        )
        self.db = self.root / "memory.sqlite"

    def tearDown(self):
        self.temp.cleanup()

    def test_build_indexes_gcn_xbox_types_tools_and_legacy_proposals(self):
        stats = build_database(self.root, self.db)
        self.assertEqual(stats["gcn_symbols_imported"], 2)
        self.assertEqual(stats["xbox_symbols_imported"], 2)
        self.assertEqual(stats["pdb_types_imported"], 1)
        self.assertEqual(stats["pdb_fields_imported"], 3)
        self.assertEqual(stats["exact_name_candidates"], 1)
        self.assertEqual(stats["migration_proposals_imported"], 2)

        context = symbol_context("foo", root=self.root, db_path=self.db)
        self.assertEqual(context["gamecube_symbol"]["module"], "game/example.c")
        self.assertEqual(context["xbox_links"][0]["xbox_module"], "EXAMPLE.OBJ")
        self.assertEqual(context["migration_proposals"][0]["proposal_kind"], "parking_legacy")

        xbox = xbox_symbol_context("foo", root=self.root, db_path=self.db)
        self.assertEqual(xbox["matches"][0]["raw_name"], "foo")
        self.assertEqual(xbox["types"], [])

        tool = tool_context("Example", root=self.root, db_path=self.db)
        self.assertEqual(tool["tools"][0]["source_kind"], "reviewed_record")
        self.assertEqual(tool["tools"][0]["constraints"], ["Test only."])

    def test_function_and_tu_references_autoresolve_from_symbol_import(self):
        (self.root / "memory_graph/records/attempt.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.foo.v1",
                    "kind": "attempt",
                    "function": "function:foo",
                    "attempted_axis": "test axis",
                    "outcome": "negative",
                }
            ),
            encoding="utf-8",
        )
        (self.root / "memory_graph/records/edge.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "edge.foo.v1",
                    "kind": "edge",
                    "source": "function:foo",
                    "relation": "part_of",
                    "target": "tu:game/example",
                }
            ),
            encoding="utf-8",
        )
        build_database(self.root, self.db)
        connection = sqlite3.connect(self.db)
        try:
            rows = dict(
                connection.execute(
                    "SELECT entity_key, attributes_json FROM entity"
                    " WHERE entity_key IN ('function:foo', 'tu:game/example')"
                ).fetchall()
            )
        finally:
            connection.close()
        self.assertIn("function:foo", rows)
        self.assertIn("auto_resolved_from", rows["function:foo"])
        self.assertIn("tu:game/example", rows)
        (self.root / "memory_graph/records/bad.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.missing.v1",
                    "kind": "attempt",
                    "function": "function:does_not_exist",
                    "attempted_axis": "test axis",
                    "outcome": "negative",
                }
            ),
            encoding="utf-8",
        )
        with self.assertRaises(MemoryGraphError):
            build_database(self.root, self.db)

    def test_bad_inbox_proposal_is_rejected_without_breaking_the_build(self):
        (self.root / "memory_graph/inbox/poison.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.poison.v1",
                    "kind": "attempt",
                    "function": "unprefixed-name",
                    "attempted_axis": "axis",
                    "outcome": "negative",
                }
            ),
            encoding="utf-8",
        )
        stats = build_database(self.root, self.db)
        self.assertEqual(len(stats["inbox_rejected"]), 1)
        self.assertIn("unknown entity", stats["inbox_rejected"][0]["error"])
        connection = sqlite3.connect(self.db)
        try:
            remaining = connection.execute(
                "SELECT COUNT(*) FROM record_ingest WHERE record_id='attempt.poison.v1'"
            ).fetchone()[0]
        finally:
            connection.close()
        self.assertEqual(remaining, 0)
        context = symbol_context("foo", root=self.root, db_path=self.db)
        self.assertIsNotNone(context["gamecube_symbol"])
        with self.assertRaises(MemoryGraphError):
            stage_record_proposal(
                {
                    "schema_version": 1,
                    "id": "attempt.poison2.v1",
                    "kind": "attempt",
                    "function": "function:not_a_symbol",
                    "attempted_axis": "axis",
                    "outcome": "negative",
                },
                root=self.root,
            )

    def test_query_surface_registry_drives_cli_and_mcp(self):
        """Query ops live once in core.build_surface_ops; consumers derive.

        The CLI must expose exactly the registry (plus lifecycle/write
        commands), and the MCP adapter must generate its read tools from the
        same registry (its in-env smoke test verifies the generated tools
        actually list and execute).
        """
        import argparse

        from memory_graph.core import build_surface_ops
        from memory_graph.gdlmem import build_parser

        registry = {op.name for op in build_surface_ops()}
        parser, ops = build_parser()
        self.assertEqual(set(ops), registry)
        sub = next(
            action
            for action in parser._actions
            if isinstance(action, argparse._SubParsersAction)
        )
        self.assertTrue(registry <= set(sub.choices))
        base = Path(__file__).resolve().parents[3] / "memory_graph"
        server_text = (base / "mcp" / "server.py").read_text(encoding="utf-8")
        self.assertIn("build_surface_ops", server_text)
        mcp_names = {op.mcp_name for op in build_surface_ops()}
        self.assertEqual(len(mcp_names), len(registry), "mcp_name collision")

    def test_audit_reports_duplicates_without_modifying_documents(self):
        build_database(self.root, self.db)
        before = (self.root / "memory_graph/legacy/PARKED.txt").read_bytes()
        audit = memory_audit(root=self.root, db_path=self.db)
        self.assertEqual(len(audit["duplicate_documents"]), 1)
        self.assertTrue(audit["duplicate_chunks"])
        self.assertEqual(audit["documents_by_class"]["operational_ledger"], 1)
        self.assertEqual(audit["document_lifecycle"], {"legacy_unreviewed": 4})
        self.assertEqual(before, (self.root / "memory_graph/legacy/PARKED.txt").read_bytes())

    def test_evidence_import_does_not_depend_on_filename_order(self):
        evidence = {
            "schema_version": 1,
            "id": "evidence.project.test",
            "kind": "evidence",
            "claim": "claim.project.test",
            "evidence_kind": "test",
            "locator": "unit-test",
            "detail": "The claim is intentionally sorted after this record.",
        }
        claim = {
            "schema_version": 1,
            "id": "claim.project.test",
            "kind": "claim",
            "subject": "project:test",
            "predicate": "has_test_evidence",
            "value": True,
            "epistemic_state": "verified",
        }
        records = self.root / "memory_graph/records"
        (records / "00-evidence.json").write_text(json.dumps(evidence), encoding="utf-8")
        (records / "99-claim.json").write_text(json.dumps(claim), encoding="utf-8")
        stats = build_database(self.root, self.db)
        self.assertEqual(stats["counts"]["claim"], 1)

    def test_ensure_refreshes_after_discovered_tool_changes(self):
        tool_path = self.root / "tools/gdl/fresh.py"
        tool_path.write_text('"""Initial discovered tool."""\n', encoding="utf-8")
        build_database(self.root, self.db)
        # Reproduce a shared database last materialized by a sibling worktree.
        # Its root label must not disable the normal fingerprint freshness gate.
        connection = sqlite3.connect(self.db)
        try:
            connection.execute(
                "UPDATE meta SET value=? WHERE key='build_root'",
                (str(self.root / "sibling-worktree"),),
            )
            connection.commit()
        finally:
            connection.close()
        tool_path.write_text('"""Updated discovered tool."""\n# changed\n', encoding="utf-8")
        ensure_database(self.root, self.db)
        tool = tool_context("Fresh", root=self.root, db_path=self.db)
        discovered = [item for item in tool["tools"] if item["source_kind"] == "source_scan"]
        self.assertEqual(discovered[0]["purpose"], "Updated discovered tool.")

    def test_register_tool_writes_review_required_valid_record(self):
        path = register_tool_proposal(
            name="Debugger",
            purpose="Inspect runtime state.",
            tool_kind="external",
            constraints=["Read-only by default."],
            root=self.root,
        )
        self.assertEqual(path.parent.name, "inbox")
        self.assertEqual(json.loads(path.read_text())["kind"], "tool")
        result = validate_records(self.root)
        self.assertEqual(result["record_count"], 3)
        build_database(self.root, self.db)
        tool = tool_context("Debugger", root=self.root, db_path=self.db)
        self.assertEqual(tool["tools"][0]["source_kind"], "proposal")

    def test_search_reaches_structured_record_bodies(self):
        # Law text lives in claim record bodies; after the legacy corpus was
        # retired, full-text search must reach it there or the graph cannot
        # answer "is there a law about X".
        (self.root / "memory_graph/records/claim-law.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "claim.law.example-quokka-rule.20260829.v1",
                    "kind": "claim",
                    "subject": "project:test",
                    "predicate": "codegen_law",
                    "epistemic_state": "verified",
                    "value": "A distinctive quokka marsupial phrase for search.",
                }
            ),
            encoding="utf-8",
        )
        build_database(self.root, self.db)
        hits = search_memory("quokka marsupial", root=self.root, db_path=self.db)
        self.assertEqual(
            hits["records"][0]["record_id"],
            "claim.law.example-quokka-rule.20260829.v1",
        )
        self.assertEqual(hits["records"][0]["record_state"], "accepted")
        # Inbox proposals stay searchable but rank behind accepted records.
        self.assertIn("snippet", hits["records"][0])

    def test_struct_layout_resolves_offsets_and_pad_gaps(self):
        # The de-fakematch lookup: given a raw byte offset, name the PDB
        # field that covers it; given a hole between fields, report the
        # exact pad size a reconstruction needs.
        build_database(self.root, self.db)
        hit = xbox_struct_layout(
            "Example", root=self.root, db_path=self.db, offset="0x4")
        entry = hit["types"][0]
        self.assertEqual([f["name"] for f in entry["fields"]],
                         ["first", "second", "third"])
        self.assertEqual(entry["offset_lookup"]["result"]["field"], "second")
        self.assertEqual(entry["pad_gaps"],
                         [{"after_field": "second", "before_field": "third",
                           "start": "0x8", "size": 4}])
        gap = xbox_struct_layout(
            "Example", root=self.root, db_path=self.db, offset="9")
        self.assertIn("in_pad_gap",
                      gap["types"][0]["offset_lookup"]["result"])

    def test_generic_record_proposal_is_validated_and_duplicate_safe(self):
        record = {
            "schema_version": 1,
            "id": "entity.block.test",
            "kind": "entity",
            "entity_type": "block",
            "key": "block:test",
            "name": "Test blocker",
        }
        path = stage_record_proposal(record, root=self.root)
        self.assertEqual(path.parent.name, "inbox")
        with self.assertRaisesRegex(Exception, "already exists"):
            stage_record_proposal(record, root=self.root)
        self.assertEqual(validate_records(self.root)["record_count"], 3)

    def test_staleness_resolves_unique_address_suffixed_report_name(self):
        (self.root / "memory_graph/records/attempt.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.foo.v1",
                    "kind": "attempt",
                    "function": "function:foo",
                    "attempted_axis": "test axis",
                    "outcome": "parked",
                }
            ),
            encoding="utf-8",
        )
        report_dir = self.root / "build/GUNE5D"
        report_dir.mkdir(parents=True)
        (report_dir / "report.json").write_text(
            json.dumps(
                {
                    "units": [
                        {
                            "functions": [
                                {
                                    "name": "foo_80001000",
                                    "fuzzy_match_percent": 95.0,
                                }
                            ]
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )

        result = attempt_staleness(root=self.root, db_path=self.db)

        self.assertEqual(result["missing_from_report"], [])
        self.assertEqual(result["valid_count"], 1)

    def test_staleness_ignores_superseded_attempts(self):
        records = self.root / "memory_graph/records"
        (records / "attempt-old.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.foo.v1",
                    "kind": "attempt",
                    "function": "function:foo",
                    "attempted_axis": "old axis",
                    "outcome": "parked",
                }
            ),
            encoding="utf-8",
        )
        (records / "attempt-new.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "id": "attempt.foo.v2",
                    "kind": "attempt",
                    "function": "function:foo",
                    "attempted_axis": "fresh exact revalidation",
                    "outcome": "exact",
                    "supersedes": "attempt.foo.v1",
                }
            ),
            encoding="utf-8",
        )
        report_dir = self.root / "build/GUNE5D"
        report_dir.mkdir(parents=True)
        (report_dir / "report.json").write_text(
            json.dumps(
                {
                    "units": [
                        {
                            "functions": [
                                {
                                    "name": "foo",
                                    "fuzzy_match_percent": 100.0,
                                }
                            ]
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )

        result = attempt_staleness(root=self.root, db_path=self.db)
        context = symbol_context("foo", root=self.root, db_path=self.db)

        self.assertEqual(result["stale_solved"], [])
        self.assertEqual(result["multi_record_functions"], [])
        self.assertEqual(
            [attempt["record_id"] for attempt in context["attempts"]],
            ["attempt.foo.v2"],
        )

    def test_markdown_cannot_anchor_a_structured_truth_record(self):
        record = {
            "schema_version": 1,
            "id": "tool.bad-markdown.v1",
            "kind": "tool",
            "tool_key": "tool:bad-markdown",
            "name": "Bad Markdown dependency",
            "tool_kind": "test",
            "status": "active",
            "purpose": "Exercise the truth-source boundary.",
            "attributes": {"evidence": ["README.md:10"]},
        }
        with self.assertRaisesRegex(Exception, "cannot use Markdown"):
            stage_record_proposal(record, root=self.root)


if __name__ == "__main__":
    unittest.main()
