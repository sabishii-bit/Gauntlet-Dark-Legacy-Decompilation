"""Re-screen dcsHandleRequest +0x1e0 now that the INVERSE direction exists.

DC's park (attempt.DC_dcshandlerequest-jumptable-settled-and-webfrank-barred)
lists four independent bars.  BAR 2 was the missing direction, which this
lane has now built.  Measure the remaining three rather than assuming them.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Rule-17 promotion damage; see the note in wf_detail.py (run-43 item 9).
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))       # tools/gdl
import webfrank as wf  # noqa: E402
from wf_detail import load  # noqa: E402

UNIT, NAME = "game/audio/dcsdrv", "dcsHandleRequest"


def main():
    ours, tgt, _ = load(UNIT, NAME)
    print(f"{NAME}: {len(ours)//4} insns")
    for off in range(0x1d8, 0x1f4, 4):
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        print(f"  +0x{off:<4x} ours 0x{ow:08x} {str(wf.decode_copy_form(ow)):20}"
              f" tgt 0x{tw:08x} {wf.decode_copy_form(tw)}")

    # Load the real object so jumptable targets / relocations are real.
    from wf_census import our_path
    op, _is_raw = our_path(UNIT)
    odata = bytearray(open(op, "rb").read())
    osec = wf._sections(odata)
    s = wf._find_symbol(odata, osec, NAME)
    jt = wf._jumptable_targets(odata, osec, s.section_index,
                              s.value, s.value + s.size)
    rels = wf._function_text_relocations(odata, osec, s.section_index,
                                         s.value, s.value + s.size)
    print(f"\n0x1e0 is a jumptable case-entry: {0x1e0 in jt}"
          f"   (jumptable targets: {len(jt)})")

    for proof in ("dominating_def_inverse", "dominating_def",
                  "unconditional"):
        try:
            wf.equivalent_copy_form(
                ours, tgt, [{"at": "0x1e0", "proof": proof}],
                relocated_offsets=set(rels),
                target_relocated_offsets=set(),
                jumptable_offsets=jt,
            )
            print(f"  proof={proof:32} ACCEPTED (!)")
        except ValueError as e:
            print(f"  proof={proof:32} REFUSED -- {e}")

    # Bar 4 in isolation: could the dominating-def scan EVER discharge here,
    # if the destination matched?  Ask prove_constant_source directly.
    words = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
    succ, _c = wf._successors(words, {o // 4 for o in rels},
                              {o // 4 for o in jt})
    entries = wf._entry_indexes(succ)
    print(f"\n  +0x1e0 index in entry_indexes: {0x1e0 // 4 in entries}")
    try:
        wf.prove_constant_source(words, 0x1e0 // 4, 31, 0, entries,
                                 {o // 4 for o in rels})
        print("  prove_constant_source(r31==0 @0x1e0): ACCEPTED")
    except ValueError as e:
        print(f"  prove_constant_source(r31==0 @0x1e0): REFUSED -- {e}")


if __name__ == "__main__":
    main()
