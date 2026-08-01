# Matching techniques found on 2026-08-01

These recipes produced real 100% function matches under the retail MWCC
profile.  Validate each use with `tools/gdl/fndiff.py --clean`; none of these
patterns is proof by itself.

## Mixed GPR/FPR argument order can reveal the original prototype

`fn_8005D0C4` was one scheduling instruction away because
`StartEnemyGrid(f32 radius, f32* position)` made MWCC load the float argument
before copying the pointer.  The target copied the pointer first.  Recovered
prototype and call order:

```c
void StartEnemyGrid(f32* position, f32 radius);
StartEnemyGrid(position, sNoDistance);
```

The ABI registers are unchanged (the pointer still uses r3 and the float f1),
but the source argument order controls scheduling.  Update the declaration,
definition, and every caller together.  Also prefer named pool globals such as
`sNoDistance` and `sCameraVisibilityRadius` when the target relocations prove
them; literals can hide the correct load schedule.

## Preload call arguments in target load order

For `fn_800D9614`, `fn_800D9648`, and `fn_800D9F20`, direct array/field
expressions caused MWCC to preserve the aggregate pointer and load arguments
in index order.  Small locals declared in the target's observed load order let
the compiler place values directly in the eventual argument registers:

```c
u32 arg1 = command[1];
u32 arg3 = command[3];
u32 arg2 = command[2];
u32 arg4 = command[4];
callee(ctx, command[0], arg1, (char*)arg2, arg3, arg4);
```

This is especially effective for six-argument calls and indirect dispatch.

## Split a double expression, then use compound assignment

`fn_800552A4` needed the intermediate double to remain in f3 while the scale
constant occupied f0.  A named `f64 vertex` fixed allocation; compound
assignment fixed the commutative `fmul` operand encoding:

```c
vertex = 41.0 * progress + 23.0;
vertex *= 0.0078125;
callee(..., (f32)vertex, ...);
```

A single expression allocated the intermediate and scale in the opposite FPRs.

## A small optimization pragma can preserve an explicit record pointer

`PrintWorldMemSizes` became exact with a function-local
`#pragma opt_propagation off`.  First form an entry pointer, then access fields
through it.  This retained target `add base,index; lwz fieldOffset(entry)`
addressing instead of folding the field offset into an indexed load.

That pass also found a semantic omission: the per-category byte count is a
third `bulletproof_printf` argument.  Always inspect live call-argument
registers when a near-match has an unexplained field load.

## Outer volatile roots can reproduce inlined sqrt stack slots

`CreateDirMatrix` matched after its inlined normalization helper accepted an
outer `volatile f32* root`, with separate roots in the caller, an explicit
8-byte pad, and function-local `#pragma opt_propagation off`.  When repeated
inlined Newton-Raphson blocks differ only in spill slots, model the spill owner
in the outer function instead of adding unrelated dead arrays to the helper.

## State-machine value reuse controls register coalescing

In `camera_init_for_gamemode`, reusing `bossIndex` for the table result and the
now-dead `bossType` for the changed-bit mask reduced a 34-line operand diff to
one swapped load pair.  This mirrors the target's r5/r3 lifetimes and is more
effective than declaration-order changes.  Reconstructing the boss range as a
real switch also restored two missing branch instructions.

## Residual-ranked queue

`tools/gdl/nearmiss.py --residuals --parked skip` measures normalized real diff
lines with the same parser/classifier as `fndiff.py`, then sorts cheap residuals
first.  Use this after a fresh `ninja`; fuzzy percentage alone over-prioritizes
large register-allocation walls.
