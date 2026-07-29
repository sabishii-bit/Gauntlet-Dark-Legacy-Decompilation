# `ml_mem.c` path-shim matching notes (2026-07-29)

## Verified matches

- `get_path` (`0x800BFBA0`, `0xE0` bytes)
- `FileSize` (`0x800BF618`, `0xF4` bytes)
- `FileExists` (`0x800BF70C`, `0xE8` bytes)

Together these added 700 exact code bytes and three exact functions. The full
linked DOL SHA-1 check remained clean.

## Techniques

- The target path constants at `0x80348ED8..0x80348EED` are typed, named
  constants:
  - `mlmPathFmtWad[6]` = `"%s/%s"`
  - `mlmPathFmt[3]` = `"%s"`
  - `mlmExtDefault[5]` = `".ps2"`
  - `mlmPathSeparator[]` = `"/"`
- Declaring the format/default strings as fixed-size `const` arrays makes MWCC
  use the target `R_PPC_EMB_SDA21` address materialization. Leaving them as
  incomplete mutable arrays produces `lis`/`addi` absolute addressing.
- `mlmRootPath` is an inline eight-byte array at `0x80127DE0`, not a `char *`.
  Declare it as an incomplete array (`extern char mlmRootPath[]`) to retain the
  target absolute address materialization. A fixed `[8]` declaration makes
  MWCC incorrectly select small-data addressing.
- Branch order matters in the inlined `get_path`: test `wad != NULL` first and
  format the WAD path on the taken branch.
- `FileSize` needs an unused eight-byte local declared *before* its 256-byte
  path buffer. This grows the frame from `0x218` to the target `0x220` without
  moving the buffer from `r1+0x110`.
- `serve_io`'s loop condition is the post-increment form
  `while (i++ < 1 && served == 0)`. Its global slot wrap must be written as
  `if (++mlmCurFileSlot >= 1)` so MWCC compares the incremented temporary
  before storing it. Scoped `#pragma opt_common_subs off` prevents the entry
  zero from being kept in a third nonvolatile register for the wrap store.
  Together those two changes make `serve_io` exact (144 bytes).
