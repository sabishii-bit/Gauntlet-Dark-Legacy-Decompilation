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

This mode selects editable source and bypasses Frank, WebFrank and P6Frank.
It is not yet wholly rewrite-free: the exception-runtime layout fixup still
runs in this mode and requires an editable-build safety audit (details below).

One target-independent ELF visibility fixup runs in both build modes for
`game/anim/atree.c`: GC 1.2.5 needs four cross-TU state objects to retain
internal linkage while compiling in order to reproduce the retail BSS layout,
so the build promotes those existing object symbols (and names the existing
zero literal `sAtreeZero`) after compilation. It does not rewrite code, data,
relocations, or addresses; it only restores the public symbol bindings used by
`pb_diag.c`, and therefore remains compatible with edited/modded source.

To print decompilation progress:

```sh
python configure.py progress
```

### Postprocessing, and how progress is reported

The normal build mixes source-built objects with extracted target objects for
unfinished units. A green DOL checksum verifies that mixed build, not a complete
source reconstruction. Its linked-source coverage is separate from objdiff's
matching score.

Inspect build provenance and fresh diagnostic reports after `ninja`:

```sh
python tools/gdl/build_provenance.py --out build/GUNE5D/build_provenance.json
python tools/gdl/reconstruction_preflight.py --smoke-tools
```

Provenance distinguishes stock/derived compiler output, postprocessor rule
classes (including manual exceptions), and the actual source/fallback link
selection. These dimensions overlap and must not be added as percentages.
GC 1.2.5n and experimental 1.2.5s are derived compilers, even when their object
is linked without a subsequent instruction rewrite. Hashes identify artifacts;
they do not prove source semantics or historical compiler provenance.

The former STRICT/EQUIVALENT progress split overstated what it measured: it
used lenient relocation scoring and subtracted only WebFrank functions, not
P6. Current reporting labels that scope instead of calling the remainder
compiler-output byte identity. A proven postprocessor rule establishes its
declared transformation, not exhaustive failure of every possible source form.

The preflight produces fresh normal and stricter relocation reports without
changing production scoring. Its PASS means the requested diagnostics executed
and their populations/inputs agree, **not** that the source is complete or all
relocations are correct. Demotions remain `UNRESOLVED` pending datum, addend,
width and operand-position review; a datum multiset alone misses transpositions.
Logs and hashes live beside its JSON in a unique generated `build/` directory.

Matching emitted bytes also does not prove that original source has been
recovered. In `world.c`, `StartWorldLoad` and `LoadWorldDone` use the
user-approved (2026-09-04) `WorldNameRef` compatibility wrapper: an ordinary
one-pointer local struct that changes MWCC's register allocation. It is
explicitly **not** a recovered game type. Both functions match without
postprocessing; the TU retains its existing `WorldSaveInitState` rule.
The wrapper does not lock either function to fixed bytes when modders edit
the source. Its compiler regression check is
`python tools/gdl/composed_census/r59_world_name_ref_probe.py`; the complete
linked build, not this instruction-only probe, verifies relocations/data.

`enemy.c` is linked with 25 WebFrank rules. Its final `do_enemy_move` rule
uses the reviewed `address_fold` proof, not a register-mask exemption:
the contiguous `add; addi; lwz` alternatives compute the same load address
(`3608 + 524 = 4132`) and every GPR is equal after the third instruction.
Only this temporary/base forwarding idiom is supported; the entire function
outside that window must already match. Input/target/output hashes,
relocation/datum binding, and control-flow/entry checks still apply.
The proof is for normal completion, not identical intermediate register
snapshots under hardware exceptions or debugging. Regression tests are in
`tools/gdl/tests/test_address_fold.py`; the source-exhaustion and census
records are searchable with `gdlmem.py context do_enemy_move`.

`python configure.py --non-matching` bypasses WebFrank, P6Frank and the Frank
object pipelines, so those rules do not require modders' edits to preserve
their input hashes. The matching build intentionally refuses a changed pinned
body. **This is not yet a wholly rewrite-free editable build:** the existing
`tools/fix_exception_objects.py` is also scheduled in non-matching mode. It
removes a weak runtime function, rewrites string/data layout and relocations,
and mutates the two exception-runtime objects in place. Provenance reports
that separately, not as raw compiler output or mere metadata cleanup; its
editable-build safety needs a dedicated audit. Target-independent atree symbol
export/rename processing also remains enabled.

The user-approved weak square-root helper in
`enemy.c` remains explicitly documented compatibility scaffolding, not a
claim of recovered header provenance.

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
