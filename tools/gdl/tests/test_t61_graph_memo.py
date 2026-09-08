"""Two-sided tests for raw_object's validated-graph memo (run 61 item 5).

A memo that serves a stale build graph is worse than a slow one: it would
name the wrong active object for a unit whose pipeline just changed. These
pin both sides — a repeat call must not revalidate, and any changed input
identity must revalidate and still refuse.

Run 62 reconciles the memo with the native-only policy: the whole native-only
verdict is computed inside `_validate_graph`, and its deciding fingerprint,
`tools/gdl/native_build.py`, is a required generator input, so the memo's
stamp set already covers it. `NativeOnlyFingerprint` pins that rather than
leaving it to inspection.
"""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl import raw_object as raw
from tools.gdl.tests.test_raw_object import UNIT, graph_fixture, save_graph

HOUR = 3600 * 10 ** 9


def backdate(root, seconds=3600):
    """Move every file's mtime into the past so _stamp will memoize it.

    Files written moments ago are deliberately NOT memoized (the racy-stat
    window), which is itself asserted below.
    """
    when = None
    for path in Path(root).rglob("*"):
        if path.is_file():
            stat = path.stat()
            when = stat.st_mtime_ns - seconds * 10 ** 9
            os.utime(path, ns=(when, when))
    return when


class GraphMemoIdentity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(raw._GRAPH_CACHE.clear)
        raw._GRAPH_CACHE.clear()
        self.root = Path(self.temp.name)

    def validations(self):
        return patch.object(raw, "_validate_graph",
                            side_effect=raw._validate_graph)

    def test_a_repeated_resolve_revalidates_nothing(self):
        graph_fixture(self.root)
        backdate(self.root)
        with self.validations() as spy:
            for _ in range(5):
                raw.resolve_object(UNIT, root=self.root)
        self.assertEqual(spy.call_count, 1)

    def test_a_freshly_written_graph_is_never_memoized(self):
        # The racy-stat window: a config rewritten in this same second must
        # not be able to serve its pre-write graph to the next reader.
        graph_fixture(self.root)
        with self.validations() as spy:
            raw.resolve_object(UNIT, root=self.root)
            raw.resolve_object(UNIT, root=self.root)
        self.assertEqual(spy.call_count, 2)
        self.assertEqual(raw._GRAPH_CACHE, {})

    def test_every_changed_generator_input_still_refuses_through_the_memo(self):
        graph, _, _ = graph_fixture(self.root)
        for name in ("build.ninja", *graph["generator_inputs"]):
            with self.subTest(path=name):
                raw._GRAPH_CACHE.clear()
                graph_fixture(self.root)
                backdate(self.root)
                raw.resolve_object(UNIT, root=self.root)   # populates the memo
                target = self.root / name
                target.write_bytes(b"changed content of a different length")
                stamp = target.stat()
                os.utime(target, ns=(stamp.st_mtime_ns - HOUR,
                                     stamp.st_mtime_ns - HOUR))
                with self.assertRaises(raw.RawObjectError):
                    raw.resolve_object(UNIT, root=self.root)

    def test_a_size_change_under_an_identical_mtime_still_misses(self):
        graph_fixture(self.root)
        backdate(self.root)
        raw.resolve_object(UNIT, root=self.root)
        target = self.root / "config/GUNE5D/splits.txt"
        keep = target.stat().st_mtime_ns
        target.write_bytes(b"config but longer")
        os.utime(target, ns=(keep, keep))
        with self.assertRaises(raw.RawObjectError):
            raw.resolve_object(UNIT, root=self.root)

    def test_a_changed_build_edges_snapshot_alone_misses(self):
        graph_fixture(self.root)
        backdate(self.root)
        selected = raw.resolve_object(UNIT, root=self.root)
        self.assertTrue(selected.relative.endswith(".o"))
        snapshot = self.root / "build/GUNE5D/build_edges.json"
        edges = json.loads(snapshot.read_text(encoding="utf-8"))
        edges["edges"][0]["rule"] = "mystery_compiler"
        save_graph(self.root, edges)
        stamp = snapshot.stat().st_mtime_ns - HOUR
        os.utime(snapshot, ns=(stamp, stamp))
        with self.assertRaises(raw.RawObjectError):
            raw.resolve_object(UNIT, root=self.root)

    def test_two_roots_do_not_share_one_entry(self):
        other = Path(tempfile.mkdtemp(dir=self.temp.name))
        graph_fixture(self.root, chain=("webfrank",))
        _, body, _ = graph_fixture(other)
        backdate(self.root)
        first = raw.resolve_object(UNIT, root=self.root).relative
        second = raw.resolve_object(UNIT, root=other).relative
        self.assertIn("/.postprocess/body/", first)
        self.assertEqual(second, body)

    def test_the_memo_is_bounded(self):
        graph_fixture(self.root)
        backdate(self.root)
        for index in range(raw._CACHE_LIMIT + 3):
            raw._GRAPH_CACHE[("phantom%d" % index, "GUNE5D")] = ({}, {}, {})
        raw.resolve_object(UNIT, root=self.root)
        self.assertLessEqual(len(raw._GRAPH_CACHE), raw._CACHE_LIMIT)


class NativeOnlyFingerprint(unittest.TestCase):
    """The memo must not outlive a change to the native-only policy input."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.addCleanup(raw._GRAPH_CACHE.clear)
        raw._GRAPH_CACHE.clear()
        self.root = Path(self.temp.name)

    def validations(self):
        return patch.object(raw, "_validate_graph",
                            side_effect=raw._validate_graph)

    def test_the_policy_file_is_part_of_the_memoized_stamp_set(self):
        graph, _, _ = graph_fixture(self.root, native_only=True)
        self.assertIn("tools/gdl/native_build.py", graph["generator_inputs"])
        backdate(self.root)
        raw.resolve_object(UNIT, root=self.root)
        stamps = raw._GRAPH_CACHE[(str(self.root), "GUNE5D")][0]
        self.assertIn("tools/gdl/native_build.py", stamps)

    def test_an_edited_policy_file_revalidates_and_refuses(self):
        graph_fixture(self.root, native_only=True)
        backdate(self.root)
        with self.validations() as spy:
            raw.resolve_object(UNIT, root=self.root)
            raw.resolve_object(UNIT, root=self.root)
            self.assertEqual(spy.call_count, 1)
            policy = self.root / "tools/gdl/native_build.py"
            policy.write_bytes(b"a different, longer native-only policy")
            stamp = policy.stat().st_mtime_ns - HOUR
            os.utime(policy, ns=(stamp, stamp))
            with self.assertRaises(raw.RawObjectError):
                raw.resolve_object(UNIT, root=self.root)
            self.assertEqual(spy.call_count, 2)

    def test_switching_a_memoized_graph_to_a_rewrite_edge_misses(self):
        graph_fixture(self.root, native_only=True)
        backdate(self.root)
        raw.resolve_object(UNIT, root=self.root)
        snapshot = self.root / "build/GUNE5D/build_edges.json"
        graph = json.loads(snapshot.read_text(encoding="utf-8"))
        graph["edges"].append(dict(rule="webfrank", outputs=["build/late.o"],
                                   inputs=["build/early.o"]))
        save_graph(self.root, graph)
        stamp = snapshot.stat().st_mtime_ns - HOUR
        os.utime(snapshot, ns=(stamp, stamp))
        with self.assertRaisesRegex(raw.RawObjectError, "rewrite edge"):
            raw.resolve_object(UNIT, root=self.root)

    def test_a_legacy_and_a_native_graph_do_not_share_one_entry(self):
        other = Path(tempfile.mkdtemp(dir=self.temp.name))
        graph_fixture(self.root, chain=("webfrank",))
        _, _, plain = graph_fixture(other, native_only=True)
        backdate(self.root)
        backdate(other)
        self.assertIn("/.postprocess/body/",
                      raw.resolve_object(UNIT, root=self.root).relative)
        self.assertEqual(raw.resolve_object(UNIT, root=other).relative, plain)


if __name__ == "__main__":
    unittest.main()
