#!/usr/bin/env python3
"""Compare a TU under normal, profile, and Frank-merged MWCC schedules.

The probe always uses the TU's exact flags from ``build.ninja``.  It answers
the question a normal compiler sweep cannot: did Frank alter this function,
and does pairing the profile compiler with the TU's configured body compiler
behave differently from decomp.me's official vanilla-1.2.5 pairing?

Example:
  python tools/gdl/frank_probe.py game/g3d/sndvoice sndVoiceUpdateAll
  python tools/gdl/frank_probe.py game/g3d/sndvoice sndVoiceUpdateAll \
      --body 1.2.5n --vanilla 1.2.5 --keep <scratch-dir>
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from flagsweep import ninja_base_cmd  # noqa: E402
from frank import MergeStats, merge_objects  # noqa: E402
from matchtool import parse, score  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"


@dataclass
class Artifact:
    label: str
    path: Path
    merge_stats: MergeStats | None = None


def _resolve_source(unit: str) -> Path:
    path = Path(unit)
    if path.suffix not in (".c", ".cc", ".cp", ".cpp", ".cxx", ".c++"):
        path = path.with_suffix(".c")
    if path.parts and path.parts[0] == "src":
        return REPO / path
    return REPO / "src" / path


def _target_for(source: Path) -> Path:
    relative = source.relative_to(REPO / "src").with_suffix(".o")
    return REPO / "build" / VERSION / "obj" / relative


def _compiler(version: str) -> Path:
    path = REPO / "build" / "compilers" / "GC" / version / "mwcceppc.exe"
    if not path.is_file():
        raise FileNotFoundError(f"missing compiler: {path}")
    return path


def _compile(source: Path, flags: list[str], version: str, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(_compiler(version)), *flags, "-c", str(source), "-o", str(output)],
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    if result.returncode or not output.is_file():
        message = (result.stdout + result.stderr).strip()
        raise RuntimeError(f"{version} compile failed:\n{message}")


def _merge(label: str, body: Artifact, profile: Artifact, output: Path) -> Artifact:
    merged, stats = merge_objects(body.path.read_bytes(), profile.path.read_bytes())
    output.write_bytes(merged)
    return Artifact(label, output, stats)


def _function_digest(function_lines: list[str]) -> str:
    return hashlib.sha256("\n".join(function_lines).encode()).hexdigest()[:16]


def _show(target: Path, function: str, artifacts: list[Artifact]) -> None:
    target_functions = parse(target)
    if function not in target_functions:
        raise KeyError(f"{function!r} is not in {target}")

    parsed = {artifact.label: parse(artifact.path) for artifact in artifacts}
    body_bytes = artifacts[0].path.read_bytes()
    print(f"target: {target.relative_to(REPO)}::{function}")
    print("artifact         score        insns  fn-digest        object==body  merge")
    for artifact in artifacts:
        lines = parsed[artifact.label].get(function)
        if lines is None:
            function_score = "MISS"
            count = 0
            digest = "-"
        else:
            function_score = score(target_functions, parsed[artifact.label], function)[function]
            count = sum(1 for line in lines if not line.startswith("    R_PPC"))
            digest = _function_digest(lines)
        same = artifact.path.read_bytes() == body_bytes
        if artifact.merge_stats is None:
            merge = "-"
        else:
            stats = artifact.merge_stats
            merge = (f"markers={stats.profile_markers},"
                     f"changed={stats.text_bytes_changed},"
                     f"fallback={int(stats.used_vanilla_fallback)}")
        print(f"{artifact.label:16} {str(function_score):12} {count:5}  "
              f"{digest:16} {str(same):12} {merge}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unit", help="source unit below src/, with or without .c")
    parser.add_argument("function")
    parser.add_argument("--body", help="body compiler; defaults to the configured compiler")
    parser.add_argument("--vanilla", default="1.2.5",
                        help="body compiler used by decomp.me's official 1.2.5e pairing")
    parser.add_argument("--profile", default="1.2.5e",
                        help="directory containing the profile-patched compiler")
    parser.add_argument("--keep", type=Path,
                        help="retain the five intermediate objects in this directory")
    args = parser.parse_args()

    source = _resolve_source(args.unit)
    if not source.is_file():
        parser.error(f"missing source: {source}")
    target = _target_for(source)
    if not target.is_file():
        parser.error(f"missing target object: {target}")

    flags, configured = ninja_base_cmd(source)
    body_version = args.body or configured
    temporary = None
    if args.keep:
        output_dir = args.keep.resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
    else:
        temporary = tempfile.TemporaryDirectory(prefix="gdl-frank-")
        output_dir = Path(temporary.name)

    try:
        body = Artifact(f"body-{body_version}", output_dir / "body.o")
        vanilla = Artifact(f"vanilla-{args.vanilla}", output_dir / "vanilla.o")
        profile = Artifact(f"profile-{args.profile}", output_dir / "profile.o")
        _compile(source, flags, body_version, body.path)
        if args.vanilla == body_version:
            shutil.copyfile(body.path, vanilla.path)
        else:
            _compile(source, flags, args.vanilla, vanilla.path)
        _compile(source, flags, args.profile, profile.path)

        official = _merge("official-frank", vanilla, profile,
                          output_dir / "official-frank.o")
        configured_frank = _merge("body-frank", body, profile,
                                  output_dir / "body-frank.o")
        _show(target, args.function,
              [body, vanilla, profile, official, configured_frank])
    finally:
        if temporary is not None:
            temporary.cleanup()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
