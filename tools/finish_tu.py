#!/usr/bin/env python3
"""Green-gated end-of-TU pipeline. One command that cannot commit a red build.

Steps:
  1. claimcheck the unit (object sections vs splits claims)
  2. flip Object(NonMatching, ...) -> Matching in configure.py (idempotent)
  3. python configure.py
  4. ninja  -- exit code checked directly, never through a pipe; the stale
     build/GUNE5D/ok file is deleted first so it can't mask a failure
  5. only if green and -m given: git add -A && git commit

Usage (from repo root):
  python tools/finish_tu.py zlib/inflate.c -m "Match zlib/inflate.c"
  python tools/finish_tu.py zlib/inflate.c            # verify only, no commit
  python tools/finish_tu.py --verify                  # no flip; just build+check

On failure it points at doldiff/claimcheck instead of leaving you to
archaeology (see the two red commits of 2026-07-24 this replaces).
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent
OK_FILE = REPO / "build" / VERSION / "ok"
PY = sys.executable or "python"


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=str(REPO), **kw)


def flip(unit_c: str) -> bool:
    cfg = REPO / "configure.py"
    text = cfg.read_text(encoding="utf-8")
    # Object("...unit...") may carry extra kwargs (cflags=, mw_version=)
    non = f'Object(NonMatching, "{unit_c}"'
    mat = f'Object(Matching, "{unit_c}"'
    if non in text:
        cfg.write_text(text.replace(non, mat), encoding="utf-8", newline="\n")
        print(f"flipped {unit_c} -> Matching")
        return True
    if mat in text:
        print(f"{unit_c} already Matching")
        return True
    print(f"ERROR: {unit_c} not found in configure.py")
    return False


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    msg = None
    if "-m" in sys.argv:
        msg = sys.argv[sys.argv.index("-m") + 1]
    verify_only = "--verify" in sys.argv

    unit_c = None
    if not verify_only:
        if not args:
            print(__doc__)
            return 1
        unit_c = args[0].replace("\\", "/")
        if not unit_c.endswith((".c", ".cpp")):
            unit_c += ".c"

        r = run([PY, "tools/claimcheck.py", unit_c])
        if r.returncode:
            print("BLOCKED: fix section claims before flipping (see above)")
            return 1

        if not flip(unit_c):
            return 1

    r = run([PY, "configure.py"], capture_output=True, text=True)
    if r.returncode:
        print("configure.py FAILED:")
        print((r.stderr or r.stdout)[-2000:])
        return 1

    OK_FILE.unlink(missing_ok=True)  # never trust a stale ok
    r = run(["ninja"], capture_output=True, text=True)
    tail = "\n".join((r.stdout or "").splitlines()[-6:])
    print(tail)
    if r.returncode != 0:
        print("\nRED BUILD (ninja exit %d) -- nothing committed." % r.returncode)
        print("diagnose with: python tools/doldiff.py"
              "  |  python tools/claimcheck.py --matching")
        return 1
    if not OK_FILE.exists():
        print("\nRED: ninja exited 0 but build/%s/ok was not produced?!" % VERSION)
        return 1

    print("\nGREEN: sha1 verified.")
    if msg:
        run(["git", "add", "-A"])
        r = run(["git", "commit", "-q", "-m", msg])
        if r.returncode:
            print("git commit failed")
            return 1
        head = subprocess.run(["git", "log", "--oneline", "-1"], cwd=str(REPO),
                              capture_output=True, text=True).stdout.strip()
        print(f"committed: {head}")
    else:
        print("(no -m given: verified only, not committed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
