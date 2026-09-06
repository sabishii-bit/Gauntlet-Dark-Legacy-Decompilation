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

To build editable versions of the source-linked units (final hash will not match):

```sh
python configure.py --non-matching
ninja
```

This mode bypasses Frank, WebFrank, P6Frank and the retail-layout exception
runtime fixups. It does **not** promote `Object(NonMatching, ...)` units to the
link: their extracted objects remain selected. An edit in one of those source
files can compile successfully without appearing in the game. Consult the
provenance manifest's `linked_object`/`linkage`, not just a successful build.

One target-independent ELF visibility fixup runs in both build modes for
`game/anim/atree.c`: the current reconstruction compiles `atree_handles`,
`atree_scroll`, and `whichatree` with internal linkage, then promotes their
existing symbols for the cross-TU interface. `natreelists` and `sAtreeZero`
are now defined publicly in source; no anonymous zero literal is renamed.
The remaining fixup does not rewrite code, data, relocations, or addresses.
Its source-export checks permit edited values and layouts; an editable-build
test verified that changing `sAtreeZero` to `1.0f` reaches the linked binary.

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

To adjudicate the source-linked subset against actual linked function bytes:

```sh
python tools/gdl/composed_census/r67_linked_shadow_audit.py --preflight build/GUNE5D/reconstruction_preflight.json
```

This checks complete function bytes in the ELF, built DOL and retail DOL at the
target address and size. It does not clear the unlinked subset or certify raw
compiler output. In the initial audit, all 17 linked demotions were exact after
linking; the other 359 remained unresolved.

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

`python configure.py --non-matching` bypasses the target-bound object pipelines,
so their rules do not require modders' edits to preserve input hashes. The
matching build intentionally refuses changed pinned bodies. Its exception
runtime compatibility stage removes a weak function and rewrites string/data
layout, relocations and exception records; it is not merely metadata cleanup
or a proven historical compiler requirement. Both raw runtime objects are now
retained under `.postprocess/body/`, and separate `fix_exception_object` edges
produce the hash-guarded matching objects without modifying their inputs.

The prior in-place fixup could silently replace an edited `exception::what()`
string with the retail literal. It is now disabled in editable builds. A real
source edit to `"MODIFIED!"` survived into the resolved returned string in both
the linked ELF and DOL; switching that edited source to matching mode refused,
and restoring the source returned the matching DOL to its verified checksum.
`tools/gdl/composed_census/r67_runtime_verify.py --mode matching` checks the
retained raw/fixed boundary. With the deliberate source edit and an editable
build, use `--mode editable --expect-string MODIFIED!` instead. These are
compile/link tests, not console boot or gameplay tests. Target-independent
atree symbol export/rename processing remains enabled in both modes.

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

The user-approved weak square-root helper in
`enemy.c` remains explicitly documented compatibility scaffolding, not a
claim of recovered header provenance.

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
4. Revisit guarded postprocessor cases only after those obligations are
   controlled. Keep raw compiler results, transformed matching results and
   editable-build behavior separately visible. A proven transformation does
   not prove the transformation is necessary.

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
Detailed findings, negative controls, scope limits and next hypotheses are
structured memory-graph records, not this overview.

### Postprocessor policy boundaries

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
