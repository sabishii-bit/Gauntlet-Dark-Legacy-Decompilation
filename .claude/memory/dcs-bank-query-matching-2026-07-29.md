# `dcsBankQuery` exact match (2026-07-29)

`dcsBankQuery` in `game/audio/dcs.c` is exact (80 bytes). The full linked DOL
SHA-1 passes.

The calculations and types were already correct. Early returns let MWCC reuse
the return register `r3` for the bank-table address and rematerialize `1` just
before returning. Retail keeps `result = 1` live in `r3` across the successful
path. Expressing the zero-bank path as an `else`, assigning `result = 0`, and
sharing the final return reproduces that lifetime:

```c
s32 result = 1;
if (bank != 0) {
    /* fill outputs */
} else {
    *handle = 0;
    *size = 0;
    result = 0;
}
return result;
```

For small leaf queries whose arithmetic matches but whose return register is
reused as scratch, compare early-return and single-exit source shapes before
trying register qualifiers.
