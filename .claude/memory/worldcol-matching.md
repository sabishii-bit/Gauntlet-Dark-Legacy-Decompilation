# `worldcol.c` wrapper matching

The first seven world-collision entry points are byte-exact:

- `CameraCollide`
- `WeaponWallCollide`
- `EnemyWallCollide`
- `PlayerWallCollide`
- `FastWallCollide`
- `FloorPos`
- `FloorCollide`

Recovered interface details:

- `WeaponWallCollide`, `EnemyWallCollide`, and `PlayerWallCollide` receive the
  collision radius in `f1` and the from/to/normal pointers in `r3`/`r4`/`r5`.
- All three use the shared result record at `lbl_8023CA40`; its query buffer is
  at `+0x58` and its output normal is at `+0x88`.
- Their mode values are respectively 4, 2, and 1. Weapon collision uses mask
  `0x23E`; enemy and player collision use `0x13A`.
- `FastWallCollide` uses the fixed radius at `lbl_80345728`, mask `0x13A`,
  and returns a normalized boolean.

MWCC source-shaping techniques:

- A register-only pointer alias can still enlarge the debug frame. In the
  three ordinary wrappers, `f32* out = normal + 0` generated the desired
  `addi` copy but enlarged the frame from 32 to 40 bytes despite never
  spilling. Using the parameter directly retained the same target
  `addi r30,r5,0` and restored the 32-byte frame.
- A typed unused array can recover a silent frame reservation without adding
  instructions. `FastWallCollide` was otherwise instruction-identical with a
  32-byte frame; a function-scope `f32 collisionScratch[4]` reproduced the
  target 48-byte frame with no loads or stores.
- Chained zero assignment,
  `normal[2] = normal[1] = normal[0] = zero_constant`, makes MWCC load the
  shared zero once and emit three stores. Three independent expressions load
  the constant three times; a named float temporary can enlarge the frame.
- The same assignment-web rule applies across a local and a global:
  `result->current = hit = 0` made MWCC put zero directly in the long-lived
  `hit` register and use it for the store. Separate initialization and store
  produced a second `li r0,0`, leaving an otherwise exact `FloorPos` four
  bytes too long.
- The floor probes use two 16-byte collision-point locals while copying only
  XYZ. Modeling the unused fourth float recovered their 16-byte stride,
  correct stack offsets, and target frames. A further dead eight-byte local
  accounts for `FloorPos`'s 80-byte frame.
