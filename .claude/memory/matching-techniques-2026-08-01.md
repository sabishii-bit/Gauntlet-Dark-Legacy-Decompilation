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

## Keep a typed array base when the target colors base before index

In `camera_mode_orbit`, forming an element pointer first left the milestone
table base in `r0` and the scaled index in `r3`.  The target used the opposite
allocation.  Keeping a typed base pointer and indexing at the field use fixed
the complete GPR cluster without changing the generated opcode stream:

```c
CameraMilestone* milestones = sMilestones;
fqdist(milestones[cam->mode].position[0] - cam->wpos[0], ...);
```

This is the register-color counterpart to the typed-local subscript law: an
early `&array[index]` element pointer can coalesce with the eventual argument
register, while a base pointer keeps the array base web live and colors the
compiler-created scaled-index temporary separately.

## Model a giant function's stack as one explicit scratch layout

`camera_run_mode` initially emitted the right operations in a 248-byte frame,
while the target used 360 bytes and every address-taken vector was displaced.
An explicit `CameraRunScratch` aggregate, with fields placed at the observed
stack offsets and enough trailing padding to put MWCC's unsigned-to-double
conversion slot immediately before the saved registers, reproduced the exact
frame and every scratch displacement at once.  This is substantially safer
than scattering unrelated dead arrays through a large state machine.

Account for the ABI's local-area base empirically: this function's aggregate
landed at `r1+12`, so its first padding region is four bytes smaller than an
`r1+8` estimate, with those four bytes restored at the tail to preserve the
total frame.  Address-taken aggregate members can still make MWCC cache two
member addresses in nonvolatile GPRs across a call; treat that as a separate
register-allocation residual rather than disturbing an otherwise exact frame.

## Switch-arm source order controls the emitted state-machine layout

For two jump tables with identical case values, matching only the cases is not
enough.  MWCC emits case bodies in source order and points the table entries at
them.  Reordering `camera_run_mode` to the target's physical body order
(`CAM_VECDIST`, game modes, object mode, follow modes, off) collapsed a huge
structural diff while leaving the behavior unchanged.  Also keep the
zero-attention switch after the nonzero-attention switch and branch to it with
a label; an early source-level `if` places the whole second switch first.

## Audit math prototypes when an otherwise-correct call gains `frsp`

`camera_mode_target` calls the MSL-named `atan2` through the engine's
PS2-facing float ABI, just like CAMERA's `sin` and `cos` calls.  Declaring it
as the standard double-returning function inserted an immediate `frsp` and
propagated conversions through the settling logic.  The recovered prototype
`f32 atan2(f32, f32)` restored the target's plain `fmr f31,f1` result path and
removed the conversion cascade.  Treat a lone post-call `frsp` as a prototype
diagnostic before attempting register-shaping changes.

For the same handler, a volatile three-float movement vector reproduced the
target's deliberate spill/reload sequence across an inlined reciprocal-square
root.  Populate it only after the root calculation; making the input vector
volatile too early changes the load schedule and over-constrains the first
distance calculation.
