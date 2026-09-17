#!/usr/bin/env python3

###
# Downloads various tools from GitHub releases.
#
# Usage:
#   python3 tools/download_tool.py wibo build/tools/wibo --tag 1.0.0
#
# If changes are made, please submit a PR to
# https://github.com/encounter/dtk-template
###

import argparse
import io
import os
import platform
import shutil
import stat
import sys
import urllib.request
import zipfile
from typing import Callable, Dict
from pathlib import Path


def binutils_url(tag):
    uname = platform.uname()
    system = uname.system.lower()
    arch = uname.machine.lower()
    if system == "darwin":
        system = "macos"
        arch = "universal"
    elif arch == "amd64":
        arch = "x86_64"

    repo = "https://github.com/encounter/gc-wii-binutils"
    return f"{repo}/releases/download/{tag}/{system}-{arch}.zip"


def compilers_url(tag: str) -> str:
    return f"https://files.decomp.dev/compilers_{tag}.zip"


def dtk_url(tag: str) -> str:
    uname = platform.uname()
    suffix = ""
    system = uname.system.lower()
    if system == "darwin":
        system = "macos"
    elif system == "windows":
        suffix = ".exe"
    arch = uname.machine.lower()
    if arch == "amd64":
        arch = "x86_64"

    repo = "https://github.com/encounter/decomp-toolkit"
    return f"{repo}/releases/download/{tag}/dtk-{system}-{arch}{suffix}"


def objdiff_cli_url(tag: str) -> str:
    uname = platform.uname()
    suffix = ""
    system = uname.system.lower()
    if system == "darwin":
        system = "macos"
    elif system == "windows":
        suffix = ".exe"
    arch = uname.machine.lower()
    if arch == "amd64":
        arch = "x86_64"

    repo = "https://github.com/encounter/objdiff"
    return f"{repo}/releases/download/{tag}/objdiff-cli-{system}-{arch}{suffix}"


def sjiswrap_url(tag: str) -> str:
    repo = "https://github.com/encounter/sjiswrap"
    return f"{repo}/releases/download/{tag}/sjiswrap-windows-x86.exe"


def wibo_url(tag: str) -> str:
    uname = platform.uname()
    arch = uname.machine.lower()
    system = uname.system.lower()
    if system == "darwin":
        arch = "macos"

    repo = "https://github.com/decompals/wibo"
    return f"{repo}/releases/download/{tag}/wibo-{arch}"


TOOLS: Dict[str, Callable[[str], str]] = {
    "binutils": binutils_url,
    "compilers": compilers_url,
    "dtk": dtk_url,
    "objdiff-cli": objdiff_cli_url,
    "sjiswrap": sjiswrap_url,
    "wibo": wibo_url,
}


def expected_length(response):
    """The Content-Length the server promised, or None if it did not."""
    raw = response.headers.get("Content-Length") if response.headers else None
    try:
        return int(raw) if raw is not None else None
    except ValueError:
        return None


def verify_length(url, output, expected, got):
    """Refuse a short write instead of leaving a truncated tool on disk.

    A truncated download used to be written out and reported as success. It
    surfaces far downstream and does not look like a download problem at all:
    dtk-linux-x86_64 arrived 31,386 bytes short, its ELF header still named a
    section-header offset past EOF, and EVERY dtk invocation then segfaulted --
    `--version` included -- which reads as a corrupt binary, not a bad fetch.
    A 1 GB compiler archive failing this way would be worse still.
    """
    if expected is None or got == expected:
        return
    try:
        os.remove(output)
    except OSError:
        pass
    raise SystemExit(
        f"download_tool: SHORT DOWNLOAD of {url}\n"
        f"  expected {expected} bytes, wrote {got}; the partial file was "
        f"removed.\n  Re-run; if it repeats, fetch it manually and verify the "
        f"size."
    )


def download(url, response, output) -> None:
    expected = expected_length(response)
    if url.endswith(".zip"):
        blob = response.read()
        verify_length(url, output, expected, len(blob))
        data = io.BytesIO(blob)
        with zipfile.ZipFile(data) as f:
            f.extractall(output)
        # Make all files executable
        for root, _, files in os.walk(output):
            for name in files:
                os.chmod(os.path.join(root, name), 0o755)
        output.touch(mode=0o755)  # Update dir modtime
    else:
        with open(output, "wb") as f:
            shutil.copyfileobj(response, f)
        st = os.stat(output)
        verify_length(url, output, expected, st.st_size)
        os.chmod(output, st.st_mode | stat.S_IEXEC)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("tool", help="Tool name")
    parser.add_argument("output", type=Path, help="output file path")
    parser.add_argument("--tag", help="GitHub tag", required=True)
    parser.add_argument(
        "--gdl-special-compilers",
        action="store_true",
        help="after downloading compilers, derive GDL's reviewed 1.2.5s profiles",
    )
    parser.add_argument(
        "--gdl-special-payload",
        type=Path,
        help="reviewed payload.bin for --gdl-special-compilers",
    )
    args = parser.parse_args()

    if args.gdl_special_compilers and args.tool != "compilers":
        parser.error("--gdl-special-compilers requires tool=compilers")
    if args.gdl_special_payload is not None and not args.gdl_special_compilers:
        parser.error("--gdl-special-payload requires --gdl-special-compilers")

    url = TOOLS[args.tool](args.tag)
    output = Path(args.output)

    print(f"Downloading {url} to {output}")
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    try:
        with urllib.request.urlopen(req) as response:
            download(url, response, output)
    except urllib.error.URLError as e:
        if str(e).find("CERTIFICATE_VERIFY_FAILED") == -1:
            raise e
        try:
            import certifi
            import ssl
        except ImportError:
            print(
                '"certifi" module not found. Please install it using "python -m pip install certifi".'
            )
            return

        with urllib.request.urlopen(
            req, context=ssl.create_default_context(cafile=certifi.where())
        ) as response:
            download(url, response, output)

    if args.gdl_special_compilers:
        repo_root = Path(__file__).resolve().parents[1]
        if str(repo_root) not in sys.path:
            sys.path.insert(0, str(repo_root))
        from tools.gdl.mwcc_p6.setup_compilers import (
            DEFAULT_PAYLOAD,
            install_special_compilers,
        )

        payload = args.gdl_special_payload or DEFAULT_PAYLOAD
        for compiler in install_special_compilers(output, payload):
            print(f"Installed {compiler}")


if __name__ == "__main__":
    main()
