# Animation and boss-camera batch (2026-07-29)

## Verified GameCube/Xbox animation mapping

The Xbox `ANIM.OBJ` roster is shifted relative to the old GameCube placeholders.
The following names were cross-checked against Ghidra behavior and call flow:

| GameCube address | Name | Evidence |
| --- | --- | --- |
| `0x8000EB54` | `AnimDone` | Tests the low stage byte at `animinfo + 0x36`. |
| `0x8000EB70` | `AnimateTree` | Calls `InitAnim` and `CalcAnimInfo`. |
| `0x8000ED70` | `InitAnim` | Initializes sequence/frame/playback state; float time is in `f1`. |
| `0x8000EF18` | `CalcAnimInfo` | Advances animation playback state. |
| `0x8000F184` | `AnimateTreeFrame` | Applies an explicit animation frame. |
| `0x8000F2D8` | `DoAnimation` | Walks animation data for a tree/object. |
| `0x8000F534` | `CalcAnimation` | Calls interpolation and angle-value helpers. |

The recovered `InitAnim` ABI is:

```c
s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active);
```

Do not restore the old `fn_8000ED70(void*, int, int, int, float)` declaration:
it placed the float and integer arguments in the wrong ABI registers.

## Exact functions in this batch

- `anim.c`: `AnimInit`, `InitAnimInfo`, `AnimDone`
- `anim_play.c`: `ZeroAnimData`, `CalcAnimData`
- `anim_play.c`: `InitAnimInvDeltaTable` (zero semantic diff; only pooled-label
  normalization in `fndiff`)
- `bosscam.c`: `BossCameraStart`

The full build moved from 1,974 / 351,172 exact functions/bytes to
1,981 / 351,904, and `build/GUNE5D/main.dol` passed the configured SHA-1 check.

## CodeWarrior techniques

- Declare proven read-only pooled floats as `extern const f32`. Otherwise MWCC
  conservatively reloads a zero/one constant after stores that might alias it.
- In the inverse-delta loop, use `double_one / (f32)i`. The narrowing cast emits
  the target `fsubs` between integer conversion and `fdiv`; `(f64)i` does not.
- Keep a tiny forwarding wrapper before its still-stubbed callee in source order.
  If MWCC sees the empty body first, it can remove the intended call.
- For `BossCameraStart`, a typed partial camera overlay plus
  `camera->view_scale *= scale` emits the target `lfs disp(base)` /
  `stfs disp(base)`. The expanded assignment emitted `lfsu` and stored at
  offset zero.

## Other recovered names

- `0x8009D400`: `AudioCursorChar`
- `0x8009D484`: `AudioBuzzer`
- `0x800D5F94`: `list_insert_size`
- `0x800D603C`: `list_insert_tail`
- `0x800D60B8`: `list_remove`

The allocator names were verified against both the Xbox MEMPOOL roster and the
actual intrusive-list pointer writes; the former placeholder names were
semantically inverted.
