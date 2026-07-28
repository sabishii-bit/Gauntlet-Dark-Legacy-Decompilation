# `adstream.c` lifecycle notes

Five GameCube streaming-audio functions are now byte-exact:

- `AdsClose` (28 instructions)
- `AdsDelete` (21 instructions)
- `AdsNew` (55 instructions)
- `AdsOpen` (47 instructions)
- `adsLockCallback` (10 instructions)

Recovered `ADSTREAM` fields include the `FileBuf` handle at `+4`, work buffer
at `+8`, ring size/used counts at `+0x10/+0x14`, ring pointer/cursors at
`+0x24/+0x28/+0x2C`, and block count at `+0x64`.

The Xbox PDB also confirms these GCN globals:

- `gADS@80320B00`
- `gBuf@8034526C`
- `gAddrSpuNext@80345278`
- `gAddrSpuTop@8034527C`
- `sizeVoiceLoop@80345280`
- `halfVoiceLoop@80345284`
- `sShortenedSizeVoiceLoop@8034528C`
- `sShortenedHalfVoiceLoop@80345290`

Codegen techniques:

- Keep `_AdsThread` under `#pragma dont_inline`; its scaffold is currently
  empty, so without the pragma MWCC inlines it away from `adsLockCallback`.
- `AdsNew` must initialize a shared `result = 0` and branch to one final return
  label. Early `return 0` statements add separate `li/b` paths. Declaring
  `result` before the stream pointer also yields the target `r31`/`r30`
  allocation.
- Spell the occupied-stream test as an explicit zero/one branch, not a C
  boolean assignment; the target retains `cmplwi/li/beq/li`.
- `AdsOpen` has two distinct paths: reuse the existing `FileBuf` with
  `FileBufOpen`, or allocate one with `FileBufStart` and register it through
  `dcsSetStreamFlag`. A stream in status `0x2000` clears that bit from the
  global mask, resets its status, and rewinds `gAddrSpuNext` by
  `sizeVoiceLoop * blocks`; other successful reopens increment `endCount`.
