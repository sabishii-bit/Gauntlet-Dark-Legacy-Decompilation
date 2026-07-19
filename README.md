Gauntlet Dark Legacy  
[![Build Status]][actions]
=============

[Build Status]: https://github.com/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation/actions/workflows/build.yml/badge.svg
[actions]: https://github.com/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation/actions/workflows/build.yml
<!--
decomp.dev progress badges — enable once the project is registered on https://decomp.dev
and CI uploads a progress report.
[Code Progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?mode=shield&measure=code&label=Code
[Data Progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation.svg?mode=shield&measure=data&label=Data
[progress]: https://decomp.dev/sabishii-bit/Gauntlet-Dark-Legacy-Decompilation
-->

A work-in-progress decompilation of **Gauntlet Dark Legacy** for the Nintendo GameCube, built with
[decomp-toolkit](https://github.com/encounter/decomp-toolkit) and the
[dtk-template](https://github.com/encounter/dtk-template) project structure.

This repository does **not** contain any game assets or assembly whatsoever. An existing copy of the game is required.

Supported versions:

- `GUNE5D`: Rev 0 (USA)

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

All other tools (compilers, objdiff-cli, sjiswrap, etc.) are downloaded automatically during the build. See [docs/dependencies.md](docs/dependencies.md) for details.

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
   See [docs/splits.md](docs/splits.md) and [docs/symbols.md](docs/symbols.md).
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
├── docs/               # Documentation for symbols, splits, common BSS, CI, etc.
├── include/            # Header files
├── orig/GUNE5D/        # Original game files (not committed)
├── research/           # Reference material (e.g. Xbox build symbols for struct matching)
├── src/                # Decompiled source code (created as work progresses)
├── tools/              # Build scripts, custom dtk.exe, local compiler copies
├── configure.py        # Project configuration; generates build.ninja and objdiff.json
└── build/GUNE5D/       # Build artifacts (generated, not committed)
```

Continuous integration
======================

Every push is built by [GitHub Actions](.github/workflows/build.yml) inside a private container that
provides the compilers and original game files, then verified against `build.sha1`.
See [docs/github_actions.md](docs/github_actions.md) for how this is set up.

Resources
=========

- [docs/getting_started.md](docs/getting_started.md) — dtk-template workflow overview
- [docs/comment_section.md](docs/comment_section.md), [docs/common_bss.md](docs/common_bss.md) — matching esoterica
- [decomp.me](https://decomp.me) — collaborate on matches online
- [GC/Wii Decompilation Discord](https://discord.gg/hKx3FJJgrV) — ask in `#dtk`
