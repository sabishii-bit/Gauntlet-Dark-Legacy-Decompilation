---
name: message-renderer-reconstruction-2026-07-31
description: "MESSAGE.OBJ renderer behavior, canonical btext ABI, mapped globals, and MWCC stack/source-shape findings"
metadata:
  node_type: memory
  type: project
---

# Message renderer reconstruction

`msgPost` (`0x800A4A38`) and `msgDraw` (`0x800A5044`) are real implementations,
not queue/renderer stubs.  `msgDraw` can be recovered to the target's exact 314
instruction opcode shape from Ghidra plus target-object call setup.

## Canonical text ABI

The float scale is the first C parameter even though the Gekko ABI places it in
`f1` independently of the integer arguments in `r3+`:

```c
s32 StringTextHeight(f32 scale, s32 msg, s32 idx, s32 spacing);
s32 StringTextWidth(f32 scale, s32 msg, s32 idx);
s32 DrawStringText(s32 x, s32 y, u32 flags, u32 color,
                   s32 msg, s32 idx, ...);
```

Do not preserve the older scale-last declarations just because they produce the
same GCN register placement; they are wrong for the native-port goal.

The constant at `0x8034861C` is `1.25f`, used for the enlarged count text in
message 101.  MESSAGE owns sdata2 `0x80348618..0x80348620`.

`DrawStringText` and `DrawStringTextMLines` are variadic and therefore need the
variadic prototype to emit the target `crclr`.  `DrawStringTextMulti` is not
variadic.  Ghidra often shows stale `r9/r10` values as extra parameters: only
write varargs where the target explicitly loads them before the call.  In
particular, the middle line of the non-Japanese message-101 path has no varargs.

## MWCC stack lever

The target frame is `0x50`, with text color at `sp+0x0C` and a 24-byte scratch
area beginning at `sp+0x10`.  This source declaration shape recovers it without
emitting instructions:

```c
int scratch[6];
u32 color;
volatile u32 stackPad;
```

The unused volatile scalar occupies `sp+8`; reversing the remaining locals then
places `color` and `scratch` exactly.  This cut the normalized real differences
from 190 to 164 while preserving the exact 314/314 instruction count.

## Confirmed message globals

- `80344CA8 gMessageValue`
- `80344CAC gMessageTextArg`
- `80344CB0 gMessageFontFlags`
- `80344CB4 gMessageCenterY`
- `80344CB8 gMessageCenterX`
- `80344CC0 gCurrentMessage`

These names follow their assignment/use roles in `msgPost`, `msgDraw`, and
`msgUpdate`, and replace the temporary `gCA8/gCB0/...` aliases.
