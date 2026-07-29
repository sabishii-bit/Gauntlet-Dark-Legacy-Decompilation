# `MBPsysSetETime` exact match (2026-07-29)

`MBPsysSetETime` in `game/mb/mb_particle.c` is byte-exact
(`0x800D0F68`, `0x10C` / 268 bytes). The full linked DOL SHA-1 passes.

## Technique

The body was already structurally correct. Its eight-byte oversized frame and
final `lfd` instead of `lfs` both came from an incorrect public signature:

```c
void MBPsysSetETime(f32 dur, f32 rep, MBObject* node)
```

not `f64` parameters. The duration/fade math still uses double literals and
therefore retains the target double multiply/clamp sequence. The final
sentinel tests must use `0.0f` explicitly:

```c
if (dur < 0.0f) { ... }
else if (rep < 0.0f) { ... }
```

This is a useful diagnostic: an unexplained eight-byte frame excess plus a
float/double constant-load mismatch can indicate wrong ABI parameter types,
even when the arithmetic body correctly promotes those parameters to double.

## `getCurrentDir`

`getCurrentDir` is also exact (224 bytes). Its only structural mismatch was an
extra `frsp` after `mbInvSqrtLookup`. The defining TU already proves that
`mbInvSqrtLookup(f64)` returns `f32`; this consumer had independently declared
it as returning `f64` and then cast the result. Correcting the imported
prototype removes the conversion and matches retail. Before trying expression
or scheduling changes around a call result, compare the declaration against
the defining TU.

## `listFindHandle`

`listFindHandle` is exact (40 bytes). Its body was already correct and differed
only by the `r4`/`r5` allocation of two short-lived locals. Declaring the loaded
chain value before the link pointer:

```c
u32 cur;
s32* link = (s32*)(base + 4);
```

produces the retail allocation (`cur` in `r4`, `link` in `r5`). For compact
leaf functions with a pure register permutation, try reversing adjacent local
declarations before introducing artificial uses or volatile storage. MWCC's
allocation direction is sensitive to the local lifetimes and is not uniform
across every function, so validate each case with `fndiff`.

## `MBTraversePsys` and the module-global block

`MBTraversePsys` is exact (136 bytes). The retail TU does not address the
traversal filter as an independent SDA symbol. It loads the absolute address of
the particle module's `.data` block, `lbl_80128710`, and reads the filter at
offset `0x58`. Model the access as a field of that block:

```c
u8* globals = lbl_80128710;
*(s32*)(globals + 0x58)
```

Keeping the anchor pointer unshifted is important: a pointer initialized to
`lbl_80128710 + 0x58` materializes an extra `addi`; accessing the field through
the unshifted aggregate folds `0x58` into the retail `lwz 88(r3)`. Other
`mb_particle.c` globals documented at `0x80128710..0x801287FB` should be migrated
to this aggregate as their functions are translated, rather than modeled as
independent SDA statics.
