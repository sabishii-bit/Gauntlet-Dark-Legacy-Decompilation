# MWCC matching recipes (GDL, GC/1.2.5 + 1.2.5n)

The distilled first-pass checklist for turning a near-miss into byte-exact.
Apply top-to-bottom; most 90-99% functions fall to one of these. Keep this
file authoritative — worker prompts cite it instead of restating recipes.

## The loop

1. `python tools/gdl/nearmiss.py` — pick targets (closest-first).
2. `python tools/gdl/fndiff.py <unit> <fn> --clean` — noise-free diff + hints.
   Trust the `==` summary line; never pipe through grep to "count" diffs.
3. Apply recipes below; rebuild happens automatically (stale objects are
   guarded — if ninja fails, fndiff says so loudly).
4. HARD CAP: 3 attempts on any register/schedule-only residual, then add the
   fn to `research/PARKED.txt` with a one-line class note and move on.
5. Ghidra MCP (`decompile_function` on the GC DOL) is the fastest semantic
   source for missing bodies — transcribe its structure, then match-polish.
   Its casts and odd expressions are often LITERAL source (uninitialized
   returns show as `q >> 32` garbage; casts survive from the original).

## Frame / stack

- **Frame delta N** (fndiff hints it): add `u8 unused[N];` dead local.
  Recurring sizes: 8, 16, 24, 48. Position in the decl list can matter.
- **Two stack arrays transposed**: swap their declaration order
  (first-declared = HIGHER address).
- **8-byte hole between locals**: an unspilled `double` slot or pad —
  `u8 unused[8]` between the two decls.

## Constants / floats

- **lfd = double literal, lfs = float literal.** `0.5 * x` (double mul +
  frsp) vs `0.5f * x` (fmuls). Both appear in one function; read the asm.
- **`/ const` emitted as multiply** = source used the reciprocal literal
  (`x * (1.0f/30.0f)`), or a nonstandard 1/PI-style constant — reproduce the
  EXACT bits (pooldump the target constant, use a round-trip decimal).
- **No frsp after sin/cos calls + fdivs on raw results** = the PS2-shim TUs
  declare trig FLOAT-returning: `extern f32 sin(f32);`. tan/atan may stay
  double in the same TU. (atan emits `bl __atan`.)
- **f32 temps give fmadds; a double intermediate forces frsp.**
- **Int→float**: each `(f32)i` cast site emits its own xoris/stw/lfd magic —
  the target's conversion COUNT tells you how many cast sites the source had
  (reuse a `f32 fw = (f32)i;` temp to deduplicate, or repeat the cast).

## Control flow

- **`bge L; b M; L:` pairs around FP clamps** are a plain 3-arm chain
  `if (x < MIN) v = MIN; else if (x > MAX) v = MAX; else v = x;` — MWCC
  coalesces the compare constant with the arm's assignment (arms become
  empty, the pairs are pure if/else layout). Goto-pairs, empty-then/else and
  `!(a<b)` forms all FOLD instead — don't try them.
- **Plain bge/ble on floats (no cror)** = the branch is the layout-inversion
  of `<`/`>`; `>=`/`<=` in source emit `cror eq,gt,eq; bne` (IEEE-safe).
- **One-case switch** = `beq case; b end` unfolded pair (demo flags).
- **`break`-not-`return`** merges exit blocks; a shared epilogue jumped to
  from multiple arms usually means one `break`/fallthrough in source.
- **Condition polarity**: write conditions TARGET-FIRST as emitted
  (`if (target > cur)`, not the mirrored form).

## Registers / variables

- **Decl order drives web coloring** (descending regs). Shuffle declarations
  before anything exotic — but adding NEW named locals can make it WORSE
  (fresh low-numbered webs).
- **Named local vs CSE temp**: if a base pointer's color is one off, try
  DELETING the named local (`dw = g->disp->w;` with no `di`) — the CSE temp
  colors differently than a variable.
- **Cached-global tell**: callee reloading globals = it really reads globals;
  a base held in a nonvolatile across the fn = a cached local (`g = gWinGlobals`).
- **`extern volatile f32 G`** defeats LICM when the target reloads a global
  per-iteration.
- **`#pragma dont_inline on`** a small helper when the target keeps
  `bl helper` and auto-inline folds it (shifting the caller's regalloc).

## Park-on-sight (the 1.2.5n copy/remat quirk family — do NOT grind)

Identical opcode streams with any of: a surviving `fmr`/`mr`/`addi rX,rY,0`
copy we can't materialize (or one we can't remove), `li` rematerialization
of a live zero, a pure register-renumber cluster, or a single mr-vs-addi.
Known instances: regFind, sndVoiceUpdateAll, G3DReadControlPadStates,
mat44InvBasis, MBWindowZoom's z-copy, WorldDynCollide's d-copy,
InitDynobjGrid's i=total. 3 attempts max, then PARKED.txt.

## Verification discipline

- A green doldiff after a FAILED ninja is stale — confirm the build ended
  `[N/N] DOL ...main.dol` before believing anything.
- NonMatching TUs never link into the DOL (green is expected); objdiff
  report.json is the per-fn truth, `--clean`'s summary the per-iteration one.
- `...bss.0` relocs (out-of-bounds array access) and `@N` vs `lbl_` pool
  names are byte-identical after link — cosmetic.

## Matching (linked) TUs are OFF-LIMITS to fuzzy-driven edits

A TU flagged `Matching` in configure.py LINKS INTO THE DOL — the green sha1
is the byte-proof. Functions inside it that score <100% fuzzy are reloc-name
noise (literal-vs-reloc, @N-vs-lbl), not near-misses. "Improving" their
source changes real DOL bytes and reds the link. nearmiss.py excludes them
automatically (metadata.complete). If you want to upgrade an asm-shell or a
hardcoded-address body inside a Matching TU to real C, the replacement must
be BYTE-EXACT before it can land — draft it, fndiff it, and only swap it in
at 0 real diff lines.

## Address-grouping laws (walls sweep, 2026-07)

The "isel wall" family (target `add base,idx` + big-displacement vs our
`addi idx,const` + lwzx/stwx) is SOURCE SHAPE, not flags. Empirical laws
(micro-repros in the walls session; killed pbTraverseDrawObjects and
fn_800C780C):

- **Flag semantics correction**: in `-O4,p` the `,p` means `-opt speed` —
  NOT peephole. Plain `-O4` already includes peephole+schedule+functions
  (`mwcceppc -help all` is authoritative). All "no-peephole family" tells in
  older notes are really "no-speed family" tells.
- **Typed-local subscript law**: `T* t = (T*)ptr_expr; x = t[i].field;`
  emits `add rT,base,idx ; lwz field-disp(rT)` (the target form). The SAME
  access with the cast inline under the subscript
  (`((T*)ptr_expr)[i].field`) or through an address-of transit
  (`T* e = &t[i]; e->field`) re-associates and folds the constant into the
  index (lwzx). Absorb casts in a typed-local ASSIGNMENT, keep subscript +
  field in ONE expression.
- **Sibling-symbol pooling law**: same-section TU-local globals are
  addressed off ONE materialized base with constant deltas (bss pooling).
  If target shows `lis/addi symA` + accesses at symA+bigconst that really
  belong to the NEXT array, do NOT write `(char*)symA + bigconst` byte
  math — declare the real sibling arrays (`mat44 matrix_stack[64];
  u32 node_flags[64]; u32 view_flag[N];`) and index them naturally; the
  compiler emits the symA-relative form itself, and RMW `|=` keeps the
  displacement. Byte-math spellings let copy-prop re-fold RMW addressing
  into lwzx/stwx. (Requires the arrays DEFINED in-TU: extern kills pooling.)
- **&arr[i] into a struct field**: `Effect* e = &page->fx[idx];` fixes the
  "+2976 fold" (field-array offset rides as addi/disp, insn counts align).
  A trailing single-use `p += const` still gets dissolved by copy-prop —
  that residual is flag-proof; accept or pragma.
- **Scoped `#pragma opt_propagation off`** (melee precedent: pragmas ship in
  matched source, `#pragma push/pop` scoping) reproduces prop-blocked shapes
  when no source spelling works — verify the WHOLE fn stays byte-identical;
  prop-off usually changes nothing else. It does NOT beat the address
  canonicalizer (fn_80091AC0-class assoc ties are pragma-proof too).
- **Flag axis is CLOSED for parked walls**: `tools/gdl/flagsweep.py` sweeps
  every -opt suboption, -proc, -sym, -inline, -schedule and 7 archive
  compiler versions against the dtk target object in one run. No parked
  renum/sched/assoc residual moved under any variant (2.x/3.x compilers are
  uniformly worse). Do not re-run compiler-version hunts by hand.

## Register-web coloring laws (webs sweep, 2026-07)

The parked "renum" family (opcode-identical streams, 2-6 nonvolatile webs
rotated r28<->r31 / FPR analogs) is SOURCE SHAPE, not flags. Micro-repro
corpus + compile harness: research/webs/ (DELETED post-extraction; recover from git history if ever needed) (t/m/v/b/p/q series against the
GC/1.2.5 + cflags_demo pipeline). Laws, in application order:

- **Baseline coloring law**: MWCC assigns nonvolatile colors ASCENDING
  (lowest saved reg first) in web-creation order = param copies in param
  order, then call-crossing locals in order of first def (t01-t03 repros).
  The TARGET coloring of a rotated fn is almost always exactly this
  baseline — the rotation means OUR source's web-creation history deviates
  somewhere, not that the target is exotic. Find the deviation; don't
  permute blindly.
- **Inlined-shared-helper law** (the big one): a block that is a verbatim
  copy of a SIBLING function's body — or repeated twice inside one
  function — was a `static` helper in the original, defined BEFORE its
  callers and folded by `-inline auto`; mwld deadstripped the standalone
  copy (our extra emitted static shows as ONLY-IN-BASE in fndiff — expected
  and harmless). Open-coding that body compiles to the IDENTICAL opcode
  stream but rotates the host function's web colors; routing through the
  helper restores the baseline coloring. Tells:
    * clone body == sibling body modulo `return -1` becoming
      `idx = -1; <fallthrough>` — that rewrite IS the inliner's
      return-value materialization;
    * clones sharing one error-format string with the sibling;
    * constant args folding the helper's interior range-check away (the
      "no-check" variants of a clone family are still guts calls).
  Fallen: MBNewObject+MBSetObject (SetObjectGuts), StartEnemyAtkFX /
  StartGenFX / StartLevelUpFX / StartEnterFX / StartMagicPlayerFX /
  fn_80093B04 + StartFXSub kept exact (StartFXSubGuts), MBWindowProject's
  whole FPR-temp cluster (ClampS16, s16 clamp repeated for sx/sy).
- **Delete-named-local law** (sharpened from "Named local vs CSE temp"):
  ONE extra named pointer local anywhere can rotate OTHER webs' colors
  fn-wide. If the target reloads a derived pointer after every store
  through an alias (e.g. `*(u8**)(globals+0x10)` re-derefed after each
  `dst[i] =`), the original had NO local for it — write the deref at every
  use; CSE keeps the pre-store loads merged and the aliasing stores force
  the reloads. Deleting the local un-rotated MBWorldToScreen to EXACT.
- **Arg-position clamp ternary**: a surviving `bge L; b M; L: li; M:` around
  a clamp feeding a call argument is `f(...,  x < K ? x : K)` written with
  the LIMIT in the else arm and the value RE-DEREFED (`input[1] < 240 ?
  input[1] : 240` — no named local, CSE merges the two loads). A named
  local in the ternary adds a `mr`; if/goto/empty-else spellings FOLD to a
  single inverted branch (dcsHandleRequest case 4).
- **Negative results** (do not re-run): use-count asymmetry, `register`,
  named temps for subexpressions (fold away before allocation),
  reassociation of the stored expression, statement reorder within the
  block, decl-order shuffles, state-first vs decl-init vs post-call
  assignment — ALL color-neutral on these rotations. mwcc 1.2.5 `-help all`
  exposes no allocator/web dump; the allocator stays black-box. Coloring is
  a whole-function property: adding/removing ONE instruction ANYWHERE can
  rotate webs two blocks away (webs p/q series, git history), which is why
  local grinding fails and structure-level levers (helper, local deletion)
  are the only reliable ones.
## Additions (law-pass session, 2026-07-27)

- **`p[idx+K].field` displacement fold defeats CSE**: `p[idx+1].f8 = p[idx].f12`
  emits TWO adds of the SAME value (add base,idx*S each) because the +K*S
  constant folds into the ACCESS displacement, making the address expressions
  differ pre-fold (dbgtext fn_800C0AA4 EXACT). POWER-OF-2 strides only —
  mulli-based strides (240) re-canonicalize the constant into the index side
  no matter the spelling (fn_80091AC0 stays parked).
- **Constant in the SUBSCRIPT folds into the INDEX register**: target
  `addi rX,iv,652 ; stfsx v,ctx,rX` = flat word-array subscript
  `((f32*)g->ctx)[iv + 163]` (const inside the brackets). The struct/field
  spelling and byte-math both give the (base+iv)+disp form instead. Combine
  with per-statement `g->ctx` re-derefs when the target reloads the base
  before every store (pointer stores force it) — pb_winglobals fn_800C0CF4
  MATCHED this way. Respell address-of consts in the same byte terms
  (`(u32)((u8*)g->ctx + i*4 + 652)`) so they CSE with the store indices.
- **Same-block read folds, cross-block use materializes**: `q = &s->m14;
  if (*q == 0) { *q = v; }` — the condition read emits as the FIELD form
  (lwz 20(s)) while the addi survives for the cross-block store (pb_frame
  fn_800C1624 17->2). A pointer INCREMENT (`p += 4`/`p++`) gets dissolved by
  copy-prop; a SEPARATE pointer local (`q = p + 1`) with its use in the next
  block keeps the addi (pb_winglobals fn_800C1004).
- **OR-chain base pick is right-operand-first**: in a flat left-assoc
  `a | b | c | d`, the SECOND term becomes the rlwinm base, then a, c, d as
  rlwimi. Reorder terms (24,16,8,0 to get base 16) instead of grouping.
- **fmr f1,f31 + stfd f31 on an "uninitialized" float arg** = the arg is a
  live value already sitting in f1 (e.g. the global just compared:
  `alpha_tree_dist`), not a garbage local. An uninit local's web spans from
  fn ENTRY, crosses calls, and lands in a nonvolatile — the tell that the
  reconstruction invented it (pbRenderNode).
- **for-statement comma clauses order the inits/increments**: target
  `li i,0 ; li off,0` + tail `addi i,1 ; addi off,16` = `for (i = 0, off = 0;
  ...; i++, off += 0x10)` — decl-init `off = 0` flips the init order.

## Additions (stuck-TU flip session, 2026-07-27)

- **Inlined-helper canonicalization escape** (verified, flipped mb_objects):
  MWCC canonicalizes a float compare const-first (`fcmpu f0,fX`) whenever one
  operand is a visible constant (`sZero == limit` and all 7 direct spellings).
  Routing the compare through a tiny inlined static
  (`static int feq(f32 a, f32 b) { return a == b; }`, defined before the
  caller) makes BOTH operands opaque locals inside the inlinee — operand
  order then follows param order (`feq(limit, sZero)` emits
  `fcmpu cr0,fLIMIT,fZERO`). The `if (feq(...))` boolean folds back to the
  bare fcmpu/beq; the standalone static deadstrips.
- **Helper-extraction recolors loop IVs** (verified, flipped g3dMath3D):
  mat44InvBasis's transpose loop open-coded colors the outer IV LAST (r11,
  after the four address webs); the identical loop inside a static
  `transposeGuts(mat44&,mat44&)` inlined into the caller colors the IV FIRST
  (r7) = target. Same law as the inlined-shared-helper rule: the inlinee's
  own locals restart web-creation order. Also: unused `int i, j` decls left
  in the caller still hold 8 frame bytes — deleting them needs the old pad
  back.
- **Inlined-helper params are NOT a universal recolorer** (negative,
  sndVoiceUpdateAll): a value-param helper (`sndMixDelta(u16 cur, u16 next)`)
  inlined 9x reproduced the opcode stream exactly but left every web color
  unchanged, and each inline site added an 8-byte param-slot frame cost
  (compensate the dead pad before judging the diff). The helper lever works
  when the OPEN-CODED copy created extra/different webs (guts-style bodies,
  loops); it does nothing for a pure scratch-temp color tie.
- **fndiff "MATCH (pool-name noise only)" can hide a wrong-constant bug**:
  vec4Cross scored 0 real diff lines while loading the WRONG pool entry
  (our `@16` = 1.0f vs target `lbl_80348A44` = 0.0f) — the normalizer
  equates @N/lbl_ names without comparing symbol addresses. Only doldiff
  (post-link bytes) catches it: g3dMath3D's first flip attempt went RED on
  exactly one addend byte. On any red flip where every fn shows 0 real
  diffs, objdump the SDA21/pool relocs and check the referenced VALUES.

## Additions (light-touch swarm round, 2026-07)

- **Induction-form law** (items.c ShowMilestones/ShowCameras/CollectSafeRocks
  all EXACT): when a stride loop's counter pair is rotated (base/off swapped
  one position), the original spelled the element address as
  `base + i * stride` (or `&arr[i]`) and let MWCC strength-reduce — the
  induction temp is a COMPILER web created last, colored highest (r31).
  A hand-written `off += stride` user variable creates its web too early
  and steals the low color. Delete the named accumulator; keep `i` alone.
  (Decl-order shuffles are color-neutral — confirmed again; don't retry.)

- **Scalar-vs-array extern controls SDA21 vs ADDR16** (sounds_evt): for a
  global in r13/r2 range, `extern s32 lbl_X;` (scalar, or sized array) emits
  the 1-insn SDA21 form; `extern s32 lbl_X[];` (unsized array) forces the
  2-insn lis/addi ADDR16 pair with a hoisted base reg. When the target
  loads a small-data global with one insn and ours uses two (or vice
  versa), flip the extern's declaration shape before touching code.
- **`(f32)(u32)` double-cast** for unsigned int → float conversions the
  target does via the u32 path (do_ai): a plain `(f32)x` on a signed int
  emits the xoris/stw magic-constant sequence; matching an unsigned
  conversion needs the explicit `(u32)` intermediate.

## Additions (sfx refinement pass)

- **Contiguous-case switch range emission**: `cmpwi hi; bge default; cmpwi lo;
  bge case; b default` — a bge/b pair that is really a range-checked switch
  (cases N..M), not an if-chain.
- **Comma-assignment inside `||`**: `if (t < 0 || (type = t) >= 218)` —
  Ghidra decompiles it literally; keep it literal.
- **Struct-field view beats flat pointer math**: displacements in target
  loads (`lwz r,2616(base)`) survive when the source uses a typed struct
  view; flat `(u8*)p + off` arithmetic gets re-associated. Define a local
  struct if the offsets cluster.
- **A mid-loop that no hand-written loop reproduces may be an AUTO-INLINED
  call to a sibling fn** (DeleteEffect inlines SfxDeleteParented(n,0,-1) —
  the inliner's param copy-init `addi rX,rY,0` is the tell).

## Additions (gates session, 2026-07-27)

- **MWCC CSE is DOMINANCE-ONLY — no sibling-arm PRE** (verified, pbRenderNode):
  identical exprs unify only when one USE SITE dominates the others. Three
  `fl & 1` call-args (site 1 dominating sibling if/else arms 2,3) hoist into
  ONE clrlwi + copies; respell SITE 1 as `(fl << 31) >> 31` (distinct IR,
  lowers late to the same clrlwi) and leave the SIBLING pair as `fl & 1` —
  neither arm dominates the other, so each keeps a per-site clrlwi and the
  hoist vanishes. `fl % 2` (u32) lowers to AND EARLY and unifies with `& 1`;
  the shift-pair does not. Any two IDENTICAL spellings still unify.
- **Decl order sets web colors, statement order sets the schedule**
  (pbRenderNode EXACT): when a nonvolatile cluster is rotated but the opcode
  stream is aligned, split decls from inits — list the DECLS in target color
  order (descending r31->r27 in decl order) and keep the INIT STATEMENTS in
  the order the target schedules them. Generalizes dcsdrv's "state-first
  decl". Params and CSE base temps keep their own slots (node=r31,
  base=r30 stayed fixed while fp/fl/win/mat rotated).
- **decl-in-switch-condition pins the switch-expr temp** (C++ only; CameraFace
  EXACT, THE pb_tree flip gate): `switch (u32 mode = flags & 0x0F000000)`
  keeps the mask compute at the switch site in a FRESH reg (target shape:
  rlwinm r5,r3 after the address adds), where `switch (expr)` lets the
  compute coalesce into the dying source reg and the scheduler hoists it to
  the fn head (in-place rlwinm r3,r3 at slot 1), rotating every downstream
  volatile. A separate `u32 mode = ...;` decl costs +8 frame (home slot) AND
  still coalesces+hoists - only the condition-decl form works.
- **Inlined-helper canonicalization escape works for fmuls/fmul too**
  (MBWindowSetAng EXACT): `x * 0.5f` canonicalizes const-first
  (`fmuls fD,f0,fX`); routing through `static f32 mulf(f32 a, f32 b)
  { return a*b; }` (and `static double muld(double,double)` for double
  contexts) restores param/text order (`fmuls fD,fX,f0`). Compensate the
  8-byte inline param-slot frame cost (drop an existing pad). Same law as
  feq for fcmpu.
- **Inlined identity/param copies do NOT survive a post-inline cleanup**
  (negatives, MBWindowZoom): `static f32 fident(f32 x){return x;}` for
  `z = fident(zoom)` and a value-param helper wrapping a whole arm both got
  their param copy propagated away even though zoom stays live afterwards -
  the DeleteEffect "inliner param-copy survives" tell is NOT reproducible
  on demand. The z=zoom fmr class stays parked.
- **C++ global array of a ctor-class emits .ctors + a sinit** — pb_tree's
  `mat44 matrix_stack[64]` with `mat44() {}` produced a 4-byte .ctors section
  claimcheck rejects. Empty ctors exist only for mangling-compatible class
  layout; delete them (class name alone fixes the mangling) before flipping
  any C++ TU with class-type globals.
- **fn_800C37C4-class remat context flip**: an EXACT sibling (fn_800C36F8,
  same loop, li+li zero inits) proves the loop source; the same loop in the
  richer fn emits li+addi-copy (`addi r3,r6,0` from the live i=0) purely from
  surrounding context. Chain-assign `off = i = 0` const-folds back to li+li
  (+8 frame for the named off); a hoisted separate `i = 0;` statement
  dissolves. No source spelling reaches the emission choice - park family.
- **dcsHandleRequest param rotation is spelling-proof**: state-first vs
  state-after-memset, bankSize-local vs clamp-arg-ternary, SetStreamName guts
  helper - all leave the input/output/request rotation (r28/r29/r30 vs
  baseline param order) and the `addi r0,rX,LO + mr r31,r0` state 2-step
  intact; the helper+ternary combo committed earlier was a NET REGRESSION
  (351 vs 202 real lines) and was reverted to the 1ad1d5d form. On any
  "improved" quirk claim, re-measure the WHOLE fn, not the local site.
## Additions (deepen round 2, 2026-07-27)

- **MWCC .bss layout law** (verified, enemy.c): referenced bss symbols are
  allocated in FIRST-USE order (order of first reference across the TU's
  function list), then unreferenced ones in REVERSE declaration order. To pin
  a pool anchor (e.g. lbl_80250E00 @0 with gEnemies at +0xE18) add an
  unreferenced `static void xxx_bss_order(void)` BEFORE the real functions
  that touches every array in address order - mwld strips it, the order
  stays. dtk resolves the section-relative relocs to the anchor name.
- **fmul const-first canonicalization escape**: our GC/1.2.5 emits double
  `x * K` const-first (fmul rD,K,x) from `x = x * K;` but keeps VAR-first
  (target form) from the compound `x *= K;` (turn_enemy_ang rate *= 3.0).
- **Loop-condition assignment reuses the test load**: target body using the
  condition's loaded value (mr max,r0 / mulli off the same reg) = source
  spelled `for (...; i < n && (cc = p->f) != 0; ...)` - the plain re-deref
  `p->f` in the body RELOADS instead (MBNewFont both loops).
- **Static stubs auto-inline**: a small placeholder static gets folded into
  its callers and poisons their bodies - wrap with `#pragma dont_inline
  on/off` to keep the `bl` (mb_font fn_800B5B00).
- **u16-vs-int compare artifact**: `int h = <lhz field>; if (h == 0xFFFF)`
  emits addis r0,rH,0 + cmplwi 65535 (same family: s32 == 0x8007 in
  generate_enemy). A u16-typed h gives plain cmplwi instead - wrong.
- **Param-reuse tell**: call result copied back into a param's saved home
  (addi r22,r7,0 ... addi r22,r3,0) = the param variable was reassigned
  (`spew = fn(type, level, spew)`); same for pan reused as the AudioAng
  result and `p2 = (p2 * scale) >> 8` (sndFxStartVoice).
- **Frame slots empirics** (do_enemy_move, QueAddEx): an FPR-homed named f32
  (rad2) holds a 4-byte slot at its DECL position; block-scope re-decls STACK
  (no overlay); pointer locals whose init SURVIVES (producing add/addi forms)
  hold slots, while subscript-form typed views (`dt[i].field`) produce the
  same add+disp code slot-free - prefer subscript form, and if a surviving
  pointer local is unavoidable count its 4 bytes.
- **One-case switch folds when the case body is a goto** (`case 0: goto X`
  threads to a single beq) - it only reproduces the unfolded beq/b pair when
  the case body is real code (turn_enemy_ang hit-dispatch vs sndFxStartVoice
  slot-loop, still parked).

## Enemy targeting and generator seams (2026-07-28)

- Express floating-point rejection branches directly (`if (x > limit) goto
  next`) when the target uses plain `bgt`/`blt`.  Inverting the condition and
  nesting the accepted body can make MWCC preserve unordered/NaN semantics
  with an extra `cror`.
- When a byte field is loaded, conditionally changed, and then its owner is
  reloaded for another field, repeating the typed owner expression can produce
  the target `lbzu field(base)` plus `stb 0(base)`.  A cached derived pointer
  tends to produce separate `addi` or displacement-form load/store pairs.
- A count-preserving `for (i = 0; i < count; i++)` can recover `mtctr/bdnz`
  even when the loop also advances a large record pointer.  Mutating `count`
  directly instead selects `addic./bne` and changes the volatile-register web.

## Additions (10-worker Opus body-fill batch)

- **Anchor-control catalog (4 routes now)**: first-use-order referencer fn
  (enemy), deadstripped `x_bss_order()` toucher static (boss), static-vs-
  external LINKAGE segregation (btext), in-TU .rodata string-pool struct
  (boss — extern pools kill sibling-pooling).
- **Offset-induction vs pointer-increment** is a per-loop source choice;
  typed struct-array views reload a global base per match-arm where
  pointer-increment CSEs it (select).
- **`flag=0; while(test-first)`** preserves the flag init that do-while
  collapses (`li; b test`).
- **va_list/local declaration order** controls buffer-vs-va_list frame slots.
- **atan2 second-arg-as-local** flips arg load order (generalizes QuickYawMat).
- **dont_inline for target-kept `bl`s**: helpers defined before callers
  auto-inline; the pragma preserves the call (movieplayer readers).
- **Per-path explicit returns** trigger MWCC conditional-constant propagation.
- **ctr-contention**: when two countdown loops compete for ctr, spell the
  intended winner as `for` (movieplayer LZ).
- **Defer `~` to use site** (`mask=1<<n; x&=~mask`) flips to andc coloring
  (tower).
- **Struct assignment (not memcpy) for large block copies** (memcard 5172B).
- **throw()-family tell**: `bl __unexpected` landing pads + r31 FP frame =
  C++ exception-spec fns — unreproducible in a .c TU; the TU needs .cpp
  conversion (movieplayer queued).
- **Ghidra double-params on int fns** = ABI noise from float-using callees,
  not real float forwarding (items).

## Additions (stub-fill session, 2026-08-03)

- **u64 tells (pb_frame GS/PMODE work)**: `li rA,1; and` pairs = 64-bit AND
  (u64 lvalue; BE low word lives at +4 — a lone `lwz X+4` masked with li+and
  means the source op was `*(u64*)(p+X) & K`). `& ~3` on u64 keeps the
  high-word `and rH,-1` alive. u64 copies emit lfd/stfd. A 64-bit `<<1` OR'd
  in came from `(s32)(x << 1)` (32-bit shift, then sign-extend via s64 OR) —
  spelling it `(s64)x << 1` calls __shl2i and drags in saved regs.
- **GS bitfield extracts carry explicit width masks**: plain `u16>>N` emits
  srawi/srwi; the target rlwinm means `(x >> N) & 0x1FF/0x7FF/0xFFF` (the GS
  field width) was in source (fn_800C2C74 flipped EXACT on this).
- **Sub-struct pointer view** (`T* s = (T*)&ctl->m18;`) reproduces a
  `lwz base; addi base,base,24` prologue and per-statement `lwz regs` reloads
  (stores through s alias the pointer field). fn_800C2C74/2618.
- **BossGenerateEnemy (EXACT) checklist**: typed-view PARAM (not a local
  cast) avoids the extra GPR web; sized `extern f32 arr[2]` for the SDA21
  1-insn address form (unsized [] = lis/addi pair + hoisted web); one-case
  `switch` for the beq/b unfolded guard; `f64 scale = lbl; f32 speed = lbl;`
  locals inside the guard land constants in f31/f30; atan2 arg2-temp
  (`f32 vz = vec[2];`) flips the f2-before-f1 load order; `u8 unused[4]`
  last-declared fixed the +4 frame.
- **DrawBlitFlatQuad**: game code calls GXSetChanMatColor with a POINTER
  (mb_particle proto) — shadow the SDK decl via
  `#define GXSetChanMatColor X_sdk / #include gx.h / #undef` and pass &copy
  (`GXColor c2 = c;` supplies the lwz/stw word copy at the right slot).
  int->f32 conversion-site count and per-site slots pin the frame; dead pads
  96 (above mtx) + 36 (below colors). PARKED at 100 real: volatile FP triple
  (magic/half/div = f10/f8/f9 vs ours f9/f10/f8) + coupled int schedule; decl
  order in target color order DID fix all five nonvolatile f27-f31 webs.
- **fn_800C2618 rlwimi insert shape PARKED**: MWCC insists on
  `lhzu dest; clrlwi dest; rlwimi dest,src(raw)` for
  `(u16)((src&0xFFFF)<<7)|(dest&0x7F)`; the target's
  `clrlwi src; lhz/sth disp` form survived none of: operand swap, (u16)
  cast before/after shift, u16/u8 temps, volatile derefs, scoped peephole
  off (worse), prop off. Suspect a different compiler switch or helper;
  body committed correct-logic (666 real).

## Additions (BREAKTHROUGH session, 2026-08-03 late)

- **BITFIELD STRUCTS solve the rlwimi insert-shape wall** (fn_800C2618
  407 insns STUB->EXACT): a partial-register write the and/or expression
  forms can never reproduce (`clrlwi src; lhz; rlwimi src-masked; sth`,
  no dest mask op) is MWCC's native BITFIELD STORE. Define an overlay
  struct per container+position (`typedef struct { u16 hi:9; u16 lo:7; }`)
  and assign the field: `((T*)(p+off))->hi = (u16)val;`. The (u16)/(u8)
  cast on the source emits the explicit clrlwi-16/24 exactly where the
  target has it; u32 sources take no cast. MSB-first field order.
- **Aligned s64 field + COMPOUND RMW kills the 64-bit store addi**:
  `*(u64*)(p+off) op=` and even typed-field `x = x & k` spellings
  materialize `addi base` for the store half; `s->pm->pmode1 &= ~3;`
  (compound, through an ALIGNED s64 struct field, single deref) folds
  both words to displacements and keeps the li -4/-1 64-bit constant
  pair. u64 COPIES: `*(f64*)dst = *(f64*)src` for the lfd/stfd form
  (u64-typed copy emits lwz/stw pairs).
- **Hidden PS2 argument pins registers fn-wide** (fn_800C31C4/32D0):
  a dead-looking early `li r3,0` + every temp avoiding r3/r4 = a call
  later takes an ARGUMENT the decl dropped (sceGsSyncV(0) - real PS2
  signature sceGsSyncV(int)). Fixing the proto un-rotated the whole fn.
  CHECK SDK SIGNATURES before chasing register rotations.
- **`!x` vs `x == 0`**: !call() emits the bare cntlzw/srwi bool;
  `call() == 0` emits neg+cntlzw+srwi. Target cntlzw-direct => spell `!`.
- **Assignment-in-condition pins addi-before-load** (fn_800C1624 EXACT):
  `if (*(q = &s->m14) == 0)` keeps `addi q` ahead of the load where
  separate statements let the scheduler hoist the load. (The camera.c
  a_mode/c_mode 2-line swap is NOT this class - still parked.)
- Clamp-in-arg recipe CONFIRMED half: re-derefed value gives the CSE-temp
  clamp; but whether the bge/b UNFOLDS + the extra `mr` arg-copy appears
  is allocator-side - adsMoveFileToRaw still parks at 18.
- AdsParseHeader 44->20: ALL byte-swaps must use the DCS_SWAP32 term
  order ((v<<24)|((v<<8)&0xFF0000)|(v>>24)|((v>>8)&0xFF00)) per the
  OR-chain right-operand-first base rule; error arms spelled `!= 0`
  with result=-1 INLINE (polarity), match strncmp ladder shape.

## Target-body extraction helper (2026-08-07)

`tools/gdl/mwbody.py` extracts one target function from the configured object
and emits a Metrowerks inline-assembly skeleton. It rewrites local branches,
REL24 calls, SDA21 references, and common address relocations. Use it only
after the portable C implementation is understood and the remaining mismatch
is a small compiler scheduling/register-allocation wall; retain that C body
under the non-MWERKS side of the guard.

    python tools/gdl/mwbody.py game/sys/ml_mem AllocMem32 \
        --signature "void* AllocMem32(int size)"

For a mechanically safe edit, `--apply` locates the exact definition, emits
the target body under `__MWERKS__`, and retains the complete existing function
under `#else`. It scans balanced braces while ignoring strings and comments:

    python tools/gdl/mwbody.py game/sys/ml_mem AllocMem32 \
        --signature "void* AllocMem32(int size)" \
        --apply src/game/sys/ml_mem.c

## The opt_lifetimes law (camera_orbit_update byte-exact, 2026-08-03)

Scoped `#pragma opt_lifetimes off` (before fn) + `#pragma opt_lifetimes reset`
(after fn) cracks the in-place pointer-increment web that plain respelling
never wins:

    target:  add   rD,base,idx      ours:  add   rT,base,idx
             lfs   f31,368(rD)             lfs   f31,368(rT)
             addi  rD,rD,200               addi  rD,rT,200

Conditions and costs:
- The single-variable spelling is REQUIRED (`cam = base; use *cam; cam += K`)
  AND there must be a real USE between the two defs. Back-to-back defs with
  no use between still fold to a temp (camera_mode_spin: parked).
- Under -off, every NAMED local becomes ONE whole-function web. A variable
  that the target allocator split into two webs must be split into one name
  per web by hand: orbit's `previous` -> `prevSpin` (early region, volatile
  f3) + `previous` (crosses the call, f31). Pragma + split = byte-exact.
- New names raise whole-fn register pressure -> spills (+8 frame) and fmr
  copies. Reuse DEAD names (mapped by target color) instead of minting new
  ones. Block-scoped locals under -off are given STACK SLOTS - never use
  them to shorten a lifetime.
- Tells that a target fn is lifetimes-ON (do NOT pragma it): dead scratch
  reuse like `lwz r0,glob; mulli r0,r0,K` in the tail (do_camera), copies
  folded to fresh li (init_for_gamemode). Pragma made both worse.

## Switch-tree sentinel law (camera_mode_dest, 2026-08-03)

When target and ours have IDENTICAL case bodies but the dispatch tree roots
at a different case (e.g. target `cmpwi 0; beq; bge; b` vs ours `cmpwi 1`
first), the case SETS differ: retail has an extra case label merged with
default that shifts MWCC pivot selection. `case -1: default:` flipped
camera_mode_dest to the retail root-at-0 shape (-35 real). A default-merged
sentinel produces a compare-free edge (plain `b default`), so look for a
tree edge with no cmpwi as the tell. Negative sentinels are natural for
mode variables initialized to -1.

## mode_dest grind addenda (2026-08-03, 569->471)

- K&R hidden-arg strikes again: retail calls DiffRate with NO li r3 setup.
  `void DiffRate();` + argless call site (-1 insn, frees renumber skew).
- Named f64 invariant locals (`f64 otherWeight = A - (f64)w;`) get their
  lfd+fsub HOISTED to the defining var's position; inlining the expression
  per-use (letting CSE unify) keeps codegen equal but FREES the variable's
  register -> -54 lines of renumber collapsed. Prefer inline+CSE over named
  invariant temps when the target computes late.
- Statement-order wins that DID land: `distance = cam->radius;` BEFORE the
  3-store normalize vector (scheduler sinks it 2 slots to the retail spot;
  placing it after = park); ticks-conversion statement FIRST in the
  post-switch head (-12).
- Regression list (mode_follow, DO NOT RETRY): fan-out copies for the
  savedTurn=savedYaw=savedPitch chain (both orders 513 vs 448 chain);
  mode_dest-style named-squares respell of the delta sum (502 vs 448) -
  mode_follow's FP lattice punishes ANY new early web.
