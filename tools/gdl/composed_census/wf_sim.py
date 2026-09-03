"""Simulate the composed permute + INVERSE-copy-form + recolor close of
game/anim/atree::fn_8001267C, using webfrank's own guards.

STEP 0 (C1 law) is run first on each window: the permutation must be legal
IN OUR COLOURING or the composition is a non-member and must not be pinned.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Rule-17 promotion damage; see the note in wf_detail.py (run-43 item 9).
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))       # tools/gdl
import webfrank as wf  # noqa: E402
from wf_detail import load  # noqa: E402

UNIT, NAME = "game/anim/atree", "fn_8001267C"
WINDOWS = [(0x1e0, 0x1ec, [1, 0, 2]), (0x378, 0x384, [2, 0, 1])]


def main():
    ours, tgt, _ = load(UNIT, NAME)
    cur = bytearray(ours)

    # --- STEP 0: permutation legality in OUR colouring -------------------
    for lo, hi, order in WINDOWS:
        region = bytes(cur[lo:hi])
        try:
            wf.check_permutation_dependences(region, order, None)
            print(f"STEP0 [0x{lo:x},0x{hi:x}) order {order}: ACCEPT "
                  f"(strictest, exit_dead=None)")
        except ValueError as e:
            print(f"STEP0 [0x{lo:x},0x{hi:x}) order {order}: REFUSE -- {e}")
            return 1

    # --- stage 1: permute ------------------------------------------------
    for lo, hi, order in WINDOWS:
        atoms = [bytes(cur[lo + i * 4:lo + i * 4 + 4])
                 for i in range((hi - lo) // 4)]
        cur[lo:hi] = b"".join(atoms[s] for s in order)
    print("stage1 permute: applied")

    # --- stage 2: inverse copy form -------------------------------------
    words = [wf._u32(cur, o) for o in range(0, len(cur), 4)]
    successors, _c = wf._successors(words, set(), set())
    entries = wf._entry_indexes(successors)
    for off in range(0, len(cur), 4):
        ow, tw = wf._u32(cur, off), wf._u32(tgt, off)
        if ow == tw:
            continue
        a, b = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
        if a and b and a[0] == "copy" and b[0] == "li" and a[1] == b[1]:
            # ours: rD <- rS ; target: rD <- K.  Need OUR rS == K here.
            try:
                d = wf.prove_constant_source(
                    words, off // 4, a[2], b[2], entries, set())
                print(f"stage2 INVERSE +0x{off:x}: ours {a} target {b} -- "
                      f"dominating `li r{a[2]},{b[2]}` proved at +0x{d*4:x}")
                import struct
                struct.pack_into(">I", cur, off, tw)
            except ValueError as e:
                print(f"stage2 INVERSE +0x{off:x}: REFUSED -- {e}")
                return 1

    # --- stage 3: copy_register_fields ----------------------------------
    pre = bytes(cur)
    recolored, n = wf.copy_register_fields(bytes(cur), tgt)
    cur = bytearray(recolored)
    print(f"stage3 copy_register_fields: {n} fields")

    # --- result ----------------------------------------------------------
    diffs = [o for o in range(0, len(cur), 4)
             if wf._u32(cur, o) != wf._u32(tgt, o)]
    print(f"\nRESULT: {len(diffs)} differing words remain "
          f"({'BYTE-EXACT' if not diffs else 'NOT EXACT'})")
    for o in diffs[:20]:
        print(f"   +0x{o:x} ours 0x{wf._u32(cur,o):08x} tgt 0x{wf._u32(tgt,o):08x}")

    try:
        wf.verify_consistent_recolor(pre, bytes(cur), jumptable_targets=set(),
                                     relocated_offsets=set(), call_targets={})
        print("verify_consistent_recolor: ACCEPT")
    except ValueError as e:
        print(f"verify_consistent_recolor: REFUSE -- {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
