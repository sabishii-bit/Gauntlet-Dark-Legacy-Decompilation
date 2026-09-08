"""Resolve current compiler/pin-input objects from the hash-bound Ninja graph.

No builds, writes, filesystem-precedence guesses, or exactness claims. An
inactive .postprocess artifact is never evidence of an active transform.
`compiler` means the configured compile edge (including its declared cleanup);
`pre_postprocessor` means the input of WebFrank/P6, which may already have
passed through Frank. These views are deliberately not interchangeable.
Object dependency freshness remains the builder's responsibility.
"""
from dataclasses import dataclass
import hashlib
import json
import time
from pathlib import Path

try:
    from .fndiff import unit_key
except ImportError:
    from fndiff import unit_key

COMPILE_RULES = {"mwcc", "mwcc_sjis", "mwcc_extab", "mwcc_sjis_extab"}
POSTPROCESS_RULES = {"webfrank", "webfrank_globalize_atree", "p6frank"}
TRANSFORMS = POSTPROCESS_RULES | {"frank", "globalize_atree", "fix_exception_object"}


class RawObjectError(ValueError):
    """No trustworthy active object selection is available."""


def _hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _local(root, value):
    if not isinstance(value, str):
        raise RawObjectError("non-string artifact path")
    value = value.replace("\\", "/")
    path = Path(value)
    if path.is_absolute() or ":" in value or ".." in path.parts or "$" in value:
        raise RawObjectError("unsupported non-local artifact path: " + value)
    full = (root / path).resolve()
    if not full.is_relative_to(root):
        raise RawObjectError("artifact escapes worktree: " + value)
    return full


# --------------------------------------------------------------------------
# Validated-graph memo.
#
# THE REPORTED DIAGNOSIS WAS REFUTED BY MEASUREMENT. Run 61 item 5 arrived as
# "resolve_object re-hashes build.ninja and all seven generator inputs on
# every call (0.106 s)". Remeasured on the native-only graph at run 62 with
# build/c62_resolve_cost.py, 40 calls for game/enemy/enemy
# (544 edges, 545 outputs, 368 units, 7 generator inputs):
#
#     resolve_object, memo cleared   0.1322 s per call
#       _output_index                0.0982 s per call   <- 74%
#       _validate_graph (whole)      0.0085 s per call   <-  6%
#         (its sha256 of the inputs is a fraction of that 6%)
#     resolve_object, memoized       0.0009 s per call   -> 147x
#
# The hashing is nearly free. The cost is `Path.resolve()` inside `_local`,
# run once per build-graph OUTPUT to build `by_output`. Memoizing only the
# hashes would have removed a couple of percent of the measured cost and the
# item would have read as fixed. So the memo caches the VALIDATED GRAPH **and**
# its derived output index together, on the (path, mtime_ns, size) identity of
# every file the validation reads, exactly as fndiff's objdump/parse memos key
# theirs.
#
# Native-only reconciliation (run 62): the whole native-only policy verdict is
# computed inside `_validate_graph` — `check_snapshot` reads only build_edges
# rules/edges/units, and the fingerprint that decides it is
# `tools/gdl/native_build.py`, which `_validate_graph` REQUIRES in
# `generator_inputs` whenever `native_only` is set. The stamp set below is
# build.ninja + build_edges.json + every generator input, so it covers that
# policy file by construction: editing native_build.py, or swapping the graph
# between native-only and legacy mode, misses the memo and revalidates.
# A graph that omits the policy file from its inventory is refused before it
# can ever be memoized, so no entry can be keyed on a partial fingerprint.
#
# A changed input misses: a new mtime_ns or size revalidates from scratch and
# raises the same refusals. Content changed under an identical mtime_ns AND
# size is the one hole, shared with the existing memos; it is closed for the
# dangerous case by _stamp() refusing to memoize a file written within
# _RACY_WINDOW_NS of now, so a tool that rewrites a config and immediately
# re-resolves is never served the pre-write graph.
#
# Never persisted to disk: this is a within-process memo whose whole staleness
# surface is one interpreter run.
_GRAPH_CACHE = {}
_CACHE_LIMIT = 8
_RACY_WINDOW_NS = 2_000_000_000


def _stamp(path):
    """(mtime_ns, size), or None when this file must not be memoized."""
    try:
        stat = path.stat()
    except OSError:
        return None
    if stat.st_mtime_ns > time.time_ns() - _RACY_WINDOW_NS:
        return None
    return (stat.st_mtime_ns, stat.st_size)


def _output_index(root, graph):
    """artifact path -> producing edge; refuses ambiguous ownership."""
    try:
        by_output = {}
        for edge in graph["edges"]:
            for output in edge["outputs"] + edge.get("implicit_outputs", []):
                output = _local(root, output)
                if output in by_output:
                    raise RawObjectError("duplicate build output: " + str(output))
                by_output[output] = edge
        return by_output
    except (KeyError, TypeError) as error:
        raise RawObjectError("malformed source pipeline: " + str(error)) from error


def _graph_index(root, version):
    """(graph, by_output), revalidated only when an input's identity changed.

    The returned graph and index are a SHARED read-only snapshot. Callers
    must not mutate them; every in-repo caller only reads.
    """
    key = (str(root), version)
    entry = _GRAPH_CACHE.get(key)
    if entry is not None:
        stamps, graph, by_output = entry
        if all(_stamp(root / name) == stamp for name, stamp in stamps.items()):
            return graph, by_output
    graph = _validate_graph(root, version)
    by_output = _output_index(root, graph)
    stamps = {}
    for name in ("build.ninja", f"build/{version}/build_edges.json",
                 *graph["generator_inputs"]):
        stamp = _stamp(root / name)
        if stamp is None:
            stamps = None
            break
        stamps[name] = stamp
    if stamps is not None:
        if len(_GRAPH_CACHE) >= _CACHE_LIMIT:
            _GRAPH_CACHE.clear()
        _GRAPH_CACHE[key] = (stamps, graph, by_output)
    return graph, by_output


def load_graph(root, version):
    """Use the existing generator snapshot; refuse missing/stale contracts.

    Returns the shared memoized snapshot; treat it as read-only.
    """
    return _graph_index(Path(root).resolve(), version)[0]


def _validate_graph(root, version):
    """The uncached hash-bound validation. Callers go through _graph_index."""
    root = Path(root).resolve()
    path = root / "build" / version / "build_edges.json"
    try:
        graph = json.loads(path.read_text(encoding="utf-8"))
        if graph["schema_version"] != 1 or graph["version"] != version:
            raise RawObjectError("unsupported build_edges schema/version")
        if graph["ninja_sha256"] != _hash(root / "build.ninja"):
            raise RawObjectError("build.ninja differs from build_edges snapshot")
        inputs = graph["generator_inputs"]
        policy_inputs = ({"tools/gdl/native_build.py"} if graph.get("native_only") else
                         {f"config/{version}/webfrank.json", f"config/{version}/p6frank.json"})
        required = {"configure.py", "tools/project.py", "tools/gdl/build_provenance.py", *policy_inputs,
                    *(f"config/{version}/{name}" for name in
                      ("config.yml", "splits.txt", "symbols.txt"))}
        if not required.issubset(inputs):
            raise RawObjectError("incomplete generator-input hash inventory")
        for name, expected in inputs.items():
            if _hash(_local(root, name)) != expected:
                raise RawObjectError("generator input changed since configure: " + name)
        if not isinstance(graph["non_matching"], bool):
            raise RawObjectError("unknown matching/editable graph mode")
        if graph.get("native_only"):
            try:
                from .native_build import check_snapshot
            except ImportError:
                from native_build import check_snapshot
            try:
                check_snapshot(graph)
            except ValueError as error:
                raise RawObjectError(str(error)) from error
        if graph["non_matching"] and any(e["rule"] in TRANSFORMS - {"globalize_atree"}
                                         for e in graph["edges"]):
            raise RawObjectError("editable graph contains a target-bound transform")
        return graph
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise RawObjectError("missing or malformed build graph: " + str(error)) from error


@dataclass(frozen=True)
class ObjectSelection:
    path: Path
    relative: str
    view: str
    description: str
    pipeline: tuple


def resolve_object(unit, *, root=None, version="GUNE5D", view="compiler", require_exists=True):
    """Select before reading; allow missing ACTIVE output only for a build target.

    This is a graph walk, not a glob. Unknown rules, ambiguous ownership,
    cycles, missing compiler ancestry or changed generator hashes refuse.
    """
    if view not in {"compiler", "pre_postprocessor"}:
        raise RawObjectError("unknown object view: " + str(view))
    root = Path(root if root is not None else Path(__file__).resolve().parents[2]).resolve()
    unit = unit_key(unit)
    graph, by_output = _graph_index(root, version)
    try:
        owners = [row for row in graph["units"] if unit_key(row["name"]) == unit]
        if len(owners) != 1 or not owners[0].get("source_object"):
            raise RawObjectError("unit lacks a unique active source object: " + unit)
        current = _local(root, owners[0]["source_object"])
        seen, chain, pin_input = set(), [], None
        while True:
            if current in seen:
                raise RawObjectError("cycle in source object pipeline")
            seen.add(current)
            edge = by_output.get(current)
            if not edge:
                raise RawObjectError("active output has no producer: " + str(current))
            rule = edge["rule"]
            chain.append(rule)
            if rule in COMPILE_RULES:
                sources = edge["inputs"]
                if len(sources) != 1 or unit_key(sources[0]) != unit:
                    raise RawObjectError("compiler ancestry belongs to a different source unit")
                break
            if rule not in TRANSFORMS:
                raise RawObjectError("unsupported source object producer: " + str(rule))
            sources = edge["inputs"]
            # The generator's Frank rule consumes vanilla first, profile
            # second. The profile object is not the vanilla compiler output.
            if len(sources) != (2 if rule == "frank" else 1):
                raise RawObjectError("unsupported transform input shape: " + rule)
            current = _local(root, sources[0])
            if rule in POSTPROCESS_RULES and pin_input is None:
                pin_input = current
        selected = pin_input if view == "pre_postprocessor" and pin_input else current
        overwritten = {_local(root, name) for edge in graph["edges"]
                       if edge["rule"] == "fix_exception_objects" for name in edge["inputs"]}
        if current in overwritten or selected in overwritten:
            raise RawObjectError("raw compiler object was overwritten in place by exception fixup")
        transformed = selected != current
        description = ("pre-postprocessor input (already transformed upstream; NOT compiler raw)"
                       if transformed else "active compiler-stage output (raw)")
        if require_exists and not selected.is_file():
            raise RawObjectError("active object is missing: " + selected.relative_to(root).as_posix()
                                 + "; build this target with ninja")
        return ObjectSelection(selected, selected.relative_to(root).as_posix(), view,
                               description, tuple(reversed(chain)))
    except (KeyError, TypeError) as error:
        raise RawObjectError("malformed source pipeline: " + str(error)) from error


if __name__ == "__main__":
    # A library, not a command. Run-59 item 9: exiting 0 with no output at
    # all is indistinguishable from a tool that ran and found nothing.
    print(__doc__.strip())
