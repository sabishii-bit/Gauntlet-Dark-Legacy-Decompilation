#!/usr/bin/env python3
"""Build a separate GC/1.2.5 PCode-layout derived compiler.

This script contains no bytes from the compiler.  It accepts only pinned,
user-supplied vanilla/1.2.5n executables and a separately built open payload.
Every PE layout, call-site, relocation, and payload invariant is checked before
an output is written.
"""

from __future__ import annotations

import hashlib
import os
import struct
import sys
import tempfile
from pathlib import Path

INPUTS = {
    "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914": "GC/1.2.5",
    "ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c": "GC/1.2.5n",
}
EXPECTED_OUTPUTS = {
    "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914": "7cbeb085205df54bca3fb89ff7a19d323003c1a63a14a942e12ef06cec7c3a31",
    "ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c": "5a4d1e1715954ddefc87a5a0dfbe38b6c3916e22214957b21af3bd147a760667",
}

PAYLOAD_SHA256 = "fc280690e5eef8246401baf7940d89b47f25bbaa6b293666a4c4ba2498085477"
CALL_VA = 0x00435AFA
ORIGINAL_CALLSITE = bytes.fromhex("e8 f1 75 06 00")
SECTION_NAME = b".p6fix\0\0"
SECTION_CHARACTERISTICS = 0x60000020  # code | execute | read


def align(value: int, boundary: int) -> int:
    return (value + boundary - 1) & -boundary


def parse_headers(image: bytearray) -> dict[str, int]:
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe : pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    machine, sections = struct.unpack_from("<HH", image, pe + 4)
    optional_size = struct.unpack_from("<H", image, pe + 20)[0]
    optional = pe + 24
    if machine != 0x14C or struct.unpack_from("<H", image, optional)[0] != 0x10B:
        raise ValueError("expected PE32 i386")
    section_table = optional + optional_size
    return {
        "pe": pe,
        "sections": sections,
        "optional": optional,
        "section_table": section_table,
        "image_base": struct.unpack_from("<I", image, optional + 28)[0],
        "section_alignment": struct.unpack_from("<I", image, optional + 32)[0],
        "file_alignment": struct.unpack_from("<I", image, optional + 36)[0],
        "size_code": struct.unpack_from("<I", image, optional + 4)[0],
        "size_initialized": struct.unpack_from("<I", image, optional + 8)[0],
        "size_uninitialized": struct.unpack_from("<I", image, optional + 12)[0],
        "size_image": struct.unpack_from("<I", image, optional + 56)[0],
        "size_headers": struct.unpack_from("<I", image, optional + 60)[0],
        "reloc_rva": struct.unpack_from("<I", image, optional + 136)[0],
        "reloc_size": struct.unpack_from("<I", image, optional + 140)[0],
    }


def sections(image: bytearray, h: dict[str, int]) -> list[dict[str, int | bytes]]:
    result = []
    for index in range(h["sections"]):
        off = h["section_table"] + index * 40
        name = bytes(image[off : off + 8])
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", image, off + 8)
        characteristics = struct.unpack_from("<I", image, off + 36)[0]
        result.append({
            "header": off, "name": name, "virtual_size": virtual_size,
            "rva": rva, "raw_size": raw_size, "raw": raw,
            "characteristics": characteristics,
        })
    return result


def rva_to_offset(rva: int, table: list[dict[str, int | bytes]]) -> int:
    for s in table:
        start = int(s["rva"])
        span = max(int(s["virtual_size"]), int(s["raw_size"]))
        if start <= rva < start + span:
            return int(s["raw"]) + rva - start
    raise ValueError(f"RVA {rva:#x} is not file backed")


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} INPUT_EXE PAYLOAD_BIN OUTPUT_EXE")
    source, payload_path, output = map(Path, sys.argv[1:])
    if source.suffix.lower() != ".exe" or output.suffix.lower() != ".exe":
        raise SystemExit("input and output must both use the .exe extension")
    if not output.stem.lower().startswith("mwcceppc-") or "_" in output.stem:
        raise SystemExit("output must use a MWCC-safe hyphenated name beginning mwcceppc-")
    resolved_source = os.path.normcase(str(source.resolve()))
    resolved_payload = os.path.normcase(str(payload_path.resolve()))
    resolved_output = os.path.normcase(str(output.resolve()))
    if resolved_output == resolved_source:
        raise SystemExit("input and output paths must differ")
    if resolved_output == resolved_payload:
        raise SystemExit("payload and output paths must differ")
    if not output.parent.resolve().is_dir():
        raise SystemExit("output directory does not exist")

    image = bytearray(source.read_bytes())
    input_hash = hashlib.sha256(image).hexdigest()
    if input_hash not in INPUTS:
        raise SystemExit(f"unsupported input SHA-256 {input_hash}")
    payload = payload_path.read_bytes()
    if hashlib.sha256(payload).hexdigest() != PAYLOAD_SHA256:
        raise SystemExit("unexpected payload hash; rebuild the reviewed open source")

    h = parse_headers(image)
    table = sections(image, h)
    if (h["image_base"], h["section_alignment"], h["file_alignment"],
        h["size_headers"], h["sections"]) != (0x400000, 0x1000, 0x200, 0x400, 9):
        raise SystemExit("unexpected PE layout")
    if h["section_table"] + (h["sections"] + 1) * 40 > h["size_headers"]:
        raise SystemExit("no room for an additional section header")
    new_header = h["section_table"] + h["sections"] * 40
    if any(image[new_header : new_header + 40]):
        raise SystemExit("new section-header slot is not zero-filled")

    reloc = next((s for s in table if s["name"].rstrip(b"\0") == b".reloc"), None)
    if reloc is None or int(reloc["rva"]) != h["reloc_rva"] or h["reloc_size"] != int(reloc["virtual_size"]):
        raise SystemExit("unexpected relocation directory")
    if int(reloc["raw"]) + int(reloc["raw_size"]) != len(image):
        raise SystemExit("overlay or non-final relocation section is unsupported")

    call_rva = CALL_VA - h["image_base"]
    call_off = rva_to_offset(call_rva, table)
    if image[call_off : call_off + 5] != ORIGINAL_CALLSITE:
        raise SystemExit("call insertion bytes do not match")

    new_rva = align(h["size_image"], h["section_alignment"])
    new_raw = align(len(image), h["file_alignment"])
    if new_raw != len(image):
        raise SystemExit("unexpected unaligned input end")
    raw_size = align(len(payload), h["file_alignment"])
    new_va = h["image_base"] + new_rva
    displacement = new_va - (CALL_VA + 5)
    image[call_off : call_off + 5] = b"\xE8" + struct.pack("<i", displacement)

    # Append relocations for the payload's two absolute operands.  Verify their
    # exact locations instead of discovering arbitrary immediates.
    if payload[3:7] != struct.pack("<I", 0x00587C74) or payload[0x13:0x17] != struct.pack("<I", 0x0049D0F0):
        raise SystemExit("payload entry absolute operands moved")
    reloc_append = int(reloc["raw"]) + h["reloc_size"]
    reloc_block = struct.pack("<IIHH", new_rva, 12, 0x3003, 0x3013)
    if reloc_append + len(reloc_block) > int(reloc["raw"]) + int(reloc["raw_size"]):
        raise SystemExit("insufficient relocation-section tail padding")
    if any(image[reloc_append : reloc_append + len(reloc_block)]):
        raise SystemExit("relocation tail is not zero-filled")
    image[reloc_append : reloc_append + len(reloc_block)] = reloc_block
    new_reloc_size = h["reloc_size"] + len(reloc_block)
    struct.pack_into("<I", image, h["optional"] + 140, new_reloc_size)
    struct.pack_into("<I", image, int(reloc["header"]) + 8, new_reloc_size)

    section_header = struct.pack(
        "<8sIIIIIIHHI", SECTION_NAME, len(payload), new_rva, raw_size, new_raw,
        0, 0, 0, 0, SECTION_CHARACTERISTICS,
    )
    image[new_header : new_header + 40] = section_header
    struct.pack_into("<H", image, h["pe"] + 6, h["sections"] + 1)
    struct.pack_into("<I", image, h["optional"] + 4, h["size_code"] + raw_size)
    struct.pack_into("<I", image, h["optional"] + 56,
                     align(new_rva + len(payload), h["section_alignment"]))
    image.extend(payload)
    image.extend(b"\0" * (raw_size - len(payload)))

    output_hash = hashlib.sha256(image).hexdigest()
    if output_hash != EXPECTED_OUTPUTS[input_hash]:
        raise SystemExit(f"derived output hash mismatch: {output_hash}")
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", delete=False, dir=output.parent,
            prefix=output.name + ".tmp-", suffix=".tmp"
        ) as handle:
            temporary = Path(handle.name)
            handle.write(image)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"input={INPUTS[input_hash]} sha256={input_hash}")
    print(f"call VA={CALL_VA:#010x} file={call_off:#x} -> section VA={new_va:#010x}")
    print(f"section header={new_header:#x} raw={new_raw:#x}+{raw_size:#x} payload={len(payload):#x}")
    print(f"new HIGHLOW relocation RVAs={new_rva+3:#x},{new_rva+0x13:#x}")
    print(f"output sha256={output_hash}")


if __name__ == "__main__":
    main()
