#!/usr/bin/env python3
"""Compare two MWCC variants using each TU's exact configured flags.

This is primarily useful for compiler-build archaeology: compile every source
with the same flags Ninja uses, swap only the compiler directory, then score
every function against the dtk-extracted target object.

Example:
  python tools/gdl/compiler_compare.py --root src/game \
      --left 1.2.5 --right 1.2.5e -j 8
"""

import argparse
import concurrent.futures as futures
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from flagsweep import compile_variant, ninja_base_cmd  # noqa: E402
from matchtool import parse, score, total_key  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"


def target_object(src: Path) -> Path:
    rel = src.relative_to(REPO / "src").with_suffix(".o")
    return REPO / "build" / VERSION / "obj" / rel


def safe_tag(src: Path, compiler: str) -> str:
    rel = src.relative_to(REPO)
    return re.sub(r"\W+", "_", f"compiler_ab_{compiler}_{rel}")


def compare_one(src: Path, left: str, right: str):
    target = target_object(src)
    if not target.exists():
        return src, None, f"missing target {target}"

    try:
        base = ninja_base_cmd(src)
    except SystemExit as exc:
        return src, None, str(exc)

    _, configured = base
    if configured != left:
        return src, None, f"configured compiler is {configured}"

    left_obj, left_err = compile_variant(
        src, f"CC={left}", safe_tag(src, left), base
    )
    if left_err:
        return src, None, f"{left}: {left_err}"
    right_obj, right_err = compile_variant(
        src, f"CC={right}", safe_tag(src, right), base
    )
    if right_err:
        return src, None, f"{right}: {right_err}"

    target_fns = parse(target)
    left_scores = score(target_fns, parse(left_obj))
    right_scores = score(target_fns, parse(right_obj))
    changed = {
        name: (left_scores.get(name), right_scores.get(name))
        for name in target_fns
        if left_scores.get(name) != right_scores.get(name)
    }
    return src, (total_key(left_scores), total_key(right_scores), changed), None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="src/game")
    parser.add_argument("--left", default="1.2.5")
    parser.add_argument("--right", default="1.2.5e")
    parser.add_argument("-j", type=int, default=8)
    parser.add_argument("--all", action="store_true",
                        help="also print invariant and regressive units")
    args = parser.parse_args()

    source_root = REPO / args.root
    sources = sorted(
        p for p in source_root.rglob("*") if p.suffix in (".c", ".cpp")
    )
    rows = []
    errors = []
    with futures.ThreadPoolExecutor(max_workers=args.j) as executor:
        jobs = [executor.submit(compare_one, src, args.left, args.right)
                for src in sources]
        for job in futures.as_completed(jobs):
            src, result, error = job.result()
            if error:
                errors.append((src, error))
            else:
                rows.append((src, *result))

    improved = sorted((row for row in rows if row[2] < row[1]),
                      key=lambda row: row[2] - row[1])
    invariant = [row for row in rows if row[2] == row[1]]
    regressed = sorted((row for row in rows if row[2] > row[1]),
                       key=lambda row: row[2] - row[1], reverse=True)
    function_improvements = []
    function_regressions = []
    exact_losses = []
    for src, _, _, changed in rows:
        for name, (before, after) in changed.items():
            before_cost = total_key({name: before})
            after_cost = total_key({name: after})
            if after_cost < before_cost:
                function_improvements.append(
                    (after_cost - before_cost, src, name, before, after)
                )
            elif after_cost > before_cost:
                function_regressions.append(
                    (after_cost - before_cost, src, name, before, after)
                )
                if before in ("OK", "OK~") and after not in ("OK", "OK~"):
                    exact_losses.append((src, name, before, after))
    function_improvements.sort(key=lambda row: row[0])

    def show(row):
        src, left_cost, right_cost, changed = row
        rel = src.relative_to(REPO)
        print(f"{rel}: {left_cost} -> {right_cost} ({right_cost-left_cost:+d})")
        for name, (before, after) in sorted(changed.items()):
            print(f"  {name}: {before} -> {after}")

    print(f"IMPROVED ({len(improved)})")
    for row in improved:
        show(row)
    if args.all:
        print(f"INVARIANT ({len(invariant)})")
        for row in invariant:
            show(row)
        print(f"REGRESSED ({len(regressed)})")
        for row in regressed:
            show(row)
    else:
        print(f"INVARIANT {len(invariant)}; REGRESSED {len(regressed)}")

    print(f"FUNCTION IMPROVEMENTS ({len(function_improvements)})")
    for delta, src, name, before, after in function_improvements:
        print(f"  {src.relative_to(REPO)}::{name}: {before} -> {after} ({delta:+d})")
    print(f"FUNCTION REGRESSIONS {len(function_regressions)}; "
          f"EXACT/RELOC-EXACT LOSSES {len(exact_losses)}")

    relevant_errors = [(src, error) for src, error in errors
                       if not error.startswith("configured compiler is ")]
    if relevant_errors:
        print(f"ERRORS ({len(relevant_errors)})")
        for src, error in relevant_errors:
            print(f"  {src.relative_to(REPO)}: {error}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
