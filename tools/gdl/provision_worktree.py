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

  python tools/gdl/provision_worktree.py --resplit
      Re-extract build/<VERSION>/obj/** from the retail DOL. See below.

It copies the ignored build inputs from the main checkout (orig/ DOL +
toolchain caches when present), verifies the retail sha1, runs
configure.py, and runs the bootstrap ninja — failing loudly at the first
broken step. This replaces the hand-rolled Copy-Item sequence that has
now nested orig/ into orig/GUNE5D/GUNE5D twice across fleets (Copy-Item
-Recurse into an existing directory nests instead of merging).

`build/<VERSION>/obj/**` IS A DTK-SPLIT REFERENCE, NOT A BUILD ARTIFACT
(run-49 item 8). Those 331 objects are extracted FROM the retail DOL by
`dtk dol split` and are the TARGET side every comparison reads — fndiff,
fnasm, datadiff --sections, regnorm, savedregs, webfrank. They live under
build/ and are gitignored, so they look disposable, and they are not.

MEASURED at cf375c09d: `build.ninja` references them on 223 lines and
declares them as an OUTPUT on ZERO — the split rule's only declared output
is `build/<VERSION>/config.json`, and obj/** is a side effect of it. So
ninja cannot know one is missing. Deleting
`build/GUNE5D/obj/dolphin/si/SIBios.o` and running a plain `ninja` leaves
it missing AND stops the build ("subcommand failed"), while
`datadiff.py --sections dolphin/si/SIBios` degrades to
`SKIP --sections: missing [...]` — a comparison silently not made. That is
the shape of the incident CU hit.

RECOVERY, verified at the same commit: delete `config.json` so the split
rule is out of date, then rebuild it — `--resplit` does exactly that and
then re-runs the full ninja. The 331 files came back and the DOL gate
printed `build/GUNE5D/main.dol: OK`.
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


def resplit(here: Path):
    """Force `dtk dol split` to re-extract build/<VERSION>/obj/**.

    The split rule's only declared output is config.json, so removing that
    is the supported way to make ninja re-run it — no hand-rolled dtk
    invocation, which would have to re-derive the rule's arguments.
    """
    config = here / "build" / VERSION / "config.json"
    objdir = here / "build" / VERSION / "obj"
    before = sum(1 for p in objdir.rglob("*") if p.is_file()) \
        if objdir.is_dir() else 0
    if config.is_file():
        config.unlink()
        print(f"removed {config.relative_to(here)} so the split rule reruns")
    step = ["ninja", str(Path("build") / VERSION / "config.json")]
    print("::", " ".join(step))
    if subprocess.run(step, cwd=here).returncode != 0:
        fail("dtk split failed — obj/ was NOT re-extracted")
    after = sum(1 for p in objdir.rglob("*") if p.is_file()) \
        if objdir.is_dir() else 0
    print(f"build/{VERSION}/obj: {before} file(s) -> {after}")
    if not after:
        fail(f"build/{VERSION}/obj is still empty after the split")
    return before, after


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if "--resplit" in sys.argv:
        if args:
            fail("usage: provision_worktree.py --resplit (no other arguments)")
        here = Path.cwd()
        if not (here / "configure.py").is_file():
            fail(f"run from the worktree root (cwd: {here})")
        resplit(here)
        step = ["ninja", "-j2"]
        print("::", " ".join(step))
        if subprocess.run(step, cwd=here).returncode != 0:
            fail("ninja failed after the resplit")
        print("RESPLIT OK — obj/ re-extracted, build green")
        return 0
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
            # One automatic retry: concurrent fleets produce transient
            # CreateProcess/link failures that succeed on a clean re-run.
            print(f":: {step[0]} exited {result.returncode} — retrying"
                  " once (fleet-contention pattern)")
            result = subprocess.run(step, cwd=here)
        if result.returncode != 0:
            fail(f"{step[0]} exited {result.returncode} (after retry)")
    print("PROVISION OK — build green, ready to work")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
