# Audio core matching: 2026-07-29

## Exact gain

- `AudioSoundExists` is exact (136 bytes).
- `sndTestAcquire` is exact (280 bytes).
- `sndCmd1` is exact (248 bytes).

## Source-shape technique

The retail two-table scan uses `lwzu` to fold each table displacement into the
temporary entry pointer, but then recomputes the unadjusted base for the handle
load. The exact source needs all three pieces:

1. Keep `sAudioState` in a named `state` base local.
2. Express the first field as an explicit pointer update inside the condition:
   `*(s32*)(entry += displacement)`.
3. Scope both `#pragma opt_common_subs off` and
   `#pragma opt_propagation off` around the function.

With only the pointer update, MWCC keeps the original pointer in another
register and emits `addi` plus `lwz`. Disabling common-subexpression reuse lets
it use `lwzu`; disabling propagation preserves the retail `add base,index`
followed by a displacement load when returning the handle.

For an address that must be formed *after* a call, materialize the call result
first, then declare the typed shifted-base pointer:

```c
s32 voice = AXAcquireVoice(...);
SndState* entry = (SndState*)((u8*)state + i * 4);
entry->voice[0] = voice;
```

In `sndTestAcquire`, this emits the retail `add r6,r30,r31` after the call and
keeps `voice[0]` as a `3304(r6)` field access.  Declaring `entry` before the
call makes it live across the call and costs another nonvolatile register;
spelling the whole address arithmetically makes MWCC fold `3304` into the
index (`addi index,3304; add base,index`) instead.

The same pattern applies when no call precedes the address formation.  In
`sndCmd1`, indexing the aggregate directly (`state->nodes[index]`) folded the
`nodes` field displacement into the scaled index.  Casting the shifted base
back to its aggregate type keeps the field displacement separate:

```c
Node* node;
SndState* entry = (SndState*)((u8*)state + index * 4);
node = entry->nodes[0];
```

Declare the loaded field (`node`) before the shifted-base local (`entry`) when
the target wants the field in `r3` and the address in `r4`.  The declaration
order removed the last register rotation; an otherwise-unused 8-byte local
restored this function's retail stack frame.

## Near-match improvement

`AudioBankQueueName` is down from ten real diff lines to four. Materializing the
first-loop bank name in a block-local pointer makes MWCC compute argument one
before overwriting `r4` with argument two, reproducing the retail address/call
schedule. The remaining differences are one constant-copy form and one final
call-argument scheduling swap; park those after the session attempt cap.
