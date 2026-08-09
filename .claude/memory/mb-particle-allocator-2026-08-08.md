# MB particle allocator matching

`allocPsysMem` (`0x800D1404`, 300 bytes) was brought from a behavioral
skeleton to byte-exact C.

## Structural recovery

- Do not address the pool members through the old split `gPool*` aliases.
  The retail function materializes `lbl_80128710` once and uses a
  `PsysMemPool` beginning at `lbl_80128710 + 0x24`. The Xbox PDB layout in
  `include/game/psys.h` is correct.
- The large-block arm creates a new `PsysMemBlock` at `block + alignedSize`,
  links it between the allocated block and its old successor, repairs
  `last`, and makes the split block the next-fit cursor.
- Splitting increments `alloc_cnt` and subtracts from `free_bytes`; it does
  not decrement `free_cnt`. Consuming the whole free block also decrements
  `free_cnt`.

## MWCC shape levers

- Spell the search as an infinite loop that breaks when the block is large
  enough. Advancing/wrapping and the exhausted-list return must be in the
  smaller-block arm. This emits the retail `bge` over that arm.
- Declare the live block pointer before the aligned-size scalar. That colors
  the block as `r7` and the size as `r8`, matching the retail function.
- Use an explicit `if (block->next == NULL) ... else ...` when updating the
  cursor. A ternary emits one fewer instruction and the wrong branch shape.

Final validation: `fndiff` exact at 75 instructions and the full DOL checksum
passes.
