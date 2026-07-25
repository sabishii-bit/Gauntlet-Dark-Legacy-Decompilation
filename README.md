Gauntlet Dark Legacy \
[![Build Status]][actions]
[![Code Progress]][progress]
[![Data Progress]][progress]
[![Linked Progress]][progress]
=============

[<img src="https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?w=512&h=256" width="512" height="256">][progress]

[Build Status]: https://github.com/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation/actions/workflows/build.yml/badge.svg
[actions]: https://github.com/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation/actions/workflows/build.yml
[Code Progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?mode=shield&measure=code&label=Code&category=all
[Data Progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?mode=shield&measure=complete_data&label=Data&category=all
[Linked Progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?mode=shield&measure=complete_code&label=Linked%20Code&category=all
[progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation

<!--
The progress badges and graphic above are served by https://decomp.dev.
They stay blank until the repository is registered there: sign in at
decomp.dev with GitHub and add this project. CI already uploads the
`report.json` progress report on every build of the default branch, which
decomp.dev ingests automatically once the project is registered.
-->

A work-in-progress decompilation of **Gauntlet Dark Legacy** for the Nintendo GameCube, built with
[decomp-toolkit](https://github.com/encounter/decomp-toolkit) and the
[dtk-template](https://github.com/encounter/dtk-template) project structure.

This repository does **not** contain any game assets or assembly whatsoever. An existing copy of the game is required.

It builds `main.dol`:

| Version    | Game ID  | SHA-1                                      |
| ---------- | -------- | ------------------------------------------ |
| Rev 0 (USA) | `GUNE5D` | `7cba77aa496eb0fc5ffec60efd9680aa9635d679` |

Dependencies
============

Windows
--------

On Windows, it's **highly recommended** to use native tooling. WSL or msys2 are **not** required.  
When running under WSL, [objdiff](#diffing) is unable to get filesystem notifications for automatic rebuilds.

- Install [Python](https://www.python.org/downloads/) and add it to `%PATH%`.
- Download [ninja](https://github.com/ninja-build/ninja/releases) and add it to `%PATH%`.
  - Quick install via pip: `pip install ninja`

macOS / Linux
--------------

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages).

[wibo](https://github.com/decompals/wibo), a minimal 32-bit Windows binary wrapper, will be automatically downloaded and used to run the CodeWarrior compilers.

All other tools (compilers, objdiff-cli, sjiswrap, etc.) are downloaded automatically during the build.

Building
========

- Clone the repository:

  ```sh
  git clone https://github.com/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.git
  ```

- Copy your game's disc image into `orig/GUNE5D/`.
  - Supported formats: ISO (GCM), RVZ, WIA, WBFS, CISO, NFS, GCZ, TGC
  - Alternatively, place extracted files directly (e.g. `orig/GUNE5D/sys/main.dol`).
  - After the initial build, the disc image can be deleted to save space (objects are extracted to the filesystem).

- Configure:

  ```sh
  python configure.py
  ```

- Build:

  ```sh
  ninja
  ```

The build is verified against [config/GUNE5D/build.sha1](config/GUNE5D/build.sha1).

### A note on the target hash

The retail DOL's `extab` (exception table) section contains uninitialized compiler data that a fresh link
cannot reproduce. The build therefore targets an **extab-cleaned** DOL (`clean_extab: true` in
[config/GUNE5D/config.yml](config/GUNE5D/config.yml)):

- `config.yml` verifies your *input* DOL against the original disc hash (`7cba77aa...`).
- `build.sha1` verifies the *output* DOL against the cleaned hash (`540bed0b...`), equivalent to running
  `dtk extab clean` on the original.

Diffing
=======

Once the initial build succeeds, an `objdiff.json` should exist in the project root.

Download the latest release from [encounter/objdiff](https://github.com/encounter/objdiff). Under project settings, set `Project directory`. The configuration should be loaded automatically.

Select an object from the left sidebar to begin diffing. Changes to the project will rebuild automatically: changes to source files, headers, `configure.py`, `splits.txt` or `symbols.txt`.

Decompilation workflow
======================

The sections below document the full matching process used in this repository, including the
project-specific tooling in [`tools/gdl/`](tools/gdl/). The core philosophy, learned the hard way:

- **Read the target assembly first.** For any function beyond ~50 instructions, a full read of the
  dtk-generated assembly (frame layout, register homes, branch structure) beats hypothesis-driven
  iteration by roughly 10x. Decompiler output (Ghidra) is used for *semantics only* — it deliberately
  loses statement boundaries, storage classes, and evaluation order, which are exactly the things
  that decide whether MWCC reproduces the bytes.
- **Hunt for reference source before writing anything.** Large parts of this DOL are library code
  with public or sibling-decomp source (Dolphin SDK, MSL C runtime, zlib 1.0.4,
  fdlibm math, PS2-SDK-derived Midway code). One `find` in a reference tree can turn a day of
  matching into an hour. See [Reference material](#reference-material).
- **Only the linked DOL hash is truth.** The per-function tools are deliberately blind to some
  whole-TU properties (see [tool blind spots](#what-the-per-function-tools-cannot-see)), so every
  TU goes through a green-gated finish step that links and verifies the SHA-1 before anything is
  committed.

Project tools (`tools/gdl/`)
----------------------------

All project-authored tooling lives in `tools/gdl/` (everything else in `tools/` is stock
dtk-template / third-party). Run everything from the repository root.

| Tool | Purpose |
| ---- | ------- |
| `matchtool.py` | Compile a unit against **both proven compilers × all flag presets in parallel** and print a score table per function (`OK` = byte-exact, `OK~` = only reloc *names* differ, `L±n` = length delta, `n` = word diffs). `--brief` prints only the best row's non-OK cells. `--fn X` narrows to one function, `--show` prints the best candidate's diff, `--matrix` runs a full cartesian flag sweep. |
| `fndiff.py` | Per-function disassembly diff of target vs our build, addresses/branch targets normalized. `--count` prints one summary line per function (`insns T/B lines N real M` — `real` excludes reloc-name-only noise; `real 0` means "rename symbols, code is done"). `--ops` collapses to opcode clusters to separate structural diffs from register noise. Auto-rebuilds the object via ninja when the source is newer. |
| `fnasm.py` | Compact target-asm reader: one line per instruction, relocations folded inline (`@sym(RELOC)`), branch targets as function-relative `->off` so branch adjacency is visible at a glance. Reads the dtk-extracted object, so it works before any source exists and can never show stale data. Supports index slicing (`fnasm.py unit fn 40:120`). |
| `fnskel.py` | Conservative C-skeleton generator (MWCC-aware, **not** a decompiler). Emits frame/param analysis, block labels, and C-ish lines *only* where the idiom is certain — conversion transits, bool-from-compare, symbol materializations, call-arg summaries — leaving everything else as `##` asm lines. Output is deliberately non-compilable: storage classes and statement grouping are whole-TU properties a local pattern matcher cannot see, and a plausible-but-wrong skeleton is worse than none. |
| `refscan.py` | Reference-source scanner: compiles an external reference TU (e.g. a melee MSL/SDK file) with mwcc, opcode-normalizes its functions, and fuzzy-aligns them against the functions of any target object (including unclaimed `auto_*` regions). Scores of `1.000` identify functions and TU boundaries before a single line of our source exists. |
| `claimcheck.py` | Verifies a unit's emitted object sections against its `splits.txt` claims (run automatically by `finish_tu.py`; `--matching` audits every Matching unit). Hard-errors only on an emitted section with no claim. |
| `doldiff.py` | Byte-level diff of the linked DOL vs original with VA/symbol attribution. The arbiter for everything the per-function tools cannot see. Suppresses the one expected `clean_extab` diff. |
| `finish_tu.py` | Green-gated end-of-TU pipeline: claimcheck → flip `NonMatching`→`Matching` → configure → **ninja with the exit code checked directly** (the stale `build/GUNE5D/ok` file is deleted first) → commit only on green. Batch mode (`finish_tu.py a.c b.c -m "msg"`) flips several TUs with a single build and a single commit. It is impossible to commit a red build through this tool — never hand-roll flip+ninja+commit. |
| `xbmod.py` / `pdb20_dump.py` / `search_xbox_symbols.py` | Xbox `shell3D.pdb` mining: full module (TU) list of the sibling Xbox build, 7405 named functions with per-TU source order and sizes. `xbmod.py --module FOO.OBJ` lists one module. Names discovered here become GameCube symbol names. |

The TU matching loop
--------------------

The end-to-end process for one translation unit:

1. **Scout.** Identify the region: list its functions from `config/GUNE5D/symbols.txt`, read a few
   with `fnasm.py`, and check the Xbox PDB (`xbmod.py`) for the module identity. If reference source
   might exist (SDK/MSL/zlib/etc.), run `refscan.py <ref.c> <auto obj>` — a run of `1.000` scores
   identifies functions, names, and the TU boundary in one shot. Do not skip this step: wrong
   region identity costs more than any other mistake (this repo's file layer was misidentified
   twice — g3dMath, then ML_MEM — before `fakelib.obj` was confirmed by an exact size match).
2. **Wire the split.** Add the TU to `config/GUNE5D/splits.txt` (`.text` range first; data claims
   come later), add an `Object(NonMatching, ...)` to `configure.py`, run `python configure.py`,
   then `ninja build/GUNE5D/obj/<unit>.o` once so dtk extracts the target object.
3. **Probe.** `python tools/gdl/matchtool.py probe <unit> --brief`. The probe compiles the unit
   under every preset × both compilers in parallel. Early on this pins the flag family and
   compiler; `ties:N` in the brief output tells you how flag-sensitive the code is.
4. **Write / iterate.** For each non-OK function: read it (`fnasm.py`, or `fnskel.py` for a
   scaffold), fix the source, then score with `fndiff.py <unit> --count`. Use `--ops` when you
   need to separate structure from register noise — but remember `--ops` *hides* register and
   reloc diffs, so always confirm with the `--count` line totals.
5. **Names.** Rename `fn_`/`lbl_` symbols in `symbols.txt` as identities firm up (our extern names
   must match `symbols.txt` for relocations to pair — `OK~` scores and `real 0` counts almost
   always mean "rename the data symbols").
6. **Data claims.** Find the TU's emitted data sections (`objdump -h` on our object), locate the
   target addresses (reloc names via `fnasm.py`, neighbors via `symbols.txt`), and add
   `.data`/`.rodata`/`.sdata`/`.sdata2`/`.bss`/`.sbss` ranges to the split. Sizes must account for
   section-end alignment (dtk will error with exact boundaries if a claim overruns — the error
   message contains the real section end).
7. **Finish.** `python tools/gdl/finish_tu.py <unit.c> -m "Match <unit>"` — claim check, flip,
   full build, SHA-1 verify, commit. If it reports RED, diagnose with `doldiff.py` /
   `claimcheck.py --matching`; the DOL diff attributes every byte to a unit and symbol.

Compilers and flag families
---------------------------

Two compiler versions are proven in this DOL and both are probed by default:

- **GC/1.2.5n** — most of the Dolphin SDK; tell: pre-RA hoisting of prologue copies.
- **GC/1.2.5** — game code, zlib, MSL, C++ math (`g3dMath3D.cpp`), the ODEMU debug driver;
  tell: `lwz r0,x(r1); li r3,0; lmw` epilogues.

Flag families that have actually discriminated (all defined in `matchtool.py` as presets):
`sdk` (`-O4,p -str reuse`), `runtime` (MSL: `-use_lmw_stmw on -str reuse,pool,readonly -common off`),
`inline1` (si/exi: `-inline auto,level=1`), and the `demo` family (`-O4` **without** peephole,
`-Cpp_exceptions on`, `-str reuse,readonly`) used by the Midway demo-library and most game TUs.
Many functions are flag-insensitive (probe shows `ties:15`); let the probe table decide, never
guess, and never run archive-wide compiler sweeps — the two versions above are the proven set.

What the per-function tools cannot see
--------------------------------------

The probe and fndiff compare *functions* with reloc names normalized. Four whole-TU properties are
invisible to them and only caught by the link + `doldiff.py`:

1. **Function emission order** inside the TU. The linker lays functions in object order; if it
   differs from the target, every caller's `bl` displacement shifts (seen as uniform `+0xNN`
   deltas across the DOL). GDL's MSL revision reorders several TUs relative to melee's source.
2. **Data layout**: `.sbss`/`.sdata` variable order (usually declaration order with `-common off`,
   *reverse* declaration order with `-common on`; occasionally neither — verify), and
   `.sdata2` **constant-pool order**, which is whole-TU first-use order *including dead-stripped
   functions* (dead statics still order the pool).
3. **Stack frame internals** — spill-slot and conversion-temp placement affect only offsets/bytes
   that reloc-normalized comparison can see, but their *causes* (declaration order, storage class)
   are source-level decisions.
4. **Constant values behind relocs** (pool doubles, epsilon values) — read them from the DOL, not
   from the decompiler.

MWCC codegen field notes
------------------------

Hard-won behaviors of this vintage of mwcceppc that recur constantly (see the memory notes in the
project history for the full catalog):

- **Inlining**: a function can only be inlined at call sites *after* its definition
  (defined-before rule). `#pragma dont_inline on/off` is **call-site scoped** — it governs calls
  emitted while active, not callees. `-inline auto` refuses functions containing loops, and has a
  per-caller budget; `inline`-qualified functions bypass the budget. `mwld` strips unreferenced
  static functions, but stripped statics still order the constant pool.
- **`#pragma defer_codegen on` reverses both the emission order and the code-generation order of
  a file's functions** — which also reverses string-literal pool layout. MSL's `printf.c` and
  `buffer_io.c` require it (one pragma fixed instruction diffs *and* string-pool offsets at once).
- **Conversion transits never CSE across cast sites**: each `(f32)int` site gets its own
  `xoris/stw/stw/lfd/fsubs` stack pair and each `(s32)float` its own `stfd/lwz`, even when the
  converted value is identical (one `fctiwz` may be shared). Grouped cast statements produce
  overlapping (distinct) pairs; casts inside larger expressions let slots be reused sequentially.
- **Float compares follow source text order** (`x != 0.0f` → `fcmpo x, 0`); `if (floatvar)`
  truthiness emits var-first. Float `>=`/`<=` conditions need `cror`; a bare `bge` means the
  source was written with `<` and the branch inverted.
- **Storage classes decide everything about float-heavy code.** Locals in one address-taken
  scratch array accessed through a pointer (`f32 v[50]; f32* vp = v;`) produce memory-resident,
  reload-per-statement code with intra-statement CSE — a PS2-heritage pattern in Midway TUs that
  neither `volatile` (kills intra-statement CSE) nor plain locals (register-allocated) reproduce.
- **Common idioms**: `neg/addic/subfe` = `(x != 0)`; `srawi/subfc/adde` = `(x >= 0)`;
  bitfield stores compile to single `rlwimi` inserts (manual shift-or does not);
  `li rX, sym@sda21` is an SDA *address* materialization disguised as a load-immediate;
  pointer differences of `int*` compile to `sub/srawi 2` with `addze` only for signed division
  (a bare shift means the source wrote `>> 3`, not `/ 8`).
- **Local pointer caching**: `PBWINGLOBALS* g = gWinGlobals;` once per function, then
  `g->current->field` re-derefs per statement. After any store through a may-alias pointer, field
  loads through globals reload — but a *local* pointer variable keeps its register value.
  Redundant-looking inner `if`s and rematerialized constants usually indicate an **inlined static
  helper** (the inliner runs after GCSE and dead-store elimination, so inlined bodies keep
  redundancies the optimizer would otherwise fold).

Iteration discipline
--------------------

Adopted after measuring attempt-to-progress ratios across sessions:

- **Classify residuals before iterating.** `--ops` clean but line count high = register-rotation /
  scheduling class. Structural diffs converge at roughly one fix per attempt — iterate freely.
  Scheduler/regalloc-only residuals converge near zero — hard cap of **3 source-shape attempts**,
  then park the function with notes and move on (revisit once, fresh, in a later session).
- **Suspect the disassembly read before exotic theories.** If a shape seems to need a bizarre
  source construct, re-read the raw bytes and recompute branch targets first; label-adjacency
  misreads have cost more than any compiler behavior.
- **Kill dead test axes**: after two identical A/B results (e.g. compiler versions for a given
  TU), stop testing that axis.
- Commit messages are plain one-liners; commits only ever happen through the green gate.

Reference material
------------------

- **melee decomp**: Dolphin SDK sources under
  `extern/dolphin/src/`, MSL C runtime under `src/MSL/` (GDL's MSL is a slightly newer revision —
  expect function-order and small body deltas), MetroTRK, runtime.
- **Xbox `shell3D.pdb`** (`research/xbox_symbols/`): the Xbox build of this same game, cracked into
  a full module list with function names, sizes, and source order (`functions_by_module.txt`).
  The GameCube build shares most game-code TUs; function order within a TU usually survives even
  when the port diverges. This is the primary naming source for game code.
- **Public library code**: zlib 1.0.4, fdlibm (MSL's double-precision math), BSD libc lineage
  (`vsprintf.c`), Sony PS2 SDK samples (`mathfunc.c` = `libsamprel` mathfunc, and `fakelib.c`
  reimplements the PS2 `sce*` API over GameCube services).

To link non-matching code for testing (final hash will not match):

```sh
python configure.py --non-matching
ninja
```

To print decompilation progress:

```sh
python configure.py progress
```

Xbox debug symbols
==================

[research/xbox_symbols/](research/xbox_symbols/) contains ~3,500 type definitions (structs, enums,
unions — with exact field offsets and sizes) extracted from leaked PDB debug symbols of the **Xbox**
version of the game. Because Midway shared the game codebase across platforms, these layouts carry
over to the GameCube binary.

> [!NOTE]
> **Verified:** decompiling `AtreeFindNode`/`AtreeMatch` in the GUNE5D DOL shows every field offset,
> struct size, and array stride matching the PDB definitions of `atree`, `anode`, `anodeinfo`,
> `atreelist`, and `atreeinfo` exactly (11/11 offsets checked — e.g. `atree.nanodes` @ 0x3C,
> `sizeof(anode)` = 0x28, `atreeinfo.offset` @ 0x20).

Workflow when decompiling game code:

1. **Look up the types.** Find candidate structs in
   [research/xbox_symbols/type_index.txt](research/xbox_symbols/type_index.txt), then read their
   definitions in the mapped header. Portable game types live in `game.h`, `graphics.h`, `math.h`,
   `audio.h`, `misc.h`, `ps2.h`, and `util.h`. Ignore `d3d.h`, `xbox.h`, and `windows_*.h` — those
   are Xbox platform types that don't exist on GameCube.
2. **Verify before trusting.** Compare the disassembly's field offsets and array strides against the
   PDB layout (both platforms are 32-bit little/big-endian with the same pointer size, so layouts
   generally match — but compiler padding, bitfields, and any embedded platform types can differ).
3. **Port the struct** into a header under `include/`, keeping the original names and commenting
   each field with its offset.
4. **Reuse the names.** The PDB preserves original naming conventions (`atree`, `anodeinfo`,
   `enemy`, `smworld_t`, …) — use them for functions and globals in
   [config/GUNE5D/symbols.txt](config/GUNE5D/symbols.txt) and when writing source, instead of
   inventing new ones.
5. **Import the structs into Ghidra** (see below) so its decompiler renders named field accesses —
   this makes reconstructing the C source dramatically faster.
6. **Decompile with types applied.** Retype the function's parameters/locals in Ghidra's decompiler
   (see below). Raw pointer math like `*(int*)(param + 0x3c)` becomes `tree->nanodes`, and the
   pseudocode reads close to the original source.
7. **Write and match the source.** Use the typed pseudocode to write the C file, then re-enter the
   [decompilation workflow](#decompilation-workflow) at step 3: register the TU in `configure.py`,
   build, and iterate in objdiff until it matches.
8. **Feed names back.** Once a function/global is confirmed, record its name in
   [config/GUNE5D/symbols.txt](config/GUNE5D/symbols.txt) *and* rename it in Ghidra, so the build
   system, objdiff, and the Ghidra database stay in sync.

### Importing the structs into Ghidra

The dump headers are **not directly parseable** — they contain duplicate definitions, repeated
`enum __unnamed` blocks, and cross-file dependencies. Always curate a small scratch header first:

1. **Curate a mini-header.** Copy just the structs you need (plus their dependencies) from the
   `research/xbox_symbols/` headers into a scratch `.h` file. Work bottom-up: for `atree` you also
   want `anode`, `anodeinfo`, and `animinfo`. Replace pointers to types you don't care about yet
   with `void*`, and unknown trailing regions with `char unkXX[N];` padding — you can refine later.
   Keep the `// Offset=... Size=...` comments; they're valid C comments and serve as documentation.
2. **Parse it:** in the CodeBrowser, **File -> Parse C Source…**, add your scratch header to
   *Source files to parse*, clear the profile's include paths if it complains, and hit
   **Parse to Program**. The types appear in the *Data Type Manager* under your program's category.
   - Alternative for one or two structs: in the *Data Type Manager*, right-click the program ->
     **New -> Structure**, and enter fields manually using the offsets from the dump.
3. **Sanity-check sizes.** Hover the new type in the Data Type Manager — its size must equal the
   `Size=0x..` in the dump comment (`atree` = 0x48, `anode` = 0x28, `anodeinfo` = 0x3C). If it's
   off, a field type or padding is wrong.
4. **Apply the types.** In the decompiler view, click a function parameter or local, press
   **Ctrl+L** (*Retype Variable*), and enter e.g. `atree *`. Or right-click the function ->
   **Edit Function Signature** and set the full prototype
   (`void * AtreeFindNode(atree * tree, char * name, int len)`). Field accesses, array strides,
   and nested structs then render by name throughout the decompilation.

This exact procedure was used to verify the Atree family: after retyping one parameter, the
decompiler output collapsed into `strncmp(tree->anodeinfo[i].mbdesc, name, len)`-style code that
can be transcribed into a source file almost directly.
