---
description: "G3DPAD.OBJ reconstruction status and source-shape discoveries"
---

# G3DPAD.OBJ (`game/g3d/g3dpad.c`)

Pass completed 2026-07-28. The TU remains `NonMatching`, but all four
functions are translated and the full DOL is SHA1-green.

## Current status

- `G3DInitPadStatus`: exact.
- `G3DInitStickCurve`: opcode-exact (`fndiff real 0`); only private-pool
  relocation names differ.
- `G3DAnalogToStickXY`: 146/146 instructions, 24 real diff lines, all FPR
  register allocation in the index calculation.
- `G3DUpdatePadStatus`: 282/282 instructions, 72 real diff lines, all four
  repeated branchless-absolute-value expressions using the opposite scratch
  register pairing.
- `.sdata2` ownership is now claimed at `0x80349350..0x80349390`;
  `datadiff` verifies the emitted 0x3C bytes exactly with four zero pad bytes.

## Source-shape techniques

### Reuse a dead parameter instead of naming a new local

The analog converter's computed minor axis belongs in `rawY` after the
raw-X/raw-Y ratio has been consumed:

```c
rawY = (s32)((f32)index * (1.0f / 128.0f) * (f32)rawX);
magnitude = g3dSqrt((f32)(rawX * rawX + rawY * rawY));
```

A separate `minor` local made MWCC preserve `rawY`, use an extra caller-saved
register for the conversion, and cascade the difference through the function.
Reusing the dead parameter reduced the residual from 60 to 24 real lines.
General lesson: when target assembly destructively converts or overwrites a
parameter and later reuses that same register for a derived value, reflect
that lifetime transition in C instead of introducing a semantically cleaner
local.

### Delete a named CSE local

In `G3DInitStickCurve`, naming `divisor = 1.25f + ratio` forced a three-way FPR
rotation. Repeating `(1.25f + ratio)` in both expressions lets MWCC perform its
own CSE and emits the retail FPR allocation:

```c
x = (s32)(0.5f + 90.0f / (1.25f + ratio));
y = (s32)(0.5f + (90.0f * ratio) / (1.25f + ratio));
```

This changed the function from ten real diff lines to opcode-exact without
changing instruction count.

### Seed private constant-pool order with dead helpers

The retail `.sdata2` order is `72.0f, 1.0f, 0.5f, 90.0f, 1.25f`. Five
unreferenced static return helpers above `G3DInitStickCurve` seed MWCC's pool
in that order. The linker dead-strips their `.text`, while their literals
remain available to the live function and reproduce the target data bytes.
Always claim and verify the resulting section with `datadiff`; `fndiff`
normalizes private pool names and cannot validate values or order.

### Retail uninitialized state may be intentional

The octant `flags` byte in `G3DAnalogToStickXY` is intentionally uninitialized.
The target reads the incoming residue in `r9`; initializing it adds an
instruction and changes the behavior/shape. Keep the compiler warning and
document it rather than “cleaning up” the source.

## Do not repeat

- A separate `major` local for the raw-axis swap worsened the analog register
  rotation.
- Named/register float constants in `G3DInitStickCurve` preserved opcode count
  but expanded the register residual from 10 to 82 lines.
- An inline-asm replacement for only the four absolute-value comparisons made
  MWCC conservatively preserve more state, changing the whole pad-update
  allocation and adding eight instructions. A local asm island is not a
  viable fix there.
