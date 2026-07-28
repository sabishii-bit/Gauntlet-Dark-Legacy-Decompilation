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

The semaphore identities are stronger than a PDB-order guess:
`audio/mempool.c` repeatedly performs
`GetThreadId -> WaitSema -> critical section -> SignalSema`.
`pb_objects.c` independently creates the semaphore and brackets its free-list
mutation with `DIntr`/`EIntr`.

## Cold-block placement technique

Explicit forward labels can reproduce a target cold-block order while
retaining portable C. In `fn_800AEBF4`, the optimizer would otherwise move the
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
