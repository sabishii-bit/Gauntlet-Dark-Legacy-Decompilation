# OPTIONS string-pool matching (2026-07-29)

`start_audioslider` in `game/ui/options.c` is exact (208 bytes). The full
linked DOL SHA-1 passes.

The instruction stream was already correct, but C string literals created a
new compiler-local pool with different addends. Retail pools the four slider
asset names from the OPTIONS rodata anchor at `0x80113830`:

```c
char* strings = optionsStringPool;
s->empty = MBNewBlit(strings + 520, 0, 0);
s->pink  = MBNewBlit(strings + 532, 0, 0);
s->ml    = MBNewBlit(strings + 544, 0, 0);
s->mr    = MBNewBlit(strings + 556, 0, 0);
```

The fifth name, `"slider"`, belongs to the TU's SDA2 string pool and should
remain a literal. When a function is opcode-exact but every `addi` from one
rodata base has a consistent pool-layout displacement, map the retail anchor
and express its known offsets rather than padding or duplicating unrelated
strings. This makes the reconstruction relocatable and avoids hardcoded
absolute addresses.

## `finish_optmenu`: shifted BSS subobjects

`finish_optmenu` is exact (320 bytes). Retail relocates the four-pointer option
stack as the BSS object at `optglobals + 0x40`, so its loop uses a shifted
HA/LO address and `stw 0(r4)`. Accessing the member through the full aggregate
instead produced an `optglobals` relocation and `stw 64(r4)`: the final address
was equivalent, but the instruction bytes were not.

The symbol map now splits the known `0xA0` OPTIONS BSS range into:

- `optglobals` at `0x80274E00`, size `0x40` (text scratch);
- `optionsStack` at `0x80274E40`, size `0x10`;
- `optionsAudioAndPrefs` at `0x80274E50`, size `0x50`.

Using `optionsStack[i]` restores the shifted relocation in both clear loops.
For aggregate-member mismatches where target and base calculate the same final
address using different displacements, check whether retail emitted a distinct
subobject symbol before trying register or scheduling changes.
