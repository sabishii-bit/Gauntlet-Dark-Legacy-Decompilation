#!/usr/bin/env python3
"""Green-gated end-of-TU pipeline. One command that cannot commit a red build.

Steps:
  1. claimcheck the unit (object sections vs splits claims)
  2. flip Object(NonMatching, ...) -> Matching in configure.py (idempotent)
  3. python configure.py
  4. ninja -j2  -- exit code checked directly, never through a pipe; the
     stale build/GUNE5D/ok file is deleted first so it can't mask a failure
  5. only if green and -m given: git add <configure.py + the flipped TU
     sources only> && git commit -- never `add -A`, which swept sibling
     workers' uncommitted files and inbox records (fleet report, 2026-08-31)

Usage (from repo root):
  python tools/gdl/finish_tu.py zlib/inflate.c -m "Match zlib/inflate.c"
  python tools/gdl/finish_tu.py MSL/a.c MSL/b.c -m "..."  # batch: flip all, ONE build+commit
  python tools/gdl/finish_tu.py zlib/inflate.c            # verify only, no commit
  python tools/gdl/finish_tu.py --verify                  # no flip; just build+check

On failure it points at doldiff/claimcheck instead of leaving you to
archaeology (see the two red commits of 2026-07-24 this replaces).
"""

import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OK_FILE = REPO / "build" / VERSION / "ok"
PY = sys.executable or "python"


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=str(REPO), **kw)


def flip(unit_c: str) -> bool:
    import re as _re
    cfg = REPO / "configure.py"
    text = cfg.read_text(encoding="utf-8")
    # single-line form: Object(NonMatching, "unit", kwargs...)
    non = f'Object(NonMatching, "{unit_c}"'
    mat = f'Object(Matching, "{unit_c}"'
    if non in text:
        cfg.write_text(text.replace(non, mat), encoding="utf-8", newline="\n")
        print(f"flipped {unit_c} -> Matching")
        return True
    if mat in text:
        print(f"{unit_c} already Matching")
        return True
    # multi-line form:
    #   Object(
    #       NonMatching,
    #       "unit",
    #       extra_cflags=[...],
    #   )
    pat = _re.compile(r'(Object\(\s*)NonMatching(,\s*"' + _re.escape(unit_c) + '")')
    if pat.search(text):
        cfg.write_text(pat.sub(r"\1Matching\2", text, count=1),
                       encoding="utf-8", newline="\n")
        print(f"flipped {unit_c} -> Matching (multi-line form)")
        return True
    if _re.search(r'Object\(\s*Matching,\s*"' + _re.escape(unit_c) + '"', text):
        print(f"{unit_c} already Matching")
        return True
    print(f"ERROR: {unit_c} not found in configure.py")
    return False


def main():
    argv = sys.argv[1:]
    msg = None
    if "-m" in argv:
        i = argv.index("-m")
        msg = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    args = [a for a in argv if not a.startswith("-")]
    verify_only = "--verify" in sys.argv
    units = []

    if not verify_only:
        if not args:
            print(__doc__)
            return 1
        for a in args:
            u = a.replace("\\", "/")
            if not u.endswith((".c", ".cpp")):
                u += ".c"
            units.append(u)

        for u in units:
            r = run([PY, "tools/gdl/claimcheck.py", u])
            if r.returncode:
                print(f"BLOCKED by {u}: fix section claims before flipping (see above)")
                return 1
            r = run([PY, "tools/gdl/datadiff.py", u.rsplit(".", 1)[0]])
            if r.returncode:
                print(f"BLOCKED by {u}: data bytes differ from DOL (see above); "
                      "wrong constants or emission order never link green")
                return 1

        cfg_snapshot = (REPO / "configure.py").read_text(encoding="utf-8")
        for u in units:
            if not flip(u):
                return 1

    def unflip():
        # a red build must not leave Matching flips behind: a later plain
        # `git add -A` would commit a red-linking configure.py (see the
        # g3dMath3D incident, 2026-07-25)
        if not verify_only:
            (REPO / "configure.py").write_text(cfg_snapshot, encoding="utf-8",
                                               newline="\n")
            print("(rolled configure.py flips back)")

    r = run([PY, "configure.py"], capture_output=True, text=True)
    if r.returncode:
        print("configure.py FAILED:")
        print((r.stderr or r.stdout)[-2000:])
        unflip()
        return 1

    OK_FILE.unlink(missing_ok=True)  # never trust a stale ok
    r = run(["ninja", "-j2"], capture_output=True, text=True)
    tail = "\n".join((r.stdout or "").splitlines()[-6:])
    print(tail)
    if r.returncode != 0:
        print("\nRED BUILD (ninja exit %d) -- nothing committed." % r.returncode)
        print("diagnose with: python tools/gdl/doldiff.py"
              "  |  python tools/gdl/claimcheck.py --matching")
        unflip()
        return 1
    if not OK_FILE.exists():
        print("\nRED: ninja exited 0 but build/%s/ok was not produced?!" % VERSION)
        unflip()
        return 1

    print("\nGREEN: sha1 verified.")
    if msg:
        # Explicit pathspec only: `add -A` swept sibling workers' dirty
        # files and uncommitted inbox records in shared checkouts.
        paths = ["configure.py"]
        for u in units:
            src = REPO / "src" / u
            if src.exists():
                paths.append(str(src.relative_to(REPO)))
            else:
                print(f"warning: src/{u} not found; committing"
                      " configure.py flip only for it")
        run(["git", "add", "--"] + paths)
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
