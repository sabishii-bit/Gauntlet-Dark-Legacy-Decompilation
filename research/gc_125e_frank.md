# GC MW 1.2.5e and Frank

## What 1.2.5e is

`GC/1.2.5e` is not a recovered retail compiler build. The compiler executable
differs from vanilla 1.2.5 by one byte, enabling CodeWarrior's profile
instrumentation. Its output contains an eight-byte `bl; nop` marker at return
sites. Those markers perturb the scheduler.

decomp.me compiles a TU twice and runs Frank:

1. vanilla `GC/1.2.5` produces the authoritative ELF object;
2. profile-patched `GC/1.2.5e` produces a differently scheduled `.text`;
3. `frank.py` removes the markers, repairs return/epilogue patterns, and places
   the resulting `.text` into the vanilla object.

The current definition is `mwcc_233_163e` in decomp.me's
`backend/coreapp/compilers.py`.

## Melee history

Melee added Frank in commit `5ecacba74` (`Postprocess epilogues with
frank.py`). It initially applied Frank to a small explicit TU list and later
used it across Melee/sysdolphin/card/pad objects.

Melee commit `c368df305` (`Remove frank, build with 1.2.5n`) removed that
pipeline and directly selected `GC/1.2.5n` for the same object families.
Current Melee configuration still calls this choice `fix_epilogue`.

The local `GC/1.2.5n/README.md` explains the direct patch: it marks stack-frame
deallocation as side-effecting so the scheduler cannot move stack accesses
below `addi r1,r1,N`. This is the likely fix from Metrowerks's missing build
167. The 1.2.5n executable differs from vanilla by 53 bytes; the profile 1.2.5e
executable differs by one byte.

Therefore 1.2.5e+Frank and 1.2.5n are two mechanisms for the same epilogue
scheduler correction, not two independent general schedulers.

## GDL results

Use the focused probe with the TU's configured compiler as Frank's body:

```text
python tools/gdl/frank_probe.py game/g3d/sndvoice sndVoiceUpdateAll \
    --body 1.2.5n --vanilla 1.2.5
```

For `sndVoiceUpdateAll`:

| artifact | target/current | normalized residual |
| --- | ---: | ---: |
| configured 1.2.5n | 822/822 | 82 register-only lines |
| vanilla 1.2.5 | 822/822 | 82 register-only lines |
| official Frank (1.2.5 + profile) | 822/822 | 82 register-only lines |
| custom Frank (1.2.5n + profile) | 822/822 | 82 register-only lines |

The official Frank object and custom Frank object are byte-identical to the
direct 1.2.5n object. The result is unchanged across all 23 unique historical
`sndvoice.c` source revisions that still compile with the current headers.

The remaining mismatch is internal to the `mixDirty` block: the opcode stream,
frame, prologue, and epilogue are exact, but one temporary web uses `r7` where
the target uses `r8`, followed by an `r8/r9` pair. Frank cannot change this
because the profile compiler produces the same allocation after marker removal.
This is a register-allocation residual, not the stack-deallocation scheduler
bug that Frank/1.2.5n repairs.

Frank must remain selective. For example, current `cvt` in `vsprintf.c` is
exact with vanilla 1.2.5, while Frank changes its epilogue and creates a
two-line mismatch.

## Project tooling

- `tools/gdl/frank.py` is a vendored, library-friendly implementation of the
  final Melee algorithm. It reports marker count, changed `.text` bytes, and
  size-mismatch fallback.
- `tools/gdl/frank_probe.py` compiles the configured body, vanilla body,
  profile object, official Frank object, and a custom body+profile object under
  the TU's exact Ninja flags.
- `tools/gdl/flagsweep.py` accepts
  `CC=1.2.5e;FRANK_BODY=<version>` for targeted sweeps.
- The normal Ninja harness accepts `frank_profile_mw_version` on any object.
  `mw_version` selects Frank's authoritative body object and
  `frank_profile_mw_version="GC/1.2.5e"` adds the profile compile and merge.
  An optional ordinary `postprocess` runs after Frank, so the stages compose.
  The stacked Frank+`webfrank` path was build-tested on `sndvoice`; its checked
  configuration omits Frank because that stage is byte-identical to direct
  1.2.5n there and only `webfrank` contributes to the match.
- `tools/gdl/webfrank.py` is a separate, hash-guarded register-web
  postprocessor for the allocator-only class that Frank cannot affect. It
  decodes a deliberately small set of PowerPC integer forms and recolors only
  whitelisted GPR operands inside function-relative ranges. Complete before
  and after function hashes make source/compiler drift a hard build failure.

The current `sndVoiceUpdateAll` rule uses `webfrank` to correct the two known
temporary-register webs. The resulting function bytes match the target
exactly, while the object remains classified NonMatching so its unrelated
data is not linked into the DOL. This raises overall matched progress without
changing the already-matching final executable.

Do not switch whole libraries to 1.2.5e. Probe a specific function and retain
the hybrid only when its function and sibling scores improve. `webfrank` is a
different mechanism and must not be described as Frank, a recovered compiler,
or evidence that 1.2.5e produced the retail allocation.
