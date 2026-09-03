#!/usr/bin/env python3
"""Measure every repo path the memory-graph test suite actually reads.

This is the FALSIFIER for `memory_graph.test_graph --changed`. That gate
skips a ~41s per-commit suite run when the tree touches nothing the suite
reads, and its `GRAPH_SUITE_INPUT_ROOTS` list is only as good as the
measurement behind it. Re-run this whenever the suite grows a test that
reaches outside a temp root; if a new root appears in the histogram and is
not in `GRAPH_SUITE_INPUT_ROOTS`, the gate is unsound and must be widened.

Method: patch builtins.open / os.stat / os.scandir / os.listdir before
importing the suite, log every path under the repo root, then run all
tests in-process.

Measured 2026-09-03 (run 43, 336 tests, 0 failures, 2,164 distinct paths):

    memory_graph/records   1856      memory_graph/legacy       3
    tools/gdl               277      config/GUNE5D             3
    memory_graph/inbox       13      research/xbox_symbols     3
    build                     3      memory_graph/{core,gdlmem,schema,...}

Nothing under src/, include/, configure.py or AGENTS.md is read at all.

Usage (from the repository root):
    python tools/gdl/composed_census/t13_suite_inputs.py [--out PATH]
"""
from __future__ import annotations

import argparse
import builtins
import json
import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))

TOUCHED: dict[str, set[str]] = {}
_ROOT_PREFIX = str(ROOT).replace("\\", "/").rstrip("/") + "/"

_open = builtins.open
_stat = os.stat
_scandir = os.scandir
_listdir = os.listdir


def _note(kind: str, target) -> None:
    try:
        text = os.fspath(target)
    except TypeError:
        return
    if isinstance(text, bytes):
        text = text.decode("utf-8", "replace")
    if not isinstance(text, str):
        return
    norm = text.replace("\\", "/")
    if not norm.lower().startswith(_ROOT_PREFIX.lower()):
        return
    TOUCHED.setdefault(norm[len(_ROOT_PREFIX):], set()).add(kind)


def _open_p(file, *args, **kwargs):
    _note("open", file)
    return _open(file, *args, **kwargs)


def _stat_p(path, *args, **kwargs):
    _note("stat", path)
    return _stat(path, *args, **kwargs)


def _scandir_p(path="."):
    _note("scandir", path)
    return _scandir(path)


def _listdir_p(path="."):
    _note("listdir", path)
    return _listdir(path)


def _histogram() -> dict[str, int]:
    """Group by root, one level deeper inside the shared top directories."""
    roots: dict[str, int] = {}
    for rel in TOUCHED:
        parts = rel.split("/")
        head = parts[0]
        if head in ("memory_graph", "tools", "config", "research", "src",
                    "include") and len(parts) > 1:
            head = "/".join(parts[:2])
        roots[head] = roots.get(head, 0) + 1
    return dict(sorted(roots.items(), key=lambda item: -item[1]))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        default=str(ROOT / "build" / "GUNE5D" / "t13_suite_inputs.json"),
        help="where to write the full path list (default under build/)")
    args = parser.parse_args(argv)

    builtins.open = _open_p
    os.stat = _stat_p
    os.scandir = _scandir_p
    os.listdir = _listdir_p
    try:
        from memory_graph import test_graph
        suite = unittest.TestLoader().loadTestsFromModule(test_graph)
        with _open(os.devnull, "w", encoding="utf-8") as sink:
            result = unittest.TextTestRunner(verbosity=0,
                                             stream=sink).run(suite)
    finally:
        builtins.open = _open
        os.stat = _stat
        os.scandir = _scandir
        os.listdir = _listdir

    roots = _histogram()
    failures = len(result.failures) + len(result.errors)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(
        {"tests_run": result.testsRun, "failures": failures,
         "root_counts": roots,
         "paths": {key: sorted(value)
                   for key, value in sorted(TOUCHED.items())}},
        indent=2), encoding="utf-8")

    print(f"tests_run={result.testsRun} failures={failures}")
    print(f"distinct repo paths read: {len(TOUCHED)}")
    for name, count in roots.items():
        print(f"  {name:40s} {count}")

    from memory_graph.test_graph import (GRAPH_SUITE_INPUT_ROOTS,
                                         graph_suite_relevant)
    uncovered = sorted({name for name in roots
                        if not graph_suite_relevant([name + "/x"])
                        and name not in ("build", ".git")})
    if uncovered:
        print("UNCOVERED by GRAPH_SUITE_INPUT_ROOTS"
              f" ({', '.join(GRAPH_SUITE_INPUT_ROOTS)}): "
              + ", ".join(uncovered))
    else:
        print("every read root is covered by GRAPH_SUITE_INPUT_ROOTS")
    print(f"wrote {out}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
