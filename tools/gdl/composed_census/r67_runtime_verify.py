"""Verify runtime raw/fixed boundaries and a resolved editable literal.

Read-only diagnostic over an already-built default GUNE5D graph. It does not
configure, compile, edit source, or infer a runtime boot from successful linking.
Use --mode matching for the unchanged baseline, or --mode editable with
--expect-string MODIFIED! after the controlled exception::what source edit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT), str(ROOT / "tools/gdl/composed_census")]
from tools import fix_exception_objects as fix
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census import r66_exception_mod_probe as prior


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def linked_observation(elf_path, dol_path):
    """Resolve the actual three-instruction what body and its pointed-to bytes."""
    elf = fix.Elf(str(elf_path))
    matches = [elf.sym(i) for i in range(elf.symcount)
               if elf.symname(i) == prior.FUNCTION]
    if len(matches) != 1 or matches[0][2] != 12:
        raise ValueError("linked exception::what is not one 12-byte symbol")
    address = matches[0][1]

    def read_elf(at, length):
        matches = [s for s in elf.sh if s[1] != 8 and s[3] <= at
                   and at + length <= s[3] + s[5] and s[2] & 2]
        if len(matches) != 1:
            raise ValueError(f"ELF address {at:#x} does not resolve uniquely")
        s = matches[0]
        start = s[4] + at - s[3]
        return bytes(elf.data[start:start + length])

    dol = Path(dol_path).read_bytes()
    if len(dol) < 0x100:
        raise ValueError("short DOL header")
    offsets = struct.unpack_from(">18I", dol, 0)
    addresses = struct.unpack_from(">18I", dol, 0x48)
    sizes = struct.unpack_from(">18I", dol, 0x90)

    def read_dol(at, length):
        matches = [(off, base) for off, base, size in zip(offsets, addresses, sizes)
                   if size and base <= at and at + length <= base + size]
        if len(matches) != 1:
            raise ValueError(f"DOL address {at:#x} does not resolve uniquely")
        off, base = matches[0]
        value = dol[off + at - base:off + at - base + length]
        if len(value) != length:
            raise ValueError("DOL section extends beyond its file")
        return value

    body = read_elf(address, 12)
    words = struct.unpack(">3I", body)
    if (words[0] & 0xFFFF0000, words[1] & 0xFFFF0000, words[2]) != (
            0x3C600000, 0x38630000, 0x4E800020):
        raise ValueError("unsupported linked what body; expected lis/addi/blr")
    low = words[1] & 0xFFFF
    string_address = (((words[0] & 0xFFFF) << 16) + (low if low < 0x8000 else low - 0x10000)) & 0xFFFFFFFF
    data = bytearray()
    for offset in range(256):
        byte = read_elf(string_address + offset, 1)
        if byte == b"\0":
            break
        data.extend(byte)
    else:
        raise ValueError("no terminator in bounded what literal")
    encoded = bytes(data) + b"\0"
    if read_dol(address, 12) != body or read_dol(string_address, len(encoded)) != encoded:
        raise ValueError("DOL does not carry the linked ELF function and its actual datum")
    return {"function_address": f"0x{address:08x}", "words": [f"{w:08x}" for w in words],
            "string_address": f"0x{string_address:08x}", "string": data.decode("ascii"),
            "elf_and_dol_agree": True, "elf_sha256": sha(elf_path), "dol_sha256": sha(dol_path)}


def verify(root, mode, expected):
    snapshot = json.loads((root / "build/GUNE5D/build_edges.json").read_text())
    if snapshot.get("schema_version") != 1 or snapshot.get("ninja_sha256") != sha(root / "build.ninja"):
        raise ValueError("missing/current schema or stale active build snapshot")
    if snapshot.get("non_matching") != (mode == "editable"):
        raise ValueError("active configuration does not match requested mode")
    transforms = [edge for edge in snapshot["edges"]
                  if edge["rule"] in {"fix_exception_object", "fix_exception_objects"}]
    if mode == "editable" and transforms:
        raise ValueError("editable graph still contains a runtime rewrite edge")
    if mode == "matching" and (len(transforms) != 2 or
            any(e["rule"] != "fix_exception_object" for e in transforms)):
        raise ValueError("matching graph does not have exactly two separate runtime rewrites")
    if mode == "editable" and any(e["rule"] in {
            "frank", "webfrank", "webfrank_globalize_atree", "p6frank", "retail_dol"}
            for e in snapshot["edges"]):
        raise ValueError("editable graph still contains retail-byte postprocessing")
    edges = cv.read_edges()
    rows = []
    for unit, kind in (("Runtime.PPCEABI.H/NMWException", "nmw"),
                       ("Runtime.PPCEABI.H/ExceptionPPC", "exppc")):
        raw = root / edges[unit]["body_o"]
        final = root / "build/GUNE5D/src" / (unit + ".o")
        row = {"unit": unit, "raw_object": str(raw.relative_to(root)),
               "raw_sha256": sha(raw), "final_object": str(final.relative_to(root)),
               "final_sha256": sha(final)}
        if mode == "matching":
            expected_raw, expected_fixed = fix.OBJECT_HASHES[kind]
            if raw.resolve() == final.resolve() or (row["raw_sha256"], row["final_sha256"]) != (expected_raw, expected_fixed):
                raise ValueError(f"{unit}: raw/fixed separation or baseline fingerprint failed")
            matched_edges = [e for e in transforms if str(final.relative_to(root)).replace("\\", "/") in e["outputs"]]
            if len(matched_edges) != 1 or matched_edges[0]["inputs"] != [str(raw.relative_to(root)).replace("\\", "/")]:
                raise ValueError(f"{unit}: fixed edge does not read precisely the raw object")
        elif raw.resolve() != final.resolve():
            raise ValueError(f"{unit}: editable linker input is not the compiler output")
        rows.append(row)
    nmw = root / rows[0]["final_object"]
    what = prior.inspect_object(nmw)
    if what["what_strings"] != [expected]:
        raise ValueError("compiled exception::what does not reference the expected string")
    linked = linked_observation(root / "build/GUNE5D/main.elf", root / "build/GUNE5D/main.dol")
    if linked["string"] != expected:
        raise ValueError("linked exception::what does not reference the expected string")
    return {"runtime_objects": rows, "linked": linked,
            "object_what_strings": what["what_strings"], "runtime_transform_edges": transforms}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("matching", "editable"), required=True)
    parser.add_argument("--expect-string", default="exception")
    parser.add_argument("--out", type=Path, default=Path("build/r67_runtime_verify.json"))
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if (not output.is_relative_to((ROOT / "build").resolve()) or output.suffix != ".json"
            or not output.name.startswith("r67_runtime_")):
        parser.error("--out must be an r67_runtime_*.json artifact under this checkout's build/")
    result = {"schema_version": 1, "status": "UNRESOLVED", "mode": args.mode,
              "expected_string": args.expect_string,
              "scope": "active runtime pipeline and resolved what literal in linked ELF/DOL; no boot test"}
    try:
        result.update(verify(ROOT, args.mode, args.expect_string))
        result["status"] = "PASS"
    except (OSError, ValueError, KeyError, struct.error) as error:
        result["error"] = str(error)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], args.mode, result.get("error", result.get("linked", {}).get("string")), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
