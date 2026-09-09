#!/usr/bin/env sh
set -eu

# Cross-platform counterpart to build_payload.ps1 for CI. The COFF target and
# final SHA-256 make host defaults irrelevant and fail closed on tool drift.
clang_bin="${CLANG:-clang-18}"
nasm_bin="${NASM:-nasm}"
link_bin="${LLD_LINK:-lld-link-18}"
objcopy_bin="${LLVM_OBJCOPY:-llvm-objcopy-18}"

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir="$source_dir/build"
expected=966c72419f83e9c6da9fbc508d784adda2b326b07f2fa8e0bdff8a36176ae34d

mkdir -p "$build_dir"
"$clang_bin" --target=i686-pc-windows-msvc -Os -ffreestanding -fno-builtin \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fomit-frame-pointer \
    -c "$source_dir/p6fix.c" -o "$build_dir/p6fix.obj"
"$nasm_bin" -f win32 "$source_dir/p6fix_entry.asm" \
    -o "$build_dir/entry.obj"
"$link_bin" /entry:p6fix_entry /subsystem:console /nodefaultlib /fixed \
    /base:0x5A5000 /filealign:0x200 /align:0x1000 \
    "/out:$build_dir/payload.exe" \
    "$build_dir/entry.obj" "$build_dir/p6fix.obj"
"$objcopy_bin" --dump-section ".text=$build_dir/payload.bin" \
    "$build_dir/payload.exe"

actual=$(sha256sum "$build_dir/payload.bin" | awk '{print $1}')
if [ "$actual" != "$expected" ]; then
    echo "payload hash mismatch: $actual" >&2
    echo "expected: $expected" >&2
    exit 1
fi
echo "payload OK: $actual"
