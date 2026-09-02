"""Generate + validate the fn_8001267C composed rule through real apply_patch.

Run from the repository root:
  python tools/gdl/composed_census/wf_mkrule.py [--out PATH]

PROMOTION DAMAGE, repaired run 38. This script was written in the WF lane's
scratch directory at the repository ROOT and promoted into
tools/gdl/composed_census/ unchanged, which broke it in both of the ways
AGENTS.md rule 17 names: its `ROOT` was computed as HERE/.. (correct beside
the repo root, two levels short from here), so it died on
`ModuleNotFoundError: No module named 'webfrank'` before reaching any
webfrank call at all; and it wrote a generically-named `rule.json` BESIDE
itself, into a tracked directory every lane shares, where the next lane to
draft a rule would silently overwrite it. The output is now `--out`,
defaulting under build/GUNE5D/ (gitignored and per-worktree).

The run-38 work order named a different cause — `_relocation_sha256` called
with one argument since the name-bound migration. That call is CORRECT:
`symbols` is an optional parameter, and the empty relocation list this
script passes never reaches the lookup that needs it.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402

OURS = os.path.join(ROOT, "build/GUNE5D/src/game/anim/.postprocess/body/atree.o")
TGT = os.path.join(ROOT, "build/GUNE5D/obj/game/anim/atree.o")
NAME = "fn_8001267C"
WINDOWS = [(0x178, 0x180, [1, 0]),
           (0x1e0, 0x1ec, [1, 0, 2]),
           (0x378, 0x384, [2, 0, 1])]

AUDIT = (
    "r3<->r4 TRANSPOSITION WHOSE TWO DEFINING WORDS ARE IDENTICAL. "
    "verify_consistent_recolor infers the renaming forward from each "
    "definition's own register fields, so at +0x178 `li r4,0` and +0x17c "
    "`li r3,0` -- byte-IDENTICAL in both streams -- it records the identity "
    "map g3->3, g4->4, and then refuses the first swapped use it reaches "
    "(+0x1c8: `use of g3 does not correspond to g4 under the running "
    "renaming'). The renaming IS consistent; it is a transposition the "
    "forward inference cannot represent, because at the two defs either "
    "pairing is equally consistent with the bytes. MANUAL EQUIVALENCE "
    "AUDIT, all four facts measured on the objects this session: (1) the "
    "swap is confined to [0x178,0x1cc) -- ours r3+=1 / r5=r0+r4 / r4+=0x24 "
    "/ cmpw r3,r0 against the target's r4+=1 / r5=r0+r3 / r3+=0x24 / cmpw "
    "r4,r0 -- and is a complete 2-cycle with no third register involved. "
    "(2) BOTH registers are initialised to the SAME constant 0 by the "
    "identical pair at +0x178/+0x17c, so the two colourings agree on the "
    "machine state entering the span and the entry-side pairing is "
    "immaterial. (3) BOTH are DEAD at the span's exit: after the loop "
    "back-edge at +0x1cc the first touch of r3 is a DEF at +0x1e0 and the "
    "first touch of r4 is a DEF at +0x1f8, each before any use on every "
    "path out, so no swapped value escapes the span. (4) every word "
    "outside the span is either byte-identical or covered by the proven "
    "stages. The composed result is byte-equal to the target (0 differing "
    "words, measured). The narrow sound fix is to let the checker consider "
    "both pairings at a definition whose two registers provably hold the "
    "same constant; that is checker work, not rule work, and is recorded "
    "as the follow-up."
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(
        ROOT, "build", "GUNE5D", "wf_mkrule_rule.json"),
        help="where to write the rule draft (default: under build/GUNE5D/,"
             " which is gitignored and per-worktree)")
    args = ap.parse_args()
    odata = bytearray(open(OURS, "rb").read())
    tdata = bytearray(open(TGT, "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    s = wf._find_symbol(odata, osec, NAME)
    t = wf._find_symbol(tdata, tsec, NAME)
    ostart = osec[s.section_index].offset + s.value
    tstart = tsec[t.section_index].offset + t.value
    ours = bytes(odata[ostart:ostart + s.size])
    tgt = bytes(tdata[tstart:tstart + t.size])

    orels = wf._function_text_relocations(
        odata, osec, s.section_index, s.value, s.value + s.size)

    windows = []
    for lo, hi, order in WINDOWS:
        region = ours[lo:hi]
        n = (hi - lo) // 4
        atoms = [region[i * 4:i * 4 + 4] for i in range(n)]
        permuted = b"".join(atoms[src] for src in order)
        inwin = [(o - lo) for o in orels if lo <= o < hi]
        if inwin:
            print(f"WINDOW [0x{lo:x},0x{hi:x}) HAS RELOCATIONS {inwin} "
                  f"-- hashes must be derived, not assumed")
        recs = []
        empty = wf._relocation_sha256(recs)
        windows.append({
            "start": hex(lo), "end": hex(hi), "order": order,
            "before_sha256": wf._sha256(region),
            "after_sha256": wf._sha256(permuted),
            "before_relocations_sha256": empty,
            "after_relocations_sha256": empty,
        })
        print(f"window [0x{lo:x},0x{hi:x}) order {order}: relocs in window = "
              f"{len(inwin)}")

    patch = {
        "function": NAME,
        "before_sha256": wf._sha256(ours),
        "after_sha256": wf._sha256(tgt),
        "audit": {"classification": "SCHEDULE_CANDIDATE",
                  "instructions": s.size // 4,
                  "permuted_instructions": 5},
        "mechanism": "PLACEHOLDER",
        "instruction_permutation": windows,
        "equivalent_copy_form": [
            {"at": "0x1e8", "proof": "dominating_def_inverse"},
            {"at": "0x380", "proof": "dominating_def_inverse"},
        ],
        "copy_register_fields": True,
    }
    if os.environ.get("WF_AUDIT"):
        patch["unproven_recolor_audit"] = AUDIT

    probe = bytearray(odata)
    before, after, changed = wf.apply_patch(probe, patch, bytes(tdata))
    final = bytes(probe[ostart:ostart + s.size])
    print(f"\napply_patch OK: changed={changed}")
    print(f"  before={before[:16]} after={after[:16]}")
    print(f"  BYTE-EQUAL TO TARGET: {final == tgt}")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(patch, handle, indent=2)
    print(f"  wrote {args.out}")


if __name__ == "__main__":
    main()
