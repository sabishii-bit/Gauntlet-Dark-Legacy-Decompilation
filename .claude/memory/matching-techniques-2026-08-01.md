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

## Reuse one TU base in giant camera supervisors

`camera_mode_follow` has six independent position/attention/direction triples,
four normalization vectors, three square-root spills, a projection pair, and
an averaged focus vector. Letting MWCC pack those as ordinary locals produced
the right behavior in a 432-byte frame, while retail used a 512-byte frame.
A single `CameraFollowScratch` overlay allocated at `r1+0x0C` recovered every
important retail offset from `r1+0x10` through `r1+0x168` and the exact frame.

For this unusually large function, `#pragma opt_common_subs off` stopped MWCC
from hoisting scratch-member addresses across `StandardCamera`; retail instead
rematerializes each `addi r3,r1,offset` just before `DoShake`. Also derive the
camera, focus-history array, backup camera, and projection matrix from one
`gCameraState` base. The target retains that common base in one saved GPR,
where separately named globals produce extra `lis/addi` pairs and lifetimes.

## Recover hidden float parameters from callee prologues

Ghidra initially rendered `camera_collide_step` as a one-argument call, but its
retail prologue immediately copies `f1` into saved `f27`. The two callers load
different constants into `f1`, proving the routine actually takes
`(s32 cameraIndex, f32 blendThreshold)`. Restoring that prototype recovered
the level-camera call sequence and explains why its trigger-node selection
threshold differs from the normal orbit path. When a decompile appears to
hard-code a float, inspect the callee's first basic block for an untyped FPR
parameter before transcribing it as a global constant.

`camera_mode_level` also demonstrates an exact-frame overlay with an explicit
tail gap: its address-taken vectors and two 4x4 matrices occupy a 0x16C-byte
aggregate at `r1+0x0C`, while the retail saved-register area begins at
`r1+0x17C`. The otherwise-unused final eight bytes are necessary to obtain the
retail 0x198-byte frame; accounting only for the highest live matrix produces
a frame that is eight bytes too small.

## Reverse local-array declarations and pad a reused vector

`camera_mode_dest` showed the opposite failure mode from the giant scratch
overlays: combining all of its arrays into one aggregate made the outgoing
varargs area keep the aggregate twelve bytes too high. MWCC allocates separate
local arrays in reverse declaration order. Declaring the matrix first and the
short-lived normalization vectors last put them in retail order, then sizing
the three-used-element angle workspace as seven floats preserved retail's
four-word lifetime gap. That recovered the exact 0xD0 frame and the important
transform, offset, and matrix locations without emitting dead instructions.

The debug print in the same routine is also a useful ABI tell. Declaring
`dbgTextPrintfCol(s32, s32, char*, ...)` restores the target `crset` for a
floating vararg. Pass the already-rounded double expression directly; an
explicit cast back to `f32` inserts an extra `frsp` immediately before the
call and changes the surrounding FPR allocation.

## Let compiler homes establish the aggregate base before padding it

`camera_debug_supervisor` needs both an explicit scratch layout and MWCC's own
eight-byte float-to-integer conversion homes. A first-pass aggregate assumed
the usual `r1+0x0C` base and made the frame 32 bytes too large. Inspection of
the compiled displacements showed that MWCC had placed the aggregate at
`r1+0x30`, above its implicit homes. Reducing only the aggregate's leading pad
by `0x24` then recovered the retail 0x130-byte frame and, simultaneously, the
exact offsets for eight inline-fabs temporaries (`+0x4C..+0x68`), two projected
s16 pairs (`+0x6C/+0x70`), the alternate position (`+0xA4`), the future
position (`+0xB8`), and conversion homes (`+0xC8..+0xE4`).

The function's late "temporarily move player" path reuses the original future
position's three stack words for the saved player position. Reusing the same
aggregate field in C preserves that lifetime overlay; a separately declared
saved vector is promoted into FPRs and loses the retail spill/reload sequence.

## Use singleton switches to preserve two-branch dispatches

`camera_orbit_update` contained three places where retail emitted an explicit
conditional branch followed by an unconditional branch (`beq case; b exit`).
Ordinary `if`/`else` and equivalent `goto` forms were folded to one inverted
branch. Expressing the dispatch as a `switch` with one named case and a
`default` preserved the retail two-branch form without inline assembly. This
worked both for a real value dispatch (`case 2`) and for zero-valued guard
globals. It also preserved register allocation, unlike inline `asm { b ... }`,
which changed the function's floating-register lifetimes.

The same function's absolute-yaw bit clear needed its address-taken float at
`r1+0x10`. A two-word scratch record with the live float first and an unused
four-byte tail made MWCC allocate the record at that exact offset while
retaining the retail 0x38-byte frame. Later crossing differences must use a
separate, non-address-taken `f32`; reusing the scratch float forces two
unwanted store/reload pairs.

Finally, model the angular step as a dedicated `f32`, even though each test
promotes it to `f64`. Explicitly round it after the wrap and scale operations,
then multiply it by the float frame-tick value. That produces retail's
`fmr`/`frsp` chain and final `fmuls`; keeping the step in a `f64` causes an
extra conversion and changes its FPR. The completed 269-instruction function
is parked on only one four-instruction pointer-load scheduling tie (8 real
diff lines), after the documented scheduler-attempt limit.
