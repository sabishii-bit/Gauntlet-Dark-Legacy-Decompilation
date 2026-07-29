# `new_player` exact match (2026-07-29)

`new_player` in `game/game/player.c` is exact (104 bytes). The full linked DOL
SHA-1 passes.

The final mismatch identified the byte at player offset `0xA8B` as a signed
sentinel:

```c
PF(p, 0xA8B, s8) = -1;
PF(p, 0x3358, s32) = -1;
```

Using `u8` made MWCC materialize a separate `255` constant. With `s8`, it
reuses the retail `li r0,-1` for the byte store and the following word store,
also restoring the surrounding zero register and move selection. When adjacent
stores use `-1` but one is emitted as `255`, audit the narrow field's signedness
before attempting statement-order or register-allocation tricks.
