#!/usr/bin/env python3
"""Reproduce world.c's approved aggregate-local compiler compatibility idiom.

Run from a configured, fully built checkout. Compile using the actual Ninja
edge, require whole-object fidelity with the shipped RAW compiler object,
then compare the two function bodies with the extracted target. A generated
scalar-local negative control must reproduce the extra instruction in each.
No source, build graph, reference object, or postprocessor rule is modified.

This tests raw instructions, not relocation/data equality or modified-game
runtime behavior. A full `ninja` / main.dol: OK is the link-level authority.
"""
import argparse
import tempfile
from pathlib import Path

from cn_analyze import load, target_object
from cv_probe import REPO, compile_with, read_edges


UNIT = "game/world/world"
EXPECTED_COUNTS = {"StartWorldLoad": 122, "LoadWorldDone": 84}


def scalar_control(source):
    """Undo precisely the two approved wrappers; refuse a changed source shape."""
    replacements = {
        "    WorldNameRef load;\n    char* buf = lbl_801151D8;\n"
        "    load.base = gWorldName;":
        "    char* worldNameBase = gWorldName;\n    char* buf = lbl_801151D8;",
        "    WorldNameRef load;\n    s32 size;\n    load.base = gWorldName;":
        "    char* worldNameBase = gWorldName;\n    s32 size;",
    }
    if source.count("WorldNameRef load;") != 2:
        raise ValueError("expected exactly two WorldNameRef locals")
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError("wrapper declaration/initialization has changed")
        source = source.replace(old, new, 1)
    if "load.base" not in source:
        raise ValueError("no member uses found for the scalar negative control")
    return source.replace("load.base", "worldNameBase")


def compile_edge(edge, output):
    result, error = compile_with(edge, edge["mw"], edge["cflags"],
                                 output, output.parent)
    if error or result is None:
        raise RuntimeError(error or "compiler returned no object")
    return result


def function_bytes(path, function):
    body = load(str(path), function)[3]
    if not body or len(body) % 4:
        raise ValueError(f"invalid/empty PPC body for {function} in {path}")
    return body


def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    edge = read_edges()[UNIT]
    if not edge["raw"]:
        raise ValueError("expected world.c's pre-WebFrank raw compile edge")
    shipped = REPO / edge["body_o"]
    target = Path(target_object(UNIT))
    source = (REPO / edge["src"]).read_text(encoding="utf-8")
    control_source = scalar_control(source)
    # The temporary directory is confined to build/ and removed on every exit.
    with tempfile.TemporaryDirectory(prefix="r59-world-name-ref-",
                                     dir=REPO / "build") as scratch:
        scratch = Path(scratch)
        baseline = compile_edge(edge, scratch / "baseline.o")
        if baseline.read_bytes() != shipped.read_bytes():
            raise ValueError("fidelity failed: run ninja before this probe")
        print(f"FIDELITY: raw whole-object identity ({edge['mw']})")
        generated = scratch / "world_scalar_control.c"
        generated.write_text(control_source, encoding="utf-8")
        control = compile_edge(dict(edge, src=str(generated)),
                               scratch / "scalar.o")
        for function, count in EXPECTED_COUNTS.items():
            expected = function_bytes(target, function)
            actual = function_bytes(baseline, function)
            scalar = function_bytes(control, function)
            if len(expected) != count * 4 or actual != expected:
                raise ValueError(f"{function}: wrapper no longer raw-byte exact")
            if len(scalar) != len(expected) + 4:
                raise ValueError(f"{function}: scalar +1 instruction cap changed")
            print(f"{function}: wrapper T/O={count}/{len(actual)//4}, "
                  f"raw bytes EXACT; scalar T/O={count}/{len(scalar)//4}")
    print("PASS: two raw exact functions; two scalar +1 instruction controls")


if __name__ == "__main__":
    main()
