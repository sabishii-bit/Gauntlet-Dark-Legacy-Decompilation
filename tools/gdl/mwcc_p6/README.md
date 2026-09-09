# Experimental GC/1.2.5s compiler extension

This directory contains an opt-in compiler patch experiment for two recovered
MWCC PCode scheduling/layout rules. It is not part of the normal build yet.

The patch recognizes a semantic control-flow carrier before MWCC constructs
predecessor lists. It does not inspect function names, target objects, target
hashes, PowerPC instruction bytes, or retail addresses. A function changes only
when exactly one candidate satisfies every graph and idempotence proof; zero or
multiple candidates leave the function unchanged.

The second hook is at the `SpillCode_MarkLastUses` call to the generic PCode
instruction remover. Live tracing established that the remover clears the
block's already-scheduled flag even when `MarkLastUses` has proved a dead
`LI vreg, 0`; the later physical scheduler then needlessly reorders the seven
remaining instructions. The extension retains the existing schedule only for
the confirmed eight-instruction block and dead-LI operand shape. Every other
removal, caller, opcode, immediate and block size follows stock behavior.

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

For a fresh setup, build the open payload and let the normal compiler downloader
derive and install both profiles after extracting the pinned compiler archive:

```powershell
.\tools\gdl\mwcc_p6\build_payload.ps1
python tools\download_tool.py compilers build\compilers `
  --tag 20251118 `
  --gdl-special-compilers `
  --gdl-special-payload tools\gdl\mwcc_p6\build\payload.bin
```

If the ordinary compiler archive is already installed, avoid downloading it
again and run only the authenticated setup step:

```powershell
python tools\gdl\mwcc_p6\setup_compilers.py build\compilers `
  --payload tools\gdl\mwcc_p6\build\payload.bin
```

Both paths verify the two base compilers, payload, callsites and derived output
hashes before atomically installing either profile.

The equivalent manual derivation is shown below for auditing and development.

```powershell
python patch_pe.py `
  ..\..\..\build\compilers\GC\1.2.5\mwcceppc.exe `
  build\payload.bin `
  build\mwcceppc-125s.exe
```

Expected derived SHA-256 values for the current two-hook payload:

- GC/1.2.5: `67d65dcb09f40823a55284e823c65e135a6c01ec8c61a7664d5c61d20ecad870`
- GC/1.2.5n: `96c858461ed60bb348ba9f1551c98364e0d6b5914305b8ca4541a3dae536841a`

The stock-derived executable is named `GC/1.2.5s` and is selected for
`gamemain.c`. The 1.2.5n-derived executable is named `GC/1.2.5sn` and preserves
the earlier registry experiment. Install both beside the license DLL from
their respective base compiler, then select the explicit profile:

```powershell
New-Item -ItemType Directory -Force `
  ..\..\..\build\compilers\GC\1.2.5s | Out-Null
Copy-Item build\mwcceppc-125s.exe `
  ..\..\..\build\compilers\GC\1.2.5s\mwcceppc.exe
Copy-Item ..\..\..\build\compilers\GC\1.2.5\lmgr326b.dll `
  ..\..\..\build\compilers\GC\1.2.5s\lmgr326b.dll
New-Item -ItemType Directory -Force `
  ..\..\..\build\compilers\GC\1.2.5sn | Out-Null
python patch_pe.py `
  ..\..\..\build\compilers\GC\1.2.5n\mwcceppc.exe `
  build\payload.bin `
  build\mwcceppc-125sn.exe
Copy-Item build\mwcceppc-125sn.exe `
  ..\..\..\build\compilers\GC\1.2.5sn\mwcceppc.exe
Copy-Item ..\..\..\build\compilers\GC\1.2.5n\lmgr326b.dll `
  ..\..\..\build\compilers\GC\1.2.5sn\lmgr326b.dll
cd ..\..\..
python configure.py --experimental-p6-compiler
ninja
```

The `s` suffix denotes this control-flow scheduling/layout derivative; `sn`
denotes the same payload layered on the Ninji epilogue derivative. The
configuration verifies both executable hashes, compiles `gamemain.c` with the
stock-derived profile, and retains the 1.2.5n-derived profile for `registry.c`.
No object postprocessor is involved. Without the flag, the normal extracted-
fallback build remains unchanged.

## Evidence and policy

The original layout hook was validated against all 94 exact 1.2.5n Ninja
commands: 93 whole objects were byte-identical and only `game/sys/registry.c`
changed, matching the independent live-debugger result. A synthetic corpus
verified that zero-candidate, multiple-candidate, nonzero-assignment and
side-effecting near misses remain unchanged.

For the schedule-retention hook, a debugger counterfactual changed only the two
adjacent `game_main` words (plus the object checksum) and made all 28 retail
function bodies exact. An incremental 93-command 1.2.5n corpus produced zero
changes from the prior layout-only derivative. Among the 142 other stock
1.2.5 commands, four objects contain the same guarded shape; the opt-in build
does not select 1.2.5s for those TUs. With 1.2.5s selected only for
`gamemain.c`, a fresh full link passes `build/GUNE5D/main.dol: OK`, including
linked data, relocations and exception metadata.

These remain experimental compiler profiles selected only for the reviewed TUs.
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
`SchedulerOpcodeTiming`. The allocator frontier additionally names the
simplify/select/commit pipeline, interference/coalescing and liveness passes,
class counters/windows, and packed `PCodeBlockLiveness` and
`InterferenceNode` layouts. Names described as inferred in their plate
comments are semantic working names, not claimed original Metrowerks
identifiers.

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
