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
| Rev 1 (USA) | `GUNE5D` | `7cba77aa496eb0fc5ffec60efd9680aa9635d679` |

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

The build is verified against [config/GUNE5D/build.sha1](config/GUNE5D/build.sha1), and also
produces `build/GUNE5D/main.retail.dol`, byte-identical to the retail disc image.

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

### Postprocessing, and how progress is reported

Most objects are compared exactly as the compiler emitted them. A minority
are rewritten first by the project's fail-closed Frank / WebFrank / P6Frank
harness, which is why `configure.py progress` ends with a line of this
shape:

```text
Postprocessor split: STRICT matched NN.NN% (N fns, compiler output byte-identical)
  + EQUIVALENT N.NN% (N fns, WebFrank-assisted: proven equivalent modulo
  regalloc/schedule; origin of the variance unattributed)
```

The two halves mean different things and are not interchangeable:

- **STRICT** — the compiler's own output is byte-identical to the retail
  target. Nothing was rewritten.
- **EQUIVALENT** — the object matches after a postprocessor rule that is
  machine-proven equivalent to the compiler's output modulo register
  allocation or instruction scheduling. The rule closes the residual; it
  does not explain where the variance came from.

`AGENTS.md` requires that both halves always be published together:
"Progress reporting always publishes the STRICT/EQUIVALENT split; never
quote the combined matched% alone in a record or report." A single
"matched %" figure taken from the first `All:` line is the combined
number and must not be quoted on its own.

Three constraints govern the harness itself, quoted from `AGENTS.md`:

- It is "used exactly within the constraints returned by
  `gdlmem.py tool <name>`. Never weaken a guard, add an unaudited rule, or
  use postprocessing to hide structural, operand, relocation-payload, ABI,
  semantic, or data differences."
- **Source-exhaustion provenance.** "A new rule additionally requires
  SOURCE-EXHAUSTION provenance: the function must carry a parked/capped
  attempt record with literal `probed_form` axes (or a law proving its
  residual class source-unreachable), and the rule's attempt record must
  cite it. Mechanical closability alone is not sufficient … Functions with
  no such record get a source-first pass BEFORE any rule."
- **Class ceiling.** "Every postprocessor class must be attributable to
  allocator/scheduler variance under a proven compiler. The relational
  value-equality mode is the outer boundary — no class may cross into 'any
  semantically equivalent stream'. Proposals for new classes go to the
  integrator as records, never shipped unilaterally."

A postprocessed function reads `real 0` by construction, so scores taken
from a pinned function measure the rule rather than the source. Screen
`config/GUNE5D/webfrank.json` before ranking any roster by measured
`real`.

### Matching work queues

Use the low-match queue for semantic and structural reconstruction work:

```sh
python tools/gdl/lowmatch.py --refresh
python tools/gdl/lowmatch.py --sort impact --min-size 200 --parked skip
```

It excludes already linked translation units and, by default, lists functions
at or below 50% fuzzy match. `--sort lowest` emphasizes the least reconstructed
functions; `--sort impact` emphasizes their estimated remaining byte gap. Use
`tools/gdl/nearmiss.py` separately when deliberately closing already high-match
functions. Both queues honor the project's maintained parked-function cap list.

Memory graph MCP server
=======================

[memory_graph/](memory_graph/) is the project's structured knowledge base — verified
compiler behaviors, per-function attempt history, and reviewed tool policies. It ships
with an optional MCP server that exposes its query surface as tools for AI-assisted
workflows. Register it with Claude Code from the repository root:

```sh
claude mcp add gdl-memory -- uv run --project memory_graph/mcp python memory_graph/mcp/server.py
```

It requires [`uv`](https://docs.astral.sh/uv/) and runs no daemon — the host launches
it per session. The same queries are available without an MCP host via
`python memory_graph/gdlmem.py`. See [memory_graph/README.md](memory_graph/README.md)
for the architecture and full usage.

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
