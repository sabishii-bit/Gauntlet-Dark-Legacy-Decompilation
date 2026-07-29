# `fakelib.c` PS2 mapping and layout notes

## Confirmed SDK symbols

These identities are reflected in source, `config/GUNE5D/symbols.txt`, all
callers, and the Ghidra project:

- `sceFileSize@800AEAD0`
- `WaitSema@800AF1B8`
- `SignalSema@800AF1C0`
- `CreateSema@800AF1C8`
- `GetThreadId@800AF1D0`
- `DIntr@800AF1E0`
- `EIntr@800AF1E8`
- `scePadSetActAlign@800AF24C`
- `sceMtapInit@800AF538`

The 2026-07-29 shim audit removed the rest of the address-only function
names.  The Xbox PDB roster supplies the spelling; GCN call arguments and call
order supply the identity:

- IOP bootstrap: `sceSifInitRpc@800AEA54`,
  `sceSifSyncIop@800AEA58`, `sceSifRebootIop@800AEA60`,
  `sceSifLoadFileReset@800AEA68`, `sceSifLoadModule@800AEA70`,
  `sceSifInitIopHeap@800AEA78`, `sceSifLoadElfPart@800AEA80`, and
  `sceFsReset@800AEA90`.
- Cache/pad: `FlushCache@800AF1D8`, `sceGsSyncPath@800AF1DC`,
  `scePadEnterPressMode@800AF1F0`, `scePadInfoPressMode@800AF1F8`,
  `scePadSetMainMode@800AF200`, `scePadGetReqState@800AF208`, and
  `scePadInfoAct@800AF2D4`.
- MTAP/GS: `sceMtapPortClose@800AF540`,
  `sceGsExecLoadImage@800AF544`, `sceGsSetDefLoadImage@800AF54C`,
  `Deci2Call@800AF554`, `sceGsSyncV@800AF55C`,
  `sceGsSwapDBuff@800AF564`, `sceGsSetDefDBuff@800AF568`,
  `sceGsResetPath@800AF56C`, and `sceGsResetGraph@800AF570`.

Particularly useful discriminator: the raw instructions in `joyGetStatus`
retain the SDK constants even though all five callees are constant-return
stubs.  `(port, slot, 1, 3)` identifies `scePadSetMainMode`;
`(port, slot, -1, 0)` identifies `scePadInfoAct`; the no-extra-argument poll
between those requests identifies `scePadGetReqState`.  The neighboring
state-machine phases then pin `scePadInfoPressMode` and
`scePadEnterPressMode`.  This is much stronger than assigning names by
address order.

The PB error renderer gives the equivalent graphics signature test:
descriptor setup followed by a pixel pointer is
`sceGsSetDefLoadImage`/`sceGsExecLoadImage`; the six-argument display-buffer
descriptor call is `sceGsSetDefDBuff`; the surrounding empty calls are
reset/swap/sync/cache operations.  `pb_frame`'s field-return test pins the
constant-one function as `sceGsSyncV`, while the three-argument debug bridge
pins `Deci2Call`.

`DiskErrorStr` is an exact Xbox PDB data name.  The GCN-only DVD helper and
state have semantic names (`sDvdReadSync`, `sFileSlots`, `sDvdBusy`,
`gDvdScratchFileInfo`, `gDiskErrorShown`) because there is no Xbox
implementation with equivalent storage to borrow.

The semaphore identities are stronger than a PDB-order guess:
`audio/mempool.c` repeatedly performs
`GetThreadId -> WaitSema -> critical section -> SignalSema`.
`pb_objects.c` independently creates the semaphore and brackets its free-list
mutation with `DIntr`/`EIntr`.

## Cold-block placement technique

Explicit forward labels can reproduce a target cold-block order while
retaining portable C. In `sDvdReadSync`, the optimizer would otherwise move the
`status == 3` body after the DVD-error switch. Writing the source as:

1. range tests,
2. `dvd_busy`,
3. `dvd_error`,
4. `dvd_done`

reproduces the target's complete status-dispatch layout. The remaining
difference is one extra `li 0`: the target shares the message/status zero in
`r28`, while this compiler run rematerializes a volatile `r0` for two byte
stores. Pointer/u32 message types and routing the stores through the message or
status local do not merge the live ranges; treat it as an allocator/
rematerialization wall unless a new lever appears.
