#!/usr/bin/env python3
"""Window-relative relocation SYMBOL NAME maps — the one shared copy.

Since the run-28 name-bound hash migration, ``webfrank._relocation_sha256``
hashes each relocation's symbol NAME instead of its ELF ``r_info`` INDEX
(which is TU-global and renumbers on any unrelated edit), and it REFUSES a
relocation whose name it was not given. Every deriver that still passed
bare ``(offset, info, addend)`` triples therefore raised

    ValueError: relocation hash needs the symbol name for the relocation
                at +0xN

on any window carrying a relocation, while windows with none kept working —
so the breakage looked like a property of the FUNCTION rather than of the
un-migrated CALLER, and a sweep with a blanket ``except Exception`` turned
it into false "does not close" rows.

The logic lived only in tools/gdl/composed_census/ch_derive.py, which four
other derivers could not reach; this module is the shared home and
ch_derive delegates to it, so there is exactly ONE implementation to keep
in step with apply_patch.

Both functions reproduce exactly the mapping ``apply_patch`` builds for
``our_symbols``, so hashes derived here agree with the ones the shipped
stage recomputes.
"""


def region_symbols(relocations, lo, hi):
    """Window-relative relocation offset -> symbol NAME.

    ``relocations`` is ``webfrank._function_text_relocations`` output:
    function-relative RAW r_offset -> (type, name). The raw offset is NOT
    floored — an R_PPC_EMB_SDA21 relocation sits at instruction+2 — so
    membership is tested on the floored offset while the key keeps the raw
    one, which is the same keying apply_patch uses.
    """
    return {offset - lo: name
            for offset, (_type, name) in relocations.items()
            if lo <= (offset & ~3) < hi}


def moved_symbols(window_symbols, order):
    """``region_symbols`` carried through a permutation, as apply_patch does.

    ``order[d] = s`` means destination slot d takes source slot s, so the
    inverse map sends each source instruction to its destination; the
    sub-word remainder (the SDA21 +2) rides along unchanged.
    """
    destination_by_source = {s: d for d, s in enumerate(order)}
    return {destination_by_source[offset // 4] * 4 + offset % 4: name
            for offset, name in window_symbols.items()}
