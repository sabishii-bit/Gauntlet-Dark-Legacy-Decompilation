**Title:** `Promote game/world/hiscore.c to Matching`

---

`game/world/hiscore.c` was already byte-exact and already linking from source, but was still
flagged `Object(NonMatching, …)` in `configure.py`. This flips that one word. No file under
`src/` is touched, and no reconstruction was done — the diff is one line.

### Why it was already exact

From `build/GUNE5D/report.json` before the flip:

- `game/world/hiscore.c` — **100.00% fuzzy, 3/3 functions matched, 0-byte estimated gap**
- sections: `.text 0x5a8`, `.data 0x60`, `.sdata 0x10`, `.sdata2 0xa2`, `extab 0x10`, `extabindex 0x18`
- `build_edges.json` confirms `build/GUNE5D/src/game/world/hiscore.o` is built from
  `src/game/world/hiscore.c` — the source object, not an extracted fallback

### Gates run before and after the flip

| Gate | Result |
| --- | --- |
| `claimcheck.py game/world/hiscore.c` | `ok` — all six sections accounted for |
| `datadiff.py game/world/hiscore.c` | `.data` OK (0x60, 20 reloc words skipped), `.sdata` OK (0x10); `.sdata2` 0x6 zero-filled claim slack, reported structural and advisory, not a flip blocker |
| `textorder.py game/world/hiscore.c` | `ORDER-OK` — 3 shared `.text` functions, 0 target-only, 0 ours-only |
| `rm build/GUNE5D/ok && ninja -j2` | exit 0, `build/GUNE5D/main.dol: OK` |
| `sha1sum -c config/GUNE5D/build.sha1` | exit 0 |
| `ninja all_source` | exit 0, 0 FAILED |
| `git diff --check HEAD` / `--cached --check` | exit 0 |
| `python -m unittest discover tools/gdl/tests -b` | Ran 3830 tests, OK (skipped=37), exit 0 |

Result: Game Code linked **52 → 53 of 111 files** (11.54% → 11.71%); project
`complete_units` **206 → 207 of 401**. Whole-project fuzzy and matched percentages are
unchanged, as expected for a state flip.

### Gates NOT run — not claimed as passing

- `fndiff` / `real` instruction-word comparison. There was no residual to measure; closeness
  came from `report.json` fuzzy + `matched_functions`, not a word-level diff.
- `tools/gdl/reconstruction_preflight.py`.
- The Rust `fakematch-lint` source scan — no `cargo` in the build environment. No source
  changed, so its input is unchanged, but it was not run.
- `--experimental-p6-compiler` was not used, so `gamemain.c` and `registry.c` were measured in
  their fallback state.

### Reproduction environment

Built on Linux with the CodeWarrior compilers under **wibo 1.0.3**, compilers archive tag
`20251118`, dtk `v1.8.3`, ninja `1.13.2`, configured as
`python3 configure.py -v GUNE5D --compilers build/compilers`. `GC/1.2.5` and `GC/1.2.5n` hash
to the pins in `tools/gdl/mwcc_p6/README.md` (`0443b5c0…`, `ccf4b465…`).

One environment-only workaround: `tools/gdl/claimcheck.py` hardcodes
`build/binutils/powerpc-eabi-objdump.exe`, and the Linux binutils build has no `.exe` suffix,
so a symlink was created inside the ignored `build/binutils` directory. No tool source was
edited. (`datadiff.py` already exposes an `--objdump` option that would make this unnecessary.)

### CI expectation

The `build` job runs inside `ghcr.io/${{ github.repository_owner }}/gdl-build:main`, a private
container. A pull request from a fork receives no secrets, so that container pull is expected
to fail here — the gate results above are the local substitute for what CI cannot run on a
fork. Happy to re-run anything specific on request.

### Provenance

Produced with Claude (Anthropic) working under `AGENTS.md`. The commit's author field reflects
that; committer is the submitter. No attribution trailers in the commit message, per
`AGENTS.md` § "Git, workers and cleanup".

### Incidental findings, not fixed here

Three portability issues in the repo's own tooling, left alone to keep this diff to one line:

1. `tools/download_tool.py` verifies neither size nor hash after writing. It silently truncated
   `dtk-linux-x86_64` to 9,075,662 of 9,107,048 bytes; because the ELF header still pointed
   past EOF, every `dtk` invocation — `--version` included — segfaulted, which presents as a
   corrupt binary rather than a bad download. A truncated ~1 GB compiler archive would fail far
   less legibly.
2. `tools/gdl/claimcheck.py`'s hardcoded `.exe` suffix, described above.
3. `tools/gdl/finish_tu.py` has no argument passthrough, so its step 3 runs a bare
   `python configure.py`; on a setup that needs `--compilers`, the build goes red and the tool
   correctly rolls its own flip back. Its three gates work standalone.
