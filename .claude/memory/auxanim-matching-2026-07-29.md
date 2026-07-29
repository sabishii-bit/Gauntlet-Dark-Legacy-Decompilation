---
description: "auxanim.c exact-match techniques and one parked register-allocation residual"
---

# auxanim.c matching notes (2026-07-29)

## Exact techniques

- `DoSpecialTexmods`: removing the named `TEXMOD*` and spelling most accesses
  as `special_texmods[i].field` restored the retail register colors.  MWCC then
  CSE'd the final counter address too aggressively; writing only the reset
  through `((volatile TEXMOD*)&special_texmods[i])->counter` prevented that CSE
  without adding volatile loads elsewhere.  This combination is byte-exact.
- `DoObjAnimation`: reassigning the pointer parameter with
  `nodes = &nodes[idx]` restored the target's web allocation.  The out-of-range
  default texture selection needed an auto-inlined helper which returns either
  `node->tex` or zero; the helper preserves the target's final return-value
  materialization while dead-stripping as a standalone function.

## Parked

- `InitOAnimList` can be brought to identical opcode and relocation order by
  using indexed `i * 0x28` record access, separate halfword temporaries,
  `s[0] | (s[1] << 8)`, a 16-byte stack pad, and scoped
  `#pragma opt_propagation off`.
- The remaining difference is a three-way nonvolatile-register rotation:
  retail `{index, format, -1} = {r26,r27,r28}`, compiler output
  `{index, format, -1} = {r27,r28,r26}`.  Declaration reorder, nested scope,
  named format, and result-temporary variants did not resolve it.  Per the
  iteration-limit policy, leave the original implementation in the TU and
  revisit only with a genuinely new allocation lever.
