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

Contributing
============

Contributions that improve the accuracy of the decompilation are welcome. The
project's helper scripts live in [`tools/gdl/`](tools/gdl/), and each script
supports `--help`.

Before submitting a change, rebuild the project, inspect the affected object in
objdiff, and make sure the linked DOL still passes the configured hash check.
Please keep commits focused and avoid mixing unrelated cleanup with decompilation
work.

To link non-matching code for testing (final hash will not match):

```sh
python configure.py --non-matching
ninja
```

This mode links raw compiler output. It deliberately bypasses Frank, WebFrank,
P6Frank, and every other retail-target/hash-dependent object rewrite so edited
source remains usable as a normal mod build.

To print decompilation progress:

```sh
python configure.py progress
```

### Compiler variants and guarded postprocessing

The build harness supports two narrowly scoped CodeWarrior postprocessors for
historical compiler walls:

- `tools/gdl/frank.py` implements Melee's two-object GC/1.2.5e profile merge.
  It is intended for evidence-backed epilogue scheduling probes, not as a
  project-wide compiler replacement.
- `tools/gdl/webfrank.py` can correct an individually audited
  `REGISTER_ONLY` function after proving that every non-register instruction
  bit already matches. It also supports an exceptional scheduler rule that
  permutes an explicit bijection of independent straight-line instruction
  atoms, moving relocations with their atoms, before applying the same register
  proof. Rules carry exact input, target, relocation, and output hashes and fail
  the build closed on source/compiler drift.

The repository-wide Frank sweep found no improvements, so Frank is opt-in per
object. WebFrank is likewise restricted to reviewed rules in
[`config/GUNE5D/webfrank.json`](config/GUNE5D/webfrank.json); it must not be
used to hide opcode, branch, immediate, relocation-payload, ABI, semantic, or
data differences. Scheduler permutations additionally require a recorded
dependency audit, no control instructions, and an exact relocation-preserving
bijection; they are not a general target-byte-copy mechanism. See
[`gc_125e_frank.md`](.claude/memory/gc_125e_frank.md) for
the compiler history, audit results, and verification policy.

An experimental `GC/1.2.5s` open patch recipe for the recovered `regFind`
PCode-layout carrier is documented in
[`tools/gdl/mwcc_p6/`](tools/gdl/mwcc_p6/). It derives a separately named
compiler from a user-supplied, SHA-pinned GC/1.2.5 or 1.2.5n executable and
contains no proprietary compiler bytes. It is not selected by the normal build
yet. Compiler-derived, raw-compiler, and postprocessed matches must remain
distinguishable in reporting.

A `NonMatching` object may be promoted to `Matching` only when its complete
postprocessed object is exact, including code, relocations, data/BSS and
exception metadata, and a fresh full build reproduces the configured DOL hash.
The first audited closure pass used this process to link `g3dpad`, `vsprintf`,
and `mempool`; their former PARKED notes remain useful source-codegen diagnoses,
not permanent binary-matching vetoes.

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
