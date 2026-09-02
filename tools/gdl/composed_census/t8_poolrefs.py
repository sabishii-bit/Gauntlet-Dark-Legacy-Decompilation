"""List every .sdata2 pool reference inside a function, ours vs target.

Prints the text offset of each EMB_SDA21 relocation that targets the TU's
own constant pool, resolved to the pool BYTE OFFSET and the float/double
VALUE stored there, so first-use order can be compared value-for-value
across the two objects (whose pool symbol names differ: @NN vs lbl_).

Run from the repository root:
  python tools/gdl/composed_census/t8_poolrefs.py <unit> <function>
  python tools/gdl/composed_census/t8_poolrefs.py game/ui/auxscreen calc_wizard_pos

This is the VALUE view of the same evidence `fndiff --relocs` decides by
ADDRESS: the positional pass there tells you two relocations point at
different pool entries, and this tells you what is actually stored in
them. Use it to confirm a WRONG-DATUM row before touching source.

PROMOTION DAMAGE, repaired run 38 (it was `poolrefs.py`): written in a lane
scratch directory at the repository root and promoted here unchanged, so
`ROOT = parents[1]` pointed at tools/gdl and `from tools.gdl.webfrank
import ...` raised ModuleNotFoundError on every run; and the generic
basename is exactly the collision AGENTS.md rule 17(a) names. The unit and
object paths were also hardcoded to game/pb/pb_window, a TU that has since
been completed and flipped to Matching.
"""
import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    R_PPC_EMB_SDA21,
    _find_symbol,
    _function_text_relocations,
    _sections,
    _symbols,
)

VERSION = "GUNE5D"


def object_paths(unit):
    """(ours, target) for a unit, preferring the PRE-postprocess body.

    On a WebFrank-pinned TU the plain src/ object is post-rewrite, so the
    pool references it shows are the postprocessor's, not the compiler's.
    """
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    if unit.endswith((".c", ".cpp")):
        unit = unit.rsplit(".", 1)[0]
    ours = ROOT / f"build/{VERSION}/src/{unit}.o"
    body = ours.parent / ".postprocess" / "body" / ours.name
    if body.is_file():
        ours = body
    return ours, ROOT / f"build/{VERSION}/obj/{unit}.o"


def pool_map(data, sections):
    """symbol name -> (pool offset, size, value) for .sdata2 objects."""
    sdata2 = None
    for index, section in enumerate(sections):
        if section.name == ".sdata2":
            sdata2 = (index, section)
    if sdata2 is None:
        return {}
    index, section = sdata2
    blob = data[section.offset:section.offset + section.size]
    out = {}
    for symbol in _symbols(data, sections):
        if symbol.section_index != index or symbol.size == 0:
            continue
        raw = blob[symbol.value:symbol.value + symbol.size]
        if symbol.size == 4:
            value = struct.unpack(">f", raw)[0]
        elif symbol.size == 8:
            value = struct.unpack(">d", raw)[0]
        else:
            value = None
        out[symbol.name] = (symbol.value, symbol.size, value)
    return out


def refs(path, function):
    """(in_pool_rows, external_rows).

    A function can relocate .sdata2 entries that live OUTSIDE this object
    — the splitter's shared pool regions (`lbl_ADDR`) rather than the TU's
    own `@N` entries. Those carry no value here, and reporting only the
    empty in-pool list made the tool print a confident blank instead of
    saying it could not see them (AGENTS.md discipline 15: a check must
    print the values it compared).
    """
    data = path.read_bytes()
    sections = _sections(data)
    pool = pool_map(data, sections)
    symbol = _find_symbol(data, sections, function)
    relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    inside, outside = [], []
    for offset in sorted(relocations):
        reloc_type, name = relocations[offset][0], relocations[offset][1]
        if name in pool:
            pool_offset, size, value = pool[name]
            inside.append((offset, name, pool_offset, size, value))
        elif reloc_type == R_PPC_EMB_SDA21:
            outside.append((offset, name))
    return inside, outside


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("unit", help="e.g. game/ui/auxscreen")
    ap.add_argument("function")
    args = ap.parse_args()
    ours, target = object_paths(args.unit)
    for label, path in (("OURS", ours), ("TARGET", target)):
        if not path.exists():
            print(f"=== {label}  missing: {path}")
            continue
        print(f"=== {label}  {args.function}   ({path.name})")
        inside, outside = refs(path, args.function)
        seen = {}
        for offset, name, pool_offset, size, value in inside:
            first = "" if value in seen else "  <= FIRST USE"
            seen.setdefault(value, offset)
            print(f"  text+0x{offset:04x}  {name:>16}"
                  f"  pool+0x{pool_offset:02x} ({size}B) = {value!r}{first}")
        if seen:
            print("  first-use order:",
                  ", ".join(repr(v) for v, _ in
                            sorted(seen.items(), key=lambda kv: kv[1])))
        elif outside:
            print(f"  NO pool entry in this object's own .sdata2:"
                  f" all {len(outside)} EMB_SDA21 relocation(s) here name"
                  " symbols defined ELSEWHERE (the splitter's shared pool"
                  " regions), whose VALUES this tool cannot read from this"
                  " object. Read them from build/GUNE5D/asm/*sdata2.s"
                  " instead:")
            for offset, name in outside:
                print(f"    text+0x{offset:04x}  {name}")
        else:
            print("  no EMB_SDA21 relocation in this function at all")
    return 0


if __name__ == "__main__":
    sys.exit(main())
