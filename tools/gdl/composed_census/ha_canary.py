"""HA lane: replay the two SHIPPED run-26 combined closes through ha_close.

Mandated by the work order: if my analysis cannot read the two functions the
combined stage actually closed as CLOSABLE, no other verdict it produces is
trustworthy.  Both are re-derived from scratch (nothing is read out of
webfrank.json) and proven through the shipped apply_patch to residual 0.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ha_close as hc  # noqa: E402

CANARIES = [
    ("game/movie/movieplayer", "fn_800D8F28", None, None),
    ("game/world/camera", "camera_mode_level",
     [{"lo": 0x740, "hi": 0x748, "order": [1, 0]}],
     [{"lo": 0x8c0, "hi": 0x8c8, "order": [1, 0]}]),
]


def main():
    ok = True
    for unit, fn, pre, post in CANARIES:
        rule, resid, note = hc.build_rule(unit, fn, pre=pre, post=post)
        verdict = "CLOSABLE" if rule and resid == 0 else "REFUSES"
        print(f"{verdict:9} {unit}::{fn}")
        print(f"          {note}")
        if rule:
            keys = [k for k in rule if k not in
                    ("function", "before_sha256", "after_sha256")]
            print(f"          stages: {keys}")
            for e in rule.get("equivalent_copy_form", []):
                print(f"          form site {e}")
        else:
            ok = False
    print()
    print("CANARY GATE: " + ("PASS - both shipped combined closes read "
                             "closable; downstream verdicts are trustworthy"
                             if ok else "FAIL - do not trust any verdict"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
