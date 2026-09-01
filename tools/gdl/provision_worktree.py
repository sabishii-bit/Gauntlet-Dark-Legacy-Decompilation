#!/usr/bin/env python3
"""One-command worktree bootstrap for fleet workers.

Two invocations (run-25 briefs documented the two-argument form before it
existed — three workers lost a cycle to the mismatch; now both work):

  python tools/gdl/provision_worktree.py <path> <branch>
      From ANY checkout: creates the worktree (`git worktree add <path>
      -b <branch>`), then provisions it. Rerunning with an existing
      worktree/branch just re-provisions.

  python tools/gdl/provision_worktree.py
      From the ROOT of an already-created worktree: provisions in place.

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
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if len(args) == 2:
        path, branch = Path(args[0]), args[1]
        if not path.is_dir():
            add = subprocess.run(
                ["git", "worktree", "add", str(path), "-b", branch],
                cwd=Path.cwd(), capture_output=True, text=True)
            if add.returncode != 0:
                # branch may already exist from a prior attempt
                add = subprocess.run(
                    ["git", "worktree", "add", str(path), branch],
                    cwd=Path.cwd(), capture_output=True, text=True)
            if add.returncode != 0:
                fail(f"git worktree add failed:\n{add.stderr.strip()}")
            print(f"worktree created: {path} on {branch}")
        import os
        os.chdir(path)
    elif args:
        fail("usage: provision_worktree.py [<path> <branch>]")
    here = Path.cwd()
    if not (here / "configure.py").is_file():
        fail(f"run from the worktree root (cwd: {here})")
    if here.resolve() == MAIN_REPO.resolve():
        fail("this is the main checkout, not a worktree")
    # Worktree .git files are sometimes written with an MSYS-form gitdir
    # (/w/...) that native git.exe cannot resolve — BOTH shells then fail,
    # which trap #1's "switch to PowerShell" advice misdiagnoses. Repair
    # before anything else touches git.
    fixer = here / "tools" / "gdl" / "fix_worktrees.py"
    if fixer.is_file():
        subprocess.run([sys.executable, str(fixer)], cwd=here)
    gitfile = here / ".git"
    if gitfile.is_file():
        content = gitfile.read_text(encoding="utf-8", errors="replace")
        m = None
        for drive in "abcdefghijklmnopqrstuvwxyz":
            token = f"gitdir: /{drive}/"
            if content.startswith(token):
                m = drive
                break
        if m:
            fixed = content.replace(f"gitdir: /{m}/", f"gitdir: {m.upper()}:/", 1)
            gitfile.write_text(fixed, encoding="utf-8")
            print("repaired MSYS-form gitdir in .git file")
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
