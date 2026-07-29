# `ml_fmath.c` PS2 math-shim progress (2026-07-29)

The address-only Euler/angle stubs were revisited assembly-first.

## Exact gains

- `AddAngle` (`0x800BD360`) — exact opcode/addend stream; only the private
  `.sdata2` symbol spelling differs in `fndiff`.
- `SubAngle` (`0x800BD3A4`) — same.
- `FixAngle` (`0x800BD3E8`) — same.
- `GetYawPitch` (`0x800BD428`) — fully `fndiff OK`.

The three angle functions use float parameters/results but deliberately use
double literals for `2*pi`, `pi`, and `-pi`.  This produces the target
`lfd` + double add/sub + `frsp` loops.  Adding `f` suffixes changes the
instruction forms and is wrong.

`GetYawPitch` needed two independent source-shape tells:

1. `u8 unused[8]` restores the target's 0x30-byte frame.
2. A named `distance` result between `fqdist` and the second `atan2` forces
   the target `fmr f0,f1` followed by `fmr f2,f0`; an inline expression
   coalesces the result directly into `f2` and loses one instruction.

Declaring `z` before `x` also preserves the target argument-load order for
the first `atan2`.

## Newly translated constructors

`CreateYPRMatrix`, `CreateRYPMatrix`, and `CreatePYRMatrix` now contain their
full recovered matrix formulas and have exact target instruction counts
(65/65, 64/64, 67/67).

- YPR: 40 real lines, saved-FPR coloring plus expression operand order.
- RYP: byte-exact (only private pool-symbol spelling is suppressed).
- PYR: 38 real lines, saved-FPR coloring plus expression operand order.

Separate zero stores in address order (`[3]`, `[7]`, `[11]`) are required.
A chained assignment emits them in reverse order.  `CreateRYPMatrix` also
needs an unused eight-byte local for the target 0x48-byte frame.  Its final
`f6`/`f7` web swap was solved by declaring `b` before `s2` but assigning it
after `a`:

```c
f32 b;
f32 s2 = -ffsin(angles[2]);
f32 a = -c2 * s1;
b = -s2 * s1;
```

Moving only the initialized `a`/`b` declarations was neutral.  Splitting a
declaration from its assignment changes MWCC's virtual-register ordering
without changing the instruction schedule, which made all 256 bytes exact.

## Data ownership warning

Do not claim `.sdata2` yet.  The target range is approximately
`0x80348DA0..0x80348ED8`, but the current object emits only `0x120` bytes
because `ExtractYPR`, `ExtractPYR`, and `CreateDirMatrix` are still stubs.
Claiming the partial pool would shift later constants when those bodies are
filled.  `fndiff real 0` validates the angle instruction/addend shape, not
the final pool layout; the eventual TU flip still requires `datadiff`.
