# `attract.c` UI-shim work (2026-07-29)

## Verified exact gain

- `do_credits` (`0x80013D20`, `0x15C` / 348 bytes) is byte-exact.
- Full linked DOL SHA-1 remains clean.

## ABI technique

`DrawText` is variadic. An old-style declaration (`extern void* DrawText();`)
lets calls compile, but MWCC omits the target `crclr 4*cr1+eq`. Declare the
fixed arguments and trailing ellipsis:

```c
extern void* DrawText(int x, int y, int flags, u32 color,
                      const char* fmt, ...);
```

This supplied the one missing instruction in `do_credits`. It also exposed
that reconstructed `scroll_credits` calls had their arguments in the wrong
semantic order.

## `scroll_credits` reconstruction

Ghidra plus target assembly established the complete three-column algorithm:

- Carry one `y = credits_scroll` value through all three loops.
- Column lengths/steps are 92 at 21 pixels, 40 at 15 pixels, and 10 at
  15 pixels.
- Stop a column when `y < 0`; skip drawing while `y >= 384`.
- Bottom-edge alpha is `255 - ((384 - y) << 4)`, while top-edge alpha is
  `(16 - y) << 4`.
- The first column records `idx == 91 && y > 192`, and the function combines
  that with whether no line was drawn to choose its scroll increment.
- When `gFrameTicks != 0`, advance by three if the fast-scroll flag is set,
  otherwise by the computed result plus one.

The completed body now has the exact target instruction count (148/148) and
50 real diff lines. Those residuals are register allocation plus the
equivalent `addi`/`lwzx` versus `add`/`lwz` indexed-address shape; park them
instead of grinding.

## Symbol cleanup

Mapped the attract small-data constants:

- `credits_scroll` at `0x8034425C`
- `screen2dTextScale`, `titleModelName`, `numberedTextureFmt`
- `titleTexturePrefix`, `titleWindowZoom`, `creditsTextScale`
- `creditsModelName`, `gauntFontName`
