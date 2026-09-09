#!/usr/bin/env python3
"""Derive and install GDL's two reviewed local MWCC profiles.

The base compiler archive and payload remain separate inputs.  This installer
never downloads or embeds Metrowerks bytes; patch_pe.py authenticates both base
executables, the open payload, every patched callsite and each final output.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Callable, Sequence

REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.gdl.mwcc_p6 import patch_pe


HERE = Path(__file__).resolve().parent
DEFAULT_PAYLOAD = HERE / "build" / "payload.bin"
SPECIAL_COMPILERS = (
    (
        "1.2.5",
        "1.2.5s",
        "mwcceppc-125s.exe",
        patch_pe.EXPECTED_OUTPUTS[
            "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914"
        ],
    ),
    (
        "1.2.5n",
        "1.2.5sn",
        "mwcceppc-125sn.exe",
        patch_pe.EXPECTED_OUTPUTS[
            "ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c"
        ],
    ),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            delete=False,
            dir=destination.parent,
            prefix=destination.name + ".tmp-",
            suffix=".tmp",
        ) as handle:
            temporary = Path(handle.name)
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def install_special_compilers(
    compiler_root: Path,
    payload: Path = DEFAULT_PAYLOAD,
    *,
    run: Callable[..., subprocess.CompletedProcess] = subprocess.run,
) -> list[Path]:
    compiler_root = compiler_root.resolve()
    payload = payload.resolve()
    if not compiler_root.is_dir():
        raise ValueError(f"compiler root does not exist: {compiler_root}")
    if not payload.is_file():
        raise ValueError(
            f"reviewed payload does not exist: {payload}\n"
            "Build it with tools/gdl/mwcc_p6/build_payload.ps1 first."
        )
    actual_payload_hash = sha256(payload)
    if actual_payload_hash != patch_pe.PAYLOAD_SHA256:
        raise ValueError(
            f"reviewed payload hash mismatch: {actual_payload_hash}\n"
            f"Expected: {patch_pe.PAYLOAD_SHA256}"
        )

    inputs: list[tuple[Path, Path]] = []
    for base_version, _, _, _ in SPECIAL_COMPILERS:
        base_dir = compiler_root / "GC" / base_version
        compiler = base_dir / "mwcceppc.exe"
        license_dll = base_dir / "lmgr326b.dll"
        if not compiler.is_file():
            raise ValueError(f"base compiler does not exist: {compiler}")
        if not license_dll.is_file():
            raise ValueError(f"base compiler license DLL does not exist: {license_dll}")
        inputs.append((compiler, license_dll))

    installed: list[Path] = []
    with tempfile.TemporaryDirectory(
        prefix="gdl-special-compilers-", dir=compiler_root
    ) as directory:
        scratch = Path(directory)
        derived: list[tuple[Path, Path, str]] = []
        for spec, (compiler, license_dll) in zip(SPECIAL_COMPILERS, inputs):
            _, output_version, output_name, expected_hash = spec
            output = scratch / output_name
            run(
                [
                    sys.executable,
                    str(HERE / "patch_pe.py"),
                    str(compiler),
                    str(payload),
                    str(output),
                ],
                check=True,
            )
            if not output.is_file():
                raise ValueError(f"patcher did not create expected output: {output}")
            actual_hash = sha256(output)
            if actual_hash != expected_hash:
                raise ValueError(
                    f"derived {output_version} hash mismatch: {actual_hash}\n"
                    f"Expected: {expected_hash}"
                )
            derived.append((output, license_dll, output_version))

        for output, license_dll, output_version in derived:
            destination_dir = compiler_root / "GC" / output_version
            destination = destination_dir / "mwcceppc.exe"
            atomic_copy(output, destination)
            atomic_copy(license_dll, destination_dir / "lmgr326b.dll")
            installed.append(destination)
    return installed


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler_root", type=Path)
    parser.add_argument(
        "--payload",
        type=Path,
        default=DEFAULT_PAYLOAD,
        help=f"reviewed payload.bin (default: {DEFAULT_PAYLOAD})",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        installed = install_special_compilers(args.compiler_root, args.payload)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"SPECIAL COMPILER SETUP FAILED: {error}", file=sys.stderr)
        return 1
    for path in installed:
        print(f"installed {path} sha256={sha256(path)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
