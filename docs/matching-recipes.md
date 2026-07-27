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
   fn to `PARKED.txt` with a one-line class note and move on.
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
