"""Is the +0x1c8 recolor refusal caused by the WF composition, or pre-existing?

A: raw ours -> copy_register_fields(raw ours, target)      [no WF stages]
B: composed  -> copy_register_fields(composed, target)     [with WF stages]

PINNED PREMISE. WINDOWS below are literal byte offsets into one function,
recorded when both sites emitted a zero-web copy where the target emits a
fresh `li`. Improving either site retires that premise: the recorded
permutation no longer describes our stream and copy_register_fields refuses
on non-register bits. That refusal is the correct answer for a stale premise,
so it is reported rather than raised -- a dead investigation script must not
fail the build. Re-derive WINDOWS from the current streams before trusting
any output.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Rule-17 promotion damage; see the note in wf_detail.py (run-43 item 9).
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))       # tools/gdl
sys.path.insert(0, HERE)
import cliscreen  # noqa: E402
cliscreen.help_only(__doc__)  # the probe below runs a live analysis; --help must not
import webfrank as wf  # noqa: E402
from wf_detail import load  # noqa: E402

UNIT, NAME = "game/anim/atree", "fn_8001267C"
WINDOWS = [(0x1e0, 0x1ec, [1, 0, 2]), (0x378, 0x384, [2, 0, 1])]


def check(label, pre, tgt):
    try:
        post, n = wf.copy_register_fields(pre, tgt)
    except ValueError as e:
        print(f"{label}: PREMISE STALE -- copy_register_fields refused: {e}")
        print("  The pinned WINDOWS no longer describe our stream; the site "
              "was improved. Re-derive them before reading further.")
        return
    try:
        wf.verify_consistent_recolor(pre, post, jumptable_targets=set(),
                                     relocated_offsets=set(), call_targets={})
        print(f"{label}: {n} fields, verify ACCEPT")
    except ValueError as e:
        print(f"{label}: {n} fields, verify REFUSE -- {e}")


def main():
    ours, tgt, _ = load(UNIT, NAME)
    cur = bytearray(ours)
    for lo, hi, order in WINDOWS:
        atoms = [bytes(cur[lo + i * 4:lo + i * 4 + 4])
                 for i in range((hi - lo) // 4)]
        cur[lo:hi] = b"".join(atoms[s] for s in order)
    for off in (0x1e8, 0x380):
        struct.pack_into(">I", cur, off, wf._u32(tgt, off))
    check("B composed then recolor ", bytes(cur), tgt)

    # Where is r3 last defined before +0x1c8, and what is the loop shape?
    words = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
    succ, _c = wf._successors(words, set(), set())
    preds = {}
    for i, ts in enumerate(succ):
        for t in ts:
            preds.setdefault(t, []).append(i)
    site = 0x1c8 // 4
    print(f"\npreds of +0x1c8: {[hex(p*4) for p in preds.get(site, [])]}")
    for i in range(site - 1, site - 24, -1):
        _r, w = wf._word_effects(words[i])
        if ("g", 3) in w:
            print(f"last def of g3 before +0x1c8: +0x{i*4:x} "
                  f"0x{words[i]:08x} (target 0x{wf._u32(tgt,i*4):08x})")
            break
        if i * 4 in [p * 4 for p in preds.get(site, [])]:
            pass
    else:
        print("no def of g3 in the 23 words before +0x1c8")
    print(f"preds of +0x184..+0x1cc region entry: ")
    for i in range(0x184 // 4, 0x1d0 // 4):
        if len(preds.get(i, [])) > 1 or (preds.get(i) and
                                         preds[i] != [i - 1]):
            print(f"   +0x{i*4:x} preds {[hex(p*4) for p in preds[i]]}")


def usage():
    print(__doc__)
    print(f"usage: {os.path.basename(__file__)} [--help]")
    print(f"Takes no arguments. Probes {UNIT}::{NAME} at the pinned WINDOWS "
          "above and prints whether the recolor verifies.")


if __name__ == "__main__":
    if {"-h", "--help"} & set(sys.argv[1:]):
        usage()
        sys.exit(0)
    main()
