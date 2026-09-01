"""Compose the pb_window rules and prove them through apply_patch itself.

Every hash below is computed from the objects ninja just produced; nothing
is copied from a banked record.

Usage: python tools/gdl/build_rule.py [unit]   (default game/pb/pb_window)
Runs from ANY checkout; the OURS side prefers the raw compiler output
(.postprocess/body/) — reading the post-webfrank object as input is only
correct when webfrank is disabled for the unit.
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    SHT_RELA,
    _find_symbol,
    _function_text_relocations,
    _relocation_sha256,
    _sections,
    _sha256,
    _u32,
    apply_patch,
    check_permutation_dependences,
)
from tools.gdl.reloc_symbols import (  # noqa: E402
    moved_symbols,
    region_symbols,
)

_args = [a for a in sys.argv[1:] if not a.startswith("--")]
UNIT = (_args[0] if _args else "game/pb/pb_window").strip("/")
OUT = Path(next((a.split("=", 1)[1] for a in sys.argv
                 if a.startswith("--out=")),
                ROOT / "build" / "GUNE5D" / "rules"
                / f"{UNIT.replace('/', '_')}_rules.json"))
_ours = ROOT / "build" / "GUNE5D" / "src" / (UNIT + ".o")
_raw = _ours.parent / ".postprocess" / "body" / _ours.name
OURS = _raw if _raw.is_file() else _ours
TARGET = ROOT / "build" / "GUNE5D" / "obj" / (UNIT + ".o")


def function_bytes(path, name):
    data = bytearray(path.read_bytes())
    sections = _sections(data)
    symbol = _find_symbol(data, sections, name)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return data, sections, symbol, bytes(data[start:start + symbol.size])


def region_records(data, sections, symbol, start, end):
    """Region-relative (offset, info, addend) triples, as apply_patch builds."""
    relocation_section = [
        section for section in sections
        if section.section_type == SHT_RELA and section.info == symbol.section_index
    ][0]
    entry = relocation_section.entry_size or 12
    records = []
    section_start = symbol.value + start
    section_end = symbol.value + end
    for offset in range(
        relocation_section.offset,
        relocation_section.offset + relocation_section.size,
        entry,
    ):
        r_offset, r_info, r_addend = struct.unpack_from(">IIi", data, offset)
        if section_start <= r_offset < section_end:
            records.append((r_offset - section_start, r_info, r_addend))
    return records


def window(data, sections, symbol, body, start, end, order):
    region = body[start:end]
    atoms = [region[i:i + 4] for i in range(0, len(region), 4)]
    permuted = b"".join(atoms[source] for source in order)

    check_permutation_dependences(region, order)
    print(f"    dependence audit PASSED for +0x{start:x}..+0x{end:x}")

    before = region_records(data, sections, symbol, start, end)
    destination_by_source = {s: d for d, s in enumerate(order)}
    after = sorted(
        (destination_by_source[offset // 4] * 4 + offset % 4, info, addend)
        for offset, info, addend in before
    )
    # Name-bound relocation hashing (run-28 migration): passing the bare
    # triples raises "relocation hash needs the symbol name" on every
    # window that carries a relocation. This file died that way on its own
    # default unit until the symbol maps were threaded through.
    relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size)
    window_symbols = region_symbols(relocations, start, end)
    after_symbols = moved_symbols(window_symbols, order)
    return {
        "start": f"0x{start:x}",
        "end": f"0x{end:x}",
        "order": list(order),
        "before_sha256": _sha256(region),
        "after_sha256": _sha256(permuted),
        "before_relocations_sha256": _relocation_sha256(
            before, window_symbols),
        "after_relocations_sha256": _relocation_sha256(
            after, after_symbols),
    }, permuted


def compose(name, windows, copy_forms=None):
    data, sections, symbol, body = function_bytes(OURS, name)
    _tdata, _ts, _tsym, target_body = function_bytes(TARGET, name)
    print(f"=== {name} ({len(body)} bytes)")

    patch = {
        "function": name,
        "before_sha256": _sha256(body),
        "after_sha256": _sha256(target_body),
    }
    specs = []
    for start, end, order in windows:
        spec, _permuted = window(data, sections, symbol, body, start, end, order)
        specs.append(spec)
    patch["instruction_permutation"] = specs if len(specs) > 1 else specs[0]
    if copy_forms:
        patch["equivalent_copy_form"] = copy_forms
    patch["copy_register_fields"] = True

    # Prove it the only way that counts: through apply_patch, on a copy of
    # the real object, against the real target object.
    scratch = bytearray(OURS.read_bytes())
    before, after, changed = apply_patch(
        scratch, json.loads(json.dumps(patch)), TARGET.read_bytes()
    )
    print(f"    apply_patch ACCEPTED: {changed} atoms/fields adjusted")
    print(f"    after == target body: {after == _sha256(target_body)}")
    return patch


rules = []
rules.append(compose(
    "pbWinSetup",
    [(0x188, 0x194, [0, 2, 1]),
     (0x2c0, 0x2d8, [0, 2, 5, 3, 1, 4])],
    copy_forms=[{"at": "0x2c8", "proof": "dominating_def"}],
))
rules.append(compose(
    "pbProjCalc",
    [(0x28, 0x6c, list(range(1, 17)) + [0]),
     (0x3e4, 0x3ec, [1, 0])],
))

# Output path is an ARGUMENT with a build-directory default. It used to be
# a hardcoded WF_scratch/ path inside the tracked tree: that both writes an
# untracked artifact into the repo root and hard-crashes with
# FileNotFoundError in any checkout where the foreign lane's scratch
# directory does not exist — which is every worktree but its author's.
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(rules, indent=2))
print(f"\nwrote {OUT}")
