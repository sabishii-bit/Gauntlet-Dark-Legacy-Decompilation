# `PSX2.OBJ` complete match

`src/game/sys/psx2.c` is fully translated and linked from C:

- `.text` `8008BC50..8008BF88` — 824/824 bytes
- `.rodata` `801142F0..801143F8` — 264/264 bytes
- `.sdata2` `80347E70..80347E90` — 32/32 bytes
- `extab` `80006C50..80006C60`
- `extabindex` `8000AAC8..8000AAE0`

All three target functions are exact: `LoadVU1GameLogic`, `init_psx2`, and
`load_irx_args`.

## Recovered behavior

- `init_psx2` drives the retained IOP reboot/sync compatibility stubs, loads
  the standard `sio2man`, multitap, memory-card, pad, sound, and DCS modules,
  and marks the multitap driver unavailable when its load returns false.
- `load_irx_args` uppercases module names for the `cdrom` path, adds the PS2
  `;1` version suffix, retries module loading up to 100 times with a
  `0x100000`-iteration delay, and optionally raises a fatal error.

## MWCC techniques

- **Use the original string literals, not a hand-built base pointer.** MWCC
  pooled the literals into the exact 0x108-byte `.rodata` table and
  automatically kept its base in `r31`/`r30`. A manual
  `strings = lbl_801142F0` pointer added a redundant initializer copy and
  encouraged caching `base + 0x64`, making `init_psx2` four to eight bytes
  too long.
- **An inlined helper parameter can carry a byte without reserving caller
  stack space.** `fed_upper(char* text, char c)` places the cursor in `r3`
  and the loaded character in `r4`, matching the target loop. A caller-local
  `char c` enlarged `load_irx_args`'s frame by eight bytes.
- **Move a large array after scalar declarations to select its lower stack
  slot.** Moving `path[128]` after the retry/result/base locals changed its
  address from `sp+32` to the target `sp+28` without changing frame size.
- **Keep the delay bound outside the retry loop.** A long-lived
  `delay = 0x100000` plus a countable empty loop produces the target
  `lis`/`mtctr`/`bdnz`; declaring a decrementing delay inside emits an
  `addic.` loop instead.
- `init_psx2()` deliberately retains an old-style unspecified-parameter
  definition. Its caller passes a legacy boot argument, but the target body
  ignores it; naming the unused parameter changes MWCC allocation and breaks
  the match.
