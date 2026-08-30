#!/usr/bin/env python3
"""Normalize git worktree plumbing paths to the interoperable form.

Multiple agent fleets drive this repository with different git flavors
(Windows-native git parses backslash paths; MSYS2-style git treats a
backslash as a literal filename character). Worktree add/remove churn
rewrites the registry files in whichever form the acting git prefers,
after which the OTHER flavor fails validation with errors like:

  fatal: validation failed, cannot remove working tree:
  '<path>' does not point back to '.git/worktrees/<name>'

The one form BOTH flavors parse is an absolute Windows path with forward
slashes only (W:/Repositories/...). This script rewrites, for every
registered worktree:

  <maindir>/.git/worktrees/<name>/gitdir   -> "W:/.../<worktree>/.git"
  <worktree>/.git (the link file)          -> "gitdir: W:/.../.git/worktrees/<name>"

It touches ONLY these two plumbing files per worktree - never tracked
content, never another fleet's commits - so it is safe for any fleet to
run from the main checkout whenever git starts refusing worktree
commands:

  python tools/gdl/fix_worktrees.py            # report + fix
  python tools/gdl/fix_worktrees.py --check    # report only, exit 1 if dirty
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


def _forward(path: str) -> str:
    path = path.strip().replace("\\", "/")
    # Collapse MSYS drive form /w/... to W:/... so both flavors agree.
    match = re.fullmatch(r"/([A-Za-z])(/.*)?", path)
    if match:
        path = f"{match.group(1).upper()}:{match.group(2) or ''}"
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true",
                        help="report only; exit 1 if anything needs fixing")
    args = parser.parse_args()

    git_common = Path(
        subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    ).resolve()
    worktrees_dir = git_common / "worktrees"
    if not worktrees_dir.exists():
        print("no linked worktrees registered")
        return 0

    fixed = 0
    dirty = 0
    for admin in sorted(worktrees_dir.iterdir()):
        gitdir_file = admin / "gitdir"
        if not gitdir_file.exists():
            continue
        raw = gitdir_file.read_text(encoding="utf-8").strip()
        want = _forward(raw)
        state = "ok"
        if raw != want:
            dirty += 1
            state = "normalize"
            if not args.check:
                gitdir_file.write_text(want + "\n", encoding="utf-8")
                fixed += 1
                state = "fixed"
        print(f"{admin.name}: gitdir {state} ({want})")

        # The worktree side: <worktree>/.git must point back at admin dir.
        wt_git = Path(want[: -len("/.git")] if want.endswith("/.git") else want)
        link = wt_git / ".git"
        if link.is_file():
            raw_link = link.read_text(encoding="utf-8").strip()
            want_link = "gitdir: " + _forward(
                raw_link[len("gitdir:"):] if raw_link.startswith("gitdir:")
                else str(admin)
            )
            expected = "gitdir: " + _forward(str(admin))
            if want_link != expected:
                want_link = expected
            if raw_link != want_link:
                dirty += 1
                if not args.check:
                    try:
                        # Windows refuses truncating writes to HIDDEN files;
                        # git marks worktree .git links hidden. Drop the
                        # attribute for the write, then restore it.
                        hidden = False
                        if sys.platform == "win32":
                            import ctypes
                            attrs = ctypes.windll.kernel32.GetFileAttributesW(
                                str(link))
                            hidden = attrs != -1 and bool(attrs & 0x2)
                            if hidden:
                                ctypes.windll.kernel32.SetFileAttributesW(
                                    str(link), attrs & ~0x2)
                        link.write_text(want_link + "\n", encoding="utf-8")
                        if hidden:
                            ctypes.windll.kernel32.SetFileAttributesW(
                                str(link), attrs)
                        fixed += 1
                        print(f"{admin.name}: .git link fixed ({want_link})")
                    except OSError as error:
                        # A live worker's git may hold the file; safe to
                        # retry after its current operation finishes.
                        print(f"{admin.name}: .git link LOCKED, retry later"
                              f" ({error})")
                else:
                    print(f"{admin.name}: .git link needs fixing")

    if args.check and dirty:
        print(f"{dirty} plumbing file(s) need normalizing")
        return 1
    print(f"normalized {fixed} plumbing file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
