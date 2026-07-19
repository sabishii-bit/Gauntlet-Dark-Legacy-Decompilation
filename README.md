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

1. **Pick a function.** Split assembly is written under `build/GUNE5D/asm/`. Every `.s` file corresponds to
   one original translation unit — all functions in a file must eventually live together in one source file.
2. **Define the split.** Add the file and its address ranges to
   [config/GUNE5D/splits.txt](config/GUNE5D/splits.txt), and name symbols in
   [config/GUNE5D/symbols.txt](config/GUNE5D/symbols.txt).
3. **Write the source.** Create the matching C/C++ file under `src/` and register it in
   [configure.py](configure.py) under `config.libs`, starting as `NonMatching`:

   ```python
   Object(NonMatching, "Game/example.c"),
   ```

4. **Match it.** Reconfigure, build, and compare against the original in objdiff. Iterate until the diff is
   clean. [decomp.me](https://decomp.me) is useful for collaborating on tough functions.
5. **Mark it `Matching`.** Rebuild — the object is now linked into the DOL, and the final hash check
   confirms the match is byte-perfect.

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

Project structure
=================

```
.
├── config/GUNE5D/      # dtk configuration: config.yml, symbols.txt, splits.txt, build.sha1
├── include/            # Header files
├── orig/GUNE5D/        # Original game files (not committed)
├── research/           # Reference material (e.g. Xbox build symbols for struct matching)
├── src/                # Decompiled source code (created as work progresses)
├── tools/              # Build scripts
├── configure.py        # Project configuration; generates build.ninja and objdiff.json
└── build/GUNE5D/       # Build artifacts (generated, not committed)
```

Resources
=========

- [decomp.me](https://decomp.me) — collaborate on matches online
- [GC/Wii Decompilation Discord](https://discord.gg/hKx3FJJgrV) — ask in `#dtk`
