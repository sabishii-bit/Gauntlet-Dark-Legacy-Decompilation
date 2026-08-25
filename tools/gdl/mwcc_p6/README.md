# Experimental GC/1.2.5s compiler extension

This directory contains an opt-in compiler patch experiment for one recovered
MWCC PCode block-layout rule. It is not part of the normal build yet.

The patch recognizes a semantic control-flow carrier before MWCC constructs
predecessor lists. It does not inspect function names, target objects, target
hashes, PowerPC instruction bytes, or retail addresses. A function changes only
when exactly one candidate satisfies every graph and idempotence proof; zero or
multiple candidates leave the function unchanged.

The recipe contains no Metrowerks code. It derives a new, separately named
compiler from a user-supplied executable whose SHA-256 is either:

- GC/1.2.5: `0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`
- GC/1.2.5n: `ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`

## Build the open payload

The reviewed payload was produced with LLVM 18.1.2, NASM 2.16.03, and
PowerShell. The final payload hash is pinned by both scripts.

```powershell
cd tools/gdl/mwcc_p6
.\build_payload.ps1
```

Tool paths may be supplied explicitly:

```powershell
.\build_payload.ps1 `
  -Clang C:\LLVM\bin\clang.exe `
  -Nasm C:\NASM\nasm.exe `
  -Link C:\LLVM\bin\lld-link.exe `
  -Objcopy C:\LLVM\bin\llvm-objcopy.exe
```

## Derive a compiler

Never overwrite the supplied compiler. The patcher rejects path aliases and
writes the derived executable atomically.

```powershell
python patch_pe.py `
  ..\..\..\build\compilers\GC\1.2.5n\mwcceppc.exe `
  build\payload.bin `
  build\mwcceppc-125s.exe
```

Expected derived SHA-256 values:

- GC/1.2.5: `7cbeb085205df54bca3fb89ff7a19d323003c1a63a14a942e12ef06cec7c3a31`
- GC/1.2.5n: `5a4d1e1715954ddefc87a5a0dfbe38b6c3916e22214957b21af3bd147a760667`

To try the compiler on `registry.c`, install the derived executable beside a
copy of the original license DLL and select the explicit profile:

```powershell
New-Item -ItemType Directory -Force `
  ..\..\..\build\compilers\GC\1.2.5s | Out-Null
Copy-Item build\mwcceppc-125s.exe `
  ..\..\..\build\compilers\GC\1.2.5s\mwcceppc.exe
Copy-Item ..\..\..\build\compilers\GC\1.2.5n\lmgr326b.dll `
  ..\..\..\build\compilers\GC\1.2.5s\lmgr326b.dll
cd ..\..\..
python configure.py --experimental-p6-compiler
ninja
```

The `s` suffix denotes this control-flow scheduling/layout derivative. The
configuration verifies the derived executable hash and then compiles
`registry.c` directly, without P6Frank. Without the flag, the normal exact
build remains unchanged.

## Evidence and policy

The final 1.2.5n derivative compiled all 94 exact Ninja commands currently
using that compiler. Ninety-three whole objects were byte-identical to stock.
Only `game/sys/registry.c` changed, and its result was byte-identical to the
independent P6Frank and live-debugger result. A synthetic corpus verified that
zero-candidate, multiple-candidate, nonzero-assignment, and side-effecting near
misses remain unchanged.

This remains an experimental compiler profile selected only for `registry.c`.
Keep independent exact-object/DOL gates and keep mod builds free of all
target-dependent object postprocessors. A derived compiler match must be
reported distinctly from raw stock-compiler and postprocessed matches.

## Import the recovered Ghidra annotations

`ghidra_import.py` preserves the address work behind this experiment without
including any compiler bytes. It accepts only these original executable hashes:

- GC/1.2.5: `0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`
- GC/1.2.5n: `ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`

The import adds recovered function/global names, AST and PCode capture
bookmarks, the P6 pre-predecessor hook boundary, the 1.2.5n epilogue-patch
sites, and the recovered stage-2 scheduler. Scheduler annotations include its
driver, dependency builders, four-level picker, default machine-model hooks,
model/timing globals, graph-ready boundary, and each tie-break boundary. Packed
types cover `PCodeBlock`, `PCodeInstruction`, `PCodeOperand`, `PCodeLink`,
`PCodeLabel`, `SchedulerNode`, `SchedulerEdge`, `SchedulerMachineModel`, and
`SchedulerOpcodeTiming`. Names described as inferred in their plate comments
are semantic working names, not claimed original Metrowerks identifiers.

The scheduler comments also preserve the important distinction established by
live GC/1.2.5 traces: `AllocFile` reaches the final earlier-textual tie, while
`sysPollResetButton` is decided at the preceding release-count tier. They are
therefore not evidence for one simple reversed-text-order rule.

In the Ghidra GUI, import a user-owned `mwcceppc.exe` as PE/i386, run normal
analysis, add this directory to Script Manager's script paths, and run
`ghidra_import.py` from `GDL.MWCC`. The script checks Ghidra's recorded
executable SHA-256 before changing the program. If an older Ghidra import lacks
that property, pass the original executable path as the script's first
argument.

For a fresh headless project:

```powershell
analyzeHeadless.exe C:\ghidra-projects mwcc125 -import C:\owned\mwcceppc.exe `
  -scriptPath tools\gdl\mwcc_p6 -postScript ghidra_import.py
```

For an existing program whose executable hash property is unavailable:

```powershell
analyzeHeadless.exe C:\ghidra-projects mwcc125 -process mwcceppc.exe `
  -scriptPath tools\gdl\mwcc_p6 `
  -postScript ghidra_import.py C:\owned\mwcceppc.exe
```

An unsupported hash, a mismatched supplied file, a non-32-bit program, or an
unmapped profile address aborts the import. Derived `1.2.5s` executables are
intentionally rejected: annotate the exact stock or 1.2.5n input first so the
research database retains unambiguous provenance.
