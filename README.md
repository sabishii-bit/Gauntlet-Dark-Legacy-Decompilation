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

The required original `main.dol` input is:

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

The build is verified against [config/GUNE5D/build.sha1](config/GUNE5D/build.sha1),
which targets the extab-cleaned reference, not the original input hash above.
The former retail-byte-spliced `main.retail.dol` is no longer generated.

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

Agents should read [AGENTS.md](AGENTS.md). Investigate directly from the source,
headers, target disassembly, Git history and tests; coordinate exclusive file/TU
ownership before editing and include reproducible measurements in the handoff.
The [source-structure screen](AGENTS.md#source-structure-screen-before-declaring-compiler-variance)
adapts SMS's program-reconstruction and MWCC guidance into GDL-specific checks.
Use it before diagnosing a postprocessor residual as compiler variance; it does
not authorize foreign flags, artificial helpers or unverified layout changes.

Before submitting a change, rebuild the project, inspect the affected object in
objdiff, and make sure the linked DOL still passes the configured hash check.
Please keep commits focused and avoid mixing unrelated cleanup with decompilation
work.

To build editable versions of the source-linked units (final hash will not match):

```sh
python configure.py --non-matching
ninja
```

Both modes now compile without object postprocessing. This mode does **not**
promote `Object(NonMatching, ...)` units to the link: extracted objects remain
selected. Edits to their source may compile without appearing in the game.
Consult `linked_object`/`linkage` in the provenance manifest. Cross-TU
visibility and complete source linking remain reconstruction obligations.

To print decompilation progress:

```sh
python configure.py progress
```

### Native-only build and honest progress

The user-directed reset (2026-09-07) removes Frank/WebFrank/P6Frank,
atree symbol promotion, exception-runtime layout rewriting, assembler ELF
fixups and post-link retail-byte splicing from the generated build. The
three former JSON configurations are removed; lint policy is a small TOML
file. Legacy analysis modules/tests are retained where useful, not scheduled
as patchers. Historical rules can be inspected at commit `1c9273631`.

Twenty-four formerly source-linked TUs are demoted to NonMatching, including
the two exception-runtime TUs and the TRK assembly TU. Other dependent TUs
were already NonMatching. This is a disclosed loss of claimed source coverage,
not a native closure. Source and compiler settings are retained for repair.
The normal build still mixes source objects with extracted fallback, and its
green checksum verifies that mixed build.

The generator refuses postprocessing in native-only mode. Inspect current
source/fallback selection and compiler provenance after `ninja`:

```sh
python tools/gdl/build_provenance.py --out build/GUNE5D/build_provenance.json
python tools/gdl/reconstruction_preflight.py --smoke-tools
```

GC 1.2.5n and optional 1.2.5s remain **derived compilers**. No postprocessing
does not mean stock-compiler output or recovered original source. Report
scores, actual linked-source coverage and source authenticity are separate.
The existing checksum targets the documented extab-cleaned reference;
reference extraction/normalization remains, but no `main.retail.dol` splice
is generated. Reproducing those original padding bytes natively is still open.

Former source exceptions remain visible debt: the `WorldNameRef` wrapper
in `world.c` and weak square-root helper in `enemy.c` are not recovered
original structures. Both TUs now link extracted fallback until complete
native reconstruction. No new scaffolding is authorized by the reset.

Fable's run-61 split additions cover 14 ranges in 12 TUs (4416 bytes).
Independent checks confirmed non-overlap, seven literal/string byte runs
with zero-only trailing slack, five table-owning function populations and
two BSS extents. Some table branch offsets still differ; BSS extent equality
does not prove fine-grained object identity. The changes are useful ownership
work, not an all-data or all-source matching certificate.

The embedded static-asset payload has a separate extraction hazard: words in
serialized asset data can resemble native addresses, causing DTK to infer
relocations which corrupt the payload when an editable build moves symbols.
The split configuration now suppresses the 58 remaining inferred relocations
in the verified `0x80129734..0x80238290` range. Before this correction, even an
unchanged-source editable build changed 43 payload bytes; after it, the entire
1,108,828-byte range stays exact in both matching and shifted editable builds.
This corrects extraction metadata, not compiled instructions. Verify with:

```sh
python tools/gdl/composed_census/r67_asset_relocation_audit.py --dol build/GUNE5D/main.dol --out build/GUNE5D/asset_audit.json
```

The audit is deliberately limited to that hash-identified payload. It does
not suppress real pointers elsewhere or claim the game has been boot-tested.

### Reconstruction priorities from the R67 investigation

The measured next milestone is **linking every configured source TU**, followed
by exact code, data, relocations and exception metadata. It is not another
fuzzy-score threshold. The ordinary editable build still links extracted
objects for unfinished TUs. To expose the actual source-link failures without
changing production link inputs:

```sh
python configure.py --non-matching
ninja -j2
ninja -j2 all_source
python tools/gdl/composed_census/r67_runtime_allsource_probe.py
python configure.py
ninja -j2
```

The probe first reproduces the normal ELF exactly, then substitutes all
configured source objects in a scratch link. Its `PASS` means the experiment
ran faithfully, not that the trial linked. Initially, replacing 42 unfinished
objects exposed duplicate data definitions and unresolved public symbols,
while retaining 84 explicitly counted automatic data/BSS inputs. The linker
stopped at its diagnostic cap: counts are lower bounds, and removing early
errors can reveal new names without indicating a regression.

The recommended order is:

1. Repair genuine cross-TU visibility/name disagreements and reconcile each
   source datum with its extracted owner. Keep pointer-bearing RTTI, exception
   data and serialized assets distinct. Do not force the link with missing
   function stubs, duplicate-tolerant flags or invented symbol aliases.
2. Reconstruct complete TU context: initialized tables, literal and BSS pools,
   prototypes, data visibility, source order and pragma boundaries. Use target
   bytes and callers as authority; Xbox types are corroboration. Test proposed
   TU merges rather than treating a shared pool address as proof of one TU.
3. Run compiler/flag controls against a byte-identical fresh raw baseline.
   Measure whole-TU effects and pragma overrides, not just the nearest function
   score. A finite failed matrix is not evidence that no source form exists.
4. Repair the formerly postprocessed functions natively under the verified
   whole-TU context. Keep them NonMatching until code, data, relocations and
   EH all pass; do not substitute a new rule or artificial source construct.

Two experiments explain that ordering. Stock GC 1.2.5 emits the exact
372-byte `AudioStreamPlay` instruction body under a diagnostic local wrapper,
restored literal prefix and compensated existing pad. This proves that body
shape is reachable, **not** that the artificial source is acceptable or its
TU is matched; none of that scaffolding was retained. Separately, merging
`sounds_evt` and `sounds` preserves all 151 raw bodies without closing their
residuals. Five independent sound arrays reconstruct 340 data bytes and 32
pointer bindings at retail bases, and normal compiler data pooling can retain
their unreferenced filename table. Missing data context is a demonstrated
lead; the exact original GC grouping and production placement remain open.

Reproduce those bounded experiments with
`r67_audio_context_probe.py --flags`, `r67_audio_effective_string_audit.py`,
and `r67_sound_boundary_probe.py` under `tools/gdl/composed_census/`.
Inspect those scripts, their tests and relevant Git commits for the bounded
controls and scope limits; this overview is not a complete source-recovery proof.

### Native reconstruction policy

The full policy is in [AGENTS.md](AGENTS.md). Build postprocessing is forbidden.
Use source/target evidence, not a percentage gain, to justify data constructs,
declarations, compiler flags and TU boundaries. A failed finite source matrix
does not prove a source form impossible. A matching instruction body is not
proof of correct data ownership, positional pointer bindings or a linked TU.
Keep extracted fallback explicitly reported until all native promotion gates
pass.

### Reconstruction source lint

Install Node.js 22, pnpm 10.25.0 and Python 3.11+ (no game or compiler is needed
for linting). The pinned ast-grep backend supplies structural C++ matching;
the Python reporter applies semantic heuristics and reviewed-exception policy.

```sh
pnpm install --frozen-lockfile
pnpm run test:lint
pnpm run test:lint:integration
pnpm run lint:decomp
# Focus on an owned TU; output must stay under ignored build/.
python .vscode/lint/fakematch_lint.py src/game/movie/movieplayer.cpp --out build/movie-lint.json
```

| Rule | Review candidate |
| --- | --- |
| FM001 | Pointer-cast offset accesses and indexed cast views, including decimal/symbolic offsets |
| FM002 | Nested dereference-through-cast expressions beyond depth one |
| FM003 | Lexically unused local arrays, volatile/trash/padding declarations, and write-only updated locals |
| FM004 | Numeric byte arrays with big-endian float or aligned GameCube-address shapes |
| FM005 | GNU/MWCC assembly, except exact reviewed macro definitions |
| FM006 | Pragmas and recognized function optimization attributes against a per-file/scope allowlist |
| FM007 | Hex expression literals outside named constants/enums and direct bitwise-mask operands |
| FM008 | Reintroduced legacy WebFrank/P6Frank configuration |
| FM009 | Unnamed numeric offsets into visibly declared pointers/arrays, including decimal pool offsets |

The push/PR workflow runs rule tests and a complete `src/` + `include/` scan in
the independent **Reconstruction source lint** job, publishing
`reconstruction_source_lint` with all findings, source hashes and parser-recovery
regions. **Outstanding errors fail CI**, including existing debt. This keeps
the cleanup job red until debt is resolved or legitimate uses are reviewed.
Warnings remain visible without failing the job unless promoted. The independent
DOL build additionally enforces the native-only pipeline.
`pnpm run lint:decomp` includes `--fail-on-findings --postprocessors`. The underlying
Python command remains usable for reporting-only scans without those flags.
Exit 1 means blocking findings; invalid inputs/scanner failures return 2.
Exit 1 still produces a complete report; do not consume an older report after
an exit-2 scanner failure. `--limit` caps only human/GitHub console
output; `--rule FM001` narrows the report deliberately. `pnpm run lint:ast` is
a lower-level four-family diagnostic, **not** the full reconstruction report.

#### VS Code / Cursor errors

After installing the pinned dependencies, open the repository folder and run
**Tasks: Run Task → GDL: lint current file** for a quick check. For ongoing
checks, run **GDL: watch reconstruction debt** and wait for its first scan to
finish. Both populate Problems and inline diagnostic squiggles, not syntax-theme
colors. The watcher refreshes on **saved** source/config changes; unchanged
source snapshots are cached. A command run in an unrelated terminal does not
populate the editor's Problems collection.
For startup on folder open, use **Tasks: Manage Automatic Tasks → Allow Automatic
Tasks**, then reopen the trusted workspace. Automatic execution is controlled
by the editor, not forced by this repository. Stop it with **Tasks: Terminate
Task**. See the [VS Code task documentation](https://code.visualstudio.com/docs/debugtest/tasks#processing-task-output-with-problem-matchers).
No additional editor extension is required. The existing Ninja build task is
unchanged. This is a task-based watcher, not an unsaved-buffer language server.
Restart the task after changing the scanner itself or installing dependencies.

FM008 detects legacy rule files if reintroduced. It does not establish that a
rule fired or that the compiler cannot produce native output, and cannot be
hidden by source exceptions. Full graph enforcement is independently performed
by the native-only build policy, including non-JSON rewrite stages.

FM003 also screens Fable's dead-but-incremented local pattern: a local such as
`timeOffset` whose only uses are standalone assignments/increments. This caught
a real induction-variable reconstruction issue in Critter; it is not a general
dead-store proof, especially with macros, aliasing or unrecovered declarations.

For example, `!(mp->flags & 0x1000)` in `enemy.c` is a legitimate direct mask
and is exempt from FM007. The same literal in `p + 0x1000` or `call(0x1000)`
is still a review candidate. Regression tests cover masks on either side,
parentheses, complements and compound updates without exempting arithmetic
or calls nested inside bitwise expressions.

These are review heuristics, not a fakematch verdict or automatic fixes. A used
array can still be artificial; lexical non-use is not CFG liveness. Float-shaped
bytes may be legitimate assets. Volatile, offsets and masks can be legitimate.
The C++ grammar is used even for `.c`/`.h` because reconstructed MWCC files mix
C and C++; vendor syntax may require parser recovery. Macro bodies receive a
separate syntax scan, but macros/includes are not expanded or type-checked.
Optimization flags in build configuration still require the existing compiler-
provenance audit; this scanner covers source pragmas/attributes, not Ninja flags.

Rules and fixtures live in `.vscode/lint/`. Review approvals live in
`.vscode/lint/fakematch_lint.toml`: `exceptions` entries require an exact report
`fingerprint` plus `reason`. Changed assembly macro bodies invalidate their approval;
direct assembly cannot be exempted by a fingerprint. Reviewed rows stay visible
with their reasons. No existing debt is blanket-approved.

All reported `#pragma` directives are **warnings**, not suppressed exceptions.
They remain visible in CLI, report and editor as compatibility debt, including
`dont_inline`, scheduling and optimization pragmas. `#pragma once` remains
excluded; optimization attributes remain errors. This does not approve new
pragmas or prove original source structure.
To make warnings build-breaking locally or in CI, use:

```sh
pnpm run lint:decomp --warnings-as-errors
```

FM009 flags unnamed decimal or hexadecimal offsets into visibly declared
pointers/arrays, including `MBOX_FindTexture(strings + 364, 0)`. It requires
lexical declaration evidence, so ordinary scalar arithmetic is not classified
as pointer access. Typedef-hidden pointers, member expressions and missing
include/type information remain limitations. The diagnostic requests recovery
of the referenced string, field or element; it does not prescribe a struct or
loop. Actual pool bytes and relocation ownership decide the source repair.

Legacy TOML `warning_pragmas` and `pragma_allowlist` fields are still accepted,
but cannot change pragma severity or hide these warnings.

### Matching work queues

Use the low-match queue for semantic and structural reconstruction work:

```sh
python tools/gdl/lowmatch.py --refresh
python tools/gdl/lowmatch.py --sort impact --min-size 200
```

It excludes already linked translation units and, by default, lists functions
at or below 50% fuzzy match. `--sort lowest` emphasizes the least reconstructed
functions; `--sort impact` emphasizes their estimated remaining byte gap. Use
`tools/gdl/nearmiss.py` separately when deliberately closing already high-match
functions. Queues are discovery aids, not proof that an approach is untried or
a function is free to edit. Check current source, native build provenance,
relevant Git history and active worker ownership, then reproduce the residual
before selecting a target.

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
