---
name: tu-matching-loop
description: "Per-TU loop, fully tooled: matchtool probe (both compilers, OK~ scoring) -> fndiff/--ops -> finish_tu.py green-gated commit; doldiff/claimcheck diagnostics; never hand-roll flip+ninja+commit"
metadata: 
  node_type: memory
  type: project
  originSessionId: 41924897-2313-4e94-a919-a79fc3e516c7
  modified: 2026-07-24T20:04:50.125Z
---

The matching loop used for every TU, fully tooled as of 5df93df (2026-07-24):

1. Write the TU (reference source + Ghidra decompiles), wire configure.py/splits.txt/symbols.txt, `python configure.py && ninja build/GUNE5D/src/<unit>.o`.
2. **`python tools/gdl/matchtool.py probe <unit>`** — now compiles every preset under BOTH proven compilers (GC/1.2.5n and GC/1.2.5, rows `preset@ver`) and scores `OK` / `OK~` / n / `L±n`. **`OK~` = instructions+addends identical, only reloc symbol NAMES differ (dtk lbl/jumptable vs our real names) — treat as matched, sha1 arbitrates.** One probe table now answers both the flag family AND the compiler (would have saved the whole inftrees 1.2.5 hunt).
3. **`python tools/gdl/fndiff.py <unit>.c [Fn ...]`** for the residual lines; **`--ops` flag** prints the opcode-cluster view (SequenceMatcher on mnemonics only) — use it FIRST on big fns to separate structural deltas (missing statements, moved blocks, extra calls) from register-renumber noise; it also prints "opcode streams identical" when a diff is reloc-name-only. Read residuals against [[mwcc-codegen-tells]] before regalloc theories.
4. Fix source shape, goto 2. When clean: **`python tools/gdl/finish_tu.py <unit>.c -m "msg"`** — runs claimcheck, flips Object to Matching, configure, ninja with the exit code checked directly (deletes the stale `ok` file first), and commits ONLY if green. Never hand-roll flip+ninja+commit: piping ninja hides its exit code and `build/GUNE5D/ok` survives failed runs — that combination produced two red commits on 2026-07-24.
5. Diagnostics when red: **`python tools/gdl/doldiff.py`** (maps diffs file→VA→symbol/unit, decodes header/section-size shifts, suppresses the expected 2-byte clean_extab diff) and **`python tools/gdl/claimcheck.py --matching`** (hard-errors only on the real killer: an emitted section with NO splits claim, e.g. a switch jumptable in .data; size mismatches are advisory because mwld dead-strips fns AND statics).

Flip-to-Matching checklist (learned on vi.c): (a) verify literal DATA bytes vs the DOL before flipping — dump our object's section and compare against the DOL section table (GDL tweaks melee tables: vi timing[3] PAL_DS 35→33/626→624); reloc'd words (jumptables) legitimately differ in the object. (b) `dtk elf info build/.../src/<unit>.o` gives exact section sizes/symbol offsets to carve splits.txt extents — don't guess string-blob ends. (c) If the link then fails with `undefined: name_80XXXXXX referenced from lbl_YYYYYYYY in auto_*`, it's a dtk address-scan FALSE POSITIVE: a packed-data blob contains a word that looks like a pointer into your newly-local data. Scan the DOL for BE words in the claimed range to confirm no genuine pointer, then add `noreloc` to the REFERENCING blob's symbols.txt entry.

**Why:** raw `diff` of objdump output cascades hundreds of lines from one shifted instruction; the normalization makes single-instruction tells (cmpwi vs cmplwi, mr. fusion, missing unroll block, frame-size delta) directly readable.

**How to apply:** don't re-type heredoc parsers — use the tool. Ad-hoc experiment files (test compiles of loop variants etc.) go in the session scratchpad dir, not an in-repo `sc/` folder (used sc/ this session and had to clean it out of two commits).
## Token-lean loop (2164e4c, after the pb_window/odenotstub retro)
- **Read target asm with `tools/gdl/fnasm.py <unit> <fn> [i:j]`** — one line/insn, relocs folded inline as `@sym(RELOC)`, branch targets as `->off` function-relative (adjacency visible; prevents the bge-to-next misread class). `fnasm.py <unit>` lists fns. Reads the dtk obj, never stale.
- **Score iterations with `fndiff <unit> --count`** — one line per DIFF fn: `insns T/B  lines N  real M` (real = excluding reloc-name noise). `real 0` = reloc-only = effectively OK, just rename symbols. Replaces the grep -c pipelines. fndiff now AUTO-REBUILDS via ninja when source is newer than the base obj (--no-build to skip) — stale-object trap closed.
- **Probe with `--brief`** — best row + non-OK cells only (2 lines vs 32). Full matrix only at TU start.
- Keep edit-script anchors SHORT (2-3 unique lines), not 30-line block reproductions.
- Scout BEFORE writing: melee tree (`find W:/Repositories/melee -iname "*<name>*"`) + PDB names. Reference source = 10x speed (odenotstub: 14/14 fns in <1h).
- Backlog (unbuilt, add if pain recurs): claimhelp.py (section bounds + neighbor symbols -> suggested split line; the .sdata-end 80344159 red-build class), finish_tu batch mode (multi-unit one commit; note: interleaved single runs mis-sequence commits — 2nd 'git commit failed' harmlessly when a prior run swept its flip).

**datadiff.py added (f4d78b2)**: pre-link byte compare of emitted .rodata/.data/.sdata/.sdata2 vs DOL at claimed ranges (reloc'd words skipped+counted). Run on any TU with nontrivial data BEFORE finish_tu — catches wrong constant VALUES (fndiff normalizes them away; mathfunc's 1e-14 epsilon lesson) and pool/table emission ORDER. `--matching` sweeps everything (validated: all green TUs pass byte-exact). fnasm now falls back to auto_03_*/auto_* object names for dtk-merged stub runs.

**finish_tu hardened (18ef8ff):** (1) datadiff now runs in the pre-flip gate (wrong data bytes block the flip); (2) a RED build rolls the Matching flips back out of configure.py — a failed finish_tu used to leave the flip behind, and a later plain `git add -A` committed a red-linking configure (g3dMath3D incident: 4 commits carried it, caught only by the next full build). Corollary: after any red finish_tu, or before trusting a plain commit, run `finish_tu --verify`.

**Heredoc corruption trap (cost a full debug cycle):** writing tool source via `python - <<'EOF'` heredocs interprets escapes in the OUTER python string literals — a `` intended as regex word-boundary became a literal backspace byte (0x08) in the written file; `file` reports it as 'with overstriking', the regex silently never matches, and the IDE-open buffer can fight shell rewrites. Rule: write/patch tool source with the Write/Edit TOOLS, never via heredoc python, whenever the content contains backslash escapes. fnsurvey --callers = whole-DOL reverse call index (e.g. fn_800D23A0 has exactly 2 callers: cheat stepper + sound-test screen fn_800D4BF4).

## Late-loop source controls (2026-07-31)

- A mixed `f64`/`f32` expression can be pinned by separating declaration order
  from assignment order.  `write_stage_info` became 114/114 with real 0 by
  declaring the double first, declaring the float second, then loading the
  float before the double.  MWCC used declaration order for the FPR homes and
  assignment order for the retail load schedule.
- Function-local `#pragma opt_propagation off/reset` is worth testing before
  rewriting an integer expression tree.  It reduced `msgWidth` from 12 real
  lines to one commutative `add` operand-order mismatch while preserving all
  80 instructions; leave the last operand-order tie parked unless another
  source lifetime naturally breaks it.
- Do not turn a C/inline-asm function in a linked TU into a function-level
  `asm` body solely to repair relocations.  `G3DReadControlPadStates` became an
  exact 48/48 object match that way, but lost its compiler-generated exception
  metadata and shifted the downstream extab tables (800+ DOL diff runs).  Its
  existing hard-coded-address form is link-byte-correct; `doldiff` is the final
  arbiter, so the object-only "win" was reverted.
