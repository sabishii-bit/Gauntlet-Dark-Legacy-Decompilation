"""List every .sdata2 pool reference inside a function, ours vs target.

Prints the text offset of each EMB_SDA21 relocation that targets the TU's
own constant pool, resolved to the pool BYTE OFFSET and the float/double
VALUE stored there, so first-use order can be compared value-for-value
across the two objects (whose pool symbol names differ: @NN vs lbl_).
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    _find_symbol,
    _function_text_relocations,
    _sections,
)

OURS = ROOT / "build/GUNE5D/src/game/pb/.postprocess/body/pb_window.o"
TARGET = ROOT / "build/GUNE5D/obj/game/pb/pb_window.o"


def pool_map(data, sections):
    """symbol name -> (pool offset, size, value) for .sdata2 objects."""
    out = {}
    sdata2 = None
    for index, section in enumerate(sections):
        if section.name == ".sdata2":
            sdata2 = (index, section)
    if sdata2 is None:
        return out
    index, section = sdata2
    blob = data[section.offset:section.offset + section.size]
    # walk the symbol table for objects in this section
    from tools.gdl.webfrank import _symbols
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
    data = path.read_bytes()
    sections = _sections(data)
    pool = pool_map(data, sections)
    symbol = _find_symbol(data, sections, function)
    relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    out = []
    for offset in sorted(relocations):
        name = relocations[offset][1]
        if name in pool:
            pool_offset, size, value = pool[name]
            out.append((offset, name, pool_offset, size, value))
    return out


function = sys.argv[1] if len(sys.argv) > 1 else "pbProjCalc"
for label, path in (("OURS", OURS), ("TARGET", TARGET)):
    print(f"=== {label}  {function}")
    seen = {}
    for offset, name, pool_offset, size, value in refs(path, function):
        first = "" if value in seen else "  <= FIRST USE"
        seen.setdefault(value, offset)
        print(f"  text+0x{offset:04x}  {name:>16}  pool+0x{pool_offset:02x} "
              f"({size}B) = {value!r}{first}")
    print("  first-use order:",
          ", ".join(repr(v) for v, _ in sorted(seen.items(), key=lambda kv: kv[1])))
