# `do_screen2d` exact match (2026-07-29)

`do_screen2d` in `game/ui/attract.c` is byte-exact (452 bytes) in the project
report, and the full linked DOL SHA-1 passes. `fndiff` shows only the cosmetic
name of one relocation whose address is identical: retail's `sFlags` is the
low word at `gControllerButtons + 4`.

The final real mismatch was the order of three SDA loads. A separate local for
the accumulator produced the desired order but enlarged the stack frame by
eight bytes. The matching source instead exposes a common subexpression:

```c
state = lbl_80344298;
lbl_8034422C = lbl_8034422C + gFrameTicks;
delta = gFrameTicks;
```

MWCC loads `state`, then the accumulator, then `gFrameTicks`, and reuses the
tick value for `delta` later in the function. This is useful when a named
temporary fixes scheduling but creates a debug/home stack slot: duplicate a
pure global read in source and let CSE retain it, rather than paying for the
extra local.
