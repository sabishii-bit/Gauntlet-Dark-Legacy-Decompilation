"""Re-derive permutation specs against the CURRENT objects.

Never trusts banked offsets: everything below is read out of the objects
that ninja just produced.

Usage: python tools/gdl/rule_derive.py [unit]   (default game/pb/pb_window)
Runs from ANY checkout: paths resolve relative to this file's repo, and
the OURS side prefers the raw compiler output (.postprocess/body/) so a
unit that already has webfrank rules is derived from its true residual.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    _find_symbol,
    _function_text_relocations,
    _sections,
    _u32,
)

UNIT = (sys.argv[1] if len(sys.argv) > 1 else "game/pb/pb_window").strip("/")
_ours = ROOT / "build" / "GUNE5D" / "src" / (UNIT + ".o")
_raw = _ours.parent / ".postprocess" / "body" / _ours.name
OURS = _raw if _raw.is_file() else _ours
TARGET = ROOT / "build" / "GUNE5D" / "obj" / (UNIT + ".o")


def load(path, name):
    data = path.read_bytes()
    sections = _sections(data)
    symbol = _find_symbol(data, sections, name)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    body = data[start:start + symbol.size]
    relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    return body, relocations


def report(name):
    ours, our_relocations = load(OURS, name)
    target, target_relocations = load(TARGET, name)
    print(f"=== {name}: ours {len(ours)} bytes, target {len(target)} bytes")
    if len(ours) != len(target):
        print("  SIZE MISMATCH")
        return
    differing = [
        offset for offset in range(0, len(ours), 4)
        if _u32(ours, offset) != _u32(target, offset)
    ]
    print(f"  differing words: {len(differing)}")
    runs = []
    for offset in differing:
        if runs and offset == runs[-1][-1] + 4:
            runs[-1].append(offset)
        else:
            runs.append([offset])
    for run in runs:
        print(f"  run +0x{run[0]:x}..+0x{run[-1] + 4:x}  ({len(run)} words)")
        for offset in run:
            our_reloc = our_relocations.get(offset) or our_relocations.get(offset + 2)
            their_reloc = (target_relocations.get(offset)
                           or target_relocations.get(offset + 2))
            marks = []
            if our_reloc:
                marks.append(f"ours={our_reloc[0]}:{our_reloc[1]}")
            if their_reloc:
                marks.append(f"tgt={their_reloc[0]}:{their_reloc[1]}")
            print(f"    +0x{offset:03x}  {_u32(ours, offset):08x} -> "
                  f"{_u32(target, offset):08x}  {' '.join(marks)}")


for function in sys.argv[1:] or ["pbWinSetup", "pbProjCalc"]:
    report(function)
