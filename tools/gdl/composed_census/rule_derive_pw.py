"""PW copy of tools/gdl/rule_derive.py, pointed at THIS worktree.

The committed tools/gdl/rule_derive.py hardcodes
W:\\Repositories\\GDL-Claude-WfMulti for both sys.path and the objects, so it
cannot run anywhere else. This copy derives every path from the repo root
and compares the PRE-webfrank body object (webfrank's actual input) against
the extracted target object.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    _find_symbol,
    _function_text_relocations,
    _sections,
    _u32,
)

OURS = ROOT / "build/GUNE5D/src/game/pb/.postprocess/body/pb_window.o"
TARGET = ROOT / "build/GUNE5D/obj/game/pb/pb_window.o"


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
