# `mb_font.c` width-helper notes

`0x800B5AD8` and `0x800B5B00` are the PDB functions `MBFontHeight` and
`MBFontStringWidth`. The BSS array at `0x8029E3C8` is the PDB global
`mbfont_space` (35 signed widths), not a font-name pointer table.

`MBFontStringWidth` is fully translated at 115/115 instructions. Its behavior:

- clamps a negative current-font index to zero;
- walks unsigned bytes and accumulates glyph widths;
- interprets `*A` through `*Z` as a font-height-sized control sequence;
- for flagged fonts, accepts digits, maps `.`/`-` to glyphs 58/59, and consumes
  the byte following an extended byte;
- asks `mbBlitCalcX` for each cell width and applies the current X scale; and
- falls back to `mbfont_space[current_font_index]` for a zero-width space.

Codegen techniques:

- A `switch (ch)` with one `'*'` case preserves the target's peephole-off
  `beq` followed by an explicit `b`; a plain `if` folds it into one `bne`.
- Test `*(u8*)s != 0` to obtain the target's `cmplwi`, avoiding signed-char
  `extsb.`.
- Write ordinary glyph scaling as `(f32)x * scale` to preserve the target
  `fmuls f0,f0,f1`; the special height arm uses `scale * height`.
- Cache `mbfont_space` in a base local to hoist its address and obtain the
  correct frame/register count.

The remaining differences are register allocation plus two equivalent address
forms for the font table (`add+lwz imm` versus `addi+lwzx`). The opcode count is
exact; park unless a new reassociation barrier is found.
