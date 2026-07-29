# `gamemain.c` loading-HUD match (2026-07-29)

`fn_800521E8` (`0x800521E8`, `0x100` / 256 bytes) is byte-exact.

## Register/web technique

The target keeps the long-lived timer index in `r30` and the game-busy flag
in `r29`. Declaring `idx` before the initialized `flag` rotates those two
webs into the target registers without changing the algorithm.

The final nested expression

```c
((u8*)((void**)txt)[4])[idx] = 0;
```

made MWCC choose `r0` for the data pointer and `r3` for zero. Naming the
interior pointer first:

```c
textData = ((void**)txt)[4];
textData[idx] = 0;
```

selects the target `lwz r3` / `li r0` / `stbx r0,r3,r30` sequence. That named
pointer adds an eight-byte stack home even though no spill instructions are
emitted, so the existing dead pad must shrink from 16 bytes to eight to retain
the target 40-byte frame.
