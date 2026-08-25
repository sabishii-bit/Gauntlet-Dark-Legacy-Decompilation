#!/usr/bin/env python3
"""Sweep every configured 1.2.5-era TU through Frank and rank improvements.

The sweep extracts each source's exact MWCC command from Ninja, compiles both
the configured body and the 1.2.5e profile object, and scores the merged object
function-by-function against the extracted target.  For 1.2.5n TUs it also
tests decomp.me's official vanilla-1.2.5 body pairing.

Artifacts and a machine-readable report are written below
``build/GUNE5D/frank-sweep``.  No checked-in source or normal build object is
modified.

Examples:
  python tools/gdl/frank_sweep.py
  python tools/gdl/frank_sweep.py -j 12 --max-score 300
  python tools/gdl/frank_sweep.py --grep game/audio
"""

from __future__ import annotations

import argparse
import concurrent.futures as futures
import json
import re
import shlex
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from frank import merge_objects  # noqa: E402
from matchtool import parse, score  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"
ELIGIBLE_COMPILERS = {"1.2.5", "1.2.5n"}


@dataclass(frozen=True)
class CompileJob:
    source: str
    compiler: str
    flags: tuple[str, ...]


@dataclass
class FunctionResult:
    unit: str
    function: str
    compiler: str
    body_score: str | int
    frank_score: str | int
    official_score: str | int | None
    best_score: str | int
    best_artifact: str
    gain: int


@dataclass
class UnitResult:
    unit: str
    compiler: str
    markers: int = 0
    changed_text_bytes: int = 0
    fallback: bool = False
    official_markers: int = 0
    official_changed_text_bytes: int = 0
    official_fallback: bool = False
    error: str | None = None
    functions: list[FunctionResult] | None = None


def _score_value(value: str | int) -> int:
    if value in {"OK", "OK~"}:
        return 0
    if isinstance(value, int):
        return value
    match = re.search(r"/D(\d+)$", value)
    if match:
        return int(match.group(1))
    return 1_000_000


def _discover_jobs(grep: str | None) -> list[CompileJob]:
    result = subprocess.run(
        ["ninja", "-t", "commands"],
        cwd=REPO,
        capture_output=True,
        text=True,
        check=True,
    )
    jobs: dict[str, CompileJob] = {}
    for line in result.stdout.splitlines():
        if "mwcceppc.exe" not in line:
            continue
        tokens = shlex.split(line.replace("\\", "/"))
        try:
            compiler_at = next(
                index for index, token in enumerate(tokens)
                if token.endswith("mwcceppc.exe")
            )
            compile_at = tokens.index("-c", compiler_at)
            source = tokens[compile_at + 1]
        except (StopIteration, ValueError, IndexError):
            continue
        if not source.startswith("src/") or (grep and grep not in source):
            continue
        version_match = re.search(
            r"compilers/GC/([^/]+)/mwcceppc", tokens[compiler_at]
        )
        if not version_match or version_match.group(1) not in ELIGIBLE_COMPILERS:
            continue

        flags: list[str] = []
        args = tokens[compiler_at + 1:]
        skip = 0
        for token in args:
            if skip:
                skip -= 1
                continue
            if token == "-MMD":
                continue
            if token in {"-c", "-o"}:
                skip = 1
                continue
            flags.append(token)
        jobs.setdefault(
            source,
            CompileJob(source, version_match.group(1), tuple(flags)),
        )
    return sorted(jobs.values(), key=lambda job: job.source)


def _compiler(version: str) -> Path:
    return REPO / "build" / "compilers" / "GC" / version / "mwcceppc.exe"


def _compile(job: CompileJob, version: str, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(_compiler(version)),
        *job.flags,
        "-c",
        str(REPO / job.source),
        "-o",
        str(output),
    ]
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)
    if result.returncode or not output.is_file():
        message = (result.stdout + result.stderr).strip().replace("\n", " | ")
        raise RuntimeError(f"{version}: {message[:500]}")


def _artifact_dir(root: Path, source: str) -> Path:
    return root / Path(source).relative_to("src").with_suffix("")


def _run_job(job: CompileJob, root: Path) -> UnitResult:
    unit = str(Path(job.source).relative_to("src").with_suffix("")).replace("\\", "/")
    result = UnitResult(unit, job.compiler)
    try:
        output_dir = _artifact_dir(root, job.source)
        body_path = output_dir / "body.o"
        profile_path = output_dir / "profile.o"
        frank_path = output_dir / "frank.o"
        official_path = output_dir / "official-frank.o"

        _compile(job, job.compiler, body_path)
        _compile(job, "1.2.5e", profile_path)
        merged, stats = merge_objects(body_path.read_bytes(), profile_path.read_bytes())
        frank_path.write_bytes(merged)
        result.markers = stats.profile_markers
        result.changed_text_bytes = stats.text_bytes_changed
        result.fallback = stats.used_vanilla_fallback

        official_functions = None
        if job.compiler == "1.2.5n":
            vanilla_path = output_dir / "vanilla.o"
            _compile(job, "1.2.5", vanilla_path)
            official, official_stats = merge_objects(
                vanilla_path.read_bytes(), profile_path.read_bytes()
            )
            result.official_markers = official_stats.profile_markers
            result.official_changed_text_bytes = official_stats.text_bytes_changed
            result.official_fallback = official_stats.used_vanilla_fallback
            official_path.write_bytes(official)
            official_functions = parse(official_path)

        source_relative = Path(job.source).relative_to("src").with_suffix(".o")
        target_path = REPO / "build" / VERSION / "obj" / source_relative
        target_functions = parse(target_path)
        body_functions = parse(body_path)
        frank_functions = parse(frank_path)
        body_scores = score(target_functions, body_functions)
        frank_scores = score(target_functions, frank_functions)
        official_scores = (
            score(target_functions, official_functions)
            if official_functions is not None else {}
        )

        rows = []
        for function, body_score in body_scores.items():
            frank_score = frank_scores.get(function, "MISS")
            official_score = official_scores.get(function)
            choices = [("body", body_score), ("frank", frank_score)]
            if official_score is not None:
                choices.append(("official-frank", official_score))
            best_artifact, best_score = min(
                choices, key=lambda item: _score_value(item[1])
            )
            rows.append(
                FunctionResult(
                    unit,
                    function,
                    job.compiler,
                    body_score,
                    frank_score,
                    official_score,
                    best_score,
                    best_artifact,
                    _score_value(body_score) - _score_value(best_score),
                )
            )
        result.functions = rows
    except Exception as error:  # Keep a 236-TU sweep useful if one TU fails.
        result.error = str(error)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-j", "--jobs", type=int, default=8)
    parser.add_argument("--grep", help="only sweep source paths containing this text")
    parser.add_argument(
        "--max-score", type=int, default=500,
        help="only print improvements whose body residual is at most this value",
    )
    parser.add_argument("--top", type=int, default=100)
    args = parser.parse_args()

    root = REPO / "build" / VERSION / "frank-sweep"
    root.mkdir(parents=True, exist_ok=True)
    jobs = _discover_jobs(args.grep)
    print(f"FRANK SWEEP: {len(jobs)} eligible TUs, {args.jobs} workers")

    results: list[UnitResult] = []
    with futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        pending = [executor.submit(_run_job, job, root) for job in jobs]
        for index, future in enumerate(futures.as_completed(pending), 1):
            results.append(future.result())
            if index % 25 == 0 or index == len(pending):
                print(f"  completed {index}/{len(pending)}")

    results.sort(key=lambda item: item.unit)
    report_path = root / "report.json"
    report_path.write_text(
        json.dumps([asdict(result) for result in results], indent=2) + "\n",
        encoding="utf-8",
    )

    functions = [
        row
        for result in results if result.functions
        for row in result.functions
    ]
    improvements = [row for row in functions if row.gain > 0]
    improvements.sort(
        key=lambda row: (-row.gain, _score_value(row.best_score), row.unit, row.function)
    )
    displayed = [
        row for row in improvements
        if _score_value(row.body_score) <= args.max_score
    ]
    regressions = [
        row for row in functions
        if _score_value(row.frank_score) > _score_value(row.body_score)
    ]
    exact_gains = [row for row in improvements if _score_value(row.best_score) == 0]
    configured_improvements = sum(
        _score_value(row.frank_score) < _score_value(row.body_score)
        for row in functions
    )
    official_improvements = sum(
        row.official_score is not None
        and _score_value(row.official_score) < _score_value(row.body_score)
        for row in functions
    )
    failures = [result for result in results if result.error]
    changed_units = sum(result.changed_text_bytes > 0 for result in results)

    print(
        f"RESULT: {len(functions)} functions; {len(improvements)} best improvements "
        f"(configured {configured_improvements}, official {official_improvements}); "
        f"{len(exact_gains)} newly exact; {len(regressions)} configured-Frank "
        f"regressions; {changed_units} TUs changed; {len(failures)} failures"
    )
    print("gain  body -> best  artifact         function")
    for row in displayed[:args.top]:
        print(
            f"{row.gain:4}  {str(row.body_score):>8} -> {str(row.best_score):<8} "
            f"{row.best_artifact:16} {row.unit}::{row.function}"
        )
    if failures:
        print("\nFAILURES:")
        for failure in failures:
            print(f"  {failure.unit}: {failure.error}")
    print(f"report: {report_path.relative_to(REPO)}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
