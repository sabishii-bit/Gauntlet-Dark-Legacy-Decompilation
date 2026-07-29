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
