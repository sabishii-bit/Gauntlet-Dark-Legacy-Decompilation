param(
    [string]$Clang = 'clang.exe',
    [string]$Nasm = 'nasm.exe',
    [string]$Link = 'lld-link.exe',
    [string]$Objcopy = 'llvm-objcopy.exe'
)

$ErrorActionPreference = 'Stop'
$sourceDir = $PSScriptRoot
$buildDir = Join-Path $sourceDir 'build'
$expected = '966C72419F83E9C6DA9FBC508D784ADDA2B326B07F2FA8E0BDFF8A36176AE34D'

New-Item -ItemType Directory -Force $buildDir | Out-Null
& $Clang --target=i686-pc-windows-msvc -Os -ffreestanding -fno-builtin `
    -fno-stack-protector -fno-asynchronous-unwind-tables -fomit-frame-pointer `
    -c (Join-Path $sourceDir 'p6fix.c') -o (Join-Path $buildDir 'p6fix.obj')
if ($LASTEXITCODE) { throw "clang failed: $LASTEXITCODE" }
& $Nasm -f win32 (Join-Path $sourceDir 'p6fix_entry.asm') `
    -o (Join-Path $buildDir 'entry.obj')
if ($LASTEXITCODE) { throw "nasm failed: $LASTEXITCODE" }
& $Link /entry:p6fix_entry /subsystem:console /nodefaultlib /fixed `
    /base:0x5A5000 /filealign:0x200 /align:0x1000 `
    "/out:$(Join-Path $buildDir 'payload.exe')" `
    (Join-Path $buildDir 'entry.obj') (Join-Path $buildDir 'p6fix.obj')
if ($LASTEXITCODE) { throw "lld-link failed: $LASTEXITCODE" }
& $Objcopy --dump-section ".text=$(Join-Path $buildDir 'payload.bin')" `
    (Join-Path $buildDir 'payload.exe')
if ($LASTEXITCODE) { throw "llvm-objcopy failed: $LASTEXITCODE" }

$actual = (Get-FileHash -Algorithm SHA256 (Join-Path $buildDir 'payload.bin')).Hash
if ($actual -ne $expected) { throw "payload hash mismatch: $actual" }
Write-Host "payload OK: $actual"
