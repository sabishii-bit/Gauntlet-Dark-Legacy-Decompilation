# `btext.c` font-init matching (2026-07-29)

`FontInitSpecial` (44 bytes) and `FontInitDefault` (56 bytes) are exact. The
full linked DOL SHA-1 passes.

## `FontInitSpecial`: recover forwarded parameters

The target preserves incoming `r3`/`r4` as `LoadFonts` arguments two and three,
then loads mode 13 into `r3`. The zero-argument skeleton was therefore a wrong
prototype, not a scheduling problem:

```c
void FontInitSpecial(void* def, void* def2)
{
    LoadFonts(13, def, def2);
}
```

Check call sites before treating shim arguments as dead. `attract.c` already
declared and called this function with the correct two-argument shape.

## `FontInitDefault`: distinguish a table from its first entry

The retail symbols at `0x80118AF8` and `0x80118B2C` are arrays of pointers. The
target loads the first word from each table before calling `LoadFonts`; passing
the array names directly instead passes their addresses. The matching form is:

```c
LoadFonts(0, gFontDefs8x8[0], gFontDefs[0]);
```

The symbol map now names the two data objects `gFontDefs8x8` and `gFontDefs`
instead of address placeholders. The same rule applies to the indexed loop in
`FontInit`: use `table[i]`, not `table`, when the target has a final `lwz` after
forming the table address.
