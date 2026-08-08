# Ghidra stub-to-exact workflow (2026-08-08)

## `SumnerSpeechEnd`: an empty 0x3B4-byte stub to exact C

The live Ghidra decompile was useful for recovering the high-level branch
tree, but `tools/gdl/fnasm.py game/world/tower SumnerSpeechEnd` was the source
of truth for prototypes, stack layout, and expression order. Reconstruct the
semantic body first, then use the compact target assembly to correct types and
local layout before doing register-level polishing.

Two layout facts collapsed the initial 141-line residual to exact:

- The target frame placed `f32 world[12]` at `sp+8` and a six-word effect
  scratch block at `sp+56`. MWCC allocates these arrays in reverse declaration
  order, so declaring the six-word block first reproduced both offsets without
  padding.
- Repeated target `addi state,0x2C` instructions identified a real temporary
  name buffer inside the tower state object. Modeling that field as
  `char effectName[0x20]` at offset `0x2C`, then using
  `state->effectName`, stopped MWCC from creating and mutating a separate
  pointer local. That removed the final two instructions and restored the
  target's saved-register mapping.

Small-data address types also mattered. The `"SHARD%d"`, `"RUNE%d"`, and
`"RUNE13"` symbols are scalar small-data anchors; declaring them as scalar
`char` objects and passing their addresses emitted the target SDA21 address
loads. Declaring them as incomplete arrays instead produced `lis/addi` pairs
and enlarged the function.

Validation sequence:

1. `fndiff --count` reported 237/237 and exact.
2. The full DOL checksum passed.
3. The report credited all 948 bytes, increasing overall matching by 0.09%.

This is a good template for the remaining large tower stubs: use Ghidra for
control flow and semantics, use `fnasm.py` for exact ABI/layout evidence, and
promote repeated raw offsets into verified struct fields before attempting
compiler-shape tweaks.
