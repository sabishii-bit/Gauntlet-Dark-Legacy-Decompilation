#!/usr/bin/env python3
"""One-shot probe scorer for the matching iteration loop.

Collapses the ubiquitous edit-loop pair (`ninja <obj>` then `fndiff --count`)
into a single call with explicit verdicts — no more comparing numbers against
scrollback by eye. Tracks best/last per function in build/GUNE5D/gate/.

Usage:
  python tools/gdl/probe.py game/game/player do_players          # build+score
  python tools/gdl/probe.py game/game/player do_players --ops    # + ops scan
  python tools/gdl/probe.py game/game/player do_players --reset  # forget best

Output: one line per probe —
  IMPROVED  real 1109 -> 1070 (best was 1109)     [best updated]
  REGRESSED real 1070 -> 1093 (best 1070)         [revert advised]
  NEUTRAL   real 1070 (insns 1174/1160)
The verdict compares against the BEST recorded real, so a probe sequence
never loses track of the high-water mark even across reverts.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
TOOLS = Path(__file__).resolve().parent

COUNT_RE = re.compile(
    r"^DIFF\s+(\S+)\s+insns\s+(\d+)/(\d+)\s+lines\s+(\d+)\s+real\s+(\d+)"
)


def normalize_unit(unit):
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    return re.sub(r"\.(c|cpp)$", "", unit)


def state_path(unit, fn):
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", f"{unit}_{fn}")
    path = Path(f"build/{VERSION}/gate/probe_{slug}.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2 or args[0] in ("--help", "-h"):
        print(__doc__)
        return 2
    unit, fn = normalize_unit(args[0]), args[1]
    state_file = state_path(unit, fn)
    if "--reset" in sys.argv and state_file.exists():
        state_file.unlink()
        print("probe state reset")
        return 0

    build = subprocess.run(
        ["ninja", f"build/{VERSION}/src/{unit}.o"],
        capture_output=True, text=True,
    )
    if build.returncode != 0:
        print("BUILD FAILED:")
        print((build.stdout + build.stderr).strip()[-1500:])
        return 1

    count = subprocess.run(
        [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
         "--count", "--no-build"],
        capture_output=True, text=True,
    ).stdout
    real = None
    for line in count.splitlines():
        match = COUNT_RE.match(line.strip())
        if match and match.group(1) == fn:
            _, ti, bi, lines, real_text = match.groups()
            real = int(real_text)
            insns = f"{ti}/{bi}"
            break
    else:
        if re.search(rf"^OK\s+{re.escape(fn)}\s*$", count, re.M) or not count.strip():
            # fndiff --count prints nothing for byte-identical functions
            real, insns = 0, "exact"

    if real is None:
        print(f"could not score {fn}:")
        print(count.strip()[:800])
        return 1

    state = {}
    if state_file.exists():
        state = json.loads(state_file.read_text(encoding="utf-8"))
    best = state.get("best_real")
    if best is None:
        verdict = f"BASELINE  real {real} (insns {insns})"
        state["best_real"] = real
    elif real < best:
        verdict = f"IMPROVED  real {best} -> {real} (insns {insns})  [best updated]"
        state["best_real"] = real
    elif real > best:
        verdict = (f"REGRESSED real {state.get('last_real', best)} -> {real}"
                   f" (best {best}, insns {insns})  [revert advised]")
    else:
        verdict = f"NEUTRAL   real {real} (insns {insns})"
    state["last_real"] = real
    state_file.write_text(json.dumps(state), encoding="utf-8")
    print(verdict)

    # A failed probe almost always needs the ops view next — print it
    # unasked on regression so the diagnosis is zero extra calls.
    if verdict.startswith("REGRESSED") and "--ops" not in sys.argv:
        ops = subprocess.run(
            [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
             "--ops", "--no-build"],
            capture_output=True, text=True,
        ).stdout
        print("\n".join(ops.strip().splitlines()[:16]))

    if "--ops" in sys.argv:
        ops = subprocess.run(
            [sys.executable, str(TOOLS / "fndiff.py"), unit, fn,
             "--ops", "--no-build"],
            capture_output=True, text=True,
        ).stdout
        print(ops.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
