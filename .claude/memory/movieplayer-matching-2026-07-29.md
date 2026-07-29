# Movie-player matching: 2026-07-29

## Exact gains and behavioral names

- `0x800D9874` -> `MovieValidateFrameFormat` (312 bytes, exact). It validates
  dimensions, the `MVDV` source format, destination pixel format, and bit
  depth.
- `0x800DB91C` -> `MovieDecoderInitBuffers` (356 bytes, exact). It destroys old
  decoder arenas, allocates/alines the new buffers, and builds the 255-entry
  0x28-byte free list.

The Xbox PDB has no corresponding GameCube decoder module, so these are
conservative behavioral names supported by Ghidra and the call/data flow rather
than claimed original Xbox identifiers.

## Source-shape techniques

- Materialize sibling fields before a compound short-circuit test. Loading the
  destination width and height into `inputWidth` and `inputHeight` before the
  `width mismatch || (height mismatch && negative-height mismatch)` condition
  reproduces retail's two loads before the first compare.
- Materialize an allocation size before incrementing an unrelated global tag.
  In `MovieDecoderInitBuffers`, assigning `iVar3 = param_1[6]` before
  `lbl_803452AC++` schedules the size load between the tag load and tag update,
  exactly matching retail without volatile state.
