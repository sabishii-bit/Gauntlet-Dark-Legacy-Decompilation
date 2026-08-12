# GDL Matching Playbook (canonical, dense — READ THIS FIRST)

This is the single source of truth for matching decomp. The 1000+-line
`matching-techniques-2026-08-01.md` is now a DETAILED ARCHIVE (per-function
narratives). Do NOT read it end-to-end. Come here first; only dive into the
archive for the one function-narrative you need.

Target: MWCC GC/1.2.5 (+1.2.5n), dtk+ninja+objdiff. "matched" = byte-exact.
Build gate before ANY commit: `python configure.py && ninja` → `main.dol: OK`.
Tools live in `tools/gdl/`: fndiff.py (--ops structure / --clean regalloc noise),
fnasm.py (target asm slice), nearmiss.py, matchtool.py (compiler probe),
progress.py, doldiff.py, claimcheck.py, datadiff.py.

===============================================================================
PART 1 — PARK CLASSES  (recognize in ≤1 attempt, then STOP and park)
===============================================================================
If a residual diff is ONLY one of these, it is unfixable at these cflags. Do
NOT spend a second rebuild confirming it. Park it (report class + fn name) and
move on. Two identical A/B rebuilds on the same axis = dead; stop immediately.

  P1  Whole-function register rotation — every GPR/FPR shifted by one, output
      otherwise identical. Allocator seed differs; source-unreachable.
  P2  D-form vs X-form addressing — target `add rX,base,idx; lwz d(rX)` vs our
      `lwzx`/`addi+lwzx` (or reverse). Canonicalization, source-unreachable.
      ALSO: MWCC canonicalizes commutative-add operand order — the add's
      destination/reused-operand register does NOT change if you swap `p[i]` to
      `i[p]`. Don't attempt that rewrite to flip an add-register tie.
  P3  Truncation-CSE — clrlwi / (u16)/(u8) cast reused vs recomputed. The
      compiler's choice; adding/removing casts just moves the diff.
  P4  FPR-lifetime tie — two fp temps swap homes, spill/reload order flips.
  P5  Pre-prologue lis/addi copy, or li/addi zero-copy WITHOUT an xor idiom
      present. (WITH a zero local that can be an xor idiom → see L14, fixable.)
  P6  Goto-pair family — target emits `Bcc lbl; b other` where we emit a single
      branch (or vice-versa). Branch-layout canonicalization. CONFIRMED
      unreachable across ALL presets (demo/sdk/runtime/inline1 x 1.2.5/1.2.5n).
      In BIG functions an insn-COUNT gap can be ENTIRELY verbose goto-pair
      codegen for `if(!cond) stmt;` — NOT missing logic (our -O4 emits an
      inverted single `bne`, and MWCC deletes empty-then if-else rather than
      forming a goto-pair). Do not mistake such a gap for reconstruction work.
  P7  String-pool / literal immediates — order of pooled float/string consts.
  P8  Frame-size spill cascade — off-by-N frame size ripples every stw/lwz
      offset. Almost always a spurious extra local/temp; if you can't remove
      the temp at source, park.
  P9  Pure scheduler tie — same opcodes, adjacent independent insns reordered.

Escalation rule: >50-insn function → read the ASM FIRST (fnasm.py), write C to
match structure, THEN build. Do not iterate blind. HARD CAP 3 attempts on any
scheduler/regalloc-only residual, then park.

===============================================================================
PART 2 — ACTIVE LAWS  (real, source-reachable fixes — each stated ONCE)
===============================================================================

--- Prototype / ABI recovery ---
L1  Mixed GPR/FPR argument order reveals the true prototype. If args land in a
    surprising GPR/FPR interleave, reorder the C params to match; the ABI does
    the rest.
L2  Recover hidden float params from the callee prologue. Extra fp stores at a
    callee's top = undeclared float params; add them.
L3  Audit math prototypes when a correct call gains a stray `frsp`. A double
    return narrowed to float (or vice-versa) inserts frsp — fix the proto.
L4  `li r,0` + `and` (not `andi.`) = a 64-bit / low-half flag test. Model the
    flag as the wide type. andi. needs a POSITIVE literal mask.

--- Register/FPR home control (declaration order is the lever) ---
L5  Declaration order sets register & FPR homes. When homes are off by a
    permutation, reorder local declarations to match target home assignment.
    Split one var into separate "webs" (distinct locals per live-range) when the
    target keeps them in different registers. NUDGE: a decl-initializer
    (`s32 x = g;`) homes `x` one GPR LOWER than a body assignment (`x = g;`
    placed after the pointer/speed setup) — use to shift a single var by one.
    LIMIT: L5 does NOT reach live-range-driven 2-register swaps or whole-function
    rotations (those are P1, park).
L6  Callback/volatile-web ordering: `volatile` roots + decl order reproduce
    inlined-callee stack slots and coalescing decisions.

--- CSE control ---
L7  Struct-displacement view kills address-CSE. Access `g->field` through a
    typed struct pointer instead of `*(T*)(BASE+off)`; the compiler stops
    CSE-ing the folded address constant. (Biggest single exact-flip lever.)
L8  Cast-transit does NOT CSE — routing a value through a cast breaks a CSE the
    target also breaks.
L9  Typed array base: keep a typed `T *base = ...; base[i]` when the target
    colors the base register before the index.
L10 `base + i*stride` beats a named-offset accumulator. Write the address as an
    indexed expression, not `p += stride` — the induction web colors last and
    matches. (See also L12.)

--- Loops / induction ---
L11 do-while pins dual-induction register homes on CONSTANT-bound loops. Convert
    `for` to `do{}while` when the target has no pre-guard.
L12 Full-induction rewrite: express the loop body in terms of `i` and strides,
    not running pointers, so the induction variables color in target order.
L27 lwzu/promote-copy coupling (recognize -> PARK). `x = *(T*)(p += N)` emits a
    fused `lwzu` (single web, no separate addi) ONLY when the post-updated
    pointer flows through a following reassignment (e.g. a no-op self-cast
    `p = (U*)p`). But that reassignment splits `p` into two webs, forcing an `mr`
    promote-copy when p's persistent home is callee-saved. So a target that fuses
    `lwzu` directly INTO a callee-saved home is unreachable -- both source forms
    cost +1 insn. Tell: an extra `mr rCalleeSaved,rTemp` right after an `lwzu`
    with early use of the loaded value -> P8, park.
L28 Inlined-literal const-fold park (a P1 sub-case). A constant held in a
    CALLEE-SAVED register across a call — especially a SINGLE-USE one (tell:
    `mtctr rCalleeSaved` for a constant loop bound; no cost model hoists a
    single-use constant into a callee-saved home) — is a NON-folded inlined
    parameter in the target. At `-inline auto`, MWCC folds literal args to
    immediates and re-materializes (`li`) after each call, so it cannot keep
    them in callee-saved homes. `volatile` on the param does NOT prevent the
    fold (the literal folds before the qualifier matters). Recognize -> PARK.

--- Branch layout ---
L13 One-case switch = `beq/b`. A switch with a single real case emits a compare-
    equal + branch; write it as `switch` not `if` (or vice-versa) to match.
L14 `<1` ⇒ `<=0` compare form. Signed `x < 1` and `x <= 0` emit different
    compares; pick the one the target shows.
L15 De Morgan branch layout — invert a compound condition + swap arms to match
    the target's fallthrough/branch polarity.

--- 64-bit / bit-tricks ---
L16 u64 pair-globals: a 64-bit global split across two words. Low word via the
    `(x & 0) ^ 0` idiom; high word via `bit << 32`. Model as the u64 it is.
L17 bit-trick fabs: `*(u32*)&x &= 0x7FFFFFFF` (not `fabs()`), when target clears
    the sign bit in a GPR.
L18 u16-pointer store emits `sth` with NO `extsh`. Store through a `u16*` so the
    value isn't sign-extended first.
L19 char/u8 local through a `v`-style scratch emits `extsb` — match the signed
    narrow type.
L20 Split a double expression, then use compound assignment (`+=`), to recover a
    two-instruction sequence the target computes in place.

--- Float multiply operand order (the lever AND its two park-traps) ---
L26 Reordering the C operands of a float multiply CAN re-home the FPRs to match.
    Try it FIRST on any float-multiply FPR diff. But two sub-cases are PARKS, not
    flips — recognize them and stop:
    (a) TRAP: the multiplicand is a freshly-converted int (the s16/s32 -> f64
        magic conversion). The conversion pins its own FPRs first; writing the
        constant as the LEFT operand forces it to load before the conversion
        idiom, which rotates EVERY FPR in the block (turns a P4 home-swap into a
        P1 whole-block rotation — strictly worse). The just-converted value's
        home is unreachable via source operand order → park.
    (b) TRAP: result-register coalescing. MWCC lands the product in whichever
        operand is a dead temp / the eventual store (result) register. A
        two-constant home-swap where one constant coalesces with the final
        result register (`fmul fRES,fX,fRES` vs `fmul fRES,fRES,fX`) is a P4 tie
        — the accumulator's final home is fixed by the store, so no source
        operand order reaches it → park.

--- Inlining control ---
L21 Inlined-static device: a `static` helper defined BEFORE its caller inlines;
    this cracks bne/b splits and seeds zero-copies. Force with the definition
    order; block with `#pragma dont_inline`.
L22 Selective manual-inline: when the compiler won't inline a tiny helper the
    target inlined, paste the body at the call site.
L23 The inliner runs POST-gcse — inlined helper bodies are NOT re-CSE'd against
    the caller. Expect duplicate loads across an inline boundary; don't "fix"
    them.
L24 Explicit zero-initialization recovers a retail data-backed table (static
    array credited by address). Zero-init the local array rather than leaving it
    uninitialized.
L25 Treat an apparent void helper as unfinished until EVERY exit path is decoded
    — a missing early-return/guard is the most common near-miss. Re-sweep for a
    missing condition before declaring a regalloc wall.

===============================================================================
PART 3 — WORKFLOW & CANDIDATE SELECTION
===============================================================================
STATE (2026-08, verified): matching-POLISH is exhausted. Cycle 10 = 61
near-miss functions attempted, 0 exacts. Do NOT re-run "find the highest-fuzzy
near-miss and nudge it" — that engine is dead. The remaining matched% is behind
ASM-FIRST FULL RECONSTRUCTION of large, low-fuzzy functions.

Reconstruction loop (per target):
  1. fnasm.py <fn> → read the FULL target asm. Map prologue frame, every branch,
     every call, loop backedges.
  2. Pull structure hints: Ghidra decompile (names garbled by _savefpr — use for
     control flow, not literal C), Xbox PDB (tools/gdl — REAL names + TU source
     order), melee/ references for SDK/library TUs.
  3. Write portable C that mirrors the control-flow graph. Apply PART 2 laws by
     recognition, not by trial.
  4. Build; fndiff --ops for STRUCTURE first (ignore regalloc noise), then
     --clean. Converge structure before chasing register homes.
  5. If residual ∈ PART 1 → park (report class). Else apply the matching law.
  6. Commit ONLY when objdiff says byte-exact AND `main.dol: OK`. One-line msg
     ("Match FnName"). No attribution trailer.

Reconstruction target pool (large, low-fuzzy, high-value — pick disjoint TUs):
  joyReadPad, ControlsUpdate, ReadControls, msgPost, InitWorldInfo,
  DoWorldAnimSub, do_enemies, PlayerMotion, get_player_pos, the Critter 50–60%
  cluster, camera supervisors (newcam.c), MBOX_* loaders / MBTreeInit,
  PlayVQMovie / sDrawGeom (movieplayer), show_optmenu / DoOptions.

Genuine correctness bugs (not codegen) are still worth flipping when spotted:
wrong global (gGameBusy vs options_state, gTextWorkBuf vs gTextFormatBuf),
X*4 vs X*16 index scale, missing switch case, mis-wired call args.

Rules that never change:
  - Portable C/C++ only. Tiny `asm { b label }` branch devices are OK. NO large
    inline-asm bodies, NO embedded machine code, NO binary inclusion, NO
    percentage-gaming.
  - Never edit configure.py to reclassify NonMatching as Matching.
  - Fuzzy-% improvement is NOT proof of a match. Only byte-exact counts.
  - Never overwrite an unrelated dirty file; check `git status` first.

===============================================================================
PART 4 — MEMORY PROTOCOL  (how workers report; how redundancy is prevented)
===============================================================================
Workers DO NOT append to the shared law files. (Worktree appends get lost when
the tree is pruned, and parallel appends create duplicates.) Instead:

  * At the end of your run, in your FINAL MESSAGE, include a "NEW LAWS" section:
    each genuinely new, generalizable law in ONE line (recognition tell + fix).
    Only report laws NOT already in PART 1/PART 2 above. The primary dedupes and
    adds them here canonically.
  * Report parked functions as: `PARK <fn> — <P#> <one-line reason>`.
  * Report exacts as: `EXACT <fn> — committed <hash>` (only if built green).

Worktree .git repair (if git errors with `gitdir: /w/...`): fix via BASH, not
the Edit tool — `echo "gitdir: W:/Repositories/Gauntlet-Dark-Legacy-Decompilation/.git/worktrees/<name>" > .git`
(Bash is permission-allowed; the Edit tool prompts on .git paths.)
