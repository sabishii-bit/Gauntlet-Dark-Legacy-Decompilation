---
description: "message/mb_poly/sounds_evt exact-match and symbol-recovery techniques"
---

# 33.50% closeout pass (2026-07-29)

## Exact source shapes

- `message.c/msgUpdate`: the four-player state scan exits as soon as it finds
  an active state (`1`, `2`, `3`, or `5`).  Expressing this as a `break`
  rather than incrementing conditionally fixes three branch targets while
  retaining the identical opcode stream.  Assigning the selected message box
  in its null test eliminates one `r0`-to-`r3` copy.
- `mb_poly.c/PolyXfrmVerts`: assign `v = &verts[i]` inside the `flags` test.
  MWCC then loads the flags directly through the saved pointer register instead
  of creating a caller-saved address and copying it.
- `sounds_evt.c/AudioPlayerBreath`: materialize the ternary sound ID in a named
  local before the call.  This preserves the retail `r0` result followed by
  `mr r5,r0`.
- `sounds_evt.c/AudioPlayerTurbo`: its sole mismatch was commutative `add`
  operand order.  Spelling player access as explicit pointer arithmetic gives
  the retail base-plus-index order, but normally makes MWCC CSE the derived
  player pointer and rotates the rest of the function.  A function-scoped
  `#pragma opt_common_subs off` prevents that CSE and makes the entire function
  byte-exact.  This is a useful narrow lever when the desired address spelling
  is known but CSE changes web lifetimes.

## Message globals recovered

`gMsgDescTable`, `gMsgBoxes`, `gTriggerCameraState`,
`gGameplayPauseTimer`, `gModalRenderDepth`, `gMsgIndex`,
`gMessageState`, `gMessageDelayIndex`, `gMessageDelay`, `gCurWorld`,
`gMessageActive`, `gMsgDescCount`, and `gMessageTimer` replace their
address-style labels in `symbols.txt`, source callers, and Ghidra.
