"""CH lane run-26: DERIVER canary.

claim.CH_composed-census-derived-order-closes-the-single-window-population
.20260901.v1 makes this mandatory before reading any new derivation result:
the SCANNER canary (cn_census.py) tests the scanner, not the deriver, and the
deriver is the component that actually decides.  Replay the shipped
derivations and require BYTE-IDENTICAL orders.

Expected orders, from that record:
  calc_wizard_pos                  [0,3,2,1,4]
  next_boss_hint / next_legend_hint[0,2,1,3,4,5,6]
  ProcessCritter                   identity -> permutation stage DROPPED
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl",
                                "composed_census"))
sys.path.insert(0, os.path.dirname(__file__))
import ch_derive  # noqa: E402

EXPECT = [
    ("game/ui/auxscreen", "calc_wizard_pos", 0x58, 0x6c, [0, 3, 2, 1, 4]),
    ("game/ui/options", "next_boss_hint", 0x10c, 0x128, [0, 2, 1, 3, 4, 5, 6]),
    ("game/ui/options", "next_legend_hint", 0x10c, 0x128, [0, 2, 1, 3, 4, 5, 6]),
    ("game/enemy/critter", "ProcessCritter", 0xb0, 0xbc, [0, 1, 2]),
]


def main():
    ok = True
    for unit, fn, lo, hi, want in EXPECT:
        rule = ch_derive.run(unit, fn, lo, hi, verbose=True)
        if rule is None:
            print(f"  !! {fn}: DERIVER PRODUCED NOTHING (expected {want})")
            ok = False
            continue
        perm = rule.get("instruction_permutation")
        got = perm["order"] if perm else list(range(hi - lo >> 2))
        identity = got == list(range(len(got)))
        if got != want:
            print(f"  !! {fn}: order {got} != expected {want}")
            ok = False
        elif identity and perm is not None:
            print(f"  !! {fn}: identity order SHIPPED as a permutation stage")
            ok = False
        else:
            tag = " (identity -> stage dropped, correct)" if identity else ""
            print(f"  OK {fn}: order {got}{tag}")
    print(f"\nDERIVER CANARY: {'PASS -- deriver trustworthy' if ok else 'FAIL -- do not read results'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
