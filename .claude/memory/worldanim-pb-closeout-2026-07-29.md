# World-animation and PB close-out batch (2026-07-29)

## Verified exact gains

This batch added four exact functions and moved the full build from
1,981 / 351,904 exact functions/bytes to 1,985 / 352,508:

- `pb_winglobals.c`: `fn_800C0BD4`
- `mb_particle.c`: `MBPsysSetEVolume`
- `audio.c`: `AudioGetSoundVol`
- `gamemain.c`: `FindWobjWanim`

`build/GUNE5D/main.dol` still passes the configured SHA-1 check.

`pb_winglobals.c` is now 16/17 exact. Its remaining function,
`fn_800C1004`, has matching instructions and control flow but 38
register-allocation diff lines, so the TU must remain nonmatching.

## CodeWarrior techniques

- When MWCC produces an unexplained eight-byte stack-frame increase around
  floating-point parameters, verify the source ABI. Changing
  `MBPsysSetEVolume`'s parameters from `f64` to `f32` removed the extra frame;
  the local computation can still use `f64`.
- For list/link manipulation, variable lifetime affects register coloring.
  Reusing the long-lived selected-window variable and forming the head pointer
  as a byte-offset cast made `fn_800C0BD4` exact.
- Assignment order can select the intended add destination. In
  `AudioGetSoundVol`, masking `packedId`, then assigning the bank base to
  `firstSound`, and finally writing `firstSound = packedId + firstSound`
  emits the target `add r0,r3,r0`.
- A tiny comparison helper can be useful even when it inlines completely.
  `worldAnimIndexMatches(entry, index)` induced the target register choice in
  `FindWobjWanim` without changing the generated control flow.

## Recovered world-animation mapping

Xbox object rosters, GameCube call flow, and Ghidra behavior agree on:

| GameCube address | Name |
| --- | --- |
| `0x80055CB8` | `FindWobjWanim` |
| `0x80055D08` | `DoWorldAnimation` |
| `0x80055E04` | `WorldObjectExplode` |
| `0x80055E60` | `WorldExplosion` |

`DoWorldAnimation` is now translated and structurally correct; its remaining
26 diff lines are a saved-register rotation. `DoWorldAnimSub` takes the
animation-header base as its third argument and resolves the byte-swapped
stream-relative sequence offset before calling `CalcAnimData`. The previous
zero argument was a semantic stub and should not be restored.
