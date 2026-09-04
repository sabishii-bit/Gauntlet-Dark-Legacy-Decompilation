#!/usr/bin/env python3
"""`except Exception` handlers that cannot catch a helper's SystemExit.

SystemExit derives from BaseException, NOT Exception, so a blanket
`except Exception` around a helper that refuses by raising SystemExit does
not catch the refusal: the interpreter exits and the fail-soft the author
wrote never runs. probe.py carried three such sites (run 51, T21); this is
the screen that says whether any remain.

Run from the repository root:

    python tools/gdl/composed_census/t22_systemexit_blind_handlers.py

It parses every module under tools/gdl, tools/gdl/composed_census and
memory_graph with `ast`, collects the functions that can raise SystemExit
(including wf_word_diff's `CountAsymmetric` subclass), and reports every
`try` block that has an `except Exception` handler and calls one of them.
A `try` that ALSO lists `except SystemExit` is reported as HANDLED, so the
number to act on is the UNHANDLED count. Exit 1 when any are unhandled.

No build, no writes, no arguments.
"""
import ast
import os
import sys

ROOTS = (
    os.path.join("tools", "gdl"),
    os.path.join("tools", "gdl", "composed_census"),
    "memory_graph",
)
# SystemExit itself plus the subclasses the project defines.
EXIT_NAMES = {"SystemExit", "CountAsymmetric"}


def _raises_system_exit(node):
    for inner in ast.walk(node):
        if not isinstance(inner, ast.Raise):
            continue
        exc = inner.exc
        target = exc.func if isinstance(exc, ast.Call) else exc
        if isinstance(target, ast.Name) and target.id in EXIT_NAMES:
            return True
    return False


def _handler_names(handler):
    node = handler.type
    if node is None:
        return {"BaseException"}
    if isinstance(node, ast.Name):
        return {node.id}
    if isinstance(node, ast.Tuple):
        return {e.id for e in node.elts if isinstance(e, ast.Name)}
    return set()


def main():
    if not os.path.isdir("memory_graph"):
        print("run this from the repository root")
        return 2
    raisers = {}
    trees = {}
    for base in ROOTS:
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            if not name.endswith(".py"):
                continue
            path = os.path.join(base, name)
            try:
                with open(path, encoding="utf-8") as handle:
                    tree = ast.parse(handle.read())
            except (SyntaxError, OSError):
                continue
            stem = name[:-3]
            trees[path] = (stem, tree)
            found = {node.name for node in ast.walk(tree)
                     if isinstance(node, (ast.FunctionDef,
                                          ast.AsyncFunctionDef))
                     and _raises_system_exit(node)}
            if found:
                raisers.setdefault(stem, set()).update(found)

    print(f"modules parsed                   : {len(trees)}")
    print(f"modules with a SystemExit raiser : {len(raisers)}")
    print(f"functions that raise SystemExit  : "
          f"{sum(len(v) for v in raisers.values())}")

    handled, unhandled = [], []
    for path, (stem, tree) in sorted(trees.items()):
        rel = path.replace("\\", "/")
        for node in ast.walk(tree):
            if not isinstance(node, ast.Try):
                continue
            names = set()
            for handler in node.handlers:
                names |= _handler_names(handler)
            if "Exception" not in names:
                continue
            called = set()
            for inner in ast.walk(node):
                if not isinstance(inner, ast.Call):
                    continue
                fn = inner.func
                if isinstance(fn, ast.Attribute) and isinstance(fn.value,
                                                                ast.Name):
                    called.add((fn.value.id, fn.attr))
                elif isinstance(fn, ast.Name):
                    called.add((stem, fn.id))
            risky = sorted(
                f"{mod}.{name}" for mod, name in called
                if name in raisers.get(mod, ()) or name in raisers.get(stem, ()))
            if not risky:
                continue
            row = (rel, node.lineno, risky)
            covered = names & (EXIT_NAMES | {"BaseException"})
            (handled if covered else unhandled).append(row)

    print()
    print(f"try blocks calling a SystemExit raiser under `except Exception`:"
          f" {len(handled) + len(unhandled)}")
    for label, rows in (("HANDLED (also catches SystemExit)", handled),
                        ("UNHANDLED — the refusal escapes the guard",
                         unhandled)):
        print(f"  {label}: {len(rows)}")
        for rel, line, risky in rows:
            print(f"    {rel}:{line}  -> {', '.join(risky)}")
    return 1 if unhandled else 0


if __name__ == "__main__":
    raise SystemExit(main())
