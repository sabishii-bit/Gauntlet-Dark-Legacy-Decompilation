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
