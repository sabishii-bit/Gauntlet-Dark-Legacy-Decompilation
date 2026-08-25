#!/usr/bin/env python3
"""Reproduce and record MWCC's P6 conditional-plus-branch folding.

This is deliberately separate from :mod:`frank`: it is a compiler probe and
a synthetic remapping test bed, not a production object postprocessor.  The
probe compiles three portable-C control-flow shapes with the configured
1.2.5n body compiler, vanilla 1.2.5, and profile-patched 1.2.5e, then records
function bytes, profile markers, and local/historical Frank equality.

The ``plan_branch_pair_expansion`` helper operates on synthetic text only.  It
models all offsets that a future size-changing implementation would have to
rewrite and fails closed when the input is ambiguous.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from frank import PROFILE_MARKER, merge_objects  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
FIXTURE = REPO / "tools" / "gdl" / "tests" / "fixtures" / "p6_probe.c"
COMPILER_ROOT = REPO / "build" / "compilers" / "GC"
COMPILERS = {
    "configured": "1.2.5n",
    "vanilla": "1.2.5",
    "profile": "1.2.5e",
}
FLAGS = (
    "-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
    "-fp", "hardware", "-Cpp_exceptions", "off", "-O4,p", "-inline", "auto",
    "-pragma", "cats off", "-pragma", "warn_notinlined off", "-maxerrors", "1",
    "-nosyspath", "-RTTI", "off", "-fp_contract", "on", "-str", "reuse",
    "-multibyte", "-lang=c",
)
FOLD_SITES = {
    "p6_regfind": {"offset": 0x38, "folded": 0x41820018, "pair": 0x40820008},
    "p6_tally": {"offset": 0x18, "folded": 0x41810008, "pair": 0x40810008},
    "p6_shared_tail": {"offset": 0x18, "folded": 0x40800008, "pair": 0x41800008},
}


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class ELF32:
    """Small, strict ELF32 reader for MWCC probe objects."""

    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if self.data[:6] != b"\x7fELF\x01\x02":
            raise ValueError(f"{path}: expected ELF32 big-endian")
        section_at = struct.unpack_from(">I", self.data, 0x20)[0]
        entry_size, count, names_index = struct.unpack_from(">HHH", self.data, 0x2E)
        if entry_size < 40 or names_index >= count:
            raise ValueError(f"{path}: invalid section table")
        self.headers = [
            struct.unpack_from(">10I", self.data, section_at + i * entry_size)
            for i in range(count)
        ]
        names = self.headers[names_index]
        names_blob = self.data[names[4]:names[4] + names[5]]
        self.sections: dict[str, tuple[int, tuple[int, ...]]] = {}
        for index, header in enumerate(self.headers):
            start = header[0]
            name = names_blob[start:].split(b"\0", 1)[0].decode("ascii")
            self.sections[name] = (index, header)
        self.symbols: dict[str, list[tuple[int, int, int, int]]] = {}
        if ".symtab" in self.sections:
            _, symtab = self.sections[".symtab"]
            strings = self.headers[symtab[6]]
            string_blob = self.data[strings[4]:strings[4] + strings[5]]
            entry_size = symtab[9] or 16
            for offset in range(symtab[4], symtab[4] + symtab[5], entry_size):
                name_at, value, size, info, _, section = struct.unpack_from(
                    ">IIIBBH", self.data, offset
                )
                name = string_blob[name_at:].split(b"\0", 1)[0].decode("latin1")
                if name:
                    self.symbols.setdefault(name, []).append((value, size, info, section))

    def function(self, name: str) -> tuple[int, bytes]:
        functions = [entry for entry in self.symbols.get(name, ()) if entry[2] & 0xF == 2]
        if len(functions) != 1:
            raise ValueError(f"{self.path}: expected one {name} symbol, found {functions}")
        value, size, _, section = functions[0]
        header = self.headers[section]
        return value, self.data[header[4] + value:header[4] + value + size]

    def markers(self) -> list[dict[str, object]]:
        text_index, text = self.sections[".text"]
        payload = self.data[text[4]:text[4] + text[5]]
        markers = []
        cursor = 0
        while True:
            cursor = payload.find(PROFILE_MARKER, cursor)
            if cursor < 0:
                break
            owners = []
            for name, entries in self.symbols.items():
                for value, size, info, section in entries:
                    if info & 0xF == 2 and section == text_index and value <= cursor < value + size:
                        owners.append({"function": name, "relative_offset": cursor - value})
            markers.append({
                "profile_offset": cursor,
                "stripped_offset": cursor - 8 * len(markers),
                "owners": owners,
            })
            cursor += len(PROFILE_MARKER)
        return markers


def _compile(version: str, output: Path) -> None:
    compiler = COMPILER_ROOT / version / "mwcceppc.exe"
    if not compiler.is_file():
        raise FileNotFoundError(compiler)
    result = subprocess.run(
        [str(compiler), *FLAGS, "-c", str(FIXTURE), "-o", str(output)],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    if result.returncode or not output.is_file():
        raise RuntimeError(f"{version} failed:\n{result.stdout}{result.stderr}")


def _artifact(path: Path) -> dict[str, object]:
    elf = ELF32(path)
    functions = {}
    for name, site in FOLD_SITES.items():
        section_offset, data = elf.function(name)
        word = int.from_bytes(data[site["offset"]:site["offset"] + 4], "big")
        functions[name] = {
            "section_offset": section_offset,
            "size": len(data),
            "instruction_count": len(data) // 4,
            "sha256": _sha(data),
            "fold_offset": site["offset"],
            "fold_word": f"{word:08x}",
            "has_expected_fold": word == site["folded"],
        }
    return {
        "object_sha256": _sha(path.read_bytes()),
        "size": path.stat().st_size,
        "functions": functions,
    }


def run_probe(output: Path, historical_frank: Path | None = None) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    objects = {label: output / f"{version}.o" for label, version in COMPILERS.items()}
    for label, version in COMPILERS.items():
        _compile(version, objects[label])

    vanilla_bytes = objects["vanilla"].read_bytes()
    profile_bytes = objects["profile"].read_bytes()
    configured_bytes = objects["configured"].read_bytes()
    local_vanilla, vanilla_stats = merge_objects(vanilla_bytes, profile_bytes)
    local_configured, configured_stats = merge_objects(configured_bytes, profile_bytes)
    local_vanilla_path = output / "local-vanilla-frank.o"
    local_configured_path = output / "local-configured-frank.o"
    local_vanilla_path.write_bytes(local_vanilla)
    local_configured_path.write_bytes(local_configured)

    historical_path = None
    historical_equal = None
    if historical_frank is not None:
        historical_path = output / "historical-frank.o"
        subprocess.run(
            [sys.executable, str(historical_frank), str(objects["vanilla"]),
             str(objects["profile"]), str(historical_path)],
            cwd=REPO,
            check=True,
        )
        historical_equal = historical_path.read_bytes() == local_vanilla

    artifacts = {label: _artifact(path) for label, path in objects.items()}
    artifacts["local_vanilla_frank"] = _artifact(local_vanilla_path)
    artifacts["local_configured_frank"] = _artifact(local_configured_path)
    if historical_path is not None:
        artifacts["historical_frank"] = _artifact(historical_path)
    return {
        "fixture_sha256": _sha(FIXTURE.read_bytes()),
        "compiler_sha256": {
            label: _sha((COMPILER_ROOT / version / "mwcceppc.exe").read_bytes())
            for label, version in COMPILERS.items()
        },
        "compilers": COMPILERS,
        "flags": list(FLAGS),
        "profile_markers": ELF32(objects["profile"]).markers(),
        "merge": {
            "local_vanilla": asdict(vanilla_stats),
            "local_configured": asdict(configured_stats),
            "historical_local_byte_equal": historical_equal,
        },
        "artifacts": artifacts,
    }


@dataclass(frozen=True)
class Relocation:
    offset: int
    kind: str
    symbol: str
    addend: int = 0


@dataclass(frozen=True)
class Symbol:
    name: str
    value: int
    size: int


@dataclass(frozen=True)
class BranchPairPlan:
    old_size: int
    new_size: int
    site: int
    new_text: bytes
    relocations: tuple[Relocation, ...]
    symbols: tuple[Symbol, ...]


def plan_branch_pair_expansion(
    text: bytes,
    *,
    site: int,
    expected_sha256: str,
    expected_folded: int,
    pair_conditional: int,
    relocations: tuple[Relocation, ...] = (),
    symbols: tuple[Symbol, ...] = (),
) -> BranchPairPlan:
    """Plan one synthetic P6 expansion and every required offset remap.

    This intentionally accepts raw synthetic ``.text`` rather than an object.
    It cannot be wired into Frank accidentally, and its exact hash/word/control
    checks document the minimum proof a production implementation would need.
    """
    if _sha(text) != expected_sha256:
        raise ValueError("stale text hash")
    if site % 4 or site < 0 or site + 4 > len(text):
        raise ValueError("unaligned or out-of-range site")
    needle = expected_folded.to_bytes(4, "big")
    occurrences = [offset for offset in range(0, len(text), 4) if text[offset:offset + 4] == needle]
    if occurrences != [site]:
        raise ValueError(f"ambiguous folded instruction: {occurrences}")
    if expected_folded >> 26 != 16 or expected_folded & 3:
        raise ValueError("folded instruction is not a relative non-link conditional branch")
    if pair_conditional >> 26 != 16 or pair_conditional & 3 or pair_conditional & 0xFFFC != 8:
        raise ValueError("pair conditional must be a relative non-link branch to site+8")
    if any(relocation.offset == site for relocation in relocations):
        raise ValueError("candidate branch carries relocation metadata")
    if len({relocation.offset for relocation in relocations}) != len(relocations):
        raise ValueError("ambiguous duplicate relocation offsets")

    old_disp = expected_folded & 0xFFFC
    if old_disp & 0x8000:
        old_disp -= 0x10000
    old_target = site + old_disp
    if old_target <= site or old_target > len(text) or old_target % 4:
        raise ValueError("folded branch must have one forward in-section destination")
    unconditional = 0x48000000 | (old_disp & 0x03FFFFFC)
    new_text = bytearray(text[:site] + pair_conditional.to_bytes(4, "big")
                         + unconditional.to_bytes(4, "big") + text[site + 4:])

    relocation_offsets = {relocation.offset for relocation in relocations}
    for old_source in range(0, len(text), 4):
        if old_source == site:
            continue
        word = int.from_bytes(text[old_source:old_source + 4], "big")
        opcode = word >> 26
        if opcode == 19 and word != 0x4E800020:
            raise ValueError(f"unsupported indirect control instruction at {old_source:#x}")
        if opcode not in (16, 18):
            continue
        if word & 2:
            raise ValueError(f"absolute branch at {old_source:#x}")
        if word & 1:
            if old_source not in relocation_offsets:
                raise ValueError(f"unrelocated call/control instruction at {old_source:#x}")
            continue
        mask = 0xFFFC if opcode == 16 else 0x03FFFFFC
        width = 0x10000 if opcode == 16 else 0x04000000
        sign = 0x8000 if opcode == 16 else 0x02000000
        displacement = word & mask
        if displacement & sign:
            displacement -= width
        old_destination = old_source + displacement
        if old_destination < 0 or old_destination > len(text) or old_destination % 4:
            raise ValueError(f"out-of-section branch at {old_source:#x}")
        new_source = old_source + (4 if old_source > site else 0)
        new_destination = old_destination + (4 if old_destination > site else 0)
        new_displacement = new_destination - new_source
        if not -(width // 2) <= new_displacement < width // 2:
            raise ValueError(f"branch displacement overflow at {old_source:#x}")
        rewritten = (word & ~mask) | (new_displacement & mask)
        new_text[new_source:new_source + 4] = rewritten.to_bytes(4, "big")

    mapped_relocations = tuple(
        Relocation(r.offset + (4 if r.offset > site else 0), r.kind, r.symbol, r.addend)
        for r in relocations
    )
    containers = [s for s in symbols if s.value <= site < s.value + s.size]
    if symbols and len(containers) != 1:
        raise ValueError(f"expected one containing function symbol, found {containers}")
    mapped_symbols = []
    for symbol in symbols:
        if symbol in containers:
            mapped_symbols.append(Symbol(symbol.name, symbol.value, symbol.size + 4))
        elif symbol.value > site:
            mapped_symbols.append(Symbol(symbol.name, symbol.value + 4, symbol.size))
        else:
            mapped_symbols.append(symbol)
    return BranchPairPlan(
        len(text), len(new_text), site, bytes(new_text), mapped_relocations,
        tuple(mapped_symbols),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="scratch object directory")
    parser.add_argument("--historical-frank", type=Path,
                        help="authoritative historical Melee frank.py")
    parser.add_argument("--record", type=Path, help="write JSON evidence to this path")
    args = parser.parse_args()
    report = run_probe(args.output.resolve(), args.historical_frank)
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.record:
        args.record.write_text(rendered)
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
