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
