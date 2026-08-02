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

A later `camera_mode_dest` pass raised the function from about 91.20% to
94.93%. Parenthesize the radius decay as `ticks * (rate * difference)`;
left-associating it as `ticks * rate * difference` produces an equivalent but
different FPR web. Spell the trigger-direction length as three independent
squares and two additions, then use `guess * guess * distance` in every
reciprocal-root correction and group the final result as
`distance * (half * guess * correction)`.

Three volatile-qualified stores of the weighted trigger vector prevent MWCC
from retaining its first component across construction of the second vector.
This recovered the retail instruction count for that variant and improved the
linked fuzzy score even though the residual register diff became noisier.
For wrapped yaw, preserve a separate `rawAngle` and explicitly assign the
wrapped result in all three arms; this restores retail's default-arm `fmr`.
Write commutative threshold tests in the same operand orientation as retail
(`yawDelta <= yawStep` / `yawDelta >= yawStep`). Finally, cache the target and
current pitch globals in distinct scalars before the late approach step. That
last source-identity change alone recovered most of the tail's FPR allocation.

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

## Treat an apparent void helper as unfinished until every exit is decoded

`camera_collide_step` originally looked like a void rail updater because the
decompiler lost the tail at `_restfpr_27`. Raw disassembly showed a common
tail that takes the absolute angular distance from pi and returns `-1` or `0`.
Changing the prototype to `(s32, f32) -> s32` also exposed three early zero
returns. Do not accept a void prototype merely because callers ignore `r3`;
inspect every predecessor of the restore thunk and the instructions directly
before it.

The same function demonstrates how one scratch overlay can reproduce several
unrelated compiler homes without inventing fake behavior. Its address-taken
floats occupy retail stack `+0x28`, `+0x2C`, `+0x30`, `+0x34`, and `+0x38`,
while `PointLineColl` writes its closest point at `+0x40`. Those slots cover
the final wrapped angle, two late square-root spills, the trigger-distance
root, the bit-cleared vertical delta, and the output vector. Keep the root
members volatile so MWCC retains the retail store/reload pairs.

For trigger-camera selection, the active polarity is zero, distance is from
`attn_dest_no_offset`, and the score is Euclidean distance plus the absolute
vertical delta. The previous selected trigger is evaluated separately, then
the two candidates may be swapped before the blend. Recovering those details
raised the function from roughly 23% to 64% fuzzy before register-allocation
tuning; the key progress came from rebuilding the state machine, not from
chasing isolated instructions.

`camera_mode_orbit` also shows that algebraically identical update syntax can
change the whole volatile-FPR web. Materializing the three scaled components,
using `zero *= tickScale`, and writing the X update as
`scaledX + positionX` reduced a 34-line register diff to one commutative
`fadds` operand-order tie. Combining conversion and scaling as
`scale * (f64)(u32)ticks` let MWCC reuse the dead conversion constant register,
matching the retail `fsub`/`fmul` web. Prefer these source-level accumulator
forms before accepting a broad register-only residual.

`camera_mode_spin` benefited from variable-identity reuse across disjoint
phases. Replace separate `delta[3]` and `offset[3]` arrays with three scalar
deltas, then reuse those same scalars for the later orbit offsets. MWCC carried
the X/Z affinities into the post-square-root update and removed the large FPR
rotation. Preserve the retail frame separately with dead padding; splitting
the original 24-byte array footprint into a 20-byte gap plus an 8-byte tail
put the volatile root at stack `+0x14` while retaining the 72-byte frame.

For the reciprocal-square-root refinement, associate every correction as
`guess * guess * distance`, not `distance * guess * guess`. The former emits
retail's `fmul guess,guess` followed by `fnmsub distance,...`; the latter
creates a different but equivalent `distance*guess` accumulator web. Together
with a two-stage squared-distance sum, this reduced the mode from 68 to 8 real
diff lines.

`camera_collide_step` reached the retail 444-instruction length by removing
four code-generation artifacts. Share an early zero result with the final
false return through a common label so MWCC tail-merges both exits. Build each
three-component squared distance as staged assignments (`y*y`, then `x*x +`
the accumulator, then `z*z +` it); a single expression made MWCC preserve the
root input with an extra `fmr` at both square-root sites. Finally, read the
immutable zero and maximum-distance globals through volatile-qualified float
pointers. That makes MWCC load each value directly into its long-lived FPR
home instead of loading through a temporary and copying it. These changes
removed the final five extra instructions without altering behavior. The
remaining selector delta is register coloring and scheduling, so exact length
is a useful stopping checkpoint even though the fuzzy score remains about
90.26%.

## Recover stack layouts from reverse declaration allocation and overlays

`camera_mode_target` exposed a complete local layout from its stack accesses.
MWCC allocated its address-taken declarations in reverse source order: final
direction at `+0x10`, final position at `+0x1C`, final attention at `+0x28`,
normalization at `+0x34`, moving direction/position/attention at
`+0x40/+0x4C/+0x58`, two root spills at `+0x64/+0x68`, the matrix at `+0x6C`,
the local offset at `+0xAC`, and the movement vector at `+0xB8`. Reordering
the declarations to that reverse sequence recovered every displacement. The
late `WorldVector` output also belongs in the now-dead movement-vector slots;
using a separate transformed vector prevents the retail lifetime overlay.

A four-byte leading field inside the lowest direction aggregate moved all
live locals up one word without emitting code. A separate dead eight-byte
array then enlarged only the aligned frame, recovering the retail 240-byte
frame and its compiler conversion homes. For the two inline square roots,
independent squared-component temporaries produced retail's three `fmuls`
plus two `fadds`, while the `guess * guess * distance` association aligned the
entire refinement opcode stream. Express the sign split as `if (value < 0)`
with the negative arm first; this emits retail's direct `bge` to the positive
arm instead of a `cror`-based greater-or-equal test and reversed block order.
These changes brought the function to the exact 452 instructions and about
98.93% fuzzy, with only register/scheduler differences remaining.

`camera_mode_level` confirmed that declaration order can control the entire
saved-GPR web even when it does not alter stack layout. Moving the initialized
level-camera pointer below the short-lived loop declarations placed it in the
retail `r27`; moving the player cursor after the loop counter restored its
retail `r30`. Reusing the later `tries` counter for the initial four-player
scan removed another artificial variable lifetime. For member pointers that
retail derives from the TU state base, spell the known state offset directly
(`state + 0x1C8`) instead of deriving it through a typed camera local; this
keeps the canonical base available to the allocator.

The last reciprocal-root expression needs a grouping that is stricter than
the refinement steps: use `distance * (half * root * correction)`. A flat
`distance * half * root * correction` lets MWCC precompute `distance * half`
before the refinement chain, rotating its FPR web even when every intermediate
Newton step is otherwise associated correctly. Materializing a camera's
`+0xC8` adjustment before `ProcCamera` likewise restores the retail pointer
lifetime; applying it only after the call lets MWCC fold the adjustment into
all subsequent field displacements. The remaining one-instruction length
delta is an address-form tie: retail emits `addi` plus `lfs` for
`gDefaultPlayerPosition`, while MWCC currently folds the same access into
`lfsu`. The function is parked at 581/582 instructions and about 96.08% fuzzy.

`debug_camera_pos` showed that a contiguous retail stack layout does not
necessarily come from one source aggregate. Keeping the projected screen pair
inside the scratch structure made MWCC hoist its address into a saved GPR for
the whole player loop. Splitting the same footprint into an eight-byte dead
pad, a standalone `s16 projected[2]`, and the remaining scratch structure
preserved every absolute stack offset while making MWCC rematerialize the
stack address at each projection call, as retail does.

Retain constant camera indices as named integer locals when the function also
uses them for address arithmetic. The explicit value `5` reconstructs retail's
indexed matrix address instead of collapsing it to `state + 0x888`. An
explicit typed `sourceCamera = state + 0xC8` similarly recovers the target's
temporary `addi` followed by the call-argument `mr`. Reading the late saved
attention mode directly from `state + 0x97C` keeps the state base live in its
retail saved register. Finally, reuse the earlier saved-pitch scalar for the
later radius lifetime and cache the step-limit global in the existing scale
scalar. Together these source identities brought `debug_camera_pos` from
422/427 instructions and 221 real diff lines to the exact 427/427 length with
78 real lines, while matching its stack frame and saved-GPR layout.

`do_camera` demonstrates that assigning the same C pointer or counter again
later can still make MWCC build one function-long register web. A single
`CameraTarget*` reused for the pre-camera and post-camera projection loops took
retail's `r30`, displacing the projection matrix and every loop register.
Split it into `projectedTarget` and `limitedTarget`; likewise give the three
logical loops distinct counter identities. Then declare the long-lived matrix
pointer immediately after the state base, before those counters. MWCC keeps
the matrix in retail `r30`, coalesces the short target cursors in `r28`, and
uses the expected loop register. Reusing the earlier `moving` scalar as the
camera loop's zero value removes the last broad coloring difference. This
reduced the exact-length 247-instruction function from 116 to 4 real diff
lines and raised it from about 98.745% to 99.271%; the residue is only the
three-instruction loop-initializer schedule.

`camera_debug_supervisor` confirmed three more source-shape tells. In each
Newton refinement, write the correction product as `root * root * distance`,
and group the final result as `distance * (half * root * correction)`. This
restores retail's multiply/FNMSUB dependency chain. For an outside-of-screen
test that follows an integer-coordinate equality test, spell the range as
`!(value > low && value < high)`; the algebraically equivalent pair of
`<= || >=` comparisons makes MWCC emit two IEEE-safe `cror` sequences instead
of retail's direct `ble`/`blt` branches.

When target assembly materializes two field addresses before a long floating
calculation and dereferences them much later, retain two typed pointer locals
even if direct byte-offset loads are shorter C. Here, separate pointers for
the camera X and Z fields recovered the retail `addi` homes and improved the
later boss-camera register web. The two square-root results also really pass
through the known volatile stack slots; combining these field aliases with
the stack spills improved the supervisor from about 90.37% to 92.54% (566 to
474 real diff lines). Either change alone was weaker, so test related lifetime
evidence as a set rather than rejecting one clue solely on instruction count.

`camera_run_mode` showed why reproducing absolute stack offsets with one large
scratch aggregate can still poison code generation. Ghidra's accesses came
from independent address-taken vectors and scalar spills. Declaring those
locals separately, in reverse stack-allocation order, and putting an otherwise
dead eight-byte frame pad first preserved the retail 360-byte frame and every
local displacement while ending the false function-long pointer lifetimes.
That removed eight extra instructions across four switch arms. Keep the root
results as independent volatile scalar spills; sharing them reconnects their
register webs.

Two control-flow and expression-order details then removed most of the
remaining delta. A failed `MoveCam_walk`/`init_game_cam` exits its switch arm,
so use `break`, not an early `return`; MWCC then emits the retail epilogue and
continuation branches. For repeated camera distances, assign X before Y and
Z, but form the sum as `z*z + (x*x + y*y)`. MWCC schedules the loads in retail
order and reuses the Y register destructively through the first multiply.
For the initial delta distance, `distance = z*z; distance += x*x + y*y;`
preserves the target instruction count and leaves only a two-register coloring
difference. Finally, keeping `gPlayers` in a named pointer local makes MWCC
retain the array base across the object-player loop, as retail does. Together
these changes reduced `camera_run_mode` from 839 instructions/408 real diff
lines to the exact 831 instructions and 28 real diff lines.

The later `camera_mode_follow` pass supersedes the earlier recommendation to
keep its entire 0x160-byte scratch area as one aggregate. The aggregate found
the frame, but it also tied every address-taken vector into one source object
and rotated almost the whole function. Split the members back into independent
locals in descending stack-offset order, retaining explicit byte gaps at the
top and bottom. This keeps the retail 512-byte frame and all offsets while
letting each `StandardCamera`/`DoShake` expansion rematerialize its own stack
addresses.

For large supervisors, split repeated loop counters even when their live
ranges do not visibly overlap in C: three uses of one `player` variable made
MWCC build a function-wide saved-GPR web. The final viewport loop specifically
wants both a typed `Player*` advanced by one structure and a 0..3 counter, plus
a named projection-matrix pointer. Declare the matrix before the camera, then
declare the final-loop offscreen flag before the player pointer and counter;
MWCC colors them as retail's r29/r28 and r27/r26/r25 set. A local cached target
history count likewise prevents three invented global reload/index sequences.

Two apparent redundancies in the target were source-shape evidence: the idle
transition guard rechecks the already-zero request flag, and nests another
`camIdx == 0` around the timer increment even though the outer condition has
the same test. Restoring both and spelling the threshold as `>= 90` repaired
the physical state-machine layout. Finally, use the established
`root * root * distance` Newton association and group the last multiply as
`distance * (half * root * correction)`. These changes moved
`camera_mode_follow` from 1053 instructions/1093 real diff lines (about 84%
fuzzy) to 1049 instructions/401 real diff lines (about 91.49% fuzzy), and the
camera TU from about 95.31% to 95.96% fuzzy.

`camera_mode_level` showed that a pointer kept alive for a late three-float
copy can rotate nearly every saved GPR in the second half of a function. The
typed `cam0` local was only needed late to copy its attention into camera 3;
reading the same three fields from their mapped `gCameraState` offsets ended
that lifetime after camera 0 setup. Declaring the typed `Player*` cursor before
the shared loop counter then recovered the retail prologue coloring, while
moving the late camera index before that cursor gave it retail's r30.

Do not keep a zero scalar alive across `CreateYPRMatrix` merely to initialize
two stack floats afterward. Retail reloads the zero constant after the call;
using the short-lived scalar only for camera fields and the global for the two
post-call stores avoids an unnecessary saved FPR and restores exact function
length. Finally, component order can matter even for an equivalent squared
distance: assigning Y before X and forming `y*y + x*x + z*z` best reproduced
this function's FPR web. Together these changes reduced `camera_mode_level`
from 581/582 instructions and 269 real diff lines to the exact 582/582 length
and 92 real diff lines before report scoring.

In an exact-length function, scalar declaration position can select the SSA
destination register even when statement order and stack layout are unchanged.
`camera_mode_target` declared its reused `distance` before all component and
angle scalars; moving only that declaration below `dx`, `dy`, `dz`, and the
three squared-component temporaries made MWCC retain the retail final-distance
FPR through both square-root chains. The function stayed exactly 452
instructions while dropping from 86 to 50 real diff lines. Converting the
player cursor to the mapped `Player*`/`Player.state` representation was codegen
neutral, so semantic cleanup can accompany this kind of allocation fix safely.

For command wrappers around a large global aggregate, the first member-address
expression can decide whether MWCC constructs the aggregate base directly in a
saved register or creates an extra pre-prologue copy. In `sndCmd8`, declaring
the `SndState*` without initializing it, calling `memset(g.in, ...)` directly,
and assigning `s = &g` immediately afterward removed the extra address copy and
made the function byte-exact. The pointer remains the ordinary typed aggregate
base for every later field access, so the source stays portable.

Always run the dual-compiler probe before polishing a mixed-era game TU.
`soundmgr.c` under the demo flags matched 26 of 30 functions with vanilla
`GC/1.2.5`, versus fewer under inherited `GC/1.2.5n`; selecting the compiler per
object recovered several exact wrappers without source contortions. In the same
TU, caching `&gBig` as a typed `BigState*` made the initializer retain the
retail aggregate base across its array clears and reduced `sndSysInit` from 64
to 22 real diff lines.

A branch-local conditional initializer can control how MWCC delivers a value
to an immediately following call. In `sounds_evt.c`, declaring `int id` inside
the guarded block and initializing it with a ternary made MWCC load both table
choices into a temporary register and then move the selected value into ABI
argument register `r3`, matching retail exactly. A neighboring function needed
that form in only one arm; keeping ordinary `if` assignments in the other arm
allowed MWCC to load its table value directly into `r3`. Mixed source shapes
inside sibling branches are therefore useful when retail uses different value
coalescing for otherwise similar calls.

Nested self-assignments can also steer a single commutative operand without
adding instructions. `camera_mode_orbit` was otherwise exact, but spelling
the X update in its natural reversed order rotated the preceding float
conversion temporaries, while the original order emitted the final `fadds`
operands backwards. This portable form matched both constraints:

```c
cam->wpos[0] = cam->wpos[0] + (cosineInput = cosineInput);
```

MWCC keeps the old position in the retail temporary and splits the right-hand
value's SSA/coalescing web; the self-assignment disappears completely. The
same family of source shape associates squared distances without extra code:
`distance = z*z + (distance = x*x + y*y)`. In `camera_run_mode` and
`camera_mode_spin`, that nested result assignment recovered the retail FPR
webs where ordinary `distance = x*x + y*y; distance = z*z + distance;` did
not. Validate each instance with `fndiff --clean`, since operand order and
declaration changes around it can still rotate neighboring temporaries.

A plain self-assignment can also change allocation order for two long-lived
integer pointer webs. `GetMaxPlayerModelSize` had retail's slot-zero maximum
pointer in `r6` and the current-player-slot pointer in `r7`; the otherwise
identical translation colored them in the opposite registers. Adding
`player_multiple_models[0].model_max = player_multiple_models[0].model_max;`
immediately after computing the current slot emitted no instruction, but made
MWCC allocate every pointer exactly like retail. This recovered all 420 linked
bytes. Keep this form close to the first real access of the self-assigned field:
placing a self-assignment on the cursor itself moved that cursor all the way to
`r31` and made the mismatch worse.

In unrolled aggregate loops, naming both sides of a copy can recover several
registers at once. `MBNewPoly` initially rotated its source base, destination
base, and the two induction offsets even though the loop was structurally
exact. Giving each iteration `f32* in = &verts[i * 3]` and
`PolyVert* out = &dv[i]`, then accessing `in[0..2]` and `out->...`, reproduced
retail's pointer and counter coloring. Its preceding zero-fill loop also needed
the already-dead `free` local reused as `flags = (free = 0)`; this made MWCC
place the integer zero in retail's register without adding code. Together the
two changes recovered all 608 linked bytes.

When a loop walks two static arrays with independent byte offsets, spelling a
named base pointer can trigger MWCC strength reduction: the compiler replaces
`base + offset` with a pointer induction variable and removes the offset web.
Using the static array name directly at the access site retains a hoisted base
register plus the independent offset, matching the retail instruction shape.
This moved the translated `fn_80011BBC` atree wrapper from a pointer-IV shape
to the target's two-base/three-counter loop. Its remaining mismatch is register
coloring plus one address CSE, so do not re-test compiler versions or flag
presets; both axes are already neutral for this class.

For call-argument scheduling, split a rematerialized constant from an existing
long-lived local even when both have the same value. `beginSaveTransaction`
keeps `size = 0x310000` live in `r30` for the subsequent heap-end calculation,
but retail independently materializes the same value into argument register
`r4` before forming the first argument in `r3`. Passing
`(transferSize = 0x310000)` from a separate, otherwise dead local preserved the
retail `lis r4,49; addis r3,r31,-49` order; passing `size`, assigning `size`
again, or using only the literal all chose a different schedule. The temporary
emits no instruction or stack growth after optimization.

For a large function whose opcode stream is already identical but a late
block still has volatile-register coloring differences, test a function-local
`#pragma opt_propagation off` before reshaping the block itself. In
`camera_run_mode`, that pragma corrected the final player-search loop across a
3,324-byte function, but initially changed one earlier floating-point multiply
from `f1` to `f3`. Staging the double expression through an already-live `f64`
local before converting it to `f32` restored the retail `fmul f1` / `frsp f3`
pair:

```c
stepDouble = 0.08333333 * (15.0 - (f64)cam->radius);
rate = (f32)stepDouble;
```

The combination made the whole function byte-exact. This is especially useful
when `flagsweep.py` reports `-opt noprop` as the only variant that improves a
register-only residual: use the pragma for the coloring, then repair isolated
propagation fallout with typed staging temporaries.

## Callback ABI and volatile-web declaration order

An indirect call can reveal a missing callback parameter even when the callee
type is otherwise unknown. Every `soundmgr.c` node callback site already left
the owning `Node*` in ABI argument register `r3`; declaring the callback as
`void (*)(Node*)` and passing the node preserved the exact sites and changed
`sndSysUpdate` from a 16-line register rotation to byte-exact. Audit the live
argument registers at an indirect call before accepting a no-argument
prototype from a decompiler.

For a no-call loop, `#pragma opt_propagation off` can make declaration order
directly determine volatile GPR coloring. `SetPlayerVars` needed its long-lived
locals declared as base pointer, byte offset, cached boss type, entry pointer,
then loop index; this produced retail's `r7` through `r11` allocation. Reusing
the offset's initial zero for the two preceding global clears kept that web
live and avoided a separate rematerialized zero. This combination recovered
all 65 instructions exactly; declaration shuffles without `noprop` did not.

`flagsweep.py` can also identify a narrowly useful `-opt nocse` result. For
`list_insert_size`, naming the repeatedly loaded `*link` value as `current`
gave the traversal node its own register web, while a function-local
`#pragma opt_common_subs off` prevented MWCC from folding that web back into
the head pointer. Neither change was sufficient alone; together they matched
all 42 instructions. Prefer this scoped pairing over changing the TU-wide
compiler flags when only one intrusive-list traversal needs the older shape.

## Recovering `li` + `and` from a low-half 64-bit flag test

When retail loads a 32-bit flag symbol but then emits `li mask; and` where a
direct `s32_flags & constant` reconstruction emits one `rlwinm`, inspect the
adjacent symbol layout before trying optimizer flags. In `sndSysSync`,
`sFlags` at `0x803445CC` is the low half of the 64-bit
`gControllerButtons` word at `0x803445C8`. Writing
`gControllerButtons & 0x20` made MWCC eliminate the unused high half while
retaining the original `li 32; and` sequence. The object relocation is named
`gControllerButtons+4` instead of `sFlags`, but both resolve to the same
address and the linked function is byte-exact. Treat this target sequence as
source-type evidence, and use the containing 64-bit symbol rather than a
volatile mask or a TU-wide optimization change.

## Combining scoped no-CSE, inlined argument staging, and frame balancing

`pbCameraUpdate` had three independent residual classes: an extra `fmr` in its
square-root path, reversed load order for the two `atan2` arguments, and
swapped FPR identities in two float/double scale expressions. Treat these as
separate levers. A function-local `#pragma opt_common_subs off` removed the
structural `fmr` and the two unwanted `lfsu` forms. A tiny `static inline`
two-argument wrapper, compiled with `#pragma dont_inline off`, preserved the
retail left-to-right `lfs f1` / `lfs f2` argument staging without emitting a
real helper. Finally, assigning each double constant to a reused `double`
temporary and the field value to a reused `f32` temporary produced retail's
`lfd f0; lfs f1; fmul f0,f1,f0` web.

Inlining and typed staging change MWCC's dead parameter/local home space even
when they add no instructions. Rebalance the pre-existing unused byte arrays
only after the opcode/register diff is zero: here replacing the old 4+12 byte
pads with one 8-byte tail pad kept both the 64-byte frame and the volatile
float rounding slot at `20(r1)`. This combination recovered all 124
instructions (496 linked bytes) exactly.

## Typed overlays for indexed records and duplicated float conversions

Flattening a repeated record access into `base + constant + index * stride`
can leave an otherwise exact function with reversed source operands on PPC
`add`, `cmpw`, and `and.` instructions. In `player_get_powerup_state`, model
the 16-byte power-up record and a padded player overlay explicitly, then use
`overlay->powerups[j].field`. MWCC preserves the retail `base + index` tree
and folds the field displacement into the following load/store. A block-local
byte pointer plus nested `if` statements similarly preserved the retail
left/right operand encoding in the search loop; the equivalent compound
expression canonicalized both operands in the opposite order.

Also inspect conversion duplication before caching a result. Retail converted
the same timer float to integer twice and stored the two `fctiwz` results in
separate 8-byte stack slots. Repeating the cast in the comparison, instead of
testing the cached integer, reproduced both stores. Declaring the player alias
as `register` prevented the otherwise dead source alias from adding an 8-byte
home slot, keeping the retail 40-byte frame. Together these source-shape clues
recovered all 54 instructions exactly.

## Pair named loop floats with propagation-off for FPR identity

When an invariant float threshold and a per-iteration field load are swapped
between `f0` and `f1`, a propagation pragma alone may be insufficient if both
values are still anonymous expression temporaries. In `AudioFindPlayerSlot`,
declare both `value` and `threshold`, assign the field first and the global
threshold second inside the loop, and compile only the function with
`#pragma opt_propagation off`. LICM still hoists the invariant global load,
but the named assignment order survives long enough to give it retail's `f0`
identity and the field load `f1`. This matched all 24 instructions exactly;
either the named locals or propagation-off by itself left the original FPR
swap unchanged.

## Explicit zero initialization can recover a retail data-backed table

If a function's instructions are semantically correct but its target uses a
`lis/addi` relocation into `.data` while the reconstruction addresses a TU
`.bss` anchor, inspect the target bytes before changing the code. The saved
atree-list table at `0x80118128` is 0x60 bytes of explicit zeros in `.data`.
Writing `= {{0}, {0}, {0}}` on the source aggregate makes MWCC emit the same
data-backed object; leaving it tentative places it in `.bss` and changes the
entire address tree.

`AtreeInitLists` also demonstrates that auto-inlining is not uniform between
adjacent callers: `AtreeSetEmpty` remains a real call here even though MWCC
normally inlines its loop body. A function-scoped `#pragma dont_inline on`
restored the call and removed 23 extra instructions. `matchtool probe` reports
the resulting 36-instruction function as `OK~` (only meaningful-vs-`lbl_`
relocation names differ), and objdiff counts all 144 code bytes exact.
