# Player BSS anchor matching (2026-07-29)

## Folded sibling-symbol displacements

`PLAYER.OBJ` anchors accesses to the player records on its first in-TU BSS
symbol, `potionicon_tab`, rather than on the semantic cross-TU `gPlayers`
symbol.  When the final address is the same but `fndiff` reports a relocation
and displacement mismatch, use the owning TU's actual pooled anchor.

For the position helpers, keep the induction base at `potionicon_tab` and fold
the `0xC40` distance to the records into each field displacement:

```c
u8* p = (u8*)potionicon_tab + i * 0x335C;
out[0] = *(f32*)(p + 0xC84);
```

Do not first form `potionicon_tab + 0xC40` as a typed `Player*`; MWCC emits a
separate `addi 3136`, while the target keeps `potionicon_tab` as the relocation
anchor and uses the full `0xC84`/`0xCA4` load offsets.

This made both `GetPlayerPos` and `GetPlayerColPos` byte-exact and is applicable
to the other near-matching `PLAYER.OBJ` functions currently gated on the same
BSS-pooling pattern.
