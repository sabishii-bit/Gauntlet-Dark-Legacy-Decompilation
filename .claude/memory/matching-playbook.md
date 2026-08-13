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
    ALSO fixes a MIS-HOMED PARAMETER: an address-CSE of `*(T*)((u8*)obj+off)`
    touched 3+ times across calls consumes a callee-saved reg, pushing the first
    param off its target home (e.g. r30); carving `obj->field` frees it.
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
L49 Jump-table `switch` case bodies are emitted in SOURCE DECLARATION ORDER, not
    case-value order (the jump table itself adjusts automatically). To match a
    jumbled target block layout, reorder the source `case` labels to the
    target's PHYSICAL body sequence — dump the jump-table data object
    (`build/GUNE5D/asm/auto_*_data.s`) to read the exact case->block map,
    shared-case groups, and compiler-filled gaps. (Independently confirmed on
    CritterCopyAnim + InitWorldInfo; the single biggest structural lever on
    switch-heavy functions.) Order the source cases by ASCENDING target BLOCK
    ADDRESS (from the jump table), not by case value — wrong order = whole-case
    block-position diffs.
L14 `<1` ⇒ `<=0` compare form. Signed `x < 1` and `x <= 0` emit different
    compares; pick the one the target shows.
L15 De Morgan branch layout — invert a compound condition + swap arms to match
    the target's fallthrough/branch polarity.

--- 64-bit / bit-tricks ---
L16 u64 pair-globals: a 64-bit global split across two words. Low word via the
    `(x & 0) ^ 0` idiom; high word via `bit << 32`. Model as the u64 it is.
    GDL SPECIFIC: `gControllerButtons` is a u64 whose LOW word aliases `sFlags`;
    `(gControllerButtons & 0x80) != 0` compiles to a convoluted 8-insn block
    (`li hi,0; and; li lo,0x80; and; xor,_0; xor,_0; or.`) — recognize it as a
    plain `(u64 & imm) != 0`, not two separate flag tests.
L17 bit-trick fabs: `*(u32*)&x &= 0x7FFFFFFF` (not `fabs()`), when target clears
    the sign bit in a GPR.
L18 u16-pointer store emits `sth` with NO `extsh`. Store through a `u16*` so the
    value isn't sign-extended first.
L50 Byte-swap-through-memory helper: `return b[0] | (b[1]<<8)` vs
    `(b[1]<<8) | b[0]` flips the `rlwimi` dest/source register tie by commutative
    operand order — the LOW-byte operand written FIRST becomes the rlwimi
    destination/result register. High-value when the helper is INLINED at many
    sites (one reorder erased 408 diff lines in InitWorldInfo + improved
    DoWorldAnimSub).
L51 Over-caching a memory field in a C local forces a callee-saved `mr` where the
    target RELOADS (`lwz`) the field per call-arg. Cache a field across ONLY the
    one call that needs it; otherwise access `p->field` directly each use so MWCC
    reloads like the target.
L52 `.sdata2` read-only data needs `extern const T x[N];` for EMB_SDA21
    addressing — a non-const sized `T x[N]` still emits absolute lis/addi. The
    `const` (not just the size) is the real sdata2/rodata-split lever (split at
    ~8 bytes). Applies to strings and const tables alike.
L53 Early-exit-as-else branch layout: write `if(!cond){main}else{exit}`, NOT
    `if(cond){exit;break;} main`. MWCC then keeps the main path as fall-through
    and branches to the later-placed exit, matching the target's bge/blt-to-exit
    (companion to L15).
L54 Cache a cross-call global array/const ADDRESS into a local pointer
    (`T* p = gGlobal;`) to collapse a whole-function GPR permutation (GPR
    analogue of L36). When a global's address is used before AND after a call,
    MWCC re-materializes it and the register pressure pushes params/locals off
    their target callee-saved homes; binding it to a local gives it a stable
    callee-saved reg and the whole coloring falls into place (turned
    CritterInitGeo 318->103 in one edit).
L55 A constant (e.g. `-1`) that inits several fields AND later seeds a cross-call
    accumulator should be ONE local, reused: `mt=-1; c->a=mt; c->b=mt; ...;
    mt=Find();` reproduces the target's single `li rN,-1` kept in a callee-saved
    reg for both the stores and the accumulator. Separate literals split it into
    two materializations. (Only effective once the owning object is already in
    its correct register.)
L19 char/u8 local through a `v`-style scratch emits `extsb` — match the signed
    narrow type.
L20 Split a double expression, then use compound assignment (`+=`), to recover a
    two-instruction sequence the target computes in place.

--- Float/int conversion & frame codegen (reconstruction laws) ---
L29 Assignment-in-condition loads straight into the callee-saved home. Target
    does `lwz rHome; cmpwi rHome`, but `p = load; if (p==NULL)` emits
    `lwz r0; cmpwi r0; mr rHome`. Rewrite as `if ((p = load) == NULL)` so MWCC
    loads directly into p's home — removes the mr AND the +1-insn branch-offset
    ripple it causes. (Worth trying whenever a load feeds an immediate null/zero
    test into a value that persists.)
L30 `(s16)floatval` = `fctiwz` + read-back + `extsh`; `(s32)floatval` OMITS the
    `extsh`. A stray `extsh` after a float->int conversion means the cast target
    is s16, not s32 (int analogue of L19).
L31 Oversized scratch arrays inflate the frame. Size a scratch array to EXACTLY
    max-index-used+1 (`xf[11]` not `xf[16]`); extra elements push MWCC's
    fctiwz/magic-conversion double temp to a higher slot, rippling frame size and
    every r1-offset (a self-inflicted P8). Trim to kill a frame-size cascade.
L32 `s32` vs `u32` for a `f & MASK` flag local controls `cmpwi` vs `cmplwi` on a
    later `if (flag)` re-test. Make the saved-flag local SIGNED to reuse the
    register with signed `cmpwi`; unsigned gives `cmplwi`.
L33 A DOUBLE-literal multiplier forces FMA+frsp. `K * dt` with K a double literal
    (`30.0`) emits `fnmsub/fmadd` then `frsp`; a float literal (`30.0f`) emits
    the single-precision `*s` form with no frsp. Pick the literal type the target
    shows (companion to L3).
L34 Single-bit-test boolean form. `!(x & bit)` materialized as a bool emits
    `clrlwi/rlwinm; cntlzw; srwi` (NO neg); `(x & bit) == 0` emits a REDUNDANT
    `neg` first. For a materialized boolean of a single-bit test, write the
    `!(...)` form. (High-value lever; flipped CritterActivate.)
L35 Int->float magic-conversion width follows the CAST TARGET: `(f32)(s32)x`
    ends the conversion in `fsubs` (single); `(f64)(s32)x` ends in `fsub`
    (double). Match the target's fsubs/fsub (companion to L30/L33).
L36 Loop-invariant f64 const hoisted into a SAVED FPR: copy it into a local
    BEFORE the loop (`f64 k = lbl_...;`) and use the local inside; MWCC then
    keeps it in a saved reg instead of reloading `lfd` every iteration.
L37 5-arg direct call to a 6-param function (K&R/mismatched-proto passthrough,
    no r8 set in asm) is NOT reproducible via a function-pointer cast (that
    emits an indirect `blrl`). Passing the real 6th arg keeps a direct `bl` at
    the cost of one extra `li` -> treat that `li` as a park UNLESS a caller-side
    5-param declaration is introduced.
L38 Merge-reuse local. A struct-field value loaded for a null/zero check is
    reused in the fall-through branch but RE-READ in a merge-reached (else)
    branch of a short-circuit `&&`. Bind it to an explicit local (`T h = p->f;`)
    to force ONE load reused across the merge — removes the reload and its
    branch-offset ripple.
L39 Double sentinel in an f32 COMPARISON: an `f32` local compared against a large
    sentinel emits `lfd` (double) when the literal is written `2000000.0`, `lfs`
    (single) when `2000000.0f`. Pick the width the target's lfd/lfs shows
    (comparison companion to L33, which covers multiplies).
L40 Signed `cmpw` for flag-EQUALITY: `(u32field == u32mask)` emits `cmplw`; cast
    BOTH sides to `s32` to get the target's signed `cmpw` on an equality of
    bit-flag values (companion to L32, which covers a re-tested saved flag).
L41 Leftover-float mismatched call (recognize -> often PARK the residual `li`).
    A helper with float params can be called from a TU-local caller with FEWER
    args (floats omitted) — the callee reads the caller's already-computed values
    left in f1/f2. A 6-param decl with the float params omitted can still set up
    the right registers for a 7-param callee when the declared pointer params
    land the GPR args correctly (proven by CritterCollidePlayers). COROLLARY: you
    cannot fix such a callee's declaration without breaking EVERY caller that
    depends on the leftover-float form — treat cross-fn signature changes here as
    coupled/park unless you reconstruct all callers together.
L42 `a ? a : NULL` emits a redundant `load; cmplwi; bne; li 0`. When the target
    shows a load followed by a null-test that stores 0 in the null case (a
    semantic no-op), the source is a self-ternary `x ? x : NULL`, NOT a plain
    assignment (which the optimizer collapses). Write the self-ternary.
L43 Clamp/compute in a scalar LOCAL, then assign the global ONCE. `g = expr;
    if(g<lo)g=lo; else if(g>hi)g=hi;` emits a `stw` to the global inside EACH
    clamp branch; instead `t=g; if(t<lo)t=lo; ...; g=t;` clamps in a register and
    stores once at the merge — matches a target with a single trailing `stw`.
L44 Frame-GROW complement to L31. When the target's fctiwz/stfd conversion temps
    sit at a HIGHER r1 offset than yours (fndiff "frame delta +N"), add an
    unreferenced `u8 unused[N];` local; MWCC reserves it and shifts the
    conversion slots UP to match. (L31 trims; L44 grows.)
L45 Typed-row-pointer forces the D-form indexed store (concrete P2-avoidance).
    `T* row=(T*)(base+i*stride); row[k]` emits `add row,base,idx; stw
    (k*sizeof)(row)`; the folded `*(T*)(base+i*stride+k*sizeof)` emits the
    `addi; stwx` X-form. Use the typed-row form when the target shows D-form.
L46 Shared set-flag block => short-circuit `||` (inverse of L25). When a switch/if
    arm sets `x |= bit` from TWO paths and the target shows ONE `ori` reached by
    both a `beq` (fast-guard) and a fall-through (computed condition), write
    `if (guard || computed) x |= bit;`. Two separate `if{x|=bit}` emit two `ori`.
    (L25 splits an `||` when the target DUPLICATES the block; L46 merges when it
    SHARES the block.)
L47 Embed statements in a `||` operand via an inlinable static helper. To put an
    inner `if`+local (e.g. a manual fmod) inside a `||` second operand that must
    be skipped when the first operand is true, factor it into a `static` helper
    returning the value; at `-O4 -inline auto` it inlines (leaving a deadstripped
    standalone = fndiff BASE_ONLY) and reuses ONE int<->float conversion slot.
L48 Float `<=` compare-form is a NaN-semantics tell. `a <= b` emits
    `fcmpo; cror eq,lt,eq; b(n)e` (IEEE-strict, NaN->false); `!(a > b)` emits the
    sloppy not-gt `bgt`/`ble` (NaN->true). Pick per the target's cror-vs-ble.
    (The accompanying `ble->then; b->skip` goto-pair layout itself stays P6.)

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
    order; block with `#pragma dont_inline`. NOTE: to STOP an in-TU global callee
    from auto-inlining, wrap the CALLEE's DEFINITION in `#pragma dont_inline
    on/off` — `#pragma auto_inline off` around the CALLER does NOT work.
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

PRIMARY MERGE DISCIPLINE (mandatory): after cherry-picking ANY worker wave,
rebuild and compare matched% against the pre-wave value AND spot-check the TU's
previously-exact functions. Workers reporting "clean/only P-class residuals"
have silently (a) shrunk OTHER functions' `unused[N]` pads and (b) regressed an
exact fn from a header/field change. A wave that drops matched% must be fixed
(restore the broken fn) or the offending commit reverted before pushing.

WORKTREE EDIT HAZARD (bit 2 workers, wasted build cycles + risked main): a
worktree-isolated agent's Edit/Read tools resolve an ABSOLUTE
`W:/Repositories/Gauntlet-Dark-Legacy-Decompilation/src/...` path to the MAIN
checkout, NOT your worktree — silently editing main while your worktree file
stays stale (ninja keeps compiling the old code; `--ops` hides the frame/reg
drift that would reveal it). FIX: edit via Bash/Python on WORKTREE-RELATIVE
paths (your cwd is the worktree), OR pass the FULL
`.claude/worktrees/<name>/src/...` path to Edit. Sanity-check with `git status`
in BOTH trees before committing.

Worktree .git repair (if git errors with `gitdir: /w/...`): fix via BASH, not
the Edit tool — `echo "gitdir: W:/Repositories/Gauntlet-Dark-Legacy-Decompilation/.git/worktrees/<name>" > .git`
(Bash is permission-allowed; the Edit tool prompts on .git paths.)
