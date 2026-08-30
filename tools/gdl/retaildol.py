#!/usr/bin/env python3
"""Produce a byte-perfect retail main.dol from the verified cleaned build.

The build's primary artifact targets the extab-cleaned DOL because the retail
image contains uninitialized exception-table padding bytes that no fresh link
can reproduce. Those bytes exist in the user's own original DOL, so this step
splices them back into a COPY of the verified build output, yielding an
artifact byte-identical to the retail disc image.

Fail-closed guards:
  1. The original DOL must match the configured retail SHA-1.
  2. The built DOL must differ from retail only within the extab/extabindex
     address range parsed from splits.txt, and by at most --max-bytes bytes;
     anything more means the build is wrong, and no artifact is produced.
  3. The spliced result must hash exactly to the retail SHA-1.

Usage (wired as a ninja post-build step; also runnable directly):
  python tools/gdl/retaildol.py build/GUNE5D/main.dol build/GUNE5D/main.retail.dol
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
VERSION = "GUNE5D"


def fail(message: str, out: Path | None = None) -> int:
    if out is not None and out.exists():
        out.unlink()
    print(f"retaildol: {message}", file=sys.stderr)
    return 1


def retail_sha1(config_yml: Path) -> str:
    match = re.search(
        r"^hash:\s*([0-9A-Fa-f]{40})", config_yml.read_text(encoding="utf-8"), re.M
    )
    if not match:
        raise ValueError(f"no retail hash found in {config_yml}")
    return match.group(1).lower()


def extab_va_range(splits: Path) -> tuple[int, int]:
    lows: list[int] = []
    highs: list[int] = []
    for line in splits.read_text(encoding="utf-8", errors="replace").splitlines():
        if re.match(r"\s*extab(index)?\s+start:", line):
            start = re.search(r"start:0x([0-9A-Fa-f]+)", line)
            end = re.search(r"end:0x([0-9A-Fa-f]+)", line)
            if start and end:
                lows.append(int(start.group(1), 16))
                highs.append(int(end.group(1), 16))
    if not lows:
        raise ValueError(f"no extab/extabindex ranges found in {splits}")
    return min(lows), max(highs)


def file_offset_to_va(dol: bytes, offset: int) -> int | None:
    offs = struct.unpack(">18I", dol[0x00:0x48])
    addrs = struct.unpack(">18I", dol[0x48:0x90])
    lens = struct.unpack(">18I", dol[0x90:0xD8])
    for i in range(18):
        if lens[i] and offs[i] <= offset < offs[i] + lens[i]:
            return addrs[i] + (offset - offs[i])
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("built", type=Path, help="verified cleaned-target DOL")
    parser.add_argument("out", type=Path, help="retail-identical output DOL")
    parser.add_argument(
        "--orig", type=Path, default=REPO / "orig" / VERSION / "sys" / "main.dol"
    )
    parser.add_argument(
        "--config", type=Path, default=REPO / "config" / VERSION / "config.yml"
    )
    parser.add_argument(
        "--splits", type=Path, default=REPO / "config" / VERSION / "splits.txt"
    )
    parser.add_argument("--max-bytes", type=int, default=16)
    args = parser.parse_args()

    if not args.orig.exists():
        return fail(f"original DOL not found at {args.orig}; cannot splice")
    orig = args.orig.read_bytes()
    expected = retail_sha1(args.config)
    if hashlib.sha1(orig).hexdigest() != expected:
        return fail(f"{args.orig} does not match the configured retail hash")
    built = args.built.read_bytes()
    if len(built) != len(orig):
        return fail("built and retail DOL sizes differ; build is not at target")

    lo, hi = extab_va_range(args.splits)
    differing = [i for i in range(len(orig)) if orig[i] != built[i]]
    if len(differing) > args.max_bytes:
        return fail(
            f"{len(differing)} differing bytes (cap {args.max_bytes}); the build"
            " does not match the cleaned target — fix the build first"
        )
    for offset in differing:
        va = file_offset_to_va(orig, offset)
        if va is None or not (lo <= va < hi):
            return fail(
                f"difference at file 0x{offset:06X} (VA {hex(va) if va else '?'})"
                f" is outside the extab range 0x{lo:08X}-0x{hi:08X}; refusing"
                " to splice a non-extab mismatch"
            )

    spliced = bytearray(built)
    for offset in differing:
        spliced[offset] = orig[offset]
    if hashlib.sha1(spliced).hexdigest() != expected:
        return fail("spliced result does not hash to retail; aborting", args.out)
    args.out.write_bytes(spliced)
    print(
        f"retaildol: wrote {args.out} (sha1 {expected}, {len(differing)}"
        f" byte(s) spliced from {args.orig})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
