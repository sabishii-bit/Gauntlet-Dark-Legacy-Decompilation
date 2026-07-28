# `sounds.c` music-helper notes

## `AudioMusicVolUpdate`

The 0x800A0FDC function is now opcode-identical (76/76). It:

- exits while audio is inactive or no select stream is open;
- refreshes level-stream state;
- selects either a three-step subtrack adjustment, the scaled fade volume, or
  the global music volume;
- slews the current volume by at most eight units per update; and
- passes the level-scaled result plus the unscaled target to `AudioDeferSlot`.

With peephole optimization disabled, combine neither early exit into an `||`.
Two separate `if (...) return;` statements emit the target's direct `beq`
followed by direct `blt`; the combined expression emits an extra `bge/b`.

## `AudioBuildMusicName`

The 0x800A1614 helper builds rotate/stop cue names for the six material names
at `lbl_801232DC`: `MET`, `ROPE`, `CHAIN`, `ICE`, `STONE`, and `*ROCK`.
The `*` entry uses `S_%sROTATE`/`S_%sSTOP`; ordinary entries use the
`S_ELV...%c[B]` variants selected by the current level.

Recovered layout:

- cue pairs are written out-of-bounds immediately after
  `sSpeechNameBuf`, beginning at `buf + 64`, stride eight;
- a local `{ -1, -1 }` aggregate retains the first valid rotate/stop pair;
- using one string-table base local reproduces the target's single preserved
  base register and immediate offsets;
- declaring `material` before the loop indices produces the target preserved
  register order.

Current result is 112/115 instructions. Remaining differences are a base-local
materialization, an `addi+lwz` where target uses `lwzu`, and associated
scheduling. The semantics and all branches/calls are recovered; do not retry
without a new pointer-update or base-local codegen lever.
