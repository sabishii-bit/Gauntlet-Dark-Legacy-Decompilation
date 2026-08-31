#!/usr/bin/env python3
"""One-command worktree bootstrap for fleet workers.

Run from the ROOT of a fresh git worktree:

  python tools/gdl/provision_worktree.py

It copies the ignored build inputs from the main checkout (orig/ DOL +
toolchain caches when present), verifies the retail sha1, runs
configure.py, and runs the bootstrap ninja — failing loudly at the first
broken step. This replaces the hand-rolled Copy-Item sequence that has
now nested orig/ into orig/GUNE5D/GUNE5D twice across fleets (Copy-Item
-Recurse into an existing directory nests instead of merging).
"""

import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
RETAIL_SHA1 = "7cba77aa496eb0fc5ffec60efd9680aa9635d679"
MAIN_REPO = Path(r"W:\Repositories\Gauntlet-Dark-Legacy-Decompilation")


def fail(msg):
    print(f"PROVISION FAILED: {msg}")
    raise SystemExit(1)


def copy_tree_merge(src: Path, dst: Path):
    """File-level copy that merges into an existing destination (never
    nests the way Copy-Item -Recurse does)."""
    for path in src.rglob("*"):
        if path.is_dir():
            continue
        rel = path.relative_to(src)
        target = dst / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(path, target)


def main():
    here = Path.cwd()
    if not (here / "configure.py").is_file():
        fail(f"run from the worktree root (cwd: {here})")
    if here.resolve() == MAIN_REPO.resolve():
        fail("this is the main checkout, not a worktree")
    src_orig = MAIN_REPO / "orig" / VERSION
    if not src_orig.is_dir():
        fail(f"main checkout has no {src_orig}")
    copy_tree_merge(src_orig, here / "orig" / VERSION)
    dol = here / "orig" / VERSION / "sys" / "main.dol"
    if not dol.is_file():
        # some layouts keep the dol at orig/GUNE5D/main.dol
        dol = here / "orig" / VERSION / "main.dol"
    if not dol.is_file():
        fail("main.dol not found after copy — check orig/ layout")
    digest = hashlib.sha1(dol.read_bytes()).hexdigest()
    if digest != RETAIL_SHA1:
        fail(f"retail sha1 mismatch: {digest}")
    print(f"orig/ provisioned, retail sha1 OK ({dol.relative_to(here)})")
    for step in (["python", "configure.py"], ["ninja", "-j2"]):
        print("::", " ".join(step))
        result = subprocess.run(step, cwd=here)
        if result.returncode != 0:
            fail(f"{step[0]} exited {result.returncode}")
    print("PROVISION OK — build green, ready to work")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
