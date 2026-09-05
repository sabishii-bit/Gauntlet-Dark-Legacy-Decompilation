"""Read-only full-image comparison for the enemy compiled-TU link experiment.

An optional --expected cleaned build must pass config/GUNE5D/build.sha1.
Without it the retail DOL is used, and exception-padding differences remain
visible. This never normalizes, patches, or writes either image.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def segments(data):
    if len(data) < 256:
        raise ValueError("truncated DOL header")
    offsets = struct.unpack_from(">18I", data, 0)
    addresses = struct.unpack_from(">18I", data, 0x48)
    sizes = struct.unpack_from(">18I", data, 0x90)
    result = []
    for i, (offset, address, size) in enumerate(zip(offsets, addresses, sizes)):
        if size:
            if offset < 256 or offset + size > len(data):
                raise ValueError("invalid DOL section extent")
            result.append((i, offset, address, size))
    return result


def compare(expected, built):
    a, b = segments(expected), segments(built)
    result = dict(expected_size=len(expected), built_size=len(built),
                  header_equal=expected[:256] == built[:256], sections=[])
    if a != b:
        result.update(layout_equal=False, expected_layout=a, built_layout=b)
        return result
    result["layout_equal"] = True
    total = 0
    for index, offset, address, size in a:
        differences = [j for j in range(size) if expected[offset+j] != built[offset+j]]
        total += len(differences)
        words = sorted({j & ~3 for j in differences})
        result["sections"].append(dict(index=index, address=hex(address), size=size,
            differing_bytes=len(differences), differing_words=len(words),
            words=[dict(address=hex(address+j),
                        expected=expected[offset+j:offset+j+4].hex(),
                        built=built[offset+j:offset+j+4].hex()) for j in words[:30]],
            words_truncated=len(words) > 30))
    result["differing_section_bytes"] = total
    # Also check padding and all header bytes, not only loaded sections.
    result["whole_file_equal"] = expected == built
    result["whole_file_differing_bytes"] = sum(x != y for x, y in zip(expected, built)) + abs(len(expected)-len(built))
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--built", type=Path, default=ROOT / "build/GUNE5D/main.dol")
    ap.add_argument("--expected", type=Path)
    ap.add_argument("--out", type=Path, default=ROOT / "build/r62_enemy_link_audit.json")
    args = ap.parse_args()
    expected = (args.expected or ROOT / "orig/GUNE5D/sys/main.dol").read_bytes()
    if args.expected:
        digest = (ROOT / "config/GUNE5D/build.sha1").read_text().split()[0]
        if hashlib.sha1(expected).hexdigest() != digest:
            raise ValueError("expected image does not pass the configured cleaned-build hash")
    built = args.built.read_bytes()
    result = compare(expected, built)
    result["expected_sha256"] = hashlib.sha256(expected).hexdigest()
    result["built_sha256"] = hashlib.sha256(built).hexdigest()
    result["reference"] = "verified cleaned build" if args.expected else "original retail (includes exception padding)"
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
